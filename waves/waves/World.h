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
		using TMediumStatic = Medium<512 + 256, 512, 1, ItemStatic>;

		static constexpr float VEL_FACTOR1 = 0.40; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr float VEL_FACTOR2 = 0.13; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr float LOC_FACTOR = 0.1; // dX = V * dT, this is dT

		static constexpr float EDGE_SLOW_DOWN_FACTOR = 0.98;

	private:
		TMediumStatic _static;
		std::array<TMedium, 2> _mediums;

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
				load_scene1(_static);
			}
			else 
			{
				load_scene0(_static);
			}
		}

		~World()
		{
		}

	private: 

		static void load_scene_edges(TMediumStatic& medium, int thickness)
		{
			for (int x = 0; x < TMedium::width(); ++x)
			{
				for (int y = 0; y < TMedium::height(); ++y)
				{
					int offset = TMedium::offset_for(x, y, 0);

					if (x < thickness)
					{
						medium.data[offset].resistance_factor *= std::pow(EDGE_SLOW_DOWN_FACTOR, thickness - x);
					}
					else if (x >= TMedium::width() - thickness)
					{
						medium.data[offset].resistance_factor *= std::pow(EDGE_SLOW_DOWN_FACTOR, x - (TMedium::width() - thickness));
					}
					if (y < thickness)
					{
						medium.data[offset].resistance_factor *= std::pow(EDGE_SLOW_DOWN_FACTOR, thickness - y);
					}
					else if (y >= TMedium::height() - thickness)
					{
						medium.data[offset].resistance_factor *= std::pow(EDGE_SLOW_DOWN_FACTOR, y - (TMedium::height() - thickness));
					}
				}
			}
		}

		static void load_scene1(TMediumStatic& medium)
		{
			for (int x = 0; x < TMedium::width(); ++x)
			{
				for (int y = 0; y < TMedium::height(); ++y)
				{
					int offset = TMedium::offset_for(x, y, 0);

					auto dist_centre = std::sqrt(std::pow(x - 256.0f, 2.0f) + std::pow(y - 256.0f, 2.0f));

					if (dist_centre < 122.0)
					{
						medium.data[offset].velocity_factor = VEL_FACTOR2;
					}
					else
						medium.data[offset].velocity_factor = VEL_FACTOR1;

					medium.data[offset].resistance_factor = 1.0;
				}
			}

			load_scene_edges(medium, 10);
		}

		static void load_scene0(TMediumStatic& medium)
		{
			for (int x = 0; x < TMedium::width(); ++x)
			{
				for (int y = 0; y < TMedium::height(); ++y)
				{
					int offset = TMedium::offset_for(x, y, 0);

					if (x < 350.0f && (std::pow(x - 350.0f, 2.0f) + std::pow(y - 256.0f, 2.0f)) < 15000.0f)
						medium.data[offset].velocity_factor = VEL_FACTOR2;
					else
						medium.data[offset].velocity_factor = VEL_FACTOR1;

					medium.data[offset].resistance_factor = 1.0;
					
					if (y > 256 + 90 || y < 256 - 90)
					{
						if (x > 270 && x < 350)
						{
							int d = std::abs(x - 310);
							medium.data[offset].resistance_factor = std::pow(EDGE_SLOW_DOWN_FACTOR, 40 - d);
						}
					}
				}
			}

			load_scene_edges(medium, 10);
		}

	public:

#pragma warning(push)
#pragma warning(disable:26451)
		bool iterate()  noexcept
		{
			auto& current = _mediums[_iteration % 2];
			auto& next = _mediums[(_iteration + 1) % 2];

			if (_scene == 1)
			{
				int fill_value = ((_iteration % 70) > 35) ? 3000 : -3000;
				if (light_enabled[0])
					fill(current, 75, 154, 5, 5, fill_value);
				if (light_enabled[1])
					fill(current, 58, 254, 5, 5, fill_value);
				if (light_enabled[2])
					fill(current, 75, 354, 5, 5, fill_value);
			}
			else
			{
				int fill_value = ((_iteration % 70) > 35) ? 3000 : -3000;
				if (light_enabled[0])
					fill(current, 38, 154, 5, 5, fill_value);
				if (light_enabled[1])
					fill(current, 38, 254, 5, 5, fill_value);
				if (light_enabled[2])
					fill(current, 38, 354, 5, 5, fill_value);
			}

			uint64_t start = __rdtsc();

			constexpr int top_neighbour = TMedium::offset_for(0, -1, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int left_neighbour = TMedium::offset_for(-1, 0, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int right_neighbour = TMedium::offset_for(1, 0, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int bottom_neighbour = TMedium::offset_for(0, 1, 0) - TMedium::offset_for(0, 0, 0);

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
								current.data[offset + top_neighbour].location +
								current.data[offset + left_neighbour].location +
								current.data[offset + right_neighbour].location +
								current.data[offset + bottom_neighbour].location ;

							const float neight_average = neigh_total * (1.0f / 4.0f);

							const float delta_x = current.data[offset].location - neight_average; // location relative to the current neightbour average 

							auto new_vel = current.data[offset].veocity - _static.data[offset].velocity_factor * delta_x;

							next.data[offset].location = current.data[offset].location + new_vel * LOC_FACTOR;
							next.data[offset].veocity = new_vel * _static.data[offset].resistance_factor;
						}
					}
				}
				);

			uint64_t end = __rdtsc();

			first_half_clocks += end- start;
			//second_half_clocks += 0;

			_iteration++;
			return true;
        }
#pragma warning(pop)

		uint64_t current_iteration() const noexcept
		{
			return _iteration;
		}

		const TMedium& get_data() const { return _mediums[_iteration % 2]; }


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
		static void fill(TMedium& medium, int x, int y, int w, int h, int value)
		{
			for (int j = y; j < y + h; ++j)
			{
				for (int i = x; i < x + w; ++i)
				{
					auto& item = medium.at(i, j, 0);
					item.location = value;
					item.veocity = 0.0f;
				}
			}
		}
    };
}
