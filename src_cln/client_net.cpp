
#include <cstring>

#include <algorithm>
#include <map>
#include <experimental/random>

#include "../lib_msg/msgthreads.h"
#include "../include/msg_types.h"

#include "client_net.h"


void ClientSend::send_01(net::msg_udp_ts& d)
{
	std::ostringstream os;
	d | to<net::msg_udp> | into >> [this, &os](auto&& d)
	{
		options.line_interface << udp_test::serializer(d)
		| if_error >> [&os]()
		{
			display << disp_msg("Ошибка передачи ClientSend::send_01!");
		};
	};
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
	options.send_interface << std::move(msg);
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

void ClientWork::work(msg_client_ts& d)
{
	std::ostringstream os;

	union cnv_type
	{
		int d_i;
		std::uint8_t d_u8;
		std::uint32_t d_u32;
		std::size_t d_sz;
	};

	d | to<net::msg_udp> | into >> [this, &os](auto&& d)
	{
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

			display << disp_msg(std::move(os.str()));

			options.self_interface
			| set_status((options.self_interface | status) | 2)
			| stop;
		}
		else
		// <<<<---- сбой ----
		{
			std::cout << "-------- ClientWork::work error --------" << std::endl;
		}
	};

}

void ClientWork::start_send(msg_client_ts&)
{
	send_empl();
}

void ClientWork::timeout(msg_client_ts&)
{
	std::cout << "-------- ClientWork::timeout --------" << std::endl;
}

void ClientNet::error_hadler(client_msg_err const& d)
{
	using std::cout, std::endl;

	std::ostringstream os;

	os << endl
	<< "---- client net error_handler!!! ----"
	<< std::this_thread::get_id()
	<< endl;
	d | to<msg_error_t> | data | into >> [&os](auto&& d)
	{
		os << "Код ошибки = " << d << endl;
	};
	display << disp_msg(std::move(os.str()));
}

void ClientNet::timeout(client_msg_err const& )
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
			thri | start<ClientWork>
			(
				ClientWork::options_t(
					send_thr
					, thri
					, file
					, next_id
				)
				, ClientWork::msg_handlers
			);
			++next_id;
			thri << start_send_t{};
		}
	}

}

void ClientNet::rcv_seq(net::msg_udp_ts& d)
{
	d | to<net::msg_udp> | into >> [this](auto&& d)
	{
		if (auto w = works.find(d.head.id);
			w != works.end()
		)
		{
			*w->second << std::move(d);
		}
		for (auto i = works.begin(); i != works.end();)
		{
			if ((*i->second | joinable)
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
	};
}



