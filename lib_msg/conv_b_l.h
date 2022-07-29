#ifndef CONV_B_L_H
#define CONV_B_L_H

#include <endian.h>


namespace msg
{

	enum class endianness
	{
		little,
		big,
		network = big,

		#if __BYTE_ORDER == __LITTLE_ENDIAN
			host = little
		#elif  __BYTE_ORDER == __BIG_ENDIAN
			host = big
		#else
			#error "unable to determine system endianness"
		#endif
	};


	namespace _detale
	{
		union b_conv_t
		{
			float f_d;
			double d_d;
			int16_t i16_d;
			int32_t i32_d;
			int64_t i64_d;
			uint16_t u16_d;
			uint32_t u32_d;
			uint64_t u64_d;
		};

		template <typename>
		constexpr bool always_false_v{false};

		template<typename T>
		inline T swap_bytes(T v) noexcept
		{
			if constexpr (sizeof(T) == 2)
			{
				_detale::b_conv_t cnv{};
				cnv.u16_d = v;
				cnv.u32_d = ((((cnv.u32_d) >> 8) & 0xffu) |
					(((cnv.u32_d) & 0xffu) << 8));
				return cnv.u16_d;
			}
			else if constexpr (sizeof(T) == 4)
			{
				return ((((v) & 0xff000000u) >> 24) |
						(((v) & 0x00ff0000u) >>  8) |
						(((v) & 0x0000ff00u) <<  8) |
						(((v) & 0x000000ffu) << 24));
			}
			else if constexpr (sizeof(T) == 8)
			{
				return ((((v) & 0xff00000000000000ul) >> 56) |
						(((v) & 0x00ff000000000000ul) >> 40) |
						(((v) & 0x0000ff0000000000ul) >> 24) |
						(((v) & 0x000000ff00000000ul) >> 8 ) |
						(((v) & 0x00000000ff000000ul) << 8 ) |
						(((v) & 0x0000000000ff0000ul) << 24) |
						(((v) & 0x000000000000ff00ul) << 40) |
						(((v) & 0x00000000000000fful) << 56));
			}
			else
			{
				static_assert(always_false_v<T>, "msg::_detale::swap_bytes: Тип не поддерживается!");
			}
		}
	} // namespace _detale


	template<endianness from, endianness to, typename T>
	constexpr
	T byte_swap(T val) noexcept
	{
		if constexpr (from == to)
		{
			return val;
		}
		else if constexpr (sizeof(T) == 1)
		{
			return val;
		}
		else if constexpr (std::is_floating_point_v<T>)
		{
			_detale::b_conv_t cnv{};
			if constexpr (sizeof(T) == 4)
			{
				cnv.f_d = val;
				cnv.u32_d = _detale::swap_bytes(cnv.u32_d);
				return cnv.f_d;
			}
			else if constexpr (sizeof(T) == 8)
			{
				cnv.d_d = val;
				cnv.u64_d = _detale::swap_bytes(cnv.u64_d);
				return cnv.d_d;
			}
			else
				static_assert(_detale::always_false_v<T>, "msg::byte_swap: Тип не поддерживается!");
		}
		else if constexpr (std::is_signed_v<T>)
		{
			_detale::b_conv_t cnv{};
			if constexpr (sizeof(T) == 2)
			{
				cnv.i16_d = val;
				cnv.u16_d = _detale::swap_bytes(cnv.u16_d);
				return cnv.i16_d;
			}
			if constexpr (sizeof(T) == 4)
			{
				cnv.i32_d = val;
				cnv.u32_d = _detale::swap_bytes(cnv.u32_d);
				return cnv.i32_d;
			}
			if constexpr (sizeof(T) == 8)
			{
				cnv.i64_d = val;
				cnv.u64_d = _detale::swap_bytes(cnv.u64_d);
				return cnv.i64_d;
			}
		}
		else
		{
			return _detale::swap_bytes(val);
		}
	}

} // namespace msg


#endif // CONV_B_L_H
