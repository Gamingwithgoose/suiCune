#include "../../../constants.h"
#include "bide.h"
#include "../effect_commands.h"
#include "../../battle_anims/core.h"
#include "../core.h"
#include "../../../home/battle_vars.h"
#include "../../../home/battle.h"
#include "../../../data/text/battle.h"

void BattleCommand_StoreEnergy(void){
    if(!bit_test(GetBattleVar(BATTLE_VARS_SUBSTATUS3), SUBSTATUS_BIDE))
        return;
    uint8_t* rc = (gBattle.turn == TURN_PLAYER)? &wram->wPlayerRolloutCount: &wram->wEnemyRolloutCount;
    if(--*rc != 0) {
        StdBattleTextbox(StoringEnergyText);
        return EndMoveEffect();
    }
    bit_reset(*GetBattleVarAddr(BATTLE_VARS_SUBSTATUS3), SUBSTATUS_BIDE);
    StdBattleTextbox(UnleashedEnergyText);
    *GetBattleVarAddr(BATTLE_VARS_MOVE_POWER) = 1;
    uint32_t* hl = &BattleParticipantForSide(gBattle.turn)->damageTaken;
    uint8_t* de = (gBattle.turn == TURN_PLAYER)? &wram->wPlayerCharging: &wram->wEnemyCharging;
    gBattle.damage = BattleScaleDamage(*hl, 2, 1);
    if(gBattle.damage == 0) {
        wram->wAttackMissed = TRUE;
    }
    *hl = 0;
    *de = 0;
    *GetBattleVarAddr(BATTLE_VARS_MOVE_ANIM) = BIDE;
    SkipToBattleCommand(unleashenergy_command);
}

void BattleCommand_UnleashEnergy(void){
    uint32_t* de = &BattleParticipantForSide(gBattle.turn)->damageTaken;
    uint8_t* bc = (gBattle.turn == TURN_PLAYER)? &wram->wPlayerRolloutCount: &wram->wEnemyRolloutCount;
    bit_set(*GetBattleVarAddr(BATTLE_VARS_SUBSTATUS3), SUBSTATUS_BIDE);
    *de = 0;
    wram->wPlayerMoveStruct.effect = 0;
    wram->wEnemyMoveStruct.effect = 0;
    *bc = (v_BattleRandom() & 1) + 2;
    BattleAnimationParameterSet(1);
    AnimateCurrentMove();
    return EndMoveEffect();
}
