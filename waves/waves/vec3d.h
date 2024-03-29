#pragma once

#include <algorithm>
#include <functional>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <sstream>

#include <immintrin.h>

namespace waves
{
#if defined(AVX2)
	struct vec3d_pd
	{
		__m256d v;

		vec3d_pd() 
			: v{ _mm256_setzero_pd() }
		{ 
		}

		vec3d_pd(const __m256d& _v)
			: v{ _v }
		{

		}

		vec3d_pd(double _x, double _y, double _z)
			: v{ _mm256_set_pd(0, _z, _y, _x) }
		{
		}

		inline double x() const noexcept
		{
			return v.m256d_f64[0];
		}

		inline double y() const noexcept
		{
			return v.m256d_f64[1];
		}

		inline double z() const noexcept
		{
			return v.m256d_f64[2];
		}

		inline double& x() noexcept
		{
			return v.m256d_f64[0];
		}

		inline double& y() noexcept
		{
			return v.m256d_f64[1];
		}

		inline double& z() noexcept
		{
			return v.m256d_f64[2];
		}

		void save_to(std::ostream& stream) const
		{
			stream.write(reinterpret_cast<const char*>(&v), sizeof(v));
		}

		void load_from(std::istream& stream)
		{
			stream.read(reinterpret_cast<char*>(&v), sizeof(v));
		}

		inline double modulo() const noexcept
		{
			//return std::sqrt(x() * x() + y() * y() + z() * z());
			auto m = _mm256_mul_pd(v, v);
			return std::sqrt(m.m256d_f64[0] + m.m256d_f64[1] + m.m256d_f64[2]);
		}

		inline static vec3d_pd cross(const vec3d_pd& lhs, const vec3d_pd& rhs) noexcept
		{
			// TODO: try to optimize this if you are ever going to use it.
			return {
				lhs.y() * rhs.z() - lhs.z() * rhs.y(),
				lhs.z() * rhs.x() - lhs.x() * rhs.z(), 
				lhs.x() * rhs.y() - lhs.y() * rhs.x(),
			};
		}

		inline static double dot(const vec3d_pd& lhs, const vec3d_pd& rhs) noexcept
		{
			auto m = _mm256_mul_pd(lhs.v, rhs.v);
			return m.m256d_f64[0] + m.m256d_f64[1] + m.m256d_f64[2];
		}

		inline vec3d_pd& operator-=(const vec3d_pd& rhs) noexcept
		{
			v = _mm256_sub_pd(v, rhs.v);
			return *this;
		}

		inline vec3d_pd& operator+=(const vec3d_pd& rhs) noexcept
		{
			v = _mm256_add_pd(v, rhs.v);
			return *this;
		}
	};

	inline vec3d_pd operator-(const vec3d_pd& lhs, const vec3d_pd& rhs) noexcept
	{
		return { _mm256_sub_pd(lhs.v, rhs.v) };
	}

	inline vec3d_pd operator+(const vec3d_pd& lhs, const vec3d_pd& rhs) noexcept
	{
		return { _mm256_add_pd(lhs.v, rhs.v) };
	}

	inline vec3d_pd operator*(const vec3d_pd& lhs, double f) noexcept
	{
		auto fmm = _mm256_set1_pd(f);
		return { _mm256_mul_pd(lhs.v, fmm) };
	}

	inline vec3d_pd operator/(const vec3d_pd& lhs, double f) noexcept
	{
		auto fmm = _mm256_set1_pd(f);
		return { _mm256_div_pd(lhs.v, fmm) };
	}

	inline vec3d_pd operator*(double f, const vec3d_pd& rhs) noexcept
	{
		auto fmm = _mm256_set1_pd(f);
		return { _mm256_mul_pd(rhs.v, fmm) };
	}

	inline vec3d_pd operator-(const vec3d_pd& lhs) noexcept
	{
		auto zero = _mm256_set1_pd(0.0);
		return {_mm256_sub_pd(zero, lhs.v) };
	}

#else  // #elif defined (AVX)
	struct vec3d_pd
	{
		__m128d v0{ _mm_setzero_pd() };
		__m128d v1{ _mm_setzero_pd() };

		vec3d_pd() = default;

		vec3d_pd(const __m128d& _v0, const __m128d& _v1)
			: v0{ _v0 }, v1{ _v1 }
		{
		}

		vec3d_pd(double _x, double _y, double _z)
			: v0{ _mm_set_pd(_y, _x) }
			, v1{ _mm_set_pd(0, _z) }
		{
		}

		inline double x() const noexcept
		{
			return v0.m128d_f64[0];
		}

		inline double y() const noexcept
		{
			return v0.m128d_f64[1];
		}

		inline double z() const noexcept
		{
			return v1.m128d_f64[0];
		}

		inline double& x() noexcept
		{
			return v0.m128d_f64[0];
		}

		inline double& y() noexcept
		{
			return v0.m128d_f64[1];
		}

		inline double& z() noexcept
		{
			return v1.m128d_f64[0];
		}

		void save_to(std::ostream& stream) const
		{
			stream.write(reinterpret_cast<const char*>(&v0), sizeof(v0));
			stream.write(reinterpret_cast<const char*>(&v1), sizeof(v1));
		}

		void load_from(std::istream& stream)
		{
			stream.read(reinterpret_cast<char*>(&v0), sizeof(v0));
			stream.read(reinterpret_cast<char*>(&v1), sizeof(v1));
		}

		inline double modulo() const noexcept
		{
			//return std::sqrt(x() * x() + y() * y() + z() * z());
			auto s = _mm_add_pd(_mm_mul_pd(v0, v0), _mm_mul_pd(v1, v1));
			return std::sqrt(s.m128d_f64[0] + s.m128d_f64[1]);
		}

		inline static vec3d_pd cross(const vec3d_pd& lhs, const vec3d_pd& rhs) noexcept
		{
			// TODO: try to optimize this if you are ever going to use it.
			return {
				lhs.y() * rhs.z() - lhs.z() * rhs.y(),
				lhs.z() * rhs.x() - lhs.x() * rhs.z(),
				lhs.x() * rhs.y() - lhs.y() * rhs.x(),
			};
		}

		inline static double dot(const vec3d_pd& lhs, const vec3d_pd& rhs) noexcept
		{
			auto s = _mm_add_pd(_mm_mul_pd(lhs.v0, rhs.v0), _mm_mul_pd(lhs.v1, rhs.v1));
			return s.m128d_f64[0] + s.m128d_f64[1];
		}

		inline vec3d_pd& operator-=(const vec3d_pd& rhs) noexcept
		{
			v0 = _mm_sub_pd(v0, rhs.v0);
			v1 = _mm_sub_pd(v1, rhs.v1);
			return *this;
		}

		inline vec3d_pd& operator+=(const vec3d_pd& rhs) noexcept
		{
			v0 = _mm_add_pd(v0, rhs.v0);
			v1 = _mm_add_pd(v1, rhs.v1);
			return *this;
		}
	};

	inline vec3d_pd operator-(const vec3d_pd& lhs, const vec3d_pd& rhs) noexcept
	{
		return { _mm_sub_pd(lhs.v0, rhs.v0), _mm_sub_pd(lhs.v1, rhs.v1) };
	}

	inline vec3d_pd operator+(const vec3d_pd& lhs, const vec3d_pd& rhs) noexcept
	{
		return { _mm_add_pd(lhs.v0, rhs.v0), _mm_add_pd(lhs.v1, rhs.v1) };
	}

	inline vec3d_pd operator*(const vec3d_pd& lhs, double f) noexcept
	{
		auto fmm = _mm_set1_pd(f);
		return { _mm_mul_pd(lhs.v0, fmm), _mm_mul_pd(lhs.v1, fmm) };
	}

	inline vec3d_pd operator/(const vec3d_pd& lhs, double f) noexcept
	{
		auto fmm = _mm_set1_pd(f);
		return { _mm_div_pd(lhs.v0, fmm), _mm_div_pd(lhs.v1, fmm) };
	}

	inline vec3d_pd operator*(double f, const vec3d_pd& rhs) noexcept
	{
		auto fmm = _mm_set1_pd(f);
		return { _mm_mul_pd(rhs.v0, fmm), _mm_mul_pd(rhs.v1, fmm) };
	}

	inline vec3d_pd operator-(const vec3d_pd& lhs) noexcept
	{
		auto zero = _mm_set1_pd(0.0);
		return { _mm_sub_pd(zero, lhs.v0), _mm_sub_pd(zero, lhs.v1) };
	}
#endif 

}