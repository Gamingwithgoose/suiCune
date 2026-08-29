#include "../../constants.h"
#include "experience.h"
#include "../../data/growth_rates.h"
#include "../../home/pokemon.h"

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

uint8_t CalcLevel(struct PartyMon* mon){
    uint32_t monExp = ((uint32_t)mon->mon.exp[0] << 16)
                    | ((uint32_t)mon->mon.exp[1] << 8)
                    | mon->mon.exp[2];
    return CalcLevelForSpeciesExp(mon->mon.species, monExp);
}

uint32_t CalcExpAtLevelWithGrowthRate(uint8_t growthRate, uint8_t d){
//  (a/b)*n**3 + c*n**2 + d*n - e
    // LD_A_addr(wBaseGrowthRate);
    // ADD_A_A;
    // ADD_A_A;
    // LD_C_A;
    // LD_B(0);
    // LD_HL(mGrowthRates);
    // ADD_HL_BC;
    struct GrowthRate g = GrowthRates[growthRate];
//  Cube the level
    // CALL(aCalcExpAtLevel_LevelSquared);
    // LD_A_D;
    // LDH_addr_A(hMultiplier);
    // CALL(aMultiply);
    uint32_t lvl = d;
    lvl = lvl * lvl * lvl;
//  Multiply by a
    // LD_A_hl;
    // AND_A(0xf0);
    // SWAP_A;
    // LDH_addr_A(hMultiplier);
    // CALL(aMultiply);
    lvl *= g.a;
//  Divide by b
    // LD_A_hli;
    // AND_A(0xf);
    // LDH_addr_A(hDivisor);
    // LD_B(4);
    // CALL(aDivide);
    lvl /= g.b;
//  Push the cubic term to the stack
    // LDH_A_addr(hQuotient + 1);
    // PUSH_AF;
    // LDH_A_addr(hQuotient + 2);
    // PUSH_AF;
    // LDH_A_addr(hQuotient + 3);
    // PUSH_AF;
    uint32_t qt = lvl;
//  Square the level and multiply by the lower 7 bits of c
    // CALL(aCalcExpAtLevel_LevelSquared);
    // LD_A_hl;
    // AND_A(0x7f);
    // LDH_addr_A(hMultiplier);
    // CALL(aMultiply);
    lvl = d;
    lvl *= lvl;
    lvl *= (g.c & 0x7f);
//  Push the absolute value of the quadratic term to the stack
    // LDH_A_addr(hProduct + 1);
    // PUSH_AF;
    // LDH_A_addr(hProduct + 2);
    // PUSH_AF;
    // LDH_A_addr(hProduct + 3);
    // PUSH_AF;
    uint32_t qt_abs = lvl;
    // LD_A_hli;
    // PUSH_AF;
    uint8_t c = g.c;
//  Multiply the level by d
    // XOR_A_A;
    // LDH_addr_A(hMultiplicand + 0);
    // LDH_addr_A(hMultiplicand + 1);
    // LD_A_D;
    // LDH_addr_A(hMultiplicand + 2);
    // LD_A_hli;
    // LDH_addr_A(hMultiplier);
    // CALL(aMultiply);
    lvl = d;
    lvl *= g.d;
//  Subtract e
    // LD_B_hl;
    // LDH_A_addr(hProduct + 3);
    // SUB_A_B;
    // LDH_addr_A(hMultiplicand + 2);
    // LD_B(0);
    // LDH_A_addr(hProduct + 2);
    // SBC_A_B;
    // LDH_addr_A(hMultiplicand + 1);
    // LDH_A_addr(hProduct + 1);
    // SBC_A_B;
    // LDH_addr_A(hMultiplicand);
    lvl -= g.e;
//  If bit 7 of c is set, c is negative
    // POP_AF;
    // AND_A(0x80);
    // IF_NZ goto subtract;
    if((c & 0x80) == 0) {
    //  Add c*n**2 to (d*n - e)
        // POP_BC;
        // LDH_A_addr(hProduct + 3);
        // ADD_A_B;
        // LDH_addr_A(hMultiplicand + 2);
        // POP_BC;
        // LDH_A_addr(hProduct + 2);
        // ADC_A_B;
        // LDH_addr_A(hMultiplicand + 1);
        // POP_BC;
        // LDH_A_addr(hProduct + 1);
        // ADC_A_B;
        // LDH_addr_A(hMultiplicand);
        // goto done_quadratic;
        lvl += qt_abs;
    }
    else {
    // subtract:
    //  Subtract c*n**2 from (d*n - e)
        // POP_BC;
        // LDH_A_addr(hProduct + 3);
        // SUB_A_B;
        // LDH_addr_A(hMultiplicand + 2);
        // POP_BC;
        // LDH_A_addr(hProduct + 2);
        // SBC_A_B;
        // LDH_addr_A(hMultiplicand + 1);
        // POP_BC;
        // LDH_A_addr(hProduct + 1);
        // SBC_A_B;
        // LDH_addr_A(hMultiplicand);
        lvl -= qt_abs;
    }

// done_quadratic:
//  Add (a/b)*n**3 to (d*n - e +/- c*n**2)
    // POP_BC;
    // LDH_A_addr(hProduct + 3);
    // ADD_A_B;
    // LDH_addr_A(hMultiplicand + 2);
    // POP_BC;
    // LDH_A_addr(hProduct + 2);
    // ADC_A_B;
    // LDH_addr_A(hMultiplicand + 1);
    // POP_BC;
    // LDH_A_addr(hProduct + 1);
    // ADC_A_B;
    // LDH_addr_A(hMultiplicand);
    lvl += qt;
    lvl &= 0xffffff;

    hram.hMultiplicand[0] = (uint8_t)((lvl >> 16) & 0xff);
    hram.hMultiplicand[1] = (uint8_t)((lvl >> 8) & 0xff);
    hram.hMultiplicand[2] = (uint8_t)(lvl & 0xff);
    // RET;
    return lvl;


// LevelSquared:
    // XOR_A_A;
    // LDH_addr_A(hMultiplicand + 0);
    // LDH_addr_A(hMultiplicand + 1);
    // LD_A_D;
    // LDH_addr_A(hMultiplicand + 2);
    // LDH_addr_A(hMultiplier);
    // JP(mMultiply);

// INCLUDE "data/growth_rates.asm"

}

uint32_t CalcExpAtLevelForSpecies(SpeciesId species, uint8_t level) {
    const struct BaseData* base = GetSpeciesBaseData(species);
    if(base == NULL)
        return 0;
    return CalcExpAtLevelWithGrowthRate(base->growthRate, level);
}

uint32_t CalcExpAtLevel(uint8_t level) {
    return CalcExpAtLevelWithGrowthRate(wram->wBaseGrowthRate, level);
}
