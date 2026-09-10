#include "../../../constants.h"
#include "../../../home/battle.h"
#include "pursuit.h"

void BattleCommand_Pursuit(void){
    bool switching = gBattle.turn == TURN_PLAYER ? wram->wEnemyIsSwitching : wram->wPlayerIsSwitching;
    if(switching)
        gBattle.damage = BattleScaleDamage(gBattle.damage, 2, 1);
}
