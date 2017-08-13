#include "meta/IndexPack.h"

static_assert(meta::IndexPack<0, 1, 2>::arity == 3, "");
