
#include <iostream>
#include <fstream>

#include <tuple>

#include <unistd.h>
#include <charconv>
#include <atomic>
#include <experimental/random>

#include <signal.h>

#include "../lib_msg/msgthreads.h"

#include <ctime>
#include <cstddef>

#include "client_net.h"

using namespace msg;
using namespace file_send;

static std::atomic<int> stop_prog{0};
static void sighandler(int){ stop_prog = 1; }


int main()
{

	signal(SIGINT, sighandler);

	file_send::client_udp_interface_t client_net(ip_addr(127, 0, 0, 1), port(7001), port(7002));

	mx_queue_t<file_t> file_queue;

	client_net.thread = std::thread(worker_t<ClientNet>(client_net, file_queue));

	std::size_t max_thr = std::thread::hardware_concurrency();
	max_thr = (max_thr < 2) ? 1 : max_thr/2;

	std::cout << " max_thr = " << max_thr << std::endl;

	while (!stop_prog)
	{
		if (file_queue.queue.size() < max_thr)
		{
			file_t f;
			uint32_t Np = std::experimental::randint(100U, 2000U);
			uint32_t Nb = 1472;
			uint8_t d = 0;
			for (uint32_t j = 0; j < Np; ++j)
			{
				auto empl_res = f.emplace(j, byte_array_t());
				for (uint32_t i = 0; i < Nb; ++i)
				{
					empl_res.first->second.emplace_back(d);
					++d;
				}
			}
			file_queue.emplace(f);
		}
		else
		{
			usleep(100 * 1000);
		}

	}

	std::cout << "-------- Клиент завершает работу --------" << std::endl;

	usleep(2000 * 1000);

	client_net.stop |= 0x01;
	client_net.join();

	return 0;
}
