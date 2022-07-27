#ifndef DISPLAY_H
#define DISPLAY_H

#include <string>
#include <sstream>

#include "../lib_msg/msgthreads.h"

using namespace msg;


struct disp_msg
{
	disp_msg()
	{
	}
	template <typename S>
	disp_msg(S&& s)
		: data{std::forward<S>(s)}
	{
	}
	std::string data{};
};
using disp_msg_ts = message_variants_t
<
	  disp_msg
>;

void print1(disp_msg_ts const&);

handlers_inline disp_handlers
(
	  hook<disp_msg>(print1)
);

inline thread_interface_t<disp_msg_ts> display;


#endif // DISPLAY_H
