#include "../../../constants.h"
#include "rollout.h"
#include "../effect_commands.h"
#include "../../../home/battle.h"
#include "../../../home/battle_vars.h"

#define MAX_ROLLOUT_COUNT (5)

void BattleCommand_CheckCurl(void){
//  checkcurl

    // LD_DE(wPlayerRolloutCount);
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto ok;
    // LD_DE(wEnemyRolloutCount);
    uint8_t* de = (gBattle.turn == TURN_PLAYER)? &wram->wPlayerRolloutCount: &wram->wEnemyRolloutCount;

// ok:
    // LD_A(BATTLE_VARS_SUBSTATUS1);
    // CALL(aGetBattleVar);
    // BIT_A(SUBSTATUS_ROLLOUT);
    // IF_Z goto reset;
    if(bit_test(GetBattleVar(BATTLE_VARS_SUBSTATUS1), SUBSTATUS_ROLLOUT)) {

        // LD_B(doturn_command);
        // JP(mSkipToBattleCommand);
        return SkipToBattleCommand(doturn_command);
    }

// reset:
    // XOR_A_A;
    // LD_de_A;
    *de = 0;
    // RET;
}

void BattleCommand_RolloutPower(void){
    if(GetBattleVar(BATTLE_VARS_STATUS) & SLP)
        return;
    uint8_t* hl = (gBattle.turn == TURN_PLAYER)? &wram->wPlayerRolloutCount: &wram->wEnemyRolloutCount;
    if(*hl == 0) {
        wram->wSomeoneIsRampaging = TRUE;
    }
    if(wram->wAttackMissed) {
        bit_reset(*GetBattleVarAddr(BATTLE_VARS_SUBSTATUS1), SUBSTATUS_ROLLOUT);
        return;
    }
    uint8_t b = ++*hl;
    if(b >= MAX_ROLLOUT_COUNT) {
        bit_reset(*GetBattleVarAddr(BATTLE_VARS_SUBSTATUS1), SUBSTATUS_ROLLOUT);
    }
    else {
        bit_set(*GetBattleVarAddr(BATTLE_VARS_SUBSTATUS1), SUBSTATUS_ROLLOUT);
    }
    if(bit_test(GetBattleVar(BATTLE_VARS_SUBSTATUS2), SUBSTATUS_CURLED)) {
        ++b;
    }
    while(b > 1) {
        gBattle.damage = BattleScaleDamage(gBattle.damage, 2, 1);
        --b;
    }
}
