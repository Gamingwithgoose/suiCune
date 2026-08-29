#include "../../constants.h"
#include "breedmon_level_growth.h"
#include "experience.h"

static u8_pair_s GetBreedMonLevelGrowth(const struct NativeBoxMon* mon){
    uint8_t level = CalcLevelForSpeciesExp(mon->species, mon->exp);
    return u8_pair(mon->level, level - mon->level);
}

u8_pair_s GetBreedMon1LevelGrowth(void){
    return GetBreedMonLevelGrowth(&gPokemon.breedMon1);
}

u8_pair_s GetBreedMon2LevelGrowth(void){
    return GetBreedMonLevelGrowth(&gPokemon.breedMon2);
}
