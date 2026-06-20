#pragma once

// Shared prerequisites for instruction-family umbrella headers.
// Individual instruction implementation headers still keep their own include
// guards; this header lets family-level smoke translation units compile without
// relying on cpueaxh_internal.hpp's old monolithic include order.

#include <intrin.h>

#include "../cpu/def.h"
#include "../memory/manager.hpp"
#include "../cpu/memory.hpp"
#include "../cpu/helper.hpp"
#include "../cpu/init.hpp"
#include "../cpu/calc.hpp"
#include "../cpu/decoded.hpp"
#include "../cpu/dispatch_helpers.hpp"
#include "cpuid.hpp"
