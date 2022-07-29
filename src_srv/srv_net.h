#ifndef SRV_NET_H
#define SRV_NET_H

#include <map>

#include "../src_shr/display.h"

using namespace msg;
using namespace file_send;


class SrvSend
{
public:
	struct options_t
	{
		options_t(net::udp_interface_t& l)
			: line_interface{l}
		{
		}
		net::udp_interface_t& line_interface;
	};

	template <typename T, typename = std::enable_if_t<
			!std::is_same_v<std::decay_t<T>, SrvSend>>>
	inline
	SrvSend(T&& o)
		: options{std::forward<T>(o)}
	{
	}
	inline
	~SrvSend()
	{
	}

	void send_01(net::msg_udp_ts&);

	handlers_inline msg_handlers
	{
		hook<net::msg_udp>(&SrvSend::send_01)
	};

private:
	options_t options;
};



class SrvWork
{
public:
	struct options_t
	{
		options_t(srv_send_interface_t& i, srv_work_iterface_t& si)
			: send_interface{i}
			, self_interface{si}
		{
		}
		srv_send_interface_t& send_interface;
		srv_work_iterface_t& self_interface;
	};

	template <typename T, typename = std::enable_if_t<
			!std::is_same_v<std::decay_t<T>, SrvWork>>>
	inline
	SrvWork(T&& o)
		: options{std::forward<T>(o)}
	{
	}
	inline
	~SrvWork()
	{
	}

	void work(net::msg_udp_ts&);

	handlers_inline msg_handlers
	{
		hook<net::msg_udp>(&SrvWork::work)
	};

private:
	options_t options;
	file_t file;
};



class SrvNet
{
public:
	using works_t = std::map<std::uint64_t, std::unique_ptr<srv_work_iterface_t>>;

	struct options_t
	{
		options_t(net::udp_interface_t& i)
			: interface{i}
		{
		}
		net::udp_interface_t& interface;
	};

	template <typename T, typename = std::enable_if_t<
			!std::is_same_v<std::decay_t<T>, SrvNet>>>
	inline
	SrvNet(T&& o)
		: options{std::forward<T>(o)}
	{
		start_thread<SrvSend>
		(
			send_thr
			, SrvSend::options_t(
				options.interface
			)
			, SrvSend::msg_handlers
		);
	}
	inline
	~SrvNet()
	{
		send_thr.stop(1);
		send_thr.join();
		for (auto& el : works)
		{
			el.second->stop(1);
			el.second->join();
		}
	}

	
	void error_hadler(net::msg_err const&);
	void rcv_seq(net::msg_udp_ts& d);

	handlers_inline msg_handlers
	{
		hook<net::msg_udp>(&SrvNet::rcv_seq)
	};

	handlers_inline error_handlers
	{
		hook<msg_error_t>(&SrvNet::error_hadler)
	};

private:
	options_t options;
	file_list_t file_list;
	srv_send_interface_t send_thr;
	works_t works;
};


#endif