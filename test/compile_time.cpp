#include "meta/IndexPack.h"

static_assert(meta::IndexPack<int, 0, 1, 2>::arity == 3, "");
