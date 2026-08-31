#ifndef SUICUNE_ENGINE_BATTLE_ANIMS_CORE_H
#define SUICUNE_ENGINE_BATTLE_ANIMS_CORE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

struct BattleBGEffect;

// Native runtime object. Script content remains byte-coded at the loader
// boundary, but animation instances are not a packed Crystal memory format.
struct BattleAnim {
    uint8_t index;
    uint8_t oamFlags;
    uint8_t fixY;
    uint8_t framesetId;
    uint8_t function;
    uint8_t palette;
    uint16_t tileId;
    uint8_t xCoord;
    uint8_t yCoord;
    uint8_t xOffset;
    uint8_t yOffset;
    uint8_t param;
    uint8_t duration;
    uint8_t frame;
    uint8_t jumptableIndex;
    uint8_t var1;
    uint8_t var2;
};

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
    uint16_t tileId;
    uint8_t xCoord;
    uint8_t yCoord;
    uint8_t xOffset;
    uint8_t yOffset;
    uint8_t frameOamFlags;
    uint8_t palette;
};

struct BattleAnimationEffectScratchState {
    uint16_t gfxTileId;
    uint8_t gfxPicHeight;
    uint8_t sineProgress;
    uint8_t sineOffset;
    uint8_t sineAmplitude;
    uint8_t sineTimer;
    uint8_t picResizeBaseTileId;
};

struct BattleAnimationSprite {
    int16_t yCoord;
    int16_t xCoord;
    uint16_t tileId;
    uint8_t attributes;
};

struct BattleAnimationTileBinding {
    uint8_t graphicsId;
    uint16_t tileOffset;
};

bool QueueBattleAnimation(void);
void DeinitBattleAnimation(struct BattleAnim* bc);
struct BattleAnim* BattleAnimationObjects(void);
struct BattleBGEffect* BattleAnimationBGEffects(void);
void SetBattleAnimationTileBinding(size_t index, uint8_t graphicsId, uint16_t tileOffset);
void AppendBattleAnimationTileBinding(uint8_t graphicsId, uint16_t tileOffset);
uint16_t BattleAnimationTileOffset(uint8_t graphicsId);
uint8_t* BattleAnimationTileWritePointer(uint16_t tileId, size_t tileCount);
const uint8_t* BattleAnimationTilePixels(uint16_t tileId);
struct BattleAnimationCommandState* BattleAnimationCommandState(void);
struct BattleAnimationRenderState* BattleAnimationRenderState(void);
struct BattleAnimationEffectScratchState* BattleAnimationEffectScratchState(void);
const struct BattleAnimationSprite* BattleAnimationRenderSprites(size_t* count);
void BeginBattleAnimationRenderFrame(void);
void ClearBattleAnimationRenderSprites(void);
void SetBattleAnimationRenderSpritePalette(uint8_t paletteMask);
void ResetNativeBattleAnimationState(void);
void ClearNativeBattleAnimationObjects(size_t objectCount);
void IncrementBattleAnimationObjectIndex(void);
// void InitBattleAnimation(struct BattleAnim* bc);
void BattleAnimOAMUpdate(struct BattleAnim* bc);
// void InitBattleAnimBuffer(struct BattleAnim* bc);
void v_ExecuteBGEffects(void);
bool v_QueueBGEffect(void);
//#include "data/battle_anims/objects.h"

#endif
