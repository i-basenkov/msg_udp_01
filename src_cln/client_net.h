#ifndef CLIENT_NET_H
#define CLIENT_NET_H

#include <map>
#include <chrono>

using namespace msg;

namespace msg::file_send
{

	enum struct msgs_client
	{
		timeout,
		start_send
	};
	using timeout_t = std::integral_constant<msgs_client, msgs_client::timeout>;
	using start_send_t = std::integral_constant<msgs_client, msgs_client::start_send>;

	using client_msg_err = message_variants_t
	<
		msg_error_t
		, msg_timeout_t
	>;

	using msg_client_ts = message_variants_t
	<
		  msg::net::msg_udp
		, msg_timeout_t
		, start_send_t
	>;



	using client_udp_interface_t = thread_interface_t<udp_thr, net::msg_udp_ts>;
	using client_send_interface_t = thread_interface_t<queue_thr, net::msg_udp_ts>;
	using client_work_iterface_t = thread_interface_t<queue_thr, msg_client_ts>;

    struct sended_seq_t
    {
	   std::uint64_t secs;
	   byte_array_t seq;
    };
    using sended_seq_list_t = std::map<std::uint32_t, sended_seq_t>;



	class ClientTimer
	{
	public:
		constexpr
		ClientTimer(thread_timer_t<client_work_iterface_t>& i, std::uint32_t t)
			: self_i{i}
			, timeout{t}
		{
		}

		inline
		void run() noexcept
		{
			msg_client_ts vmsg = msg_timeout_t{1};
			self_i.status |= 0x01;
			while (!static_cast<std::uint32_t>(self_i.stop))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
				for (auto&& el : self_i.clients())
				{
					el.second.get().send(vmsg);
				}
			}
			self_i.status |= 0x01;
		}

	private:
		thread_timer_t<client_work_iterface_t>& self_i;
		std::uint32_t timeout;
	};



	class ClientWork
	{
	public:
		ClientWork(client_work_iterface_t& si, client_send_interface_t& i, file_t& _file, std::uint64_t _id)
			: self_i{si}
			, send_i{i}
			, file{_file}
			, id{_id}
		{
			seq_total = file.size();
			for (auto&& v : file)
			{
				crc = crc32c(crc, v.second.data(), v.second.size());
			}
		}
		inline
		~ClientWork()
		{
		}

		void run();
		void send_pack(std::uint32_t sn, byte_array_t& d);
		inline void send_empl();
		auto get_rend_pos();

		void handler(msg::net::msg_udp&);
		void handler(start_send_t&);
		void handler(msg::msg_timeout_t&);

	private:
		client_work_iterface_t& self_i;
		client_send_interface_t& send_i;
		file_t file;
		std::uint64_t const id;
		std::size_t seq_total;
		sended_seq_list_t sended_seqs;
		uint32_t crc = 0;
	};

	class ClientNet
	{
	public:
		using net_sender_t = net_send_queue_t<client_send_interface_t, net::udp_interface_t>;
		using works_t = std::map<std::uint64_t, std::unique_ptr<client_work_iterface_t>>;

		ClientNet(client_udp_interface_t& i, mx_queue_t<file_t>& fq)
			: self_i{i}
			, file_queue{fq}
		{
			send_thr.thread = std::thread(worker_t<net_sender_t>(send_thr, self_i));
			timer.thread = std::thread(worker_t<ClientTimer>(timer, 1000));
		}
		inline
		~ClientNet()
		{
			timer.stop |= 0x01;
			timer.join();
			send_thr.stop |= 0x01;
			send_thr.join();
			for (auto& el : works)
			{
				el.second->stop |= 0x01;
				el.second->join();
			}
		}
		
		void run();

		void error_handler(msg_error_t const&);
		void timeout_handler(msg_timeout_t const&);

		void rcv_seq(net::msg_udp_ts&);

	private:
		client_udp_interface_t& self_i;
		mx_queue_t<file_t>& file_queue;
		client_send_interface_t send_thr;
		works_t works;
		std::uint64_t next_id = 0;
		thread_timer_t<client_work_iterface_t> timer;
	};

}

#endif