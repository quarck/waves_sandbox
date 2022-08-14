#pragma once

#include <stdint.h>

#include "World.h"

namespace waves::props
{
	constexpr int32_t ViewPortWidth{ World::TMedium::width() * 3 / 2};
	constexpr int32_t ViewPortHeight{ World::TMedium::height() * 3 / 2 };
}
