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
#include "PngLogger.h"

namespace waves
{ 
    class World
    {
	public:
		using TMedium = Medium<432, 240, 240>;
		using TMediumStatic = Medium<TMedium::width(), TMedium::height(), TMedium::depth(), ItemStatic>;

		using TSrcPictureMedium = Medium<8, TMedium::height(), TMedium::depth(), float, 0, true>;
		using TPictureMedium = Medium<20, TMedium::height(), TMedium::depth(), float, 0, true>;

		static constexpr float VEL_FACTOR1 = 0.40 ; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr float VEL_FACTOR2 = 0.2; // 0.13; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr float LOC_FACTOR = 0.1 ; // dX = V * dT, this is dT

		static constexpr float EDGE_SLOW_DOWN_FACTOR = 0.98;

		static constexpr int SOURCE_X = 11;

		static constexpr int PIC_SRC_BASE = SOURCE_X-2;
		static constexpr int PIC_BASE = 220;

	private:

		ThreadGrid _grid{ 8 };

		TMediumStatic _static;
		std::array<TMedium, 2> _mediums;

		TSrcPictureMedium _src_picture;
		TPictureMedium _picture;

        Random _random{};
		uint64_t _iteration{ 0 };

		uint64_t elapsed_cpu_clocks{ 0 };

		std::string _pictures_folder;
		uint64_t _picture_exposing_until{ 0 };
		uint64_t _exposition{ 0 };

	public:
        World(int scene)
        {	
			load_scene0(_static);
		}

		~World()
		{
		}

		void start_taking_picture(const std::string& folder, uint64_t exposition)
		{
			_exposition = exposition;
			_picture.fill(0.0f);
			_pictures_folder = folder;
			_picture_exposing_until = _iteration + exposition + 1;
		}

	private: 

		static void load_scene_edges(TMediumStatic& medium)
		{
			static constexpr int THICKNESS = 10;

			const float R = std::min(medium.depth(), medium.height()) / 2 - THICKNESS;

			// Cylinder walls 
			for (int x = 0; x < TMedium::width(); ++x)
			{
				for (int y = 0; y < TMedium::height(); ++y)
				{
					for (int z = 0; z < TMedium::depth(); ++z)
					{
						int offset = TMedium::offset_for(x, y, z);

						float dz = z - medium.depth() / 2;
						float dy = y - medium.height() / 2;

						float r = std::sqrt(dz * dz + dy * dy);
						if (r >= R)
						{
							medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, static_cast<int>(r-R));
						}
					}
				}
			}			

			for (int x = 0; x < TMedium::width(); ++x)				
			{
				for (int y = 0; y < TMedium::height(); ++y)
				{
					for (int z = 0; z < TMedium::depth(); ++z)
					{
						int offset = TMedium::offset_for(x, y, z);

						if (x < THICKNESS)
						{
							medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, THICKNESS - x);
						}
						else if (x >= TMedium::width() - THICKNESS)
						{
							medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, x - (TMedium::width() - THICKNESS));
						}

						//if (y < THICKNESS)
						//{
						//	medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, THICKNESS - y);
						//}
						//else if (y >= TMedium::height() - THICKNESS)
						//{
						//	medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, y - (TMedium::height() - THICKNESS));
						//}

						//if (z < THICKNESS)
						//{
						//	medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, THICKNESS - z);
						//}
						//else if (z >= TMedium::depth() - THICKNESS)
						//{
						//	medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, z - (TMedium::depth() - THICKNESS));
						//}
					}
				}
			}
		}		

		static void load_scene0(TMediumStatic& medium)
		{
			static constexpr int LENSE_BASE_X1 = 70;
			static constexpr int LENSE_BASE_X2 = 150;
			static constexpr float LENSE_SPHERE_X = 150;
			static constexpr float LENSE_SPEHERE_Y = TMedium::height() / 2.0;
			static constexpr float LENSE_SPEHERE_Z = TMedium::depth() / 2.0;
			static constexpr float LENSE_SPHERE_RADIUS = 123.0f;
			static constexpr float LENSE_RADIUS = TMedium::height() / 2.0 - 15;

			for (int z = 0; z < TMedium::depth(); ++z)
			{
				for (int x = 0; x < TMedium::width(); ++x)
				{
					for (int y = 0; y < TMedium::height(); ++y)
					{
						int offset = TMedium::offset_for(x, y, z);

						float yz_r = std::sqrt(std::pow(y - LENSE_SPEHERE_Y, 2.0f) + std::pow(z - LENSE_SPEHERE_Z, 2.0f));

						bool inside_sphere = 
							(std::pow(x - LENSE_SPHERE_X, 2.0f) + std::pow(y - LENSE_SPEHERE_Y, 2.0f) + std::pow(z - LENSE_SPEHERE_Z, 2.0f)) 
								< std::pow(LENSE_SPHERE_RADIUS, 2.0);

						if (x < LENSE_BASE_X2 && inside_sphere || (x >= LENSE_BASE_X2))
							medium.data[offset].velocity_bit = 1;// VEL_FACTOR2;
						else
							medium.data[offset].velocity_bit = 0; // VEL_FACTOR1;

						medium.data[offset].resistance = 127;

						if (yz_r > LENSE_RADIUS)
						{
							if (x > LENSE_BASE_X1/* && x < LENSE_BASE_X2*/)
							{
								if (x < LENSE_BASE_X1 + 10)
								{
									int i = std::abs(x - LENSE_BASE_X1) + 2;
									medium.data[offset].resistance = 127.0 * std::pow(EDGE_SLOW_DOWN_FACTOR, i);
								}
								//if (x > LENSE_BASE_X2 - 10)
								//{
								//	int i = std::abs(x - LENSE_BASE_X2) + 2;
								//	medium.data[offset].resistance = 127.0 * std::pow(EDGE_SLOW_DOWN_FACTOR, i);
								//}
								if (yz_r < LENSE_RADIUS + 10)
								{
									int i = std::abs(yz_r - LENSE_RADIUS) + 2;
									medium.data[offset].resistance *= std::pow(EDGE_SLOW_DOWN_FACTOR, i);
								}

								if ((x >= LENSE_BASE_X1 + 10) /*&& (x <= LENSE_BASE_X2 - 10)*/ && (yz_r >= LENSE_RADIUS + 10))
								{
									medium.data[offset].resistance = 0;
								}
							}
						}
					}
				}
			}

			load_scene_edges(medium);
		}

	public:

#pragma warning(push)
#pragma warning(disable:26451)
		bool iterate()  noexcept
		{
			auto& current = _mediums[_iteration % 2];
			auto& next = _mediums[(_iteration + 1) % 2];

			int fill_value = ((_iteration % 70) > 35) ? 500 : -500;
			fill(current, 11, fill_value);

			uint64_t start = __rdtsc();

			constexpr int xd_neighbour = TMedium::offset_for(-1, 0, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int xu_neighbour = TMedium::offset_for(1, 0, 0) - TMedium::offset_for(0, 0, 0);

			constexpr int yd_neighbour = TMedium::offset_for(0, -1, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int yu_neighbour = TMedium::offset_for(0, 1, 0) - TMedium::offset_for(0, 0, 0);

			constexpr int zd_neighbour = TMedium::offset_for(0, 0, -1) - TMedium::offset_for(0, 0, 0);
			constexpr int zu_neighbour = TMedium::offset_for(0, 0, 1) - TMedium::offset_for(0, 0, 0);

			_grid.GridRun(
				[&](int thread_idx, int num_threads)
			//concurrency::parallel_for(
				//0, static_cast<int>(TMedium::depth() / 32),
				//[&](int Z_big)
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

								auto new_velocity = (current.data[offset].velocity - velolicty_factor * delta_x) * resistance_factor * 0.99999;

								next.data[offset].location = current.data[offset].location + new_velocity * LOC_FACTOR;
								next.data[offset].velocity = new_velocity;
							}
						}
					}
				}
				);

			uint64_t end = __rdtsc();

			if (_picture_exposing_until != 0)
			{

				for (int x = 0; x < TSrcPictureMedium::width(); ++x)
				{
					for (int y = 0; y < TSrcPictureMedium::height(); ++y)
					{
						for (int z = 0; z < TSrcPictureMedium::depth(); ++z)
						{
							_src_picture.at(x, y, z) += std::abs(current.at(x + PIC_SRC_BASE, y, z).location);
						}
					}
				}

				for (int x = 0; x < TPictureMedium::width(); ++x)
				{
					for (int y = 0; y < TPictureMedium::height(); ++y)
					{
						for (int z = 0; z < TPictureMedium::depth(); ++z)
						{
							_picture.at(x, y, z) += std::abs(current.at(x + PIC_BASE, y, z).location);
						}
					}
				}

				if (_picture_exposing_until == _iteration)
				{
					_picture_exposing_until = 0;
					save_pictures(_picture, _pictures_folder, PIC_BASE);
					save_pictures(_src_picture, _pictures_folder, PIC_SRC_BASE);
				}
			}

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
			//if (idx >= 0 && idx < light_enabled.size())
			//{
			//	light_enabled[idx] = !light_enabled[idx];
			//}
		}

	private: 
		static void fill(TMedium& medium, int x_plane, int value)
		{
			const int32_t R = std::min(medium.depth(), medium.height())/2 - 5;
			const int32_t RSqr = R * R;

			for (int z = 0; z < medium.depth(); ++ z)
			{
				for (int y = 0; y < medium.height(); ++ y)
				{
					auto dz = z - medium.depth() / 2;
					auto dy = y - medium.height() / 2;

					auto& item = medium.at(x_plane, y, z);

					if ((std::abs(dz) < 10) || (std::abs(dy) < 10))
					{
						item.location = 0;
					}
					else
					{
						//auto dSqr = dz * dz + dy * dy;
						//if (dSqr <= RSqr)
						//{
							item.location = value;
						//}
						//else
						//{
						//	item.location = value / (dSqr - RSqr);
						//}
					}

					item.velocity = 0.0f;
				}
			}
		}

		template <typename TPicture>
		void save_pictures(TPicture& pic, const std::string& folder, int idx_offset)
		{
			auto logger = std::make_unique<PngLogger>(folder);

			logger->onViewportResize(pic.height(), pic.depth());

			for (int x = 0; x < pic.width(); ++x)
			{
				auto& data = logger->data();				

				for (int y = 0; y < pic.height(); ++y)
				{
					for (int z = 0; z < pic.depth(); ++z)
					{
						int32_t value = pic.at(x, y, z) / _exposition;
						int32_t brightness = std::max(0, std::min(255, value));

						uint32_t offs = 4 * (y * pic.depth() + z);

						data[offs] = brightness;
						data[offs + 1] = brightness;
						data[offs + 2] = brightness;
						data[offs + 3] = 255;
					}
				}
				
				logger->recordOrthogonalFrame(x + idx_offset);
			}
		}
    };
}
