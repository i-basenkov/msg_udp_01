
#include <cstring>

#include <algorithm>
#include <map>
#include <experimental/random>

#include "../lib_msg/msgthreads.h"

#include "client_net.h"

namespace msg::file_send
{

	void ClientWork::run()
	{
		msg_client_ts vmsg;
		self_i.status |= 0x01;
		while (!static_cast<std::uint32_t>(self_i.stop))
		{
			{
				std::unique_lock<std::mutex> ul(self_i.mtx);
				if (self_i.cvar.wait_for(ul, std::chrono::seconds(1),
					[this](){return !self_i.queue.empty();}))
				{
					vmsg = std::move(self_i.queue.front());
					self_i.queue.pop();
				}
				else
				{
					continue;
				}
			}
			std::visit([this](auto&& msg)
			{
				handler(msg);
			}, vmsg);
		}
		self_i.status &= ~0x01u;
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
		msg::net::msg_udp msg;
		cnv.d_sz = seq_total;
		msg.head.seq_total = cnv.d_u32;
		msg.head.type = msg::net::pack_type::put;
		msg.head.id = id;
		msg.head.seq_number = sn;
		std::copy(d.begin(), d.end(), std::back_insert_iterator(msg.data));
		send_i.send(std::move(msg));
	}

	auto ClientWork::get_rend_pos()
	{
		std::size_t h = std::experimental::randint(0UL, file.size());
		auto pos = file.begin();
		for (std::size_t i = 0; i < h; ++i)
		{
			++pos;
			if (pos == file.end())
			{
				pos = file.begin();
			}
		}
		return pos;
	}

	inline
	void ClientWork::send_empl()
	{
		while (sended_seqs.size() < 5 && file.size() > 0)
		{
			if (auto pos = get_rend_pos(); pos != file.end())
			{
				auto empl_res = sended_seqs.emplace(pos->first, sended_seq_t{5, std::move(pos->second)});
				file.erase(pos);
				send_pack(empl_res.first->first, empl_res.first->second.seq);
			}
		}
	}

	void ClientWork::handler(msg::net::msg_udp& d)
	{
		union cnv_type
		{
			int d_i;
			std::uint8_t d_u8;
			std::uint32_t d_u32;
			std::size_t d_sz;
		};

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
			in_crc = ntohl(in_crc);
			std::cout << "\n Client:" << " seq_total = " << seq_total << "\n"
			<< "id = " << id << "\n";
			std::cout << std::hex
			<< "crc = " << crc << "; in_crc = " << in_crc
			<< std::dec;
			if (crc == in_crc)
			{
				std::cout << " - OK";
			}
			else
			{
				std::cout << " - Error!";
			}
			std::cout << "\n" << std::endl;

			self_i.status |= 0x02;
			self_i.stop |= 0x01;
		}
		else
		// <<<<---- сбой ----
		{
			std::cout << "-------- ClientWork::work error --------" << std::endl;
		}
	}

	void ClientWork::handler(start_send_t&)
	{
		send_empl();
	}

	void ClientWork::handler(msg_timeout_t&)
	{
		for (auto&& el : sended_seqs)
		{
			--el.second.secs;
			if (el.second.secs == 0)
			{
				std::cout << "-------- ClientWork::timeout id = "
				<< id << " seq_number = " << el.first <<  " - send"
				<< " --------" << std::endl;
				send_pack(el.first, el.second.seq);
				el.second.secs = 5;
			}
		}
	}



	void ClientNet::run()
	{
		self_i.sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (self_i.sock <= 0)
		{
			self_i.sock = 0;
			error_handler(msg_error_t{41});
			return;
		}

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(self_i.self_port);
		addr.sin_addr.s_addr = htonl(self_i.addr);
		if (bind(self_i.sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
		{
			close(self_i.sock);
			error_handler(msg_error_t{42});
			return;
		}

		pollfd s_pfd{}, *pfd{&s_pfd};
		pfd->fd = self_i.sock;
		pfd->events = POLLIN | POLLHUP | POLLERR;
		pfd->revents = 0;

		sockaddr_in from{};
		socklen_t fromlen;

		std::unique_ptr<uint8_t[]> ubuf{new uint8_t[4096]};

		self_i.status |= 0x01;

		while (!static_cast<std::uint32_t>(self_i.stop))
		{
			pfd->revents = 0;
			int res_poll = poll(pfd, 1, 100); 
			if ((res_poll > 0) && (pfd->revents == POLLIN))
			{
				memset(&from, 0, sizeof(sockaddr_in));
				fromlen = sizeof(sockaddr_in);
				if (ssize_t blen = recvfrom(self_i.sock, ubuf.get(), 4096, 0
						, reinterpret_cast<sockaddr*>(&from), &fromlen); blen > 0)
				{
					byte_array_t bm(static_cast<std::size_t>(blen));
					memcpy(bm.data(), ubuf.get(), static_cast<std::size_t>(blen));
					net::msg_udp_ts vmsg = udp_test::deserializer(bm);
					rcv_seq(vmsg);
				}
			}
			else if (res_poll == 0)
			{
				// послать таймаут
				timeout_handler(msg_timeout_t{1});
			}
			else if (pfd->revents == POLLHUP)
			{
				error_handler(msg_error_t{43});
			}
		}
		self_i.status &= ~0x01u;
		close(self_i.sock);
		self_i.sock = 0;
	}

	void ClientNet::error_handler(msg_error_t const& d)
	{
		std::cout << std::endl
		<< "---- client net error_handler!!! ----"
		<< std::this_thread::get_id()
		<< std::endl;
		std::cout << "Код ошибки = " << d.data << std::endl;
	}

	void ClientNet::timeout_handler(msg_timeout_t const& )
	{
		bool qe = true;
		std::size_t max_thr = std::thread::hardware_concurrency();
		max_thr = (max_thr < 2) ? 1 : max_thr/2;
		while (qe && works.size() < max_thr)
		{
			file_t file;
			{
				std::lock_guard<std::mutex> lg(file_queue.mtx);
				if (!file_queue.queue.empty())
				// забрать файл из очереди
				{
					file = std::move(file_queue.queue.front());
					file_queue.queue.pop();
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
				thri.thread = std::thread(worker_t<ClientWork>
				(
					thri
					, send_thr
					, file
					, next_id
				));
				timer.add_client(next_id, thri);
				++next_id;
				thri.send(start_send_t{});
			}
		}

	}


	void ClientNet::rcv_seq(net::msg_udp_ts& in_d)
	{
		auto& d = std::get<msg::net::msg_udp>(in_d);
		if (auto w = works.find(d.head.id);
			w != works.end()
		)
		{
			w->second->send(std::move(d));
		}
		for (auto i = works.begin(); i != works.end();)
		{
			if (i->second->joinable()
				&& (i->second->status & 0x02) == 2
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

}


