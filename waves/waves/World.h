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
		using TMedium = Medium<512 + 256, 512, 1>;

		static constexpr float VEL_FACTOR1 = 0.40; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr float VEL_FACTOR2 = 0.13; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr float LOC_FACTOR = 0.1; // dX = V * dT, this is dT

		static constexpr float EDGE_SLOW_DOWN_FACTOR = 0.98;

	private:
		TMedium _medium;

        Random _random{};
		uint64_t _iteration{ 0 };

		uint64_t first_half_clocks{ 0 };
		uint64_t second_half_clocks{ 0 };

		std::array<std::atomic_bool, 3> light_enabled { true, true, true };

		int _scene{ 0 };

	public:
        World(int scene)
			: _scene { scene }
        {	
			if (scene == 1)
			{
				load_scene1();
			}
			else 
			{
				load_scene0();
			}
		}

		~World()
		{
		}

	private: 

		void load_scene_edges(int thickness)
		{
			for (int x = 0; x < TMedium::width(); ++x)
			{
				for (int y = 0; y < TMedium::height(); ++y)
				{
					int offset = TMedium::offset_for(x, y, 0);

					if (x < thickness)
					{
						_medium.data[offset].resistance_factor *= std::pow(EDGE_SLOW_DOWN_FACTOR, thickness - x);
					}
					else if (x >= TMedium::width() - thickness)
					{
						_medium.data[offset].resistance_factor *= std::pow(EDGE_SLOW_DOWN_FACTOR, x - (TMedium::width() - thickness));
					}
					if (y < thickness)
					{
						_medium.data[offset].resistance_factor *= std::pow(EDGE_SLOW_DOWN_FACTOR, thickness - y);
					}
					else if (y >= TMedium::height() - thickness)
					{
						_medium.data[offset].resistance_factor *= std::pow(EDGE_SLOW_DOWN_FACTOR, y - (TMedium::height() - thickness));
					}
				}
			}
		}

		void load_scene1()
		{
			for (int x = 0; x < TMedium::width(); ++x)
			{
				for (int y = 0; y < TMedium::height(); ++y)
				{
					int offset = TMedium::offset_for(x, y, 0);

					auto dist_centre = std::sqrt(std::pow(x - 256.0f, 2.0f) + std::pow(y - 256.0f, 2.0f));

					if (dist_centre < 122.0)
					{
						_medium.data[offset].velocity_factor = VEL_FACTOR2;
					}
					else
						_medium.data[offset].velocity_factor = VEL_FACTOR1;

					_medium.data[offset].resistance_factor = 1.0;					
				}
			}

			load_scene_edges(10);
		}

		void load_scene0()
		{
			for (int x = 0; x < TMedium::width(); ++x)
			{
				for (int y = 0; y < TMedium::height(); ++y)
				{
					int offset = TMedium::offset_for(x, y, 0);

					if (x < 350.0f && (std::pow(x - 350.0f, 2.0f) + std::pow(y - 256.0f, 2.0f)) < 15000.0f)
						_medium.data[offset].velocity_factor = VEL_FACTOR2;
					else
						_medium.data[offset].velocity_factor = VEL_FACTOR1;

					_medium.data[offset].resistance_factor = 1.0;
					
					if (y > 256 + 90 || y < 256 - 90)
					{
						if (x > 270 && x < 350)
						{
							int d = std::abs(x - 310);
							_medium.data[offset].resistance_factor = std::pow(EDGE_SLOW_DOWN_FACTOR, 40 - d);
						}
					}
				}
			}

			load_scene_edges(6);
		}

	public:

#pragma warning(push)
#pragma warning(disable:26451)
		bool iterate()  noexcept
		{
			if (_scene == 1)
			{
				int fill_value = ((_iteration % 70) > 35) ? 3000 : -3000;
				//if (light_enabled[0])
				//	fill(54, 154, 5, 5, fill_value);
				//if (light_enabled[1])
				//	fill(38, 254, 5, 5, fill_value);
				//if (light_enabled[2])
				//	fill(54, 354, 5, 5, fill_value);
				if (light_enabled[0])
					fill(75, 154, 5, 5, fill_value);
				if (light_enabled[1])
					fill(58, 254, 5, 5, fill_value);
				if (light_enabled[2])
					fill(75, 354, 5, 5, fill_value);
			}
			else
			{
				int fill_value = ((_iteration % 70) > 35) ? 3000 : -3000;
				if (light_enabled[0])
					fill(38, 154, 5, 5, fill_value);
				if (light_enabled[1])
					fill(38, 254, 5, 5, fill_value);
				if (light_enabled[2])
					fill(38, 354, 5, 5, fill_value);

				//if (light_enabled[0])
				//	fill(38, 154, 4, 359- 154, fill_value/10);
			}

			uint64_t start = __rdtsc();

			//constexpr int xn_yn_z0 = TMedium::offset_for(-1, -1, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int x0_yn_z0 = TMedium::offset_for(0, -1, 0) - TMedium::offset_for(0, 0, 0);
			//constexpr int xp_yn_z0 = TMedium::offset_for(1, -1, 0) - TMedium::offset_for(0, 0, 0);

			constexpr int xn_y0_z0 = TMedium::offset_for(-1, 0, 0) - TMedium::offset_for(0, 0, 0);
			//constexpr int x0_y0_z0 = TMedium::offset_for(0, 0, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int xp_y0_z0 = TMedium::offset_for(1, 0, 0) - TMedium::offset_for(0, 0, 0);

			//constexpr int xn_yp_z0 = TMedium::offset_for(-1, 1, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int x0_yp_z0 = TMedium::offset_for(0, 1, 0) - TMedium::offset_for(0, 0, 0);
			//constexpr int xp_yp_z0 = TMedium::offset_for(1, 1, 0) - TMedium::offset_for(0, 0, 0);

			concurrency::parallel_for(
				0, static_cast<int>(TMedium::height() / 8),
				[&](int Y_big)
				{
					for (int sub_y = 0; sub_y < 8; ++sub_y)
					{
						const int y = Y_big * 8 + sub_y;

						for (int x = 0; x < TMedium::width(); ++x)
						{
							int offset = TMedium::offset_for(x, y, 0);

							const float neigh_total =
								_medium.data[offset + x0_yn_z0].displacement +
								_medium.data[offset + xn_y0_z0].displacement +
								_medium.data[offset + xp_y0_z0].displacement +
								_medium.data[offset + x0_yp_z0].displacement ;

							const float neight_average = neigh_total / 4.0f;

							const float delta_x = _medium.data[offset].displacement - neight_average; // displacement relative to the current neightbour average 

							_medium.data[offset].veocity -= _medium.data[offset].velocity_factor * delta_x;
						}
					}
				}
				);

			uint64_t mid = __rdtsc();

			// Update the locations 
			concurrency::parallel_for(
				0, static_cast<int>(TMedium::height() / 16),
				[&](int Y_big)
				{
					for (int sub_y = 0; sub_y < 16; ++sub_y)
					{
						const int y = Y_big * 16 + sub_y;

						for (int x = 0; x < TMedium::width(); ++x)
						{
							int offset = TMedium::offset_for(x, y, 0);
							_medium.data[offset].displacement += _medium.data[offset].veocity * LOC_FACTOR;
						
							_medium.data[offset].veocity *= _medium.data[offset].resistance_factor;
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
			for (int j = y; j < y + h; ++j)
			{
				for (int i = x; i < x + w; ++i)
				{
					auto& item = _medium.at(i, j, 0);
					item.displacement = value;
					item.veocity = 0.0f;
				}
			}
		}
    };
}
