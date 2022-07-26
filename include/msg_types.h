#ifndef MSG_TYPES_H
#define MSG_TYPES_H

#include <vector>
#include <queue>
#include <map>

#include "../lib_msg/msgthreads.h"

namespace msg::file_send
{
	namespace pack_type
	{
		constexpr uint8_t ack = 0;
		constexpr uint8_t put = 1;
	}

	using file_t = std::map<std::uint32_t, byte_array_t>;
	using file_list_t = std::map<std::uint64_t, file_t>;
	using file_queue_t = std::queue<file_t>;

	struct sended_seq_t
	{
		uint8_t secs;
		byte_array_t seq;
	};
	using sended_seq_list_t = std::map<std::uint32_t, sended_seq_t>;

	namespace net
	{

		#pragma pack(push)
		#pragma pack(1)
		struct msg_head_pack_t
		{
			std::uint32_t seq_number;
			std::uint32_t seq_total;
			std::uint8_t type;
			std::byte id[8];
		};
		#pragma pack(pop)

		struct msg_head_t{
			std::uint32_t seq_number;
			std::uint32_t seq_total;
			std::uint8_t type;
			std::uint64_t id;
		};

		struct msg_udp
		{
			msg_head_t head;
			byte_array_t data;
		};

		using msg_udp_ts = message_variants_t
		<
			msg_udp
		>;

		using msg_err = message_variants_t
		<
			  msg_error_t
			//, msg_timeout_t
		>;

		using udp_interface_t = thread_interface_t<msg_udp_ts, msg_err, udp>;

	}

    using srv_send_interface_t = thread_interface_t<net::msg_udp_ts>;
	using srv_work_iterface_t = thread_interface_t<net::msg_udp_ts>;

	enum struct msgs_client
	{
		timeout,
		start_send
	};
	using timeout_t = std::integral_constant<msgs_client, msgs_client::timeout>;
	using start_send_t = std::integral_constant<msgs_client, msgs_client::start_send>;

	using client_msg_err = message_variants_t
	<
		  msg_error_t
		, msg_timeout_t
	>;

	using msg_client_ts = message_variants_t
	<
		  net::msg_udp
		, timeout_t
		, start_send_t
	>;


	using client_udp_interface_t = thread_interface_t<net::msg_udp_ts, client_msg_err, udp, timeout<1>>;
    using client_send_interface_t = thread_interface_t<net::msg_udp_ts>;
	using client_work_iterface_t = thread_interface_t<msg_client_ts>;


}


#endif
