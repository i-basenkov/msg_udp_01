#ifndef CLIENT_NET_H
#define CLIENT_NET_H

#include <map>

#include "display.h"

using namespace msg;
using namespace file_send;


class ClientSend
{
public:
	struct options_t
	{
		options_t(client_udp_interface_t& l)
			: line_interface{l}
		{
		}
		client_udp_interface_t& line_interface;
	};

	template <typename T, typename = std::enable_if_t<
			!std::is_same_v<std::decay_t<T>, ClientSend>>>
	inline
	ClientSend(T&& o)
		: options{std::forward<T>(o)}
	{
	}
	inline
	~ClientSend()
	{
	}

	void send_01(net::msg_udp_ts&);

	handlers_inline msg_handlers
	{
		  &ClientSend::send_01 | hook<net::msg_udp>
	};

private:
	options_t options;
};


class ClientWork
{
public:
	struct options_t
	{
		options_t(client_send_interface_t& i, client_work_iterface_t& si, file_t& _file, std::uint64_t _id)
			: send_interface{i}
			, self_interface{si}
			, file{std::move(_file)}
			, id{_id}
		{
		}
		client_send_interface_t& send_interface;
		client_work_iterface_t& self_interface;
		file_t file;
		std::uint64_t const id;
	};

	template<
		typename T
		, std::enable_if_t<
			!std::is_same_v<std::decay_t<T>, ClientWork>
			, bool
		> = true
	>
	inline
	ClientWork(T&& o)
		: options{std::forward<T>(o)}
	{
		seq_total = options.file.size();
		for (auto&& v : options.file)
		{
			crc = crc32c(crc, v.second.data(), v.second.size());
		}
	}
	inline
	~ClientWork()
	{
	}

	void send_pack(std::uint32_t sn, byte_array_t& d);
	inline void send_empl();
	auto get_rend_pos();

	void work(msg_client_ts&);
	void timeout(msg_client_ts&);
	void start_send(msg_client_ts&);

	handlers_inline msg_handlers
	{
		  &ClientWork::work | hook<net::msg_udp>
		, &ClientWork::timeout | hook<timeout_t>
		, &ClientWork::start_send | hook<start_send_t>
	};

private:
	options_t options;
	std::size_t seq_total;
	sended_seq_list_t sended_seqs;
	uint32_t crc = 0;
};


class ClientNet
{
public:
	using works_t = std::map<std::uint64_t, std::unique_ptr<client_work_iterface_t>>;

	struct options_t
	{
		options_t(client_udp_interface_t& i, mx_queue_t<file_t>& fq)
			: interface{i}
			, file_queue{fq}
		{
		}
		client_udp_interface_t& interface;
		mx_queue_t<file_t>& file_queue;
	};

	template <typename T, typename = std::enable_if_t<
			!std::is_same_v<std::decay_t<T>, ClientNet>>>
	inline
	ClientNet(T&& o)
		: options{std::forward<T>(o)}
	{
		send_thr | start<ClientSend>
		(
			ClientSend::options_t(
				options.interface
			)
			, ClientSend::msg_handlers
		);
	}
	inline
	~ClientNet()
	{
		send_thr | stop | join;
		for (auto& el : works)
		{
			*el.second | stop | join;
		}
	}

	
	void error_hadler(client_msg_err const&);
	void timeout(client_msg_err const&);

	void rcv_seq(net::msg_udp_ts&);

	handlers_inline msg_handlers
	{
		    &ClientNet::rcv_seq | hook<net::msg_udp>
	};

	handlers_inline error_handlers
	{
		  &ClientNet::error_hadler | hook<msg_error_t>
		, &ClientNet::timeout | hook<msg_timeout_t>
	};

private:
	options_t options;
	client_send_interface_t send_thr;
	works_t works;
	std::uint64_t next_id = 0;
};


#endif