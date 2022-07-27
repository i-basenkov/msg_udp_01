
#include <iostream>
#include <fstream>

#include <tuple>

#include <unistd.h>
#include <charconv>
#include <atomic>

#include <signal.h>

#include "../lib_msg/msgthreads.h"
#include "../lib_msg/msgutils.h"

#include "../include/msg_types.h"
#include "display.h"


#include <ctime>
#include <cstddef>

#include "srv_net.h"
#include "../include/msg_types.h"

using namespace msg;

static std::atomic<int> stop_prog{0};
static void sighandler(int){ stop_prog = 1; }


int main()
{

	signal(SIGINT, sighandler);


	file_send::net::udp_interface_t srv_net(ip_addr(127, 0, 0, 1), port(7002), port(7001));


	display | start<>(disp_handlers);

	srv_net | start<SrvNet>
	(
		SrvNet::options_t(
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

	std::cout << "-------- Сервер завершает работу --------" << std::endl;

	usleep(2000 * 1000);


	srv_net | stop | join;


	display | stop | join;



	return 0;
}
