#include "../../../constants.h"
#include "substitute.h"
#include "../effect_commands.h"
#include "../../battle_anims/core.h"
#include "../../../home/battle_vars.h"
#include "../../../home/battle.h"
#include "../check_battle_scene.h"
#include "../../../data/text/battle.h"

void BattleCommand_Substitute(void){
    BattleCommand_MoveDelay();
    struct BattlePokemon* mon = (gBattle.turn == TURN_PLAYER)? &gBattle.player.mon: &gBattle.enemy.mon;
    uint16_t* subHP = (gBattle.turn == TURN_PLAYER)? &gBattle.player.substituteHP: &gBattle.enemy.substituteHP;
    if(bit_test(GetBattleVar(BATTLE_VARS_SUBSTATUS4), SUBSTATUS_SUBSTITUTE)) {
        if(CheckUserIsCharging())
            BattleCommand_RaiseSub();
        return StdBattleTextbox(HasSubstituteText);
    }
    uint16_t cost = mon->maxHP / 4;
    if(cost == 0)
        cost = 1;
    if(mon->hp <= cost) {
        if(CheckUserIsCharging())
            BattleCommand_RaiseSub();
        return StdBattleTextbox(TooWeakSubText);
    }
    BattleApplyDamage(mon, cost);
    *subHP = cost;
    bit_set(*GetBattleVarAddr(BATTLE_VARS_SUBSTATUS4), SUBSTATUS_SUBSTITUTE);
    uint8_t* wrapCount = (gBattle.turn == TURN_PLAYER)? &wram->wPlayerWrapCount: &wram->wEnemyWrapCount;
    move_t* trappingMove = (gBattle.turn == TURN_PLAYER)? &wram->wPlayerTrappingMove: &wram->wEnemyTrappingMove;
    *wrapCount = 0;
    *trappingMove = NO_MOVE;
    if(CheckBattleScene()) {
        wram->wNumHits = 0;
        BattleAnimationIdClearHighByte();
        BattleAnimationParameterSet(0);
        LoadAnim(SUBSTITUTE);
    }
    else {
        BattleCommand_RaiseSubNoAnim();
    }
    StdBattleTextbox(MadeSubstituteText);
    RefreshBattleHuds();
}
