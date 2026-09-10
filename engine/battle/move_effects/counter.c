#include "../../../constants.h"
#include "../../../home/battle.h"
#include "counter.h"
#include "../core.h"
#include "../effect_commands.h"
#include "../../../home/battle_vars.h"


void BattleCommand_Counter(void){
    wram->wAttackMissed = TRUE;
    move_t move = GetBattleVar(BATTLE_VARS_LAST_COUNTER_MOVE_OPP);
    if(move == NO_MOVE)
        return;
    uint8_t effect = GetMoveEffect(move);
    if(effect == EFFECT_COUNTER)
        return;
    BattleCommand_ResetTypeMatchup();
    if(wram->wTypeMatchup == 0 || !CheckOpponentWentFirst())
        return;
    struct Move temp;
    GetMoveData(&temp, move - 1);
    if(temp.power == 0)
        return;
    if(temp.type >= SPECIAL)
        return;
    uint32_t dmg = gBattle.damage;
    if(dmg == 0)
        return;
    gBattle.damage = BattleScaleDamage(dmg, 2, 1);
    wram->wAttackMissed = FALSE;
}
