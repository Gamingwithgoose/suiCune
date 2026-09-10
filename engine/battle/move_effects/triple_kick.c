#include "../../../constants.h"
#include "../../../home/battle.h"
#include "triple_kick.h"
#include "../../battle_anims/core.h"

void BattleCommand_TripleKick(void){
    uint32_t multiplier = (uint32_t)BattleParticipantForSide(gBattle.turn)->kickCount + 1;
    gBattle.damage = BattleScaleDamage(gBattle.damage, multiplier, 1);
}

void BattleCommand_KickCounter(void){
    struct BattleParticipant* user = BattleParticipantForSide(gBattle.turn);
    // Triple Kick has at most three successful hits.
    if(user->kickCount < 3)
        ++user->kickCount;
    BattleAnimationParameterSet(user->kickCount);
}
