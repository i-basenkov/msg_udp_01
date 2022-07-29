
#include <algorithm>
#include <map>
#include <cstring>

#include "../lib_msg/msgthreads.h"

#include "../include/msg_types.h"

#include "srv_net.h"


void SrvSend::send_01(net::msg_udp_ts& in_d)
{
	std::ostringstream os;
	auto& d = std::get<net::msg_udp>(in_d);
	if (!options.line_interface.send(udp_test::serializer(d)))
	{
		display.send("Ошибка передачи SrvSend::send_01!");
	}
}



void SrvWork::work(net::msg_udp_ts& in_d)
{
	using std::cout, std::endl;
//	std::ostringstream os;
	union cnv_type
	{
		int d_i;
		std::uint8_t d_u8;
		std::uint32_t d_u32;
		std::size_t d_sz;
	};

	auto& d = std::get<net::msg_udp>(in_d);
	cnv_type cnv;

	file[d.head.seq_number] = std::move(d.data);

	net::msg_udp msg;
	msg.head = d.head;
	cnv.d_sz = file.size();
	msg.head.seq_total = cnv.d_u32;
	msg.head.type = pack_type::ack;

	if (file.size() == d.head.seq_total)
	{
//		os << "id = " << d.head.id << "\n";
		uint32_t crc = 0;
		for (auto&& v : file)
		{
//			os << "seq_number = " << v.first << "\n";
			crc = crc32c(crc, v.second.data(), v.second.size());
		}
//		os << "crc = " << std::hex << crc << std::dec << "\n";
//		os << endl;
		msg.data.resize(sizeof(crc));
		crc = byte_swap<endianness::host, endianness::network>(crc);
		memcpy(msg.data.data(), &crc, sizeof(crc));
		options.send_interface.send(std::move(msg));

//		display << disp_msg::msg1_t(std::move(os.str()));

		options.self_interface.set_status_flags(2);
		options.self_interface.stop(1);
	}
	else
	{
		options.send_interface.send(std::move(msg));
	}
}

void SrvNet::error_hadler(net::msg_err const& in_d)
{
	using std::cout, std::endl;

	std::ostringstream os;

	os << endl
	<< "---- server net error_handler!!! ----"
	<< std::this_thread::get_id()
	<< endl;
	auto& d = std::get<msg_error_t>(in_d);
	os << "Код ошибки = " << d.data << endl;
	display.send(std::move(os.str()));
}

void SrvNet::rcv_seq(net::msg_udp_ts& in_d)
{
	auto& d = std::get<net::msg_udp>(in_d);
	if (auto w = works.find(d.head.id);
		w != works.end()
		&& (w->second->status() & 0x02) == 0
	)
	{
		w->second->send(std::move(d));
		return;
	}
	for (auto i = works.begin(); i != works.end();)
	{
		if (i->second->joinable()
			&& (i->second->status() & 0x02) == 2
		)
		{
			i->second->join();
			i = works.erase(i);
		}
		else
		{
			++i;
		}
	}
	auto nw = works.emplace(d.head.id, std::make_unique<srv_work_iterface_t>());
	auto& thri = *(nw.first->second);
	start_thread<SrvWork>
	(
		thri
		, SrvWork::options_t(
			send_thr
			, thri
		)
		, SrvWork::msg_handlers
	);
	thri.send(std::move(d));
}



