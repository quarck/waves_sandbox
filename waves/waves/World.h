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
		static constexpr double VEL_FACTOR2 = 0.10; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr double LOC_FACTOR = 0.1; // dX = V * dT, this is dT

	private:
		TMedium _location;
		TMedium _velocity;

        Random _random{};
		int64_t _iteration{ 0 };

	public:
        World()
        {	
			for (int i = 40-5; i < 40+5; ++i)
				for (int j = 256-5; j < 256+5; ++j)
				{
					_location.at(i, j, 0) = 5000 * 4;
				}
		}

		~World()
		{
		}

	public:
		bool iterate()  noexcept
		{
			constexpr int xn_yn_z0 = TMedium::offset_for(-1, -1, 0);
			constexpr int x0_yn_z0 = TMedium::offset_for(0, -1, 0);
			constexpr int xp_yn_z0 = TMedium::offset_for(1, -1, 0);

			constexpr int xn_y0_z0 = TMedium::offset_for(-1, 0, 0);
			//constexpr int x0_y0_z0 = TMedium::offset_for(0, 0, 0);
			constexpr int xp_y0_z0 = TMedium::offset_for(1, 0, 0);

			constexpr int xn_yp_z0 = TMedium::offset_for(-1, 1, 0);
			constexpr int x0_yp_z0 = TMedium::offset_for(0, 1, 0);
			constexpr int xp_yp_z0 = TMedium::offset_for(1, 1, 0);


			concurrency::parallel_for(
				1, static_cast<int>(TMedium::width() - 1),
				[&](int x)				
				{
					int offset = TMedium::offset_for(x, 1, 0);
					constexpr int y_delta = TMedium::offset_for(0, 1, 0);

					for (int y = 1; y < TMedium::height() - 1; ++y)
					{
						//for (int z = 1; z < current.depth()-1; ++z)
						//{

					//	int offset = TMedium::offset_for(x, y, 0);

						const auto neigh_total =
							_location.data[offset + xn_yn_z0] * 0.7 + _location.data[offset + x0_yn_z0] + _location.data[offset + xp_yn_z0] * 0.7 +
							_location.data[offset + xn_y0_z0] + _location.data[offset + xp_y0_z0] +
							_location.data[offset + xn_yp_z0] * 0.7 + _location.data[offset + x0_yp_z0] + _location.data[offset + xp_yp_z0] * 0.7;

						const auto neight_average = neigh_total / (4.0 + 4.0 * 0.7);

						const auto delta_x = _location.data[offset] - neight_average; // displacement relative to the current neightbour average 

						if ((std::pow(x - 200, 2.0) + std::pow(y-256.0, 2.0)) < 50.0 * 50.0)
							_velocity.data[offset] -= VEL_FACTOR2 * delta_x;
						else 
							_velocity.data[offset] -= VEL_FACTOR1 * delta_x;

						offset += y_delta;
					}
				}
				);

			for (int x = 1; x < TMedium::width() - 1; ++x)
			{
				int offset = TMedium::offset_for(x, 1, 0);
				constexpr int y_delta = TMedium::offset_for(0, 1, 0);

				for (int y = 1; y < TMedium::height() - 1; ++y)
				{
					//for (int z = 1; z < current.depth()-1; ++z)
					//{
//					int offset = TMedium::offset_for(x, y, 0);
					_location.data[offset] += _velocity.data[offset] * LOC_FACTOR;

					offset += y_delta;
				}
			}

			// prevent the reflections from the edges 

			for (int x = 0; x < TMedium::width(); ++x)
			{
				_location.at(x, 0, 0) = _location.at(x, 1, 0);
				_location.at(x, TMedium::height() - 1, 0) = _location.at(x, TMedium::height() - 2, 0);
			}

			for (int y = 0; y < TMedium::height(); ++y)
			{
				_location.at(0, y, 0) = _location.at(1, y, 0);
				_location.at(TMedium::width() - 1, y, 0) = _location.at(TMedium::width() - 2, y, 0);
			}

			_iteration++;
			return true;
        }

		int64_t current_iteration() const noexcept
		{
			return _iteration;
		}

		const TMedium& get_data() const { return _location; }
    };
}
