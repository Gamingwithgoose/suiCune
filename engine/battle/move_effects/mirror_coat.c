#include "../../../constants.h"
#include "../../../home/battle.h"
#include "mirror_coat.h"
#include "../core.h"
#include "../effect_commands.h"
#include "../../../home/battle_vars.h"

void BattleCommand_MirrorCoat(void){
    wram->wAttackMissed = TRUE;
    move_t b = GetBattleVar(BATTLE_VARS_LAST_COUNTER_MOVE_OPP);
    if(b == NO_MOVE)
        return;
    uint8_t effect = GetMoveEffect(b);
    if(effect == EFFECT_MIRROR_COAT)
        return;
    BattleCommand_ResetTypeMatchup();
    if(wram->wTypeMatchup == 0 || !CheckOpponentWentFirst())
        return;
    struct Move temp;
    GetMoveData(&temp, b - 1);
    if(temp.power == 0)
        return;
    if(temp.type < SPECIAL)
        return;
    uint32_t dmg = gBattle.damage;
    if(dmg == 0)
        return;
    gBattle.damage = BattleScaleDamage(dmg, 2, 1);
    wram->wAttackMissed = FALSE;
}
