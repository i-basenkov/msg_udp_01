
#include <iostream>
#include <fstream>

#include <tuple>

#include <unistd.h>
#include <charconv>
#include <atomic>

#include <signal.h>

#include "../lib_msg/msgthreads.h"

#include "../include/msg_types.h"
#include "../src_shr/display.h"


#include <ctime>
#include <cstddef>

#include "srv_net.h"

using namespace msg;

static std::atomic<int> stop_prog{0};
static void sighandler(int){ stop_prog = 1; }


int main()
{

	signal(SIGINT, sighandler);


	file_send::net::udp_interface_t srv_net(ip_addr(127, 0, 0, 1), port(7002), port(7001));


	start_thread<>(display, disp_handlers);

	start_thread<SrvNet>
	(
		srv_net
		, SrvNet::options_t(
			srv_net
		)
		, SrvNet::msg_handlers
		, SrvNet::error_handlers
		, file_send::udp_test::deserializer
	);

	while (!stop_prog)
	{

		usleep(1000 * 1000);

	}

	display.send("-------- Сервер завершает работу --------");

	usleep(2000 * 1000);


	srv_net.stop(1);
	srv_net.join();

	display.stop(1);
	display.join();



	return 0;
}
