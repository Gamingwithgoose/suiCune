#include "../../constants.h"
#include "experience.h"
#include "../../data/growth_rates.h"
#include "../../home/pokemon.h"
#include <stdlib.h>

uint8_t CalcLevelForSpeciesExp(SpeciesId species, uint32_t monExp){
    const struct BaseData* base = GetSpeciesBaseData(species);
    if(base == NULL)
        return 1;

    uint8_t level = 1;
    while(level < MAX_LEVEL) {
        uint8_t nextLevel = (uint8_t)(level + 1);
        if(monExp < CalcExpAtLevelWithGrowthRate(base->growthRate, nextLevel))
            break;
        level = nextLevel;
    }
    return level;
}

uint32_t CalcExpAtLevelWithGrowthRate(uint8_t growthRate, uint8_t level){
    if(growthRate >= NUM_GROWTH_RATES)
        abort();
    const struct GrowthRate* growth = &GrowthRates[growthRate];
    if(growth->cubicDenominator == 0)
        abort();
    int64_t n = level;
    // Divide the cubic term before adding the remaining terms, as Crystal does.
    int64_t result = growth->cubicNumerator * n * n * n / growth->cubicDenominator
        + growth->quadratic * n * n + growth->linear * n + growth->constant;
    // Preserve Crystal's negative low-level threshold values. This exceptional
    // curve behavior does not cap positive native experience at 24 bits.
    if(result < 0)
        result += 0x1000000;
    if(result < 0 || result > UINT32_MAX)
        abort();
    return (uint32_t)result;
}

uint32_t CalcExpAtLevelForSpecies(SpeciesId species, uint8_t level) {
    const struct BaseData* base = GetSpeciesBaseData(species);
    if(base == NULL)
        return 0;
    return CalcExpAtLevelWithGrowthRate(base->growthRate, level);
}
