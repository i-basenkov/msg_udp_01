
#include "../lib_msg/msgthreads.h"
#include "display.h"


using namespace msg;

void print1(disp_msg_ts const& d)
{
	d | to<disp_msg> | data | into >> [](auto& d1)
	{
		std::cout << d1 << std::endl;
	};
}


