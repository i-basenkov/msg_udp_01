
#include "../lib_msg/msgthreads.h"
#include "display.h"


using namespace msg;

void print1(disp_msg_ts const& in_d)
{
	auto& d = std::get<disp_msg>(in_d);
	std::cout << d.data << std::endl;
}


