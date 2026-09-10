#include "../../../constants.h"
#include "pain_split.h"
#include "../effect_commands.h"
#include "../update_battle_huds.h"
#include "../../pokemon/health.h"
#include "../../../home/battle.h"
#include "../../../data/text/battle.h"

void BattleCommand_PainSplit(void){
    if(wram->wAttackMissed || CheckSubstituteOpp())
        return PrintDidntAffect2();
    AnimateCurrentMove();
    struct BattlePokemon* player = &gBattle.player.mon;
    struct BattlePokemon* enemy = &gBattle.enemy.mon;
    uint16_t oldPlayerHP = player->hp;
    uint16_t oldEnemyHP = enemy->hp;
    uint16_t sharedHP = ((uint32_t)oldPlayerHP + oldEnemyHP) / 2;
    player->hp = sharedHP < player->maxHP ? sharedHP : player->maxHP;
    enemy->hp = sharedHP < enemy->maxHP ? sharedHP : enemy->maxHP;
    ResetDamage();
    wram->wHPBuffer1 = player->maxHP;
    wram->wHPBuffer2 = oldPlayerHP;
    wram->wHPBuffer3 = player->hp;
    AnimateHPBar(coord(10, 9, wram->wTilemap), TURN_ENEMY);
    wram->wHPBuffer1 = enemy->maxHP;
    wram->wHPBuffer2 = oldEnemyHP;
    wram->wHPBuffer3 = enemy->hp;
    AnimateHPBar(coord(2, 2, wram->wTilemap), TURN_PLAYER);
    v_UpdateBattleHUDs();
    StdBattleTextbox(SharedPainText);
}
