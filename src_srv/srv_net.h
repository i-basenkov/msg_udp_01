#ifndef SRV_NET_H
#define SRV_NET_H

#include <map>


using namespace msg;

namespace msg::file_send
{

    using srv_send_interface_t = thread_interface_t<queue_thr, net::msg_udp_ts>;
	using srv_work_iterface_t = thread_interface_t<queue_thr, net::msg_udp_ts>;


	class SrvWork
	{
	public:
		SrvWork(srv_work_iterface_t& si, srv_send_interface_t& i)
			: self_i{si}
			, send_i{i}
		{
		}
		
		~SrvWork()
		{
		}

		void run();
		void work(net::msg_udp_ts&);

	private:
		srv_work_iterface_t& self_i;
		srv_send_interface_t& send_i;
		file_t file;
	};



	class SrvNet
	{
	public:
		using net_sender_t = net_send_queue_t<srv_send_interface_t, net::udp_interface_t>;
		using works_t = std::map<std::uint64_t, std::unique_ptr<srv_work_iterface_t>>;

		SrvNet(net::udp_interface_t& si)
			: self_i{si}
		{
			send_thr.thread = std::thread(worker_t<net_sender_t>(send_thr, self_i));
		}

		inline
		~SrvNet()
		{
			for (auto& el : works)
			{
				el.second->stop |= 0x01;
				el.second->join();
			}
			send_thr.stop |= 0x01;
			send_thr.join();
		}

		void run();
		void error_hadler(net::msg_err const&);
		void rcv_seq(net::msg_udp_ts& d);

	private:
		net::udp_interface_t& self_i;
		srv_send_interface_t send_thr;
		file_list_t file_list;
		works_t works;
	};

}

#endif