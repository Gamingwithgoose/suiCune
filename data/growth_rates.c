#include "../constants.h"
#include "growth_rates.h"

const struct GrowthRate GrowthRates[] = {
    [GROWTH_MEDIUM_FAST]   = {1, 1,   0,   0,    0},
    [GROWTH_SLIGHTLY_FAST] = {3, 4,  10,   0,  -30},
    [GROWTH_SLIGHTLY_SLOW] = {3, 4,  20,   0,  -70},
    [GROWTH_MEDIUM_SLOW]   = {6, 5, -15, 100, -140},
    [GROWTH_FAST]          = {4, 5,   0,   0,    0},
    [GROWTH_SLOW]          = {5, 4,   0,   0,    0},
};

static_assert(lengthof(GrowthRates) == NUM_GROWTH_RATES, "");
