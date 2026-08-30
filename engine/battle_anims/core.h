#include <stddef.h>
#include <stdint.h>

struct BattleAnim;
struct BattleBGEffect;

struct BattleAnimationCommandState {
    uint8_t objectId;
    uint8_t objectX;
    uint8_t objectY;
    uint8_t objectParam;
    uint8_t bgEffectId;
    uint8_t bgEffectJumptableIndex;
    uint8_t bgEffectTurn;
    uint8_t bgEffectParam;
};

struct BattleAnimationRenderState {
    uint8_t oamFlags;
    uint8_t fixY;
    uint8_t tileId;
    uint8_t xCoord;
    uint8_t yCoord;
    uint8_t xOffset;
    uint8_t yOffset;
    uint8_t frameOamFlags;
    uint8_t palette;
};

struct BattleAnimationEffectScratchState {
    uint8_t gfxTileId;
    uint8_t gfxPicHeight;
    uint8_t sineProgress;
    uint8_t sineOffset;
    uint8_t sineAmplitude;
    uint8_t sineTimer;
    uint8_t picResizeBaseTileId;
};

bool QueueBattleAnimation(void);
void DeinitBattleAnimation(struct BattleAnim* bc);
struct BattleAnim* BattleAnimationObjects(void);
struct BattleBGEffect* BattleAnimationBGEffects(void);
uint8_t* BattleAnimationTileDictionary(void);
struct BattleAnimationCommandState* BattleAnimationCommandState(void);
struct BattleAnimationRenderState* BattleAnimationRenderState(void);
struct BattleAnimationEffectScratchState* BattleAnimationEffectScratchState(void);
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
