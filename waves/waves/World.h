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

		static constexpr double VEL_FACTOR1 = 0.40; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr double VEL_FACTOR2 = 0.1; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr double LOC_FACTOR = 0.1; // dX = V * dT, this is dT

		static constexpr double F_B = 0.98;
		static constexpr double EDGE_SLOW_DOWN_FACTORS[] = { F_B * F_B * F_B * F_B, F_B * F_B * F_B, F_B * F_B, F_B };

		static constexpr double INV_SQRT_2 = 0.70710678118654752440084436210485;

	private:
		TMedium _medium;

        Random _random{};
		uint64_t _iteration{ 0 };

		uint64_t first_half_clocks{ 0 };
		uint64_t second_half_clocks{ 0 };

	public:
        World()
        {	
			for (int i = 40 - 2; i < 40 + 2; ++i)
			{
				for (int j = 256 - 10-100; j < 256 - 5 - 100; ++j)
					_medium.at(i, j, 0).displacement = 5000 * 8;

				for (int j = 256 + 5+100; j < 256 + 10+100; ++j)
					_medium.at(i, j, 0).displacement = 5000 * 8;
			}
		}

		~World()
		{
		}

	public:

#pragma warning(push, disable:26451)
		bool iterate()  noexcept
		{
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
						for (int y = 0; y < TMedium::height(); ++y)
						{
							int offset = TMedium::offset_for(X_big*16 + sub_x, y, 0);

							const auto neigh_total =
								_medium.data[offset + xn_yn_z0].displacement * INV_SQRT_2 +
								_medium.data[offset + x0_yn_z0].displacement +
								_medium.data[offset + xp_yn_z0].displacement * INV_SQRT_2 +
								_medium.data[offset + xn_y0_z0].displacement +
								_medium.data[offset + xp_y0_z0].displacement +
								_medium.data[offset + xn_yp_z0].displacement * INV_SQRT_2 +
								_medium.data[offset + x0_yp_z0].displacement +
								_medium.data[offset + xp_yp_z0].displacement * INV_SQRT_2;

							const auto neight_average = neigh_total / (4.0 + 4.0 * INV_SQRT_2);

							const auto delta_x = _medium.data[offset].displacement - neight_average; // displacement relative to the current neightbour average 

							//if ((4*std::pow(x - 200, 2.0) + std::pow(y-256.0, 2.0)) < 50.0 * 50.0)
							//	_medium.data[offset].veocity -= VEL_FACTOR2 * delta_x;
							//else 
							_medium.data[offset].veocity -= VEL_FACTOR1 * delta_x;
						}
					}
				}
				);

			constexpr float f = 0.9;

			for (int y = 0; y < TMedium::height(); ++y)
			{
				for (int i = 0; i < 4; ++i)
				{
					_medium.at(i, y, 0).veocity *= EDGE_SLOW_DOWN_FACTORS[i];
					_medium.at(TMedium::width()-1-i, y, 0).veocity *= EDGE_SLOW_DOWN_FACTORS[i];
				}
			}

			for (int x = 0; x < TMedium::width(); ++x)
			{
				for (int i = 0; i < 4; ++i)
				{
					_medium.at(x, i, 0).veocity *= EDGE_SLOW_DOWN_FACTORS[i];
					_medium.at(x, TMedium::height() - 1 - i, 0).veocity *= EDGE_SLOW_DOWN_FACTORS[i];
				}
			}


			uint64_t mid = __rdtsc();

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

			// prevent the reflections from the edges 

			//for (int x = 0; x < TMedium::width()/2; ++x)
			//{
			//	// YAY! -1
			//	_medium.at(x, -1, 0).displacement = _medium.at(x, 0, 0).displacement + _medium.at(x, 0, 0).veocity * LOC_FACTOR;
			//	//_medium.at(x, -1, 0).displacement = 2 * _medium.at(x, 0, 0).displacement - _medium.at(x, 1, 0).displacement;
			//	//_medium.at(x, TMedium::height(), 0).displacement = 2 * _medium.at(x, TMedium::height() - 1, 0).displacement - _medium.at(x, TMedium::height() - 2, 0).displacement;
			//}


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
    };
}
