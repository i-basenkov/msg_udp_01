
#include <cstring>

#include <algorithm>
#include <map>
#include <experimental/random>

#include "../lib_msg/msgthreads.h"
#include "../include/msg_types.h"

#include "client_net.h"


void ClientSend::send_01(net::msg_udp_ts& in_d)
{
	std::ostringstream os;
	auto& d = std::get<net::msg_udp>(in_d);
	if (!options.line_interface.send(udp_test::serializer(d)))
	{
		display.send("Ошибка передачи ClientSend::send_01!");
	}
}

void ClientWork::send_pack(std::uint32_t sn, byte_array_t& d)
{
	union cnv_type
	{
		int d_i;
		std::uint8_t d_u8;
		std::uint32_t d_u32;
		std::size_t d_sz;
	};
	cnv_type cnv;
	net::msg_udp msg;
	cnv.d_sz = seq_total;
	msg.head.seq_total = cnv.d_u32;
	msg.head.type = pack_type::put;
	msg.head.id = options.id;
	msg.head.seq_number = sn;
	std::copy(d.begin(), d.end(), std::back_insert_iterator(msg.data));
	options.send_interface.send(std::move(msg));
}

auto ClientWork::get_rend_pos()
{
	std::size_t h = std::experimental::randint(0UL, options.file.size());
	auto pos = options.file.begin();
	for (std::size_t i = 0; i < h; ++i)
	{
		++pos;
		if (pos == options.file.end())
		{
			pos = options.file.begin();
		}
	}
	return pos;
}

inline
void ClientWork::send_empl()
{
	while (sended_seqs.size() < 5 && options.file.size() > 0)
	{
		if (auto pos = get_rend_pos(); pos != options.file.end())
		{
			auto empl_res = sended_seqs.emplace(pos->first, sended_seq_t{5, std::move(pos->second)});
			options.file.erase(pos);
			send_pack(empl_res.first->first, empl_res.first->second.seq);
		}
	}
}

void ClientWork::work(msg_client_ts& in_d)
{
	std::ostringstream os;

	union cnv_type
	{
		int d_i;
		std::uint8_t d_u8;
		std::uint32_t d_u32;
		std::size_t d_sz;
	};

	auto& d = std::get<net::msg_udp>(in_d);
	if (auto pos = sended_seqs.find(d.head.seq_number); pos != sended_seqs.end())
	// <<<<---- удалить пакет из send_seqs
	{
		sended_seqs.erase(pos);
	}
	// <<<<---- проверить на прием всех пакетов ----
	if (d.head.seq_total < seq_total)
	// <<<<---- передаем пакет ----
	{
		send_empl();
	}
	else if (d.head.seq_total == seq_total)
	// <<<<---- проверить контрольную сумму и уйти ----
	{
		uint32_t in_crc;
		memcpy(&in_crc, d.data.data(), sizeof(in_crc));
		in_crc = byte_swap<endianness::network, endianness::host>(in_crc);
		os << "\n Client:" << " seq_total = " << seq_total << "\n"
		<< "id = " << options.id << "\n";
		os << std::hex
		<< "crc = " << crc << "; in_crc = " << in_crc
		<< std::dec;
		if (crc == in_crc)
		{
			os << " - OK";
		}
		else
		{
			os << " - Error!";
		}
		os << "\n";
		os << endl;

		display.send(std::move(os.str()));

		options.self_interface.set_status_flags(2);
		options.self_interface.stop(1);
	}
	else
	// <<<<---- сбой ----
	{
		std::cout << "-------- ClientWork::work error --------" << std::endl;
	}
}

void ClientWork::start_send(msg_client_ts&)
{
	send_empl();
}

void ClientWork::timeout(msg_client_ts&)
{
	std::ostringstream os;
	for (auto&& el : sended_seqs)
	{
		--el.second.secs;
//		os << "-------- ClientWork::timeout id = "
//		<< options.id
//		<< " seq_number = " << el.first
//		<< " secs = " <<el.second.secs
//		<< " --------" << std::endl;
		if (el.second.secs == 0)
		{
//			os << "-------- ClientWork::timeout id = "
//			<< options.id << " seq_number = " << el.first <<  " - send"
//			<< " --------" << std::endl;
			send_pack(el.first, el.second.seq);
			el.second.secs = 5;
		}
	}
	display.send(std::move(os.str()));
}

void ClientNet::error_hadler(client_msg_err const& in_d)
{
	using std::cout, std::endl;

	std::ostringstream os;

	os << endl
	<< "---- client net error_handler!!! ----"
	<< std::this_thread::get_id()
	<< endl;
	auto& d = std::get<msg_error_t>(in_d);
	os << "Код ошибки = " << d.data << endl;
	display.send(std::move(os.str()));
}

void ClientNet::timeout_proc(client_msg_err const& )
{
	bool qe = true;
	while (qe && works.size() < 14)
	{
		file_t file;
		{
			std::lock_guard<std::mutex> lg(options.file_queue.mtx);
			if (!options.file_queue.queue.empty())
			// забрать файл из очереди
			{
				file = std::move(options.file_queue.queue.front());
				options.file_queue.queue.pop();
			}
			else
			{
				qe = false;
			}
		}
		if (file.size() > 0)
		// создать новый work
		{
			auto nw = works.emplace(next_id, std::make_unique<client_work_iterface_t>());
			auto& thri = *(nw.first->second);
			start_thread<ClientWork>
			(
				thri
				, ClientWork::options_t(
					send_thr
					, thri
					, file
					, next_id
				)
				, ClientWork::msg_handlers
			);
			timer.add_client(next_id, thri);
			++next_id;
			thri.send(start_send_t{});
		}
	}

}

void ClientNet::rcv_seq(net::msg_udp_ts& in_d)
{
	auto& d = std::get<net::msg_udp>(in_d);
	if (auto w = works.find(d.head.id);
		w != works.end()
	)
	{
		w->second->send(std::move(d));
	}
	for (auto i = works.begin(); i != works.end();)
	{
		if (i->second->joinable()
			&& (i->second->status() & 0x02) == 2
		)
		{
			i->second->join();
			timer.remove_client(i->first);
			i = works.erase(i);
		}
		else
		{
			++i;
		}
	}
}



