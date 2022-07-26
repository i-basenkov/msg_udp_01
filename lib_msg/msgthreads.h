#ifndef MSGTHREADS_H
#define MSGTHREADS_H


#include <iostream>

#include <variant>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <queue>
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

#include "msgutils.h"

namespace msg
{
	using std::cout;
	using std::endl;

	template <typename T>
	struct TD;

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

	template <typename T, typename MV, typename WT>
	constexpr
	bool is_handlers_v = std::is_invocable_v<T, MV&, WT&>;

	template <typename T, typename MV, typename WT>
	using is_handlers_b = std::enable_if_t<is_handlers_v<T, MV, WT>, bool>;

	constexpr
	handlers_t null_handlers;

	#define handlers_inline static constexpr handlers_t



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

	// Прикрепить обработчик к типу сообщения
	template <typename T, typename F>
	constexpr
	auto const operator | (F&& _handler, _detale::__hook_t<T> const& _hook) noexcept
	{
		return _hook(std::forward<F>(_handler));
	}



	enum class thread_type
	{
		blockable,
		notblockable,
		tcp_client,
		tcp_server,
		udp
	};
	using blockable = std::integral_constant<thread_type, thread_type::blockable>;
	using notblockable = std::integral_constant<thread_type, thread_type::notblockable>;
	using tcp_client = std::integral_constant<thread_type, thread_type::tcp_client>;
	using tcp_server = std::integral_constant<thread_type, thread_type::tcp_server>;
	using udp = std::integral_constant<thread_type, thread_type::udp>;

	enum class blocked_t
	{
		blocked,
		unblocked
	};
	using blocked = std::integral_constant<blocked_t, blocked_t::blocked>;
	using unblocked = std::integral_constant<blocked_t, blocked_t::unblocked>;

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
		auto& operator << (MT&& d) noexcept
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

		struct _block_thread_t;
		struct _unblock_thread_t;

		template <
			typename WT
			, typename _MV, typename _EV, typename _BA, typename _TO
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
				, typename O
				, typename H
				, typename CH
				, typename DS
			>
			friend struct __start_thread_t;

			template <
				typename WT
				, typename _MV, typename _EV, typename _BA, typename _TO
				, typename _H
				, typename _CH
				, typename _DS
			>
			friend struct thread_worker_t;

			friend struct __stop_t;
			friend struct __status_t;
			friend struct __set_status_t;
			friend struct __join_t;
			friend struct __joinable_t;
			friend struct __in_work_t;

		protected:
			inline
			int stop()
			{
				return m_stop;
			}
			inline
			void stop(int _s)
			{
				m_stop = _s;
			}

			inline
			int status()
			{
				int st = m_status;
				return st;
			}

			inline
			void status(int _s)
			{
				m_status = _s;
			}

			inline
			auto& thread()
			{
				return m_thread;
			}

		private:
			std::atomic<int> m_stop{0};
			std::atomic<int> m_status{0};
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
			auto& operator << (MT&& d) noexcept
			{
				{
					std::lock_guard<std::mutex> lg(m_mtx);
					m_queue.push(std::forward<MT>(d));
				}
				m_cv.notify_one();
				return *this;
			}

			template <
				typename WT
				, typename _MV, typename _EV, typename _BA, typename _TO
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

			template <
				typename WT
				, typename O
				, typename H
				, typename CH
				, typename DS
			>
			friend struct __start_thread_t;

			template <
				typename WT
				, typename _MV, typename _EV, typename _BA, typename _TO
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
		, typename _ErrorVariants = std::variant<std::false_type>
		, typename _Type = notblockable
		, typename _Timeout = timeout<0>
		, typename = std::void_t<>
	>
	class thread_interface_t
	{
		static_assert(always_false_v<>, "Неправильные параметры типа thread_interface_t");
	};

	template <
		  typename _MessageVariants
		, typename _ErrorVariants
		, typename _Timeout
	>
	class thread_interface_t
	<
		  _MessageVariants
		, _ErrorVariants
		, blockable
		, _Timeout
		, std::enable_if_t<!std::is_same_v<_Timeout, timeout<0>>>
	>
		: public _detale::__thread_interface_queue_t<_MessageVariants>
	{
	public:
		using _detale::__thread_interface_queue_t<_MessageVariants>::__thread_interface_queue_t;

		template <
			typename WT
			, typename _MV, typename _BA, typename _TO
			, typename _H
		>
		friend struct _detale::thread_worker_t;

		friend struct _detale::_block_thread_t;
		friend struct _detale::_unblock_thread_t;

	private:
		std::mutex m_bmtx;
		std::condition_variable m_bcv;
		std::variant<unblocked, blocked> m_bv = unblocked{};
	};

	template <
		  typename _MessageVariants
		, typename _ErrorVariants
	>
	class thread_interface_t
	<
		  _MessageVariants
		, _ErrorVariants
		, notblockable
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
		, _ErrorVariants
		, udp
		, _Timeout
	>
		: public _detale::__thread_interface_net_t<_MessageVariants>
	{
	public:
		using _detale::__thread_interface_net_t<_MessageVariants>::__thread_interface_net_t;

		//Передать сообщение
		constexpr
		auto operator << (byte_array_t ba) noexcept
		-> std::optional<std::reference_wrapper<thread_interface_t
			<_MessageVariants, _ErrorVariants, udp, _Timeout>>>
		{
			int cnt = 0;
			while (this->status() != 1)
			{
				if (++cnt > 100)
				{
					return std::nullopt;
				}
				usleep(100 * 1000);
			}
			sockaddr_in _addr{};
			_addr.sin_family = AF_INET;
			_addr.sin_port = byte_swap<endianness::host, endianness::network>(this->port());
			_addr.sin_addr.s_addr = byte_swap<endianness::host, endianness::network>(this->addr());
			sendto(this->sock(), ba.data(), ba.size(), 0, reinterpret_cast<sockaddr*>(&_addr), sizeof(_addr));
			return std::make_optional(std::ref(*this));
		}
	};



	namespace _detale
	{
		// Функциональный объект потока.
		template <
			  typename WT
			, typename _MV, typename _EV, typename _BA, typename _TO
			, typename _H
			, typename _CH
			, typename _DS = std::nullopt_t
		>
		struct __thread_worker_t
		{
			constexpr
			__thread_worker_t(thread_interface_t<_MV, _EV, _BA, _TO>& _ti
								, _H const& _h) noexcept
				: thr_i{_ti}
				, handlers{_h}
			{
			}
			constexpr
			__thread_worker_t(thread_interface_t<_MV, _EV, _BA, _TO>& _ti
								, _H const& _h
								, _DS _ds) noexcept
				: thr_i{_ti}
				, handlers(_h)
				, m_deserializer{_ds}
				, worker{std::make_unique<WT>()}
			{
			}
			template <typename D>
			constexpr
			__thread_worker_t(D&& d
								, thread_interface_t<_MV, _EV, _BA, _TO>& _ti
								, _H const& _h) noexcept
				: thr_i{_ti}
				, handlers(_h)
				, worker{std::make_unique<WT>(std::forward<D>(d))}
			{
			}
			template <typename D>
			constexpr
			__thread_worker_t(D&& d
								, thread_interface_t<_MV, _EV, _BA, _TO>& _ti
								, _H const& _h
								, _CH const& _eh) noexcept
				: thr_i{_ti}
				, handlers(_h)
				, ctrl_handlers{_eh}
				, worker{std::make_unique<WT>(std::forward<D>(d))}
			{
			}
			template <typename D>
			constexpr
			__thread_worker_t(D&& d
								, thread_interface_t<_MV, _EV, _BA, _TO>& _ti
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
			thread_interface_t<_MV, _EV, _BA, _TO>& thr_i;
			_H const& handlers;
			_CH const& ctrl_handlers = null_handlers;
			_DS m_deserializer = std::nullopt;
			std::unique_ptr<WT> worker;
		};


		template <
			  typename WT
			, typename _MV, typename _EV, typename _BA, typename _TO
			, typename _H
			, typename _CH
			, typename _DS = std::nullopt_t
		>
		struct thread_worker_t : __thread_worker_t<WT, _MV, _EV, _BA, _TO, _H, _CH>
		{
			using __thread_worker_t<WT, _MV, _EV, _BA, _TO, _H, _CH>::__thread_worker_t;

			constexpr
			void operator()() noexcept
			{
				_MV vmsg;
				this->thr_i.status(this->thr_i.status() | 1);
				while (!this->thr_i.stop())
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
				std::cout << "-------- stop queue thread --------" << std::endl;
				this->thr_i.status(this->thr_i.status() & 0xfe);
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
			WT, _MV, _EV, udp, _TO, _H, _CH, _DS
		>
			: __thread_worker_t<WT, _MV, _EV, udp, _TO, _H, _CH, _DS>
		{
			using __thread_worker_t<WT, _MV, _EV, udp, _TO, _H, _CH, _DS>::__thread_worker_t;

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

				this->thr_i.status(1);

				while (!this->thr_i.stop())
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
				this->thr_i.status(0);
				shutdown(this->thr_i.sock(), SHUT_RDWR);
				close(this->thr_i.sock());
				this->thr_i.sock(0);
			}
		};
	}



	template <typename, typename = std::void_t<>, typename = std::void_t<>>
	struct is_thread_interface : std::false_type {};

	template <typename T>
	struct is_thread_interface<T
		, std::enable_if_t<T::is_thread_interface_v>
	> : std::true_type {};

	template <typename T>
	constexpr
	bool is_thread_interface_v = is_thread_interface<T>::value;

	template <typename T>
	using is_thread_interface_b = std::enable_if_t<is_thread_interface_v<T>, bool>;


	template <typename, typename = std::void_t<>, typename = std::void_t<>, typename = std::void_t<>>
	struct is_thread_interface_queue : std::false_type {};

	template <typename T>
	struct is_thread_interface_queue<T
		, std::void_t<decltype(std::declval<T>().m_stop)>
		, std::void_t<decltype(std::declval<T>().m_thread)>
		, std::void_t<decltype(std::declval<T>().m_queue)>
	> : std::true_type {};

	template <typename T>
	constexpr
	bool is_thread_interface_queue_v = is_thread_interface_queue<T>::value;

	template <typename T>
	using is_thread_interface_queue_b = std::enable_if_t<is_thread_interface_queue_v<T>, bool>;


	template <typename, typename = std::void_t<>, typename = std::void_t<>, typename = std::void_t<>>
	struct is_thread_interface_net : std::false_type {};

	template <typename T>
	struct is_thread_interface_net<T
		, std::void_t<decltype(std::declval<T>().m_stop)>
		, std::void_t<decltype(std::declval<T>().m_thread)>
		, std::void_t<decltype(std::declval<T>().m_sock)>
	> : std::true_type {};

	template <typename T>
	constexpr
	bool is_thread_interface_net_v = is_thread_interface_net<T>::value;

	template <typename T>
	using is_thread_interface_net_b = std::enable_if_t<is_thread_interface_net_v<T>, bool>;


	namespace _detale
	{
		template <
			  typename WT
			, typename O
			, typename H
			, typename CH = handlers_t<>
			, typename DS = std::nullopt_t
		>
		struct __start_thread_t
		{
			constexpr
			__start_thread_t(O _o, H const& _h) noexcept
				: options{_o}
				, handlers{_h}
			{
			}

			__start_thread_t(O _o, H const& _h, CH const& _eh) noexcept
				: options{_o}
				, handlers{_h}
				, ctrl_handlers{_eh}
			{
			}

			constexpr
			__start_thread_t(O _o, H const& _h, CH const& _eh, DS _ds) noexcept
				: options{_o}
				, handlers{_h}
				, ctrl_handlers{_eh}
				, m_ds{_ds}
			{
			}

			template <
				typename _MV, typename _EV, typename _BA, typename _TO
				, is_handlers_b<H, _MV, WT> = true
			>
			constexpr
			auto& operator()(
				thread_interface_t<_MV, _EV, _BA, _TO>& interface
			) const noexcept
			{
				interface.m_stop = 0;
				if constexpr (std::is_same_v<O, std::nullopt_t>)
				{
					if constexpr (std::is_same_v<std::decay_t<DS>, std::nullopt_t>)
					{
						interface.m_thread = std::thread(thread_worker_t
						<
							WT
							, _MV, _EV, _BA, _TO
							, H
							, CH
						>(interface, handlers));
					}
					else
					{
						interface.m_thread = std::thread(thread_worker_t
						<
							WT
							, _MV, _EV, _BA, _TO
							, H
							, CH
							, DS
						>(interface, handlers, ctrl_handlers, m_ds));
					}
				}
				else
				{
					if constexpr (std::is_same_v<std::decay_t<DS>, std::nullopt_t>)
					{
						interface.m_thread = std::thread(thread_worker_t
						<
							WT
							, _MV, _EV, _BA, _TO
							, H
							, CH
						>(std::move(options.value()), interface, handlers, ctrl_handlers));
					}
					else
					{
					interface.m_thread = std::thread(thread_worker_t
					<
						WT
						, _MV, _EV, _BA, _TO
						, H
						, CH
						, DS
					>(std::move(options.value()), interface, handlers, ctrl_handlers, m_ds));
					}
				}
				return  interface;
			}
		private:
			O options = std::nullopt;
			H const& handlers;
			CH const& ctrl_handlers = null_handlers;
			DS m_ds = std::nullopt;
		};

		template <typename WT>
		struct _create_start_thread_t
		{
			_create_start_thread_t() = default;
			_create_start_thread_t(_create_start_thread_t const&) = delete;
			_create_start_thread_t(_create_start_thread_t&&) = delete;
			_create_start_thread_t& operator=(_create_start_thread_t const&) = delete;
			_create_start_thread_t& operator=(_create_start_thread_t&&) = delete;

			//Запустить поток
			template <
				  typename _O
				, typename _H
			>
			constexpr
			auto operator()(_O&& options
				, _H const& handlers) const noexcept
			{
				return __start_thread_t<WT, std::optional<_O>, _H>
							(std::make_optional(std::forward<_O>(options)), handlers);
			}

			//Запустить поток
			template <
				  typename _O
				, typename _H
				, typename _CH
			>
			constexpr
			auto operator()(_O&& options
				, _H const& handlers
				, _CH const& ctrl_handlers) const noexcept
			{
				return __start_thread_t<WT, std::optional<_O>, _H, _CH>
							(std::make_optional(std::forward<_O>(options))
							, handlers, ctrl_handlers);
			}

			//Запустить поток
			template <
				  typename _O
				, typename _H
				, typename _CH
				, typename _DS
				, std::enable_if_t<!std::is_member_pointer_v<std::decay_t<_DS>>, bool> = true
			>
			constexpr
			auto operator()(_O&& options
				, _H const& handlers
				, _CH const& ctrl_handlers
				, _DS const& _ds) const noexcept
			{
				return __start_thread_t<WT, std::optional<_O>, _H, _CH
						, std::optional<std::reference_wrapper<const _DS>>
					>(std::make_optional(std::forward<_O>(options))
							, handlers
							, ctrl_handlers
							, std::make_optional(std::ref(_ds)));
			}

			//Запустить поток
			template <
				typename _H
			>
			constexpr
			auto operator()(_H const& handlers) const noexcept
			{
				return __start_thread_t<WT, std::nullopt_t, _H>(std::nullopt, handlers);
			}
		};
	}
	template <typename WT = nullptr_t>
	constexpr
	_detale::_create_start_thread_t<WT> start;

	//Запустить поток
	template <typename _MV, typename _EV, typename _BA, typename _TO
		, typename _WT, typename _O, typename _H, typename _CH
		, is_handlers_b<_H, _MV, _WT> = true
	>
	constexpr
	auto& operator | (thread_interface_t<_MV, _EV, _BA, _TO>& _ti_o
							, _detale::__start_thread_t<_WT, _O, _H, _CH> const& _f) noexcept
	{
		return _f(_ti_o);
	}
	//Запустить поток
	template <typename _MV, typename _EV, typename _BA, typename _TO
		, typename _WT, typename _O, typename _H, typename _CH, typename _DS
		, is_handlers_b<_H, _MV, _WT> = true
	>
	constexpr
	auto& operator | (thread_interface_t<_MV, _EV, _BA, _TO>& _ti_o
							, _detale::__start_thread_t<_WT, _O, _H,_CH, _DS> const& _f) noexcept
	{
		return _f(_ti_o);
	}


	namespace _detale
	{
		struct __stop_t
		{
			__stop_t() = default;
			__stop_t(__stop_t const&) = delete;
			__stop_t(__stop_t&&) = delete;
			__stop_t& operator=(__stop_t const&) = delete;
			__stop_t& operator=(__stop_t&&) = delete;

			//Остановить поток
			template <typename I, is_thread_interface_b<I> = true>
			constexpr
			auto& operator()(I& _i) const noexcept
			{
				_i.m_stop = 1;
				return _i;
			}
		};
	}
	//Остановить поток
	constexpr
	_detale::__stop_t stop;

	//Остановить поток
	template <typename I, is_thread_interface_b<I> = true>
	constexpr
	auto& operator | (I& _i, _detale::__stop_t const& _f) noexcept
	{
		return _f(_i);
	}



	namespace _detale
	{
		struct __status_t
		{
			__status_t() = default;
			__status_t(__status_t const&) = delete;
			__status_t(__status_t&&) = delete;
			__status_t& operator=(__status_t const&) = delete;
			__status_t& operator=(__status_t&&) = delete;

			//Возвратить статус потока
			template <typename I, is_thread_interface_b<I> = true>
			constexpr
			auto operator()(I& _i) const noexcept
			{
				return _i.status();
			}
		};
	}
	//Возвратить статус потока
	constexpr
	_detale::__status_t status;

	//Возвратить статус потока
	template <typename I, is_thread_interface_b<I> = true>
	constexpr
	auto operator | (I& _i, _detale::__status_t const& _f) noexcept
	{
		return _f(_i);
	}


	namespace _detale
	{
		struct __join_t
		{
			__join_t() = default;
			__join_t(__join_t const&) = delete;
			__join_t(__join_t&&) = delete;
			__join_t& operator=(__join_t const&) = delete;
			__join_t& operator=(__join_t&&) = delete;

			//Возвратить статус потока
			template <typename I, is_thread_interface_b<I> = true>
			constexpr
			auto& operator()(I& _i) const noexcept
			{
				if (_i.m_thread.joinable())
				{
					_i.m_thread.join();
				}
				return _i;
			}
		};
	}
	//Возвратить статус потока
	constexpr
	_detale::__join_t join;

	//Возвратить статус потока
	template <typename I, is_thread_interface_b<I> = true>
	constexpr
	auto& operator | (I& _i, _detale::__join_t const& _f) noexcept
	{
		return _f(_i);
	}



	namespace _detale
	{
		struct __joinable_t
		{
			__joinable_t() = default;
			__joinable_t(__joinable_t const&) = delete;
			__joinable_t(__joinable_t&&) = delete;
			__joinable_t& operator=(__joinable_t const&) = delete;
			__joinable_t& operator=(__joinable_t&&) = delete;

			//Возвратить статус потока
			template <typename I, is_thread_interface_b<I> = true>
			constexpr
			bool operator()(I& _i) const noexcept
			{
				return _i.m_thread.joinable();
			}
		};
	}
	//Возвратить статус потока
	constexpr
	_detale::__joinable_t joinable;

	//Возвратить статус потока
	template <typename I, is_thread_interface_b<I> = true>
	constexpr
	bool operator | (I& _i, _detale::__joinable_t const& _f) noexcept
	{
		return _f(_i);
	}



	namespace _detale
	{
		struct __in_work_t
		{
			__in_work_t() = default;
			__in_work_t(__in_work_t const&) = delete;
			__in_work_t(__in_work_t&&) = delete;
			__in_work_t& operator=(__in_work_t const&) = delete;
			__in_work_t& operator=(__in_work_t&&) = delete;

			//Возвратить статус потока
			template <typename I, is_thread_interface_b<I> = true>
			constexpr
			bool operator()(I& _i) const noexcept
			{
				return _i.m_thread.joinable();
			}
		};
	}
	//Возвратить статус потока
	constexpr
	_detale::__in_work_t in_work;

	//Возвратить статус потока
	template <typename I, is_thread_interface_b<I> = true>
	constexpr
	bool operator | (I& _i, _detale::__in_work_t const& _f) noexcept
	{
		return _f(_i);
	}



	namespace _detale
	{
		struct __set_status_t
		{
			__set_status_t() = delete;
			__set_status_t(__set_status_t const&) = delete;
			__set_status_t(__set_status_t&&) = delete;
			__set_status_t& operator=(__set_status_t const&) = delete;
			__set_status_t& operator=(__set_status_t&&) = delete;

			constexpr
			__set_status_t(int st)
				: m_status{st}
			{
			}

			//Возвратить статус потока
			template <typename I, is_thread_interface_b<I> = true>
			constexpr
			auto& operator()(I& _i) const noexcept
			{
				_i.status(m_status);
				return _i;
			}
			int m_status;
		};
		struct __create_set_status_t
		{
			__create_set_status_t() = default;
			__create_set_status_t(__create_set_status_t const&) = delete;
			__create_set_status_t(__create_set_status_t&&) = delete;
			__create_set_status_t& operator=(__create_set_status_t const&) = delete;
			__create_set_status_t& operator=(__create_set_status_t&&) = delete;

			constexpr
			auto operator()(int st) const noexcept
			{
				return __set_status_t(st);
			}
		};

	}
	//Возвратить статус потока
	constexpr
	_detale::__create_set_status_t set_status;

	//Возвратить статус потока
	template <typename I, is_thread_interface_b<I> = true>
	constexpr
	auto& operator | (I& _i, _detale::__set_status_t const& _f) noexcept
	{
		return _f(_i);
	}



	namespace _detale
	{
		template <typename D>
		struct __to_interface_t
		{
			__to_interface_t() = delete;
			__to_interface_t(__to_interface_t const&) = delete;
			__to_interface_t(__to_interface_t&&) = delete;
			__to_interface_t& operator=(__to_interface_t const&) = delete;
			__to_interface_t& operator=(__to_interface_t&&) = delete;

			constexpr
			__to_interface_t(D const& _d) noexcept : m_d{_d} {}

			//Передать сообщение
			template <typename I
				, is_thread_interface_b<I> = true
			>
			constexpr
			auto& operator () (I& _oi) noexcept
			{
				return _oi << m_d;
			}

		private:
			D const& m_d;
		};

		struct __create_to_interface_t
		{
			__create_to_interface_t() = default;
			__create_to_interface_t(__create_to_interface_t const&) = delete;
			__create_to_interface_t(__create_to_interface_t&&) = delete;
			__create_to_interface_t& operator=(__create_to_interface_t const&) = delete;
			__create_to_interface_t& operator=(__create_to_interface_t&&) = delete;

			//Передать сообщение
			template <typename D>
			constexpr
			auto operator()(D&& _d) const noexcept
			{
				return __to_interface_t<D>(std::forward<D>(_d));
			}
			//Передать сообщение
			template <typename D>
			constexpr
			auto operator << (D&& _d) const noexcept
			{
				return __to_interface_t<D>(std::forward<D>(_d));
			}
		};
	}
	//Передать сообщение
	constexpr
	_detale::__create_to_interface_t to_interface;

	//Передать сообщение
	template <typename D, template<typename> typename O, typename I
		, is_optional_b<O<std::reference_wrapper<I>>> = true
		, is_thread_interface_b<I> = true
	>
	constexpr
	auto operator | (O<std::reference_wrapper<I>> _oi
					, _detale::__to_interface_t<D> _f) noexcept
	{
		if (_oi)
			return _f(_oi.value().get());
		else
			return _oi;
	}

	//Передать сообщение
	template <typename D, typename I
		, is_thread_interface_b<I> = true
	>
	constexpr
	auto& operator | (I& _i
					, _detale::__to_interface_t<D> _f) noexcept
	{
		return _f(_i);
	}


	//Передать сообщение
	template <template<typename> typename O, typename I, typename D
		, is_optional_b<O<std::reference_wrapper<I>>> = true
		, is_thread_interface_b<I> = true
	>
	constexpr
	auto operator << (O<std::reference_wrapper<I>> _oi
					, D&& _d) noexcept
	{
		if (_oi)
			return _oi.value().get() << std::forward<D>(_d);
		else
			return _oi;
	}

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
