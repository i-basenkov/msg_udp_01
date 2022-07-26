
#include <algorithm>
#include <map>
#include <cstring>

#include "../lib_msg/msgthreads.h"

#include "../include/msg_types.h"

#include "srv_net.h"


void SrvSend::send_01(net::msg_udp_ts& d)
{
	std::ostringstream os;
	d | to<net::msg_udp> | into >> [this, &os](auto&& d)
	{
		options.line_interface << SrvNet::serializer(d)
		| if_error >> [&os]()
		{
			display << disp_msg("Ошибка передачи SrvSend::send_01!");
		};
	};
}



void SrvWork::work(net::msg_udp_ts& d)
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

//	d | to<net::msg_udp> | into >> [this, &os](auto&& d)
	d | to<net::msg_udp> | into >> [this](auto&& d)
	{
		cnv_type cnv;

		file[d.head.seq_number] = std::move(d.data);

		net::msg_udp msg;
		msg.head = d.head;
		cnv.d_sz = file.size();
		msg.head.seq_total = cnv.d_u32;
		msg.head.type = pack_type::ack;

		if (file.size() == d.head.seq_total)
		{
//			os << "id = " << d.head.id << "\n";
			uint32_t crc = 0;
			for (auto&& v : file)
			{
//				os << "seq_number = " << v.first << "\n";
				crc = crc32c(crc, v.second.data(), v.second.size());
			}
//			os << "crc = " << std::hex << crc << std::dec << "\n";
//			os << endl;
			msg.data.resize(sizeof(crc));
			crc = byte_swap<endianness::host, endianness::network>(crc);
			memcpy(msg.data.data(), &crc, sizeof(crc));
			options.send_interface << std::move(msg);

//			display << disp_msg::msg1_t(std::move(os.str()));

			options.self_interface
			| set_status((options.self_interface | status) | 2)
			| stop;
		}
		else
		{
			options.send_interface << std::move(msg);
		}
	};
}



byte_array_t SrvNet::serializer(net::msg_udp const& m)
{
	byte_array_t ba;
	net::msg_head_pack_t hd;
	hd.seq_number = byte_swap<endianness::host, endianness::network>(m.head.seq_number);
	hd.seq_total = byte_swap<endianness::host, endianness::network>(m.head.seq_total);
	hd.type = m.head.type;
	memcpy(&hd.id, &m.head.id, 8);
	ba.resize(sizeof(net::msg_head_pack_t));
	memcpy(ba.data(), &hd, sizeof(hd));
	std::move(m.data.begin(), m.data.end(), std::back_inserter(ba));
	return ba;
}

net::msg_udp_ts SrvNet::deserializer(byte_array_t& _ba)
{
	net::msg_udp msg{};
	net::msg_head_pack_t head{};
	memcpy(&head, _ba.data(), sizeof(head));
	msg.head.seq_number = byte_swap<endianness::network, endianness::host>(head.seq_number);
	msg.head.seq_total = byte_swap<endianness::network, endianness::host>(head.seq_total);
	msg.head.type = head.type;
	memcpy(&msg.head.id, &head.id, 8);
	std::move(_ba.begin()+=sizeof(head), _ba.end(), std::back_inserter(msg.data));
	return msg;
}

void SrvNet::error_hadler(net::msg_err const& d)
{
	using std::cout, std::endl;

	std::ostringstream os;

	os << endl
	<< "---- server net error_handler!!! ----"
	<< std::this_thread::get_id()
	<< endl;
	d | to<msg_error_t> | data | into >> [&os](auto&& d)
	{
		os << "Код ошибки = " << d << endl;
	};
	display << disp_msg(std::move(os.str()));
}

void SrvNet::rcv_seq(net::msg_udp_ts& d)
{
	d | to<net::msg_udp> | into >> [this](auto&& d)
	{
		if (auto w = works.find(d.head.id);
			w != works.end()
			&& ((*w->second | status) & 0x02) == 0
		)
		{
			*w->second << std::move(d);
			return;
		}
		for (auto i = works.begin(); i != works.end();)
		{
			if ((*i->second | in_work)
				&& ((*i->second | status) & 0x02) == 2
			)
			{
				*i->second | join;
				i = works.erase(i);
			}
			else
			{
				++i;
			}
		}
		auto nw = works.emplace(d.head.id, std::make_unique<srv_work_iterface_t>());
		auto& thri = *(nw.first->second);
		thri | start<SrvWork>
		(
			SrvWork::options_t(
				send_thr
				, thri
			)
			, SrvWork::msg_handlers
		) | to_interface << std::move(d);
	};
}



