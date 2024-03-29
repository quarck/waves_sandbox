#pragma once

#include <stdint.h>

namespace waves
{
	struct Item
	{
		float location;
		float velocity;
	};

	struct ItemStatic
	{
		uint8_t velocity_bit : 1;
		uint8_t conductivity : 7;
	};
	static_assert(sizeof(ItemStatic) == 1);

	template <int W, int H, int D, typename TItem=Item, int GUARD_SIZE = 4, bool skip_assert=false>
	struct Medium
	{
		static_assert(W % 16 == 0 || skip_assert);
		static_assert(H % 16 == 0 || skip_assert);
		static_assert(D % 16 == 0 || skip_assert);

		static_assert(GUARD_SIZE > 0 || skip_assert);

		static constexpr int W_GUARD = GUARD_SIZE;
		static constexpr int H_GUARD = GUARD_SIZE;
		static constexpr int D_GUARD = GUARD_SIZE;

		static constexpr int alloc_width = W + 2 * W_GUARD;
		static constexpr int alloc_height = H + 2 * H_GUARD;
		static constexpr int alloc_depth = D + 2 * D_GUARD;

		std::vector<TItem> data;

		Medium() : data(alloc_width * alloc_height * alloc_depth)
		{

		}

		static constexpr int width() noexcept { return W; }
		static constexpr int height() noexcept { return H; }
		static constexpr int depth() noexcept { return D; }

		constexpr static int offset_for(int x, int y, int z) noexcept
		{
			return (z + D_GUARD) * alloc_width * alloc_depth + (y + H_GUARD) * alloc_width + (x + W_GUARD);
			//return (x + W_GUARD)* alloc_width* alloc_depth + (y + H_GUARD) * alloc_depth + z + D_GUARD;
		}

		const auto& at(int x, int y, int z) const 
		{
			return data[offset_for(x, y, z)];
		}

		auto& at(int x, int y, int z)
		{
			return data[offset_for(x, y, z)];
		}

		void fill(TItem&& value)
		{
			std::fill(data.begin(), data.end(), value);
		}
	};

}
