#ifndef MSGTHREADS_H
#define MSGTHREADS_H


#include <iostream>
#include <ctime>

#include <functional>
#include <queue>
#include <variant>
#include <map>

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>

#include <cassert>
#include <cstddef>
#include <cstring>

extern "C"{
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
}



namespace msg
{

	struct msg_timeout_t
	{
		uint64_t data;
	};

	struct msg_error_t
	{
		uint64_t data;
	};

	template <typename... Ts>
	using message_variants_t = std::variant<Ts...>;


	struct ip_addr
	{
		constexpr
		ip_addr(uint8_t a3, uint8_t a2, uint8_t a1, uint8_t a0)
			: data_b{a3, a2, a1, a0}
		{
			data = ntohl(data);
		}
		union{
			uint8_t data_b[4];
			uint32_t data;
		};
	};

	struct port
	{
		constexpr
		port(uint16_t _data)
			: data{_data}
		{
		}
		uint16_t data;
	};


	enum class thread_type
	{
		  queue_thr
		, udp_thr
		, timer_thr
	};
	using queue_thr = std::integral_constant<thread_type, thread_type::queue_thr>;
	using udp_thr = std::integral_constant<thread_type, thread_type::udp_thr>;
	using timer_thr = std::integral_constant<thread_type, thread_type::timer_thr>;

	using byte_array_t = std::vector<std::uint8_t>;


	template <typename _MV>
	struct mx_queue_t
	{
		mx_queue_t() = default;
		mx_queue_t(mx_queue_t const&) = delete;
		mx_queue_t(mx_queue_t&&) = delete;
		mx_queue_t& operator=(mx_queue_t const&) = delete;
		mx_queue_t& operator=(mx_queue_t&&) = delete;

		template <typename MT
			, std::enable_if_t<std::is_constructible_v<_MV, MT>, bool> = true
		>
		constexpr
		auto& emplace (MT&& d) noexcept
		{
			{
				std::lock_guard<std::mutex> lg(mtx);
				queue.push(std::forward<MT>(d));
			}
			return *this;
		}
		std::queue<_MV> queue{};
		std::mutex mtx;
	};



	namespace _detale
	{

		struct _thread_interface_t
		{
			_thread_interface_t() = default;
			_thread_interface_t(_thread_interface_t const&) = delete;
			_thread_interface_t(_thread_interface_t&&) = delete;
			_thread_interface_t& operator=(_thread_interface_t const&) = delete;
			_thread_interface_t& operator=(_thread_interface_t&&) = delete;

			void join() noexcept
			{
				if (thread.joinable())
				{
					thread.join();
				}
			}
			bool joinable() noexcept
			{
				return thread.joinable();
			}

			std::atomic<std::uint32_t> status{0};
			std::atomic<std::uint32_t> stop{0};
			std::thread thread;
		};

	}



	template <
		typename... Ts
	>
	struct thread_interface_t{};


	template <
		  typename MV
	>
	struct thread_interface_t
	<
		  queue_thr
		, MV
	>
		: _detale::_thread_interface_t
	{
		using _detale::_thread_interface_t::_thread_interface_t;

		using messages_type = MV;

		//Передать сообщение
		template <typename MT
			, std::enable_if_t<std::is_constructible_v<MV, MT>, bool> = true
		>
		constexpr
		void send(MT&& d) noexcept
		{
			{
				std::lock_guard<std::mutex> lg(mtx);
				queue.push(std::forward<MT>(d));
			}
			cvar.notify_one();
		}

		std::mutex mtx;
		std::condition_variable cvar;
		std::queue<MV> queue;
	};


	template <
		  typename MV
	>
	struct thread_interface_t
	<
		  udp_thr
		, MV
	>
		: _detale::_thread_interface_t
	{
		thread_interface_t() = delete;
		thread_interface_t(thread_interface_t const&) = delete;
		thread_interface_t(thread_interface_t&&) = delete;
		thread_interface_t& operator=(thread_interface_t const&) = delete;
		thread_interface_t& operator=(thread_interface_t&&) = delete;

		using messages_type = MV;

		constexpr
		thread_interface_t(ip_addr _addr, port _port) noexcept
			: addr{_addr.data}
			, port{_port.data}
			, self_port{_port.data}
		{
		}
		constexpr
		thread_interface_t(ip_addr _addr, port _port, port _self_port) noexcept
			: addr{_addr.data}
			, port{_port.data}
			, self_port{_self_port.data}
		{
		}
		~thread_interface_t()
		{
			if (this->sock > 0) ::close(this->sock);
		}

		//Передать сообщение
		bool send(byte_array_t ba) noexcept
		{
			int cnt = 0;
			while ((this->status & 0x01) != 1)
			{
				if (++cnt > 100)
				{
					return false;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
			sockaddr_in _addr{};
			_addr.sin_family = AF_INET;
			_addr.sin_port = htons(this->port);
			_addr.sin_addr.s_addr = htonl(this->addr);
			sendto(this->sock, ba.data(), ba.size(), 0, reinterpret_cast<sockaddr*>(&_addr), sizeof(_addr));
			return true;
		}

		uint32_t addr = 0;
		uint16_t port = 0;
		uint16_t self_port = 0;
		int sock = 0;
	};


	template <
		typename _MV
	>
	struct thread_interface_t
	<
		  timer_thr
		, _MV
	>
		: _detale::_thread_interface_t
	{
		using _detale::_thread_interface_t::_thread_interface_t;
		using client_interface_t = thread_interface_t<queue_thr, _MV>;
		using clients_t = std::map<std::uint64_t, std::reference_wrapper<client_interface_t>>;

		template <
			typename... Ts
		>
		friend struct _thread_worker_t;

		void add_client(std::uint64_t id, client_interface_t& i)
		{
			m_clients.emplace(id, std::ref(i));
		}
		void remove_client(std::uint64_t id)
		{
			m_clients.erase(id);
		}
		clients_t clients()
		{
			return m_clients;
		}

		clients_t m_clients;
	};

	template <typename _I>
	struct thread_timer{};

	template <typename _MV>
	struct thread_timer<thread_interface_t<queue_thr, _MV>>
	{
		using type = thread_interface_t<timer_thr, _MV>;
	};

	template <typename _I>
	using thread_timer_t = typename thread_timer<_I>::type;



	template <typename W>
	struct worker_t
	{
		template <typename... Ts>
		worker_t(Ts&&... args)
			: worker{std::make_unique<W>(std::forward<Ts>(args)...)}
		{
		}
		void operator()()
		{
			worker->run();
		}
		std::unique_ptr<W> worker;
	};


	namespace net
	{
		namespace pack_type
		{
		constexpr uint8_t ack = 0;
		constexpr uint8_t put = 1;
		}

	   #pragma pack(push)
	   #pragma pack(1)
	   struct msg_head_pack_t
	   {
		  std::uint32_t seq_number;
		  std::uint32_t seq_total;
		  std::uint8_t type;
		  std::byte id[8];
	   };
	   #pragma pack(pop)

	   struct msg_head_t{
		  std::uint32_t seq_number;
		  std::uint32_t seq_total;
		  std::uint8_t type;
		  std::uint64_t id;
	   };

	   struct msg_udp
	   {
		  msg_head_t head;
		  byte_array_t data;
	   };

		using msg_udp_ts = message_variants_t
		<
			msg::net::msg_udp
		>;

		using msg_err = message_variants_t
		<
			msg_error_t
		>;

		using udp_interface_t = thread_interface_t<udp_thr, msg_udp_ts>;
	}

	namespace file_send
	{

		using file_t = std::map<std::uint32_t, byte_array_t>;
		using file_list_t = std::map<std::uint64_t, file_t>;
		using file_queue_t = std::queue<file_t>;

	}


    struct udp_test
    {
	   static
	   byte_array_t serializer(net::msg_udp const& m)
	   {
		  byte_array_t ba;
		  net::msg_head_pack_t hd;
		  hd.seq_number = htonl(m.head.seq_number);
		  hd.seq_total = htonl(m.head.seq_total);
		  hd.type = m.head.type;
		  memcpy(&hd.id, &m.head.id, 8);
		  ba.resize(sizeof(net::msg_head_pack_t));
		  memcpy(ba.data(), &hd, sizeof(hd));
		  std::move(m.data.begin(), m.data.end(), std::back_inserter(ba));
		  return ba;
	   }

	   static
	   net::msg_udp deserializer(byte_array_t& _ba)
	   {
		  net::msg_udp msg{};
		  net::msg_head_pack_t head{};
		  memcpy(&head, _ba.data(), sizeof(head));
		  msg.head.seq_number = ntohl(head.seq_number);
		  msg.head.seq_total = ntohl(head.seq_total);
		  msg.head.type = head.type;
		  memcpy(&msg.head.id, &head.id, 8);
		  std::move(_ba.begin()+=sizeof(head), _ba.end(), std::back_inserter(msg.data));
		  return msg;
	   }
    };


	template <typename SI, typename LI>
	class net_send_queue_t
	{
		using MV = typename SI::messages_type;

	public:
		net_send_queue_t(SI& si, LI& l)
			: self_i{si}
			, line_i{l}
		{
		}
		~net_send_queue_t()
		{
		}

		void run()
		{
			MV vmsg;
			self_i.status |= 0x01;
			while (!static_cast<std::uint32_t>(self_i.stop))
			{
				{
					std::unique_lock<std::mutex> ul(self_i.mtx);
					if (self_i.cvar.wait_for(ul, std::chrono::seconds(1),
						[this](){return !self_i.queue.empty();}))
					{
						vmsg = std::move(self_i.queue.front());
						self_i.queue.pop();
					}
					else
					{
						continue;
					}
				}
				if (!line_i.send(udp_test::serializer(std::get<net::msg_udp>(vmsg))))
				{
					std::cout << "Ошибка передачи!" << std::endl;
				}
			}
			self_i.status &= ~0x01u;
		}

	private:
		SI& self_i;
		LI& line_i;
	};


	inline
	uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len)
	{
		int k;

		crc = ~crc;
		while (len--) {
			crc ^= *buf++;
			for (k = 0; k < 8; k++)
				crc = crc & 1 ? (crc >> 1) ^ 0x82f63b78 : crc >> 1;
		}
		return ~crc;
	}


}

#endif // MSGTHREADS_H
