#pragma once

#include <stdint.h>

namespace waves
{
	template <size_t W, size_t H, size_t D, typename TValue=float> 
	struct Medium
	{
		std::vector<TValue> data;

		Medium() : data(W* H* D)
		{

		}

		size_t width() const noexcept { return W; }
		size_t height() const noexcept { return H; }
		size_t depth() const noexcept { return D; }

		static size_t offset_for(int x, int y, int z) noexcept
		{
			return x * H * D + y * D + z;
		}

		const TValue& at(int x, int y, int z) const 
		{
			return data[offset_for(x, y, z)];
		}

		TValue& at(int x, int y, int z)
		{
			return data[offset_for(x, y, z)];
		}
	};

}
