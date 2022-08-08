
#include <algorithm>
#include <map>
#include <cstring>

#include "../lib_msg/msgthreads.h"

#include "srv_net.h"

namespace msg::file_send
{

	void SrvWork::run()
	{
		net::msg_udp_ts vmsg;
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
			work(vmsg);
		}
		self_i.status &= ~0x01u;
	}

	void SrvWork::work(net::msg_udp_ts& in_d)
	{
		union cnv_type
		{
			int d_i;
			std::uint8_t d_u8;
			std::uint32_t d_u32;
			std::size_t d_sz;
		};

		auto& d = std::get<msg::net::msg_udp>(in_d);
		cnv_type cnv;

		file[d.head.seq_number] = std::move(d.data);

		msg::net::msg_udp msg;
		msg.head = d.head;
		cnv.d_sz = file.size();
		msg.head.seq_total = cnv.d_u32;
		msg.head.type = msg::net::pack_type::ack;

		if (file.size() == d.head.seq_total)
		{
			uint32_t crc = 0;
			for (auto&& v : file)
			{
				crc = crc32c(crc, v.second.data(), v.second.size());
			}
			msg.data.resize(sizeof(crc));
			crc = htonl(crc);
			memcpy(msg.data.data(), &crc, sizeof(crc));
			send_i.send(std::move(msg));

			self_i.status |= 0x02;
			self_i.stop |= 0x01;
		}
		else
		{
			send_i.send(std::move(msg));
		}
	}



	void SrvNet::run()
	{
		self_i.sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (this->self_i.sock <= 0)
		{
			net::msg_err emsg = msg_error_t{41};
			self_i.sock = 0;
			error_hadler(emsg);
			return;
		}

		sockaddr_in addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(self_i.self_port);
		addr.sin_addr.s_addr = htonl(self_i.addr);
		if (bind(self_i.sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
		{
			net::msg_err emsg = msg_error_t{42};
			::close(self_i.sock);
			error_hadler(emsg);
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
			else if (pfd->revents == POLLHUP)
			{
				net::msg_err emsg = msg_error_t{43};
				error_hadler(emsg);
				break;
			}
		}
		self_i.status &= ~0x01u;
		close(self_i.sock);
		self_i.sock = 0;
	}

	void SrvNet::error_hadler(net::msg_err const& in_d)
	{
		using std::cout, std::endl;

		cout << endl
		<< "---- server net error_handler!!! ----"
		<< std::this_thread::get_id()
		<< endl;
		auto& d = std::get<msg_error_t>(in_d);
		cout << "Код ошибки = " << d.data << endl;
	}

	void SrvNet::rcv_seq(net::msg_udp_ts& in_d)
	{

		auto& d = std::get<msg::net::msg_udp>(in_d);
		if (auto w = works.find(d.head.id);
			w != works.end()
			&& (w->second->status & 0x02) == 0
		)
		{
			w->second->send(std::move(d));
			return;
		}
		for (auto i = works.begin(); i != works.end();)
		{
			if (i->second->joinable()
				&& (i->second->status & 0x02u) == 2
			)
			{
				i->second->stop |= 0x01u;
				i->second->join();
				i = works.erase(i);
			}
			else
			{
				++i;
			}
		}
		std::size_t max_thr = std::thread::hardware_concurrency();
		max_thr = (max_thr < 2) ? 1 : max_thr/2;
		if (works.size() < max_thr)
		{
			auto nw = works.emplace(d.head.id, std::make_unique<srv_work_iterface_t>());
			auto& thri = *(nw.first->second);
			thri.thread = std::thread(worker_t<SrvWork>(thri, send_thr));
			thri.send(std::move(d));
		}

	}

}

