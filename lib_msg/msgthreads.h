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

#include "conv_b_l.h"

namespace msg
{
	using std::cout;
	using std::endl;

	template <typename>
	constexpr bool always_false_v{false};

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
			: data_b{a0, a1, a2, a3}
		{
			data = byte_swap<endianness::host, endianness::little>(data);
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

	using byte_array_t = std::vector<std::uint8_t>;

	namespace _detale
	{
		template <typename... Hs>
		struct overloaded_t : Hs...
		{
			using Hs::operator()...;
		};
		template <typename... Hs>
		overloaded_t(Hs...) -> overloaded_t<Hs...>;
	}

	// Список обработчиков сообщений, прикрепленных к типу сообщения
	// функцией hook
	template <typename... Hs>
	struct handlers_t
	{
		constexpr
		handlers_t(Hs... _hs) noexcept
			: m_hs{_hs...}
		{
		}
		template <typename MV, typename O>
		constexpr
		void operator()(MV&& m, O& _obj) const noexcept
		{
			auto func = std::visit(m_hs, std::forward<MV>(m));
			if constexpr (std::is_member_pointer_v<std::decay_t<decltype(func)>>)
				std::invoke(func, _obj, std::forward<MV>(m));
			else
				std::invoke(func, std::forward<MV>(m));
		}
	private:
		_detale::overloaded_t<Hs...> const m_hs;
	};
	template <typename... Hs>
	handlers_t(Hs...) -> handlers_t<Hs...>;

	constexpr
	handlers_t null_handlers;

	#define handlers_inline static constexpr handlers_t

	template <typename T, typename MV, typename WT>
	constexpr
	bool is_handlers_v = std::is_invocable_v<T, MV&, WT&>;

	template <typename T, typename MV, typename WT>
	using is_handlers_b = std::enable_if_t<is_handlers_v<T, MV, WT>, bool>;

	namespace _detale
	{
		template <typename T>
		struct __hook_t
		{
			__hook_t() = default;
			__hook_t(__hook_t const&) = delete;
			__hook_t(__hook_t&&) = delete;
			__hook_t& operator=(__hook_t const&) = delete;
			__hook_t& operator=(__hook_t&&) = delete;

			// Прикрепить обработчик к типу сообщения
			template <typename F>
			constexpr
			auto const operator()(F _handler) const noexcept
			{
				return [_handler](T const&){ return _handler; };
			}
		};
	}
	// Прикрепить обработчик к типу сообщения
	template <typename MessageType>
	constexpr
	_detale::__hook_t<MessageType> hook;

	enum class thread_type
	{
		  queue_thr
		, udp_thr
		, timer_thr
	};
	using queue_thr = std::integral_constant<thread_type, thread_type::queue_thr>;
	using udp_thr = std::integral_constant<thread_type, thread_type::udp_thr>;
	using timer_thr = std::integral_constant<thread_type, thread_type::timer_thr>;

	template <uint64_t i>
	using timeout = std::integral_constant<uint64_t, i>;

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

		template <
			typename WT
			, typename _MV, typename _TT, typename _EV, typename _TO
			, typename _H
			, typename _CH
			, typename _DS
		>
		struct thread_worker_t;

		class __thread_interface_t
		{
		public:
			__thread_interface_t() = default;
			__thread_interface_t(__thread_interface_t const&) = delete;
			__thread_interface_t(__thread_interface_t&&) = delete;
			__thread_interface_t& operator=(__thread_interface_t const&) = delete;
			__thread_interface_t& operator=(__thread_interface_t&&) = delete;

			static constexpr
			bool is_thread_interface_v = true;

			template <
				typename WT
				, typename _MV, typename _TT, typename _EV, typename _TO
				, typename _H
				, typename _CH
				, typename _DS
			>
			friend struct thread_worker_t;

			template <typename WT>
			friend struct __start_thread_t;

			std::uint32_t status()
			{
				return static_cast<std::uint32_t>(m_status);
			}
			void set_status_flags(std::uint32_t fl)
			{
				m_status |= fl;
			}
			void clear_status_flags(std::uint32_t fl)
			{
				m_status &= ~fl;
			}
			void stop(std::uint32_t st)
			{
				m_stop = st;
			}
			std::uint32_t stop() noexcept
			{
				return static_cast<std::uint32_t>(m_stop);
			}
			void join() noexcept
			{
				if (m_thread.joinable())
				{
					m_thread.join();
				}
			}
			bool joinable() noexcept
			{
				return m_thread.joinable();
			}

		private:
			std::atomic<std::uint32_t> m_status{0};
			std::atomic<std::uint32_t> m_stop{0};
			std::thread m_thread;
		};

		template <typename _MessageVariants>
		class __thread_interface_queue_t : public __thread_interface_t
		{
			using __thread_interface_t::__thread_interface_t;

		public:
			//Передать сообщение
			template <typename MT
				, std::enable_if_t<std::is_constructible_v<_MessageVariants, MT>, bool> = true
			>
			constexpr
			void send(MT&& d) noexcept
			{
				{
					std::lock_guard<std::mutex> lg(m_mtx);
					m_queue.push(std::forward<MT>(d));
				}
				m_cv.notify_one();
			}

			template <
				typename WT
				, typename _MV, typename _TT, typename _EV, typename _TO
				, typename _H
				, typename _CH
				, typename _DS
			>
			friend struct thread_worker_t;

		private:
			std::queue<_MessageVariants> m_queue;
			std::mutex m_mtx;
			std::condition_variable m_cv;
		};

		template <typename _MessageVariants>
		class __thread_interface_net_t : public __thread_interface_t
		{
		public:
			__thread_interface_net_t() = delete;
			__thread_interface_net_t(__thread_interface_net_t const&) = delete;
			__thread_interface_net_t(__thread_interface_net_t&&) = delete;
			__thread_interface_net_t& operator=(__thread_interface_net_t const&) = delete;
			__thread_interface_net_t& operator=(__thread_interface_net_t&&) = delete;

			constexpr
			__thread_interface_net_t(ip_addr _addr, port _port) noexcept
				: m_addr{_addr.data}
				, m_port{_port.data}
				, m_self_port{_port.data}
			{
			}
			constexpr
			__thread_interface_net_t(ip_addr _addr, port _port, port _self_port) noexcept
				: m_addr{_addr.data}
				, m_port{_port.data}
				, m_self_port{_self_port.data}
			{
			}
			~__thread_interface_net_t()
			{
				if (this->m_sock > 0) ::close(this->m_sock);
			}

			template <typename WT>
			friend struct __start_thread_t;

			template <
				typename WT
				, typename _MV, typename _TT, typename _EV, typename _TO
				, typename _H
				, typename _CH
				, typename _DS
			>
			friend struct thread_worker_t;

		protected:
			uint32_t addr()
			{
				return m_addr;
			}

			uint16_t port()
			{
				return m_port;
			}

			uint16_t self_port()
			{
				return m_self_port;
			}

			int sock()
			{
				return m_sock;
			}
			void sock(int _s)
			{
				m_sock = _s;
			}

		private:
			uint32_t m_addr = 0;
			uint16_t m_port = 0;
			uint16_t m_self_port = 0;
			int m_sock = 0;
		};
	}

	template <
		  typename _MessageVariants
		, typename _Type = queue_thr
		, typename _ErrorVariants = std::variant<std::false_type>
		, typename _Timeout = timeout<0>
	>
	class thread_interface_t
	{
		static_assert(always_false_v<_MessageVariants>, "Неправильные параметры типа thread_interface_t");
	};

	template <
		  typename _MessageVariants
		, typename _ErrorVariants
	>
	class thread_interface_t
	<
		  _MessageVariants
		, queue_thr
		, _ErrorVariants
		, timeout<0>
	>
		: public _detale::__thread_interface_queue_t<_MessageVariants>
	{
	public:
		using _detale::__thread_interface_queue_t<_MessageVariants>::__thread_interface_queue_t;
	};

	template <
		  typename _MessageVariants
		, typename _ErrorVariants
		, typename _Timeout
	>
	class thread_interface_t
	<
		  _MessageVariants
		, udp_thr
		, _ErrorVariants
		, _Timeout
	>
		: public _detale::__thread_interface_net_t<_MessageVariants>
	{
	public:
		using _detale::__thread_interface_net_t<_MessageVariants>::__thread_interface_net_t;

		//Передать сообщение
		constexpr
		bool send(byte_array_t ba) noexcept
		{
			int cnt = 0;
			while (this->status() != 1)
			{
				if (++cnt > 100)
				{
					return false;
				}
				usleep(100 * 1000);
			}
			sockaddr_in _addr{};
			_addr.sin_family = AF_INET;
			_addr.sin_port = byte_swap<endianness::host, endianness::network>(this->port());
			_addr.sin_addr.s_addr = byte_swap<endianness::host, endianness::network>(this->addr());
			sendto(this->sock(), ba.data(), ba.size(), 0, reinterpret_cast<sockaddr*>(&_addr), sizeof(_addr));
			return true;
		}
	};

	template <
		  typename _MessageVariants
		, typename _ErrorVariants
		, typename _TO
	>
	class thread_interface_t
	<
		  _MessageVariants
		, timer_thr
		, _ErrorVariants
		, _TO
	>
		: public _detale::__thread_interface_t
	{
	public:
		using _detale::__thread_interface_t::__thread_interface_t;
		using client_interface_t = thread_interface_t<_MessageVariants, queue_thr>;//, _ErrorVariants, timeout<0>>;
		using clients_t = std::map<std::uint64_t, std::reference_wrapper<client_interface_t>>;

		template <
			typename WT
			, typename _MV, typename _TT, typename _EV, typename _TOW
			, typename _H
			, typename _CH
			, typename _DS
		>
		friend struct thread_worker_t;

		void add_client(std::uint64_t id, client_interface_t& i)
		{
			clients.emplace(id, std::ref(i));
		}
		void remove_client(std::uint64_t id)
		{
			clients.erase(id);
		}

//	private:
		clients_t clients;
	};

	template <
		  typename _I
		, typename _TO
	>
	struct thread_timer{};

	template <
		  typename _MV
		, typename _EV
		, typename _ITO
		, typename _TO
	>
	struct thread_timer<thread_interface_t<_MV, queue_thr, _EV, _ITO>, _TO>
	{
		using type = thread_interface_t<_MV, timer_thr, _EV, _TO>;
	};

	template <
		  typename _I
		, typename _TO
	>
	using thread_timer_t = typename thread_timer<_I, _TO>::type;





	namespace _detale
	{
		// Функциональный объект потока.
		template <
			  typename WT
			, typename _MV, typename _TT, typename _EV, typename _TO
			, typename _H
			, typename _CH
			, typename _DS = std::nullopt_t
		>
		struct __thread_worker_t
		{
			constexpr
			__thread_worker_t(thread_interface_t<_MV, _TT, _EV, _TO>& _ti
								, _H const& _h) noexcept
				: thr_i{_ti}
				, handlers{_h}
			{
			}
			template <typename D>
			constexpr
			__thread_worker_t(D&& d
								, thread_interface_t<_MV, _TT, _EV, _TO>& _ti
								, _H const& _h) noexcept
				: thr_i{_ti}
				, handlers(_h)
				, worker{std::make_unique<WT>(std::forward<D>(d))}
			{
			}
			template <typename D>
			constexpr
			__thread_worker_t(D&& d
								, thread_interface_t<_MV, _TT, _EV, _TO>& _ti
								, _H const& _h
								, _CH const& _eh
								, _DS _ds) noexcept
				: thr_i{_ti}
				, handlers(_h)
				, ctrl_handlers(_eh)
				, m_deserializer{_ds}
				, worker{std::make_unique<WT>(std::forward<D>(d))}
			{
			}
			thread_interface_t<_MV, _TT, _EV, _TO>& thr_i;
			_H const& handlers;
			_CH const& ctrl_handlers = null_handlers;
			_DS m_deserializer = std::nullopt;
			std::unique_ptr<WT> worker;
		};

		template <
			  typename WT
			, typename _MV, typename _TT, typename _EV, typename _TO
			, typename _H
			, typename _CH
			, typename _DS = std::nullopt_t
		>
		struct thread_worker_t : __thread_worker_t<WT, _MV, _TT, _EV, _TO, _H, _CH>
		{
			using __thread_worker_t<WT, _MV, _TT, _EV, _TO, _H, _CH>::__thread_worker_t;

			constexpr
			void operator()() noexcept
			{
				_MV vmsg;
				this->thr_i.set_status_flags(1);
				while (!static_cast<std::uint32_t>(this->thr_i.stop()))
				{
					{
						std::unique_lock<std::mutex> ul(this->thr_i.m_mtx);
						if (this->thr_i.m_cv.wait_for(ul, std::chrono::seconds(1),
							[this](){return !this->thr_i.m_queue.empty();}))
						{
							vmsg = std::move(this->thr_i.m_queue.front());
							this->thr_i.m_queue.pop();
						}
						else
						{
							continue;
						}
					}
					this->handlers(vmsg, this->worker);
				}
				this->thr_i.clear_status_flags(1);
			}
		};

		template <
			  typename WT
			, typename _MV, typename _EV, typename _TO
			, typename _H
			, typename _CH
			, typename _DS
		>
		struct thread_worker_t
		<
			WT, _MV, udp_thr, _EV, _TO, _H, _CH, _DS
		>
			: __thread_worker_t<WT, _MV, udp_thr, _EV, _TO, _H, _CH, _DS>
		{
			using __thread_worker_t<WT, _MV, udp_thr, _EV, _TO, _H, _CH, _DS>::__thread_worker_t;

			constexpr
			void operator()()  noexcept
			{
				this->thr_i.sock(socket(AF_INET, SOCK_DGRAM, 0));
				if (this->thr_i.sock() <= 0)
				{
					_EV emsg = msg_error_t{41};
					this->thr_i.sock(0);
					this->ctrl_handlers(emsg, this->worker);
					return;
				}

				sockaddr_in addr{};
				addr.sin_family = AF_INET;
				addr.sin_port = byte_swap<endianness::host, endianness::network>(this->thr_i.self_port());
				addr.sin_addr.s_addr = byte_swap<endianness::host, endianness::network>(this->thr_i.addr());
				if (bind(this->thr_i.sock(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
				{
					_EV emsg = msg_error_t{42};
					::close(this->thr_i.sock());
					this->ctrl_handlers(emsg, this->worker);
					return;
				}

				pollfd s_pfd{}, *pfd{&s_pfd};
				pfd->fd = this->thr_i.sock();
				pfd->events = POLLIN | POLLHUP | POLLERR;
				pfd->revents = 0;

				sockaddr_in from{};
				socklen_t fromlen;

			    std::unique_ptr<uint8_t[]> ubuf{new uint8_t[4096]};

				this->thr_i.set_status_flags(1);

				while (!static_cast<std::uint32_t>(this->thr_i.stop()))
				{
					pfd->revents = 0;
					int res_poll = poll(pfd, 1, 100); 
					if ((res_poll > 0) && (pfd->revents == POLLIN))
					{
						memset(&from, 0, sizeof(sockaddr_in));
						fromlen = sizeof(sockaddr_in);
						if (ssize_t blen = recvfrom(this->thr_i.sock(), ubuf.get(), 4096, 0
								, reinterpret_cast<sockaddr*>(&from), &fromlen); blen > 0)
						{
							byte_array_t bm(static_cast<std::size_t>(blen));
							memcpy(bm.data(), ubuf.get(), static_cast<std::size_t>(blen));
							if constexpr (!std::is_member_pointer_v<
										std::decay_t<decltype(this->m_deserializer.value().get())>>)
							{
								_MV vmsg = this->m_deserializer.value().get()(bm);
								this->handlers(vmsg, this->worker);
							}
							else
							{
								static_assert(always_false_v<WT>, "deserializer должен быть статической функцией-членом");
							}
						}
					}
					else if constexpr (!std::is_same_v<_TO, timeout<0>>)
					{
						if (res_poll == 0)
						{
							// послать таймаут
							_EV emsg = msg_timeout_t{1};
							this->ctrl_handlers(emsg, this->worker);
						}
					}
					else if (pfd->revents == POLLHUP)
					{
						_EV emsg = msg_error_t{43};
						this->ctrl_handlers(emsg, this->worker);
						break;
					}
				}
				this->thr_i.clear_status_flags(1);
				shutdown(this->thr_i.sock(), SHUT_RDWR);
				close(this->thr_i.sock());
				this->thr_i.sock(0);
			}
		};

		template <
			  typename WT
			, typename _MV, typename _EV, typename _TO
			, typename _H
			, typename _CH
			, typename _DS
		>
		struct thread_worker_t
		<
			WT, _MV, timer_thr, _EV, _TO, _H, _CH, _DS
		> : __thread_worker_t<WT, _MV, timer_thr, _EV, _TO, _H, _CH, _DS>
		{
			using __thread_worker_t<WT, _MV, timer_thr, _EV, _TO, _H, _CH, _DS>::__thread_worker_t;

			constexpr
			void operator()() noexcept
			{
				_MV vmsg = msg_timeout_t{1};
				this->thr_i.set_status_flags(1);
				while (!static_cast<std::uint32_t>(this->thr_i.stop()))
				{
					usleep(_TO() * 1000);
					for (auto&& el : this->thr_i.clients)
					{
						el.second.get().send(vmsg);
					}
				}
				this->thr_i.clear_status_flags(1);
			}
		};

	}

	namespace _detale
	{
		template <typename WT>
		struct __start_thread_t
		{
			__start_thread_t() = default;
			__start_thread_t(__start_thread_t const&) = delete;
			__start_thread_t(__start_thread_t&&) = delete;
			__start_thread_t& operator=(__start_thread_t const&) = delete;
			__start_thread_t& operator=(__start_thread_t&&) = delete;

			//Запустить поток
			template <
				  typename _I
				, typename _H
			>
			constexpr
			void operator()(
				_I& i
				, _H const& h) const noexcept
			{
				start(i, h);
			}

			//Запустить поток
			template <
				  typename _I
				, typename _O
				, typename _H
			>
			constexpr
			void operator()(
				  _I& i
				, _O&& o
				, _H const& h) const noexcept
			{
				start(i, std::forward<_O>(o), h);
			}

			//Запустить поток
			template <
				  typename _I
				, typename _O
				, typename _H
				, typename _CH
				, typename _DS
			>
			constexpr
			void operator()(
				  _I& i
				, _O&& o
				, _H const& h
				, _CH const& ch
				, _DS ds) const noexcept
			{
				start(i, std::forward<_O>(o), h, ch, std::make_optional(std::ref(ds)));
			}

			template <
				typename _MV, typename _TT, typename _EV, typename _TO
				, typename _H
				, is_handlers_b<_H, _MV, WT> = true
			>
			constexpr
			void start(
				thread_interface_t<_MV, _TT, _EV, _TO>& interface
				, _H const& handlers
			) const noexcept
			{
				interface.stop(0);
				interface.m_thread = std::thread(thread_worker_t
				<
					WT
					, _MV, _TT, _EV, _TO
					, _H
					, decltype(null_handlers)
				>(interface, handlers));
			}

			template <
				typename _MV, typename _TT, typename _EV, typename _TO
				, typename _O
				, typename _H
				, is_handlers_b<_H, _MV, WT> = true
			>
			constexpr
			void start(
				thread_interface_t<_MV, _TT, _EV, _TO>& interface
				, _O&& options
				, _H const& handlers
			) const noexcept
			{
				interface.stop(0);
				interface.m_thread = std::thread(thread_worker_t
				<
					WT
					, _MV, _TT, _EV, _TO
					, _H
					, decltype(null_handlers)
				>(std::forward<_O>(options), interface, handlers));
			}

			template <
				typename _MV, typename _TT, typename _EV, typename _TO
				, typename _O
				, typename _H
				, typename _CH
				, typename _DS
				, is_handlers_b<_H, _MV, WT> = true
				, is_handlers_b<_CH, _MV, WT> = true
			>
			constexpr
			void start(
				thread_interface_t<_MV, _TT, _EV, _TO>& interface
				, _O&& options
				, _H const& handlers
				, _CH const& ch
				, _DS ds
			) const noexcept
			{
				interface.stop(0);
				interface.m_thread = std::thread(thread_worker_t
				<
					WT
					, _MV, _TT, _EV, _TO
					, _H
					, _CH
					, _DS
				>(std::forward<_O>(options), interface, handlers, ch, ds));
			}
		};

	}
	template <typename WT = nullptr_t>
	constexpr
	_detale::__start_thread_t<WT> start_thread;

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


} // namespace msg

#endif // MSGTHREADS_H
