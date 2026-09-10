#include "../../../constants.h"
#include "future_sight.h"
#include "../effect_commands.h"
#include "../core.h"
#include "../../../home/battle.h"
#include "../../../home/battle_vars.h"
#include "../../../data/text/battle.h"

void BattleCommand_CheckFutureSight(void){
    struct BattleParticipant* user = BattleParticipantForSide(gBattle.turn);
    if(user->futureSight.turnsRemaining != 1)
        return;
    user->futureSight.turnsRemaining = 0;
    gBattle.damage = user->futureSight.damage;
    user->futureSight.damage = 0;
    SkipToBattleCommand(futuresight_command);
}

void BattleCommand_FutureSight(void){
    if(!CheckUserIsCharging()) {
        move_t b = GetBattleVar(BATTLE_VARS_MOVE_ANIM);
        *GetBattleVarAddr(BATTLE_VARS_LAST_COUNTER_MOVE) = b;
        *GetBattleVarAddr(BATTLE_VARS_LAST_MOVE) = b;
    }
    struct BattleParticipant* user = BattleParticipantForSide(gBattle.turn);
    if(user->futureSight.turnsRemaining != 0) {
        ResetDamage();
        AnimateFailedMove();
        PrintButItFailed();
        return EndMoveEffect();
    }
    user->futureSight.turnsRemaining = 4;
    user->futureSight.damage = gBattle.damage;
    ResetDamage();
    BattleCommand_LowerSub();
    BattleCommand_MoveDelay();
    StdBattleTextbox(ForesawAttackText);
    BattleCommand_RaiseSub();
    return EndMoveEffect();
}
