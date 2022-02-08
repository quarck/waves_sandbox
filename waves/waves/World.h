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

		static constexpr double FACTOR = 0.10;

	private:
		std::array<TMedium, 2> _medium;

        Random _random{};
		int64_t _iteration{ 0 };

	public:
        World()
        {	
			for (int i = 0; i < 100; ++i)
				for (int j = 0; j < 100; ++j){
					_medium[0].at(i, j, 0) = 1000;
					_medium[1].at(i, j, 0) = 1000;
				}
		}

		~World()
		{
		}

	public:
		bool iterate()  noexcept
		{
			auto& current = _medium[_iteration % 2];
			auto& next = _medium[(_iteration + 1) % 2];

			for (int x = 1; x < current.width()-1; ++x)
			{
				for (int y = 1; y < current.height()-1; ++y)
				{
					//for (int z = 1; z < current.depth()-1; ++z)
					//{

					const auto neigh_total =
						current.at(x - 1, y - 1, 0) * 0.7 + current.at(x, y - 1, 0) + current.at(x + 1, y - 1, 0) * 0.7 +
						current.at(x - 1, y, 0) + /* 0 */ +current.at(x + 1, y, 0) +
						current.at(x - 1, y + 1, 0) * 0.7 + current.at(x, y + 1, 0) + current.at(x + 1, y + 1, 0) * 0.7;

					const auto neight_average = neigh_total / (4.0 + 4.0*0.7);

					next.at(x, y, 0) = current.at(x, y, 0) * (1-FACTOR) + neight_average * FACTOR;
				}
			}

			_iteration++;
			return true;
        }

		int64_t current_iteration() const noexcept
		{
			return _iteration;
		}

		const TMedium& get_data() const { return _medium[_iteration % 2]; }
    };
}
