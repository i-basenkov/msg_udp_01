
#include <iostream>
#include <fstream>

#include <tuple>

#include <unistd.h>
#include <charconv>
#include <atomic>
#include <experimental/random>

#include <signal.h>

#include "../lib_msg/msgthreads.h"

#include "../include/msg_types.h"
#include "display.h"

#include <ctime>
#include <cstddef>

#include "client_net.h"

using namespace msg;

static std::atomic<int> stop_prog{0};
static void sighandler(int){ stop_prog = 1; }


int main()
{

	signal(SIGINT, sighandler);

	file_send::client_udp_interface_t client_net(ip_addr(127, 0, 0, 1), port(7001), port(7002));

	display | start<>(disp_handlers);

	mx_queue_t<file_t> file_queue;
	client_net | start<ClientNet>
	(
		ClientNet::options_t
		(
			client_net
			, file_queue
		)
		, ClientNet::msg_handlers
		, ClientNet::error_handlers
		, ClientNet::deserializer
	);


	usleep(1000 * 1000);


	while (!stop_prog)
	{
		file_t f;
		uint32_t Np = std::experimental::randint(100, 2000);
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

		if (file_queue.queue.size() < 28)
		{
			file_queue << f;
		}
		else
		{
			usleep(50 * 1000);
		}

	}

	usleep(5000 * 1000);


	client_net | stop | join;


	display | stop | join;



	return 0;
}
