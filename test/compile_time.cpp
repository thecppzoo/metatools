#include "meta/IndexPack.h"

static_assert(meta::IndexPack<int, 0, 1, 2>::arity == 3, "");

#include "meta/TypeAtIndex.h"

using namespace meta;

static_assert(8 == sizeof(TypeAtIndex_t<0, long, char, int>), "");
static_assert(8 == sizeof(TypeAtIndex_t<1, char, long, char, int>), "");
static_assert(8 == sizeof(TypeAtIndex_t<3, char, int, char, long>), "");
