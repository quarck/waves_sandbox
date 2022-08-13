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
		using TMedium = Medium<512 + 256, 512, 512>;
		using TMediumStatic = Medium<512 + 256, 512, 512, ItemStatic>;

		static constexpr float VEL_FACTOR1 = 0.40 ; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr float VEL_FACTOR2 = 0.13 ; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr float LOC_FACTOR = 0.1 ; // dX = V * dT, this is dT

		static constexpr float EDGE_SLOW_DOWN_FACTOR = 0.98;

	private:

		ThreadGrid _grid{ 6 };

		TMediumStatic _static;
		std::array<TMedium, 2> _mediums;

        Random _random{};
		uint64_t _iteration{ 0 };

		uint64_t elapsed_cpu_clocks{ 0 };

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
			for (int z = 0; z < TMedium::depth(); ++z)
			{
				for (int x = 0; x < TMedium::width(); ++x)
				{
					for (int y = 0; y < TMedium::height(); ++y)
					{
						int offset = TMedium::offset_for(x, y, z);

						if (x < thickness)
						{
							medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, thickness - x);
						}
						else if (x >= TMedium::width() - thickness)
						{
							medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, x - (TMedium::width() - thickness));
						}
						if (y < thickness)
						{
							medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, thickness - y);
						}
						else if (y >= TMedium::height() - thickness)
						{
							medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, y - (TMedium::height() - thickness));
						}

						if (z < thickness)
						{
							medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, thickness - z);
						}
						else if (z >= TMedium::depth() - thickness)
						{
							medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, z - (TMedium::depth() - thickness));
						}
					}
				}
			}
		}

		static void load_scene1(TMediumStatic& medium)
		{
			for (int z = 0; z < TMedium::depth(); ++z)
			{
				for (int x = 0; x < TMedium::width(); ++x)
				{
					for (int y = 0; y < TMedium::height(); ++y)
					{
						int offset = TMedium::offset_for(x, y, z);

						auto dist_centre = std::sqrt(
							std::pow(x - 256.0f, 2.0f) + 
							std::pow(y - 256.0f, 2.0f) + 
							std::pow(z - 256.0f, 2.0f) 
						);

						if (dist_centre < 122.0)
						{
							medium.data[offset].velocity_bit = 1; // = VEL_FACTOR2;
						}
						else
							medium.data[offset].velocity_bit = 0;// = VEL_FACTOR1;

						medium.data[offset].resistance = 127;
					}
				}
			}

			load_scene_edges(medium, 10);
		}

		static void load_scene0(TMediumStatic& medium)
		{
			for (int z = 0; z < TMedium::depth(); ++z)
			{
				for (int x = 0; x < TMedium::width(); ++x)
				{
					for (int y = 0; y < TMedium::height(); ++y)
					{
						int offset = TMedium::offset_for(x, y, z);

						if (x < 350.0f 
							&& (
								std::pow(x - 350.0f, 2.0f) + 
								std::pow(y - 256.0f, 2.0f) + 
								std::pow(z - 256.0f, 2.0f)) < 
									std::pow(123.0f, 2.0)
							)
							medium.data[offset].velocity_bit = 1;// VEL_FACTOR2;
						else
							medium.data[offset].velocity_bit = 0; // VEL_FACTOR1;

						medium.data[offset].resistance = 127;

						float yz_r = std::sqrt(std::pow(y-256.0f, 2.0f) + std::pow(z - 256.0f, 2.0f));
						if (yz_r > 90)
						{
							if (x > 270 && x < 350)
							{
								int d = std::abs(x - 310);
								medium.data[offset].resistance = 127.0 * std::pow(EDGE_SLOW_DOWN_FACTOR, 40 - d);
							}
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
					fill(current, 75, 154, TMedium::depth() / 2 - 2, 5, 5, 5, fill_value);
				if (light_enabled[1])
					fill(current, 58, 254, TMedium::depth() / 2 - 2, 5, 5, 5, fill_value);
				if (light_enabled[2])
					fill(current, 75, 354, TMedium::depth() / 2 - 2, 5, 5, 5, fill_value);
			}
			else
			{
				int fill_value = ((_iteration % 70) > 35) ? 3000 : -3000;
				if (light_enabled[0])
					fill(current, 38, 154, TMedium::depth() / 2 - 2, 5, 5, 5, fill_value);
				if (light_enabled[1])
					fill(current, 38, 254, TMedium::depth() / 2 - 2, 5, 5, 5, fill_value);
				if (light_enabled[2])
					fill(current, 38, 354, TMedium::depth() / 2 - 2, 5, 5, 5, fill_value);
			}

			uint64_t start = __rdtsc();

			constexpr int xd_neighbour = TMedium::offset_for(-1, 0, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int xu_neighbour = TMedium::offset_for(1, 0, 0) - TMedium::offset_for(0, 0, 0);

			constexpr int yd_neighbour = TMedium::offset_for(0, -1, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int yu_neighbour = TMedium::offset_for(0, 1, 0) - TMedium::offset_for(0, 0, 0);

			constexpr int zd_neighbour = TMedium::offset_for(0, 0, -1) - TMedium::offset_for(0, 0, 0);
			constexpr int zu_neighbour = TMedium::offset_for(0, 0, 1) - TMedium::offset_for(0, 0, 0);

			_grid.GridRun(
			//concurrency::parallel_for(
				//0, static_cast<int>(TMedium::depth() / 32),
				//[&](int Z_big)
				[&](int thread_idx, int num_threads)
				{
					int slice = TMedium::depth() / num_threads;
					int from = thread_idx * slice;
					int to = (thread_idx + 1) * slice;

					for (int z = from; z < to; ++z)
					{
						//const int z = Z_big * 32 + sub_z;

						for (int y = 0; y < TMedium::height(); ++ y)
						{
							for (int x = 0; x < TMedium::width(); ++x)
							{
								int offset = TMedium::offset_for(x, y, z);

								const float neigh_total =
									current.data[offset + xd_neighbour].location +
									current.data[offset + xu_neighbour].location +
									current.data[offset + yd_neighbour].location +
									current.data[offset + yu_neighbour].location +
									current.data[offset + zd_neighbour].location +
									current.data[offset + zu_neighbour].location;

								const float neight_average = neigh_total * (1.0f / 6.0f);

								const float delta_x = current.data[offset].location - neight_average; // location relative to the current neightbour average 

								auto item_static = _static.data[offset];

								const float velolicty_factor = item_static.velocity_bit ? VEL_FACTOR2 : VEL_FACTOR1;
								const float resistance_factor = static_cast<float>(item_static.resistance) / 127.0f;

								auto new_velocity = (current.data[offset].velocity - velolicty_factor * delta_x) * resistance_factor;

								next.data[offset].location = current.data[offset].location + new_velocity * LOC_FACTOR;
								next.data[offset].velocity = new_velocity;
							}
						}
					}
				}
				);

			uint64_t end = __rdtsc();

			elapsed_cpu_clocks += end- start;

			_iteration++;
			return true;
        }
#pragma warning(pop)

		uint64_t current_iteration() const noexcept
		{
			return _iteration;
		}

		const TMedium& get_data() const { return _mediums[_iteration % 2]; }


		const std::tuple<uint64_t, uint64_t> get_clocks_per_iter()
		{
			if (_iteration == 0)
				return { 0, 0 };

			uint64_t clocks_per_iter{ elapsed_cpu_clocks / _iteration };
			uint64_t clocks_per_iter_per_voxel{ clocks_per_iter / (TMedium::depth() * TMedium::width() * TMedium::height()) };

			return { clocks_per_iter, clocks_per_iter_per_voxel };
		}

		void toggle_light(int idx)
		{
			if (idx >= 0 && idx < light_enabled.size())
			{
				light_enabled[idx] = !light_enabled[idx];
			}
		}

	private: 
		static void fill(TMedium& medium, int x, int y, int z, int w, int h, int d, int value)
		{
			for (int k = z; k < z + d; ++k)
			{
				for (int j = y; j < y + h; ++j)
				{
					for (int i = x; i < x + w; ++i)
					{
						auto& item = medium.at(i, j, k);
						item.location = value;
						item.velocity = 0.0f;
					}
				}
			}
		}
    };
}
