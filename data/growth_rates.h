#pragma once

#include <stdint.h>

// Native polynomial coefficients; no packed ROM representation.
struct GrowthRate {
    uint16_t cubicNumerator;
    uint16_t cubicDenominator;
    int16_t quadratic;
    int16_t linear;
    int16_t constant;
};

extern const struct GrowthRate GrowthRates[];
