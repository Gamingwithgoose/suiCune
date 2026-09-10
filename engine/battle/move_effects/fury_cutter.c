#include "../../../constants.h"
#include "../../../home/battle.h"
#include "fury_cutter.h"

void BattleCommand_FuryCutter(void){
    uint8_t* count = gBattle.turn == TURN_PLAYER ? &wram->wPlayerFuryCutterCount : &wram->wEnemyFuryCutterCount;
    if(wram->wAttackMissed)
        return ResetFuryCutterCount();
    if(*count < 5)
        ++*count;
    else
        *count = 5;
    gBattle.damage = BattleScaleDamage(gBattle.damage, 1u << (*count - 1), 1);
}

void ResetFuryCutterCount(void){
    // PUSH_HL;

    // LD_HL(wPlayerFuryCutterCount);
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto reset;
    if(gBattle.turn == TURN_PLAYER) {
        wram->wPlayerFuryCutterCount = 0;
    }
    // LD_HL(wEnemyFuryCutterCount);
    else {
        wram->wEnemyFuryCutterCount = 0;
    }

// reset:
    // XOR_A_A;
    // LD_hl_A;

    // POP_HL;
    // RET;
}
