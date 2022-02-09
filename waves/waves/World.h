#pragma once 

#define _USE_MATH_DEFINES
#include <math.h>


#include <algorithm>
#include <functional>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <unordered_set>
#include <sstream>
#include <array>

#include <ppl.h>
#include <immintrin.h> 

#include "vec3d.h"
#include "Random.h"
#include "ThreadGrid.h"

#include "Utils.h"

#include "Medium.h"

#include "Log.h"

namespace waves
{ 
    class World
    {
	public:
		using TMedium = Medium<512, 512, 1>;

		static constexpr float VEL_FACTOR1 = 0.40; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr float VEL_FACTOR2 = 0.13; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr float LOC_FACTOR = 0.1; // dX = V * dT, this is dT

		static constexpr float F_B = 0.98;
		static constexpr float EDGE_SLOW_DOWN_FACTORS[] = { F_B * F_B * F_B * F_B, F_B * F_B * F_B, F_B * F_B, F_B };

		static constexpr float INV_SQRT_2 = 0.707106781187f;

	private:
		TMedium _medium;

        Random _random{};
		uint64_t _iteration{ 0 };

		uint64_t first_half_clocks{ 0 };
		uint64_t second_half_clocks{ 0 };

		std::array<std::atomic_bool, 3> light_enabled { true, true, true };

	public:
        World()
        {	
		}

		~World()
		{
		}

#pragma warning(push)
#pragma warning(disable:26451)
		bool iterate()  noexcept
		{
			int fill_value = ((_iteration % 70) < 35) ? 10000 : -10000;
			if (light_enabled[0])
				fill(38, 154, 5, 5, fill_value);
			if (light_enabled[1])
				fill(38, 254, 5, 5, fill_value);
			if (light_enabled[2])
				fill(38, 354, 5, 5, fill_value);

			uint64_t start = __rdtsc();

			constexpr int xn_yn_z0 = TMedium::offset_for(-1, -1, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int x0_yn_z0 = TMedium::offset_for(0, -1, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int xp_yn_z0 = TMedium::offset_for(1, -1, 0) - TMedium::offset_for(0, 0, 0);

			constexpr int xn_y0_z0 = TMedium::offset_for(-1, 0, 0) - TMedium::offset_for(0, 0, 0);
			//constexpr int x0_y0_z0 = TMedium::offset_for(0, 0, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int xp_y0_z0 = TMedium::offset_for(1, 0, 0) - TMedium::offset_for(0, 0, 0);

			constexpr int xn_yp_z0 = TMedium::offset_for(-1, 1, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int x0_yp_z0 = TMedium::offset_for(0, 1, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int xp_yp_z0 = TMedium::offset_for(1, 1, 0) - TMedium::offset_for(0, 0, 0);

			concurrency::parallel_for(
				0, static_cast<int>(TMedium::width() / 16),
				[&](int X_big)
				{
					for (int sub_x = 0; sub_x < 16; ++sub_x)
					{
						const int x = X_big * 16 + sub_x;

						for (int y = 0; y < TMedium::height(); ++y)
						{
							int offset = TMedium::offset_for(x, y, 0);

							const float neigh_total =
								_medium.data[offset + xn_yn_z0].displacement * INV_SQRT_2 +
								_medium.data[offset + x0_yn_z0].displacement +
								_medium.data[offset + xp_yn_z0].displacement * INV_SQRT_2 +
								_medium.data[offset + xn_y0_z0].displacement +
								_medium.data[offset + xp_y0_z0].displacement +
								_medium.data[offset + xn_yp_z0].displacement * INV_SQRT_2 +
								_medium.data[offset + x0_yp_z0].displacement +
								_medium.data[offset + xp_yp_z0].displacement * INV_SQRT_2;

							const float neight_average = neigh_total / (4.0f + 4.0f * INV_SQRT_2);

							const float delta_x = _medium.data[offset].displacement - neight_average; // displacement relative to the current neightbour average 

							if ( x < 300 && (std::pow(x - 356.0f, 2.0f) + std::pow(y - 256.0f, 2.0f)) < 15000.0f)
								_medium.data[offset].veocity -= VEL_FACTOR2 * delta_x;
							else 
								_medium.data[offset].veocity -= VEL_FACTOR1 * delta_x;
						}
					}
				}
				);

			// prevent the reflections from the edges by dunmping the velocity at the boundaries 
			concurrency::parallel_invoke(
				[&]() 
				{
					for (int y = 0; y < TMedium::height(); ++y)
					{
						for (int i = 0; i < 4; ++i)
						{
							_medium.at(i, y, 0).veocity *= EDGE_SLOW_DOWN_FACTORS[i];
							_medium.at(TMedium::width() - 1 - i, y, 0).veocity *= EDGE_SLOW_DOWN_FACTORS[i];
						}
					}
				},
				[&]() 
				{
					for (int x = 0; x < TMedium::width(); ++x)
					{
						for (int i = 0; i < 4; ++i)
						{
							_medium.at(x, i, 0).veocity *= EDGE_SLOW_DOWN_FACTORS[i];
							_medium.at(x, TMedium::height() - 1 - i, 0).veocity *= EDGE_SLOW_DOWN_FACTORS[i];
						}
					}
				}
			);


			uint64_t mid = __rdtsc();

			// Update the locations 
			concurrency::parallel_for(
				0, static_cast<int>(TMedium::width() / 16),
				[&](int X_big)
				{
					for (int sub_x = 0; sub_x < 16; ++sub_x)
					{
						for (int y = 0; y < TMedium::height(); ++y)
						{
							int offset = TMedium::offset_for(X_big * 16 + sub_x, y, 0);
							_medium.data[offset].displacement += _medium.data[offset].veocity * LOC_FACTOR;
						}
					}
				});


			uint64_t end = __rdtsc();

			first_half_clocks += mid - start;
			second_half_clocks += end - mid;

			_iteration++;
			return true;
        }
#pragma warning(pop)

		uint64_t current_iteration() const noexcept
		{
			return _iteration;
		}

		const TMedium& get_data() const { return _medium; }


		const std::tuple<uint64_t, uint64_t> get_perf_stats()
		{
			if (_iteration == 0)
				return { 0, 0 };

			return { first_half_clocks / _iteration, second_half_clocks / _iteration };
		}

		void toggle_light(int idx)
		{
			if (idx >= 0 && idx < light_enabled.size())
			{
				light_enabled[idx] = !light_enabled[idx];
			}
		}

	private: 
		void fill(int x, int y, int w, int h, int value)
		{
			for (int i = x; i < x + w; ++i)
			{
				for (int j = y; j < y + h; ++j)
				{
					auto& item = _medium.at(x, y, 0);
					item.displacement = value;
					item.veocity = 0.0f;
				}
			}
		}
    };
}
