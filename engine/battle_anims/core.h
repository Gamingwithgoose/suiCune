#include <stddef.h>

struct BattleAnim;
struct BattleBGEffect;

bool QueueBattleAnimation(void);
void DeinitBattleAnimation(struct BattleAnim* bc);
struct BattleAnim* BattleAnimationObjects(void);
struct BattleBGEffect* BattleAnimationBGEffects(void);
uint8_t* BattleAnimationTileDictionary(void);
void ResetNativeBattleAnimationState(void);
void ClearNativeBattleAnimationObjects(size_t byteCount);
void IncrementBattleAnimationObjectIndex(void);
// void InitBattleAnimation(struct BattleAnim* bc);
bool BattleAnimOAMUpdate(struct BattleAnim* bc, uint8_t* oamIndex);
// void InitBattleAnimBuffer(struct BattleAnim* bc);
// uint8_t GetBattleAnimTileOffset(uint8_t a);
void v_ExecuteBGEffects(void);
bool v_QueueBGEffect(void);
//#include "data/battle_anims/objects.h"
