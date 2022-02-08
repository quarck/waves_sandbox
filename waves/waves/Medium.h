#pragma once

#include <stdint.h>

namespace waves
{
	template <int W, int H, int D, typename TValue=float>
	struct Medium
	{
		std::vector<TValue> data;

		Medium() : data(W* H* D)
		{

		}

		static constexpr int width() noexcept { return W; }
		static constexpr int height() noexcept { return H; }
		static constexpr int depth() noexcept { return D; }

		constexpr static int offset_for(int x, int y, int z) noexcept
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
