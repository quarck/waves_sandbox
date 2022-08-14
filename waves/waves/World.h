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
#include "lodepng.h"

namespace waves
{ 
    class World
    {
	public:
		using TMedium = Medium<432, 240, 240>;
		using TMediumStatic = Medium<TMedium::width(), TMedium::height(), TMedium::depth(), ItemStatic>;

		using TMediumPatternStatic = Medium<1, TMedium::height(), TMedium::depth(), float, 0, true>;

		// a temp object used to calculate resistance/conductivity with higher precision
		using TMediumCondStatic = Medium<TMedium::width(), TMedium::height(), TMedium::depth(), float>;

		using TSrcPictureMedium = Medium<1, TMedium::height(), TMedium::depth(), float, 0, true>;
		using TPictureMedium = Medium<40, TMedium::height(), TMedium::depth(), float, 0, true>;

		static constexpr float VEL_FACTOR1 = 0.40 ; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr float VEL_FACTOR2 = 0.2; // 0.13; // dV = -k*x/m * dT, this is k*dT/m
		static constexpr float LOC_FACTOR = 0.1 ; // dX = V * dT, this is dT

		static constexpr float EDGE_SLOW_DOWN_FACTOR = 0.98;

		static constexpr int SOURCE_X = 11;

		static constexpr int PIC_SRC_BASE = SOURCE_X;
		static constexpr int PIC_BASE = 210;

		static constexpr float FILL_VALUE = 500.0f;

	private:

		bool _initialized{ false };

		TMediumPatternStatic _pattern{};

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
        World()
        {	
			load_scene(_static);
		}

		~World()
		{
		}

		bool initialized() const noexcept
		{
			return _initialized;
		}

		void initialize(const std::string& pattern_file_name)
		{
			const int32_t R = std::min(_pattern.depth(), _pattern.height()) / 2 - 5;
			const int32_t RSqr = R * R;

			for (int z = 0; z < _pattern.depth(); ++z)
			{
				for (int y = 0; y < _pattern.height(); ++y)
				{
					const float dz = z - _pattern.depth() / 2.0f;
					const float dy = y - _pattern.height() / 2.0f;

					auto dSqr = dz * dz + dy * dy;
					if (dSqr <= RSqr)
					{
						_pattern.at(0, y, z) = FILL_VALUE;
					}
					else
					{
						_pattern.at(0, y, z) = FILL_VALUE / (dSqr - RSqr);
					}
				}
			}

			_initialized = true; // one way or another, proceed

			if (pattern_file_name != "")
			{
				std::vector<unsigned char> data;
				unsigned width;
				unsigned height;
				if (lodepng::decode(data, width, height, pattern_file_name) == 0)
				{
					if (width != _pattern.depth() || height != _pattern.height())
					{
						::MessageBox(NULL, L"Pattern size is incorrect! Expected 240x240 png", L"Re-think what you are doing! :)", MB_OK);
						return;
					}

					for (int y = 0; y < _pattern.height(); ++y)					
					{
						for (int z = 0; z < _pattern.depth(); ++z)
						{
							auto img_offs = 4 * (y * width + z);
							bool blocking = data[img_offs] < 127 && data[img_offs+1] < 127 && data[img_offs+2] < 127;
							_pattern.at(0, _pattern.height()-y-1, z) *= blocking ? 0.0f : 1.0f;
						}
					}
				}
			}
		}

		void start_taking_picture(const std::string& folder, uint64_t exposition)
		{
			_exposition = exposition;
			_picture.fill(0.0f);
			_pictures_folder = folder;
			_picture_exposing_until = _iteration + exposition + 1;
		}

	private: 

		static void load_scene_edges(TMediumCondStatic& medium)
		{
			static constexpr int THICKNESS = 10;

			const float R = static_cast<float>(std::min(medium.depth(), medium.height()) / 2 - THICKNESS);

			// Cylinder walls 
			for (int x = 0; x < TMedium::width(); ++x)
			{
				for (int y = 0; y < TMedium::height(); ++y)
				{
					for (int z = 0; z < TMedium::depth(); ++z)
					{
						const int offset = TMedium::offset_for(x, y, z);

						const float dz = static_cast<float>(z - medium.depth() / 2);
						const float dy = static_cast<float>(y - medium.height() / 2);

						const float r = std::sqrt(dz * dz + dy * dy);
						if (r >= R)
						{
							if (r - R < THICKNESS)
								medium.data[offset] *= std::powf(EDGE_SLOW_DOWN_FACTOR, r - R);
							else
								medium.data[offset] = 0;
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
						const int offset = TMedium::offset_for(x, y, z);

						if (x < THICKNESS)
						{
							medium.data[offset] *= std::powf(EDGE_SLOW_DOWN_FACTOR, static_cast<float>(THICKNESS - x));
						}
						else if (x >= TMedium::width() - THICKNESS)
						{
							medium.data[offset] *= std::powf(EDGE_SLOW_DOWN_FACTOR, static_cast<float>(x - (TMedium::width() - THICKNESS)));
						}
					}
				}
			}
		}		

		static void load_scene(TMediumStatic& medium)
		{
			static constexpr int LENSE_BASE_X1 = 70;
			static constexpr int LENSE_BASE_X2 = 150;
			static constexpr float LENSE_SPHERE_X = 150;
			static constexpr float LENSE_SPEHERE_Y = TMedium::height() / 2.0f;
			static constexpr float LENSE_SPEHERE_Z = TMedium::depth() / 2.0f;
			static constexpr float LENSE_SPHERE_RADIUS = 123.0f;
			static constexpr float LENSE_RADIUS = TMedium::height() / 2.0f - 15.0f;

			TMediumCondStatic cond_static{};

			for (int z = 0; z < TMedium::depth(); ++z)
			{
				for (int x = 0; x < TMedium::width(); ++x)
				{
					for (int y = 0; y < TMedium::height(); ++y)
					{
						const int offset = TMedium::offset_for(x, y, z);

						const float yz_r = std::sqrt(std::pow(y - LENSE_SPEHERE_Y, 2.0f) + std::pow(z - LENSE_SPEHERE_Z, 2.0f));

						const bool inside_sphere = 
							(std::pow(x - LENSE_SPHERE_X, 2.0f) + std::pow(y - LENSE_SPEHERE_Y, 2.0f) + std::pow(z - LENSE_SPEHERE_Z, 2.0f)) 
								< std::pow(LENSE_SPHERE_RADIUS, 2.0);

						if (x < LENSE_BASE_X2 && inside_sphere || (x >= LENSE_BASE_X2))
							medium.data[offset].velocity_bit = 1;// VEL_FACTOR2;
						else
							medium.data[offset].velocity_bit = 0; // VEL_FACTOR1;

						cond_static.data[offset] = 127.0;

						if (yz_r > LENSE_RADIUS)
						{
							if (x > LENSE_BASE_X1/* && x < LENSE_BASE_X2*/)
							{
								if (x < LENSE_BASE_X1 + 10)
								{
									const float i = std::abs(x - LENSE_BASE_X1) + 2.0f;
									cond_static.data[offset] = 127.0f * std::powf(EDGE_SLOW_DOWN_FACTOR, i);
								}
								if (yz_r < LENSE_RADIUS + 10)
								{
									const float i = std::abs(yz_r - LENSE_RADIUS) + 2.0f;
									cond_static.data[offset] *= std::powf(EDGE_SLOW_DOWN_FACTOR, i);
								}

								if ((x >= LENSE_BASE_X1 + 10) /*&& (x <= LENSE_BASE_X2 - 10)*/ && (yz_r >= LENSE_RADIUS + 10))
								{
									cond_static.data[offset] = 0.0f;
								}
							}
						}
					}
				}
			}

			load_scene_edges(cond_static);
			for (int z = 0; z < TMedium::depth(); ++z)
			{
				for (int x = 0; x < TMedium::width(); ++x)
				{
					for (int y = 0; y < TMedium::height(); ++y)
					{
						const int offset = TMedium::offset_for(x, y, z);
						medium.data[offset].conductivity = static_cast<uint8_t>(cond_static.data[offset]);
					}
				}
			}
		}

	public:

#pragma warning(push)
#pragma warning(disable:26451)
		bool iterate()  noexcept
		{
			auto& current = _mediums[_iteration % 2];
			auto& next = _mediums[(_iteration + 1) % 2];

			fill(current, 11, (_iteration % 70) > 35);

			const uint64_t start = __rdtsc();

			constexpr int xd_neighbour = TMedium::offset_for(-1, 0, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int xu_neighbour = TMedium::offset_for(1, 0, 0) - TMedium::offset_for(0, 0, 0);

			constexpr int yd_neighbour = TMedium::offset_for(0, -1, 0) - TMedium::offset_for(0, 0, 0);
			constexpr int yu_neighbour = TMedium::offset_for(0, 1, 0) - TMedium::offset_for(0, 0, 0);

			constexpr int zd_neighbour = TMedium::offset_for(0, 0, -1) - TMedium::offset_for(0, 0, 0);
			constexpr int zu_neighbour = TMedium::offset_for(0, 0, 1) - TMedium::offset_for(0, 0, 0);

			_grid.GridRun(
				[&](int thread_idx, int num_threads)
				{
					int slice = TMedium::depth() / num_threads;
					int from = thread_idx * slice;
					int to = (thread_idx + 1) * slice;

					for (int z = from; z < to; ++z)
					{
						for (int y = 0; y < TMedium::height(); ++ y)
						{
							for (int x = 0; x < TMedium::width(); ++x)
							{
								const int offset = TMedium::offset_for(x, y, z);
								const auto item_static = _static.data[offset];

								if (item_static.conductivity == 0)
									continue;

								const float neigh_total =
									current.data[offset + xd_neighbour].location +
									current.data[offset + xu_neighbour].location +
									current.data[offset + yd_neighbour].location +
									current.data[offset + yu_neighbour].location +
									current.data[offset + zd_neighbour].location +
									current.data[offset + zu_neighbour].location;

								const float neight_average = neigh_total * (1.0f / 6.0f);

								const float delta_x = current.data[offset].location - neight_average; // location relative to the current neightbour average 

								const float velolicty_factor = item_static.velocity_bit ? VEL_FACTOR2 : VEL_FACTOR1;
								const float conductivity_factor = static_cast<float>(item_static.conductivity) / 127.0f;

								const float new_velocity = (current.data[offset].velocity - velolicty_factor * delta_x) * conductivity_factor * 0.99999f;

								next.data[offset].location = current.data[offset].location + new_velocity * LOC_FACTOR;
								next.data[offset].velocity = new_velocity;
							}
						}
					}
				}
				);

			const uint64_t end = __rdtsc();

			if (_picture_exposing_until != 0)
			{

				for (int x = 0; x < TSrcPictureMedium::width(); ++x)
				{
					for (int y = 0; y < TSrcPictureMedium::height(); ++y)
					{
						for (int z = 0; z < TSrcPictureMedium::depth(); ++z)
						{
							_src_picture.at(x, y, z) += std::powf(current.at(x + PIC_SRC_BASE, y, z).location, 2.0f); // energy is a power of 2 of displacement or speed 
						}
					}
				}

				for (int x = 0; x < TPictureMedium::width(); ++x)
				{
					for (int y = 0; y < TPictureMedium::height(); ++y)
					{
						for (int z = 0; z < TPictureMedium::depth(); ++z)
						{
							_picture.at(x, y, z) += std::powf(current.at(x + PIC_BASE, y, z).location, 2.0f); // energy is a power of 2 of displacement or speed 
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

			elapsed_cpu_clocks += end - start;

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

			const uint64_t clocks_per_iter{ elapsed_cpu_clocks / _iteration };
			const uint64_t clocks_per_iter_per_voxel{ clocks_per_iter / (TMedium::depth() * TMedium::width() * TMedium::height()) };

			return { clocks_per_iter, clocks_per_iter_per_voxel };
		}

	private: 
		void fill(TMedium& medium, int x_plane, bool inverse)
		{
			if (!inverse)
			{
				for (int z = 0; z < medium.depth(); ++z)
				{
					for (int y = 0; y < medium.height(); ++y)
					{
						auto& item = medium.at(x_plane, y, z);
						item.location = _pattern.at(0, y, z);
						item.velocity = 0.0f;
					}
				}
			}
			else
			{
				for (int z = 0; z < medium.depth(); ++z)
				{
					for (int y = 0; y < medium.height(); ++y)
					{
						auto& item = medium.at(x_plane, y, z);
						item.location = -_pattern.at(0, y, z);
						item.velocity = 0.0f;
					}
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
						const int32_t value = static_cast<int32_t>(pic.at(x, y, z) / 1500.0);
						const int32_t brightness = std::max(0, std::min(255, value));

						const uint32_t offs = 4 * (y * pic.depth() + z);

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
