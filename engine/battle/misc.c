#include "../../constants.h"
#include "../../home/battle.h"
#include "misc.h"
#include "effect_commands.h"
#include "../gfx/place_graphic.h"
#include "../../home/battle_vars.h"
#include "../../home/text.h"
#include "../../data/battle/weather_modifiers.h"
#include "../../data/types/badge_type_boosts.h"

struct PicCoords {
    tile_t* hl;
    uint8_t b;
    uint8_t c;
};

static struct PicCoords GetEnemyFrontpicCoords(void) {
    return (struct PicCoords){.hl = coord(12, 0, wram->wTilemap), .b = 7, .c = 7};
}

static struct PicCoords GetPlayerBackpicCoords(void){
    return (struct PicCoords){.hl = coord(2, 6, wram->wTilemap), .b = 6, .c = 6};
}

void v_DisappearUser(void){
    // XOR_A_A;
    // LDH_addr_A(hBGMapMode);
    hram.hBGMapMode = BGMAPMODE_NONE;
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto player;
    struct PicCoords ccoords;
    if(gBattle.turn != TURN_PLAYER) {
        // CALL(aGetEnemyFrontpicCoords);
        // goto okay;
        ccoords = GetEnemyFrontpicCoords();
    }
    else {
    // player:
        // CALL(aGetPlayerBackpicCoords);
        ccoords = GetPlayerBackpicCoords();
    }

// okay:
    // CALL(aClearBox);
    ClearBox(ccoords.hl, ccoords.c, ccoords.b);
    // JR(mFinishAppearDisappearUser);
    return FinishAppearDisappearUser();
}

void v_AppearUserRaiseSub(void){
    // FARCALL(aBattleCommand_RaiseSubNoAnim);
    BattleCommand_RaiseSubNoAnim();
    // JR(mAppearUser);
    return AppearUser();
}

void v_AppearUserLowerSub(void){
    // FARCALL(aBattleCommand_LowerSubNoAnim);
    BattleCommand_LowerSubNoAnim();

    return AppearUser();
}

void AppearUser(void){
    // XOR_A_A;
    // LDH_addr_A(hBGMapMode);
    hram.hBGMapMode = BGMAPMODE_NONE;
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto player;
    struct PicCoords ccoords;
    uint8_t a;
    if(gBattle.turn != TURN_PLAYER) {
        // CALL(aGetEnemyFrontpicCoords);
        ccoords = GetEnemyFrontpicCoords();
        // XOR_A_A;
        a = 0;
        // goto okay;
    }
    else {
    // player:
        // CALL(aGetPlayerBackpicCoords);
        ccoords = GetPlayerBackpicCoords();
        // LD_A(0x31);
        a = 0x31;
    }
// okay:
    // PREDEF(pPlaceGraphic);
    PlaceGraphicYStaggerNative(ccoords.hl, a, ccoords.b, ccoords.c);
    return FinishAppearDisappearUser();
}

void FinishAppearDisappearUser(void){
    // LD_A(0x1);
    // LDH_addr_A(hBGMapMode);
    hram.hBGMapMode = BGMAPMODE_UPDATE_TILES;
    // RET;
}

// void GetEnemyFrontpicCoords(void){
//     hlcoord(12, 0, wTilemap);
//     LD_BC((7 << 8) | 7);
//     RET;

// }

// void GetPlayerBackpicCoords(void){
//     hlcoord(2, 6, wTilemap);
//     LD_BC((6 << 8) | 6);
//     RET;

// }

static void DoWeatherModifiers_ApplyModifier(const uint8_t* de) {
    uint32_t damage = BattleScaleDamage(gBattle.damage, de[1], 10);
    gBattle.damage = damage != 0 ? damage : 1;
}

void DoWeatherModifiers(void){
    // LD_DE(mWeatherTypeModifiers);
    const uint8_t* de = WeatherTypeModifiers;
    // LD_A_addr(wBattleWeather);
    // LD_B_A;
    uint8_t b = wram->wBattleWeather;
    // LD_A_addr(wCurType);
    // LD_C_A;
    uint8_t c = wram->wCurType;


    do {
    // CheckWeatherType:
        // LD_A_de;
        // INC_DE;
        uint8_t a = *(de++);
        // CP_A(-1);
        // IF_Z goto done_weather_types;
        if(a == 0xff)
            break;

        // CP_A_B;
        // IF_NZ goto NextWeatherType;
        if(a != b)
            continue;

        // LD_A_de;
        // CP_A_C;
        // IF_Z goto ApplyModifier;
        if(*de == c)
            return DoWeatherModifiers_ApplyModifier(de);


    // NextWeatherType:
        // INC_DE;
        // INC_DE;
        // goto CheckWeatherType;
    } while(de += 2, 1);


// done_weather_types:
    // LD_DE(mWeatherMoveModifiers);
    de = WeatherMoveModifiers;

    // LD_A(BATTLE_VARS_MOVE_EFFECT);
    // CALL(aGetBattleVar);
    // LD_C_A;
    c = GetBattleVar(BATTLE_VARS_MOVE_EFFECT);

    do {
    // CheckWeatherMove:
        // LD_A_de;
        // INC_DE;
        uint8_t a = *(de++);
        // CP_A(-1);
        // IF_Z goto done;
        if(a == 0xff)
            return;

        // CP_A_B;
        // IF_NZ goto NextWeatherMove;
        if(a != b)
            continue;

        // LD_A_de;
        // CP_A_C;
        // IF_Z goto ApplyModifier;
        if(*de == c)
            return DoWeatherModifiers_ApplyModifier(de);


    // NextWeatherMove:
        // INC_DE;
        // INC_DE;
        // goto CheckWeatherMove;
    } while(de += 2, 1);

    return;
// ApplyModifier:
    // XOR_A_A;
    // LDH_addr_A(hMultiplicand + 0);
    // LD_HL(wCurDamage);
    // LD_A_hli;
    // LDH_addr_A(hMultiplicand + 1);
    // LD_A_hl;
    // LDH_addr_A(hMultiplicand + 2);

    // INC_DE;
    // LD_A_de;
    // LDH_addr_A(hMultiplier);

    // CALL(aMultiply);

    // LD_A(10);
    // LDH_addr_A(hDivisor);
    // LD_B(4);
    // CALL(aDivide);

    // LDH_A_addr(hQuotient + 1);
    // AND_A_A;
    // LD_BC(-1);
    // IF_NZ goto Update;

    // LDH_A_addr(hQuotient + 2);
    // LD_B_A;
    // LDH_A_addr(hQuotient + 3);
    // LD_C_A;
    // OR_A_B;
    // IF_NZ goto Update;

    // LD_BC(1);


// Update:
    // LD_A_B;
    // LD_addr_A(wCurDamage);
    // LD_A_C;
    // LD_addr_A(wCurDamage + 1);


// done:
    // RET;

// INCLUDE "data/battle/weather_modifiers.asm"
}

void DoBadgeTypeBoosts(void){
    if(wram->wLinkMode != LINK_NULL)
        return;
    if(wram->wInBattleTowerBattle)
        return;
    if(gBattle.turn != TURN_PLAYER)
        return;
    const uint8_t* hl = BadgeTypeBoosts;
    uint16_t bc = (gPlayer.kantoBadges[0] << 8) | gPlayer.johtoBadges[0];
    do {
        uint8_t a = *hl;
        if(a == 0xff)
            return;
        bool carry = bc & 1;
        bc >>= 1;
        if(!carry)
            continue; 
        if(wram->wCurType == *hl) {
            uint32_t bonus = gBattle.damage / 8;
            gBattle.damage = BattleAddDamage(gBattle.damage, bonus != 0 ? bonus : 1);
            return;
        }
    } while(++hl, 1);
}
