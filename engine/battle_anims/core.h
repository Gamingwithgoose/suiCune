#ifndef SUICUNE_ENGINE_BATTLE_ANIMS_CORE_H
#define SUICUNE_ENGINE_BATTLE_ANIMS_CORE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "../../constants.h"

struct BattleBGEffect;

typedef uint16_t BattleAnimationId;

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
    uint8_t resourceKind;
    uint8_t category;
    uint8_t layer;
};

enum {
    // Surf's battle background effect is authored as one 64-sample cycle.
    // This is effect data, not a banked-WRAM allocation or renderer limit.
    BATTLE_ANIMATION_SURF_WAVE_SAMPLE_COUNT = 64,
    // Effects may prepare off-screen scanline values before the LCD consumes
    // the visible range. Keep that authored workspace separate from a Game
    // Boy bank or address-aligned allocation.
    BATTLE_ANIMATION_SCANLINE_WORKSPACE_SIZE = 0x100,
};

enum BattleRenderResourceKind {
    BATTLE_RENDER_RESOURCE_ANIMATION,
    BATTLE_RENDER_RESOURCE_HUD,
    BATTLE_RENDER_RESOURCE_BATTLER,
};

enum BattleSceneSpriteCategory {
    BATTLE_SCENE_SPRITE_BASELINE,
    BATTLE_SCENE_SPRITE_EFFECT,
    BATTLE_SCENE_SPRITE_HUD,
};

enum BattleSceneLayer {
    BATTLE_SCENE_LAYER_BASELINE,
    BATTLE_SCENE_LAYER_HUD,
    BATTLE_SCENE_LAYER_EFFECT,
    BATTLE_SCENE_LAYER_COUNT,
};

enum BattleSceneScanlineEffect {
    BATTLE_SCENE_SCANLINE_NONE,
    BATTLE_SCENE_SCANLINE_HORIZONTAL_OFFSET,
    BATTLE_SCENE_SCANLINE_VERTICAL_OFFSET,
};

enum BattleSceneBattlerId {
    BATTLE_SCENE_BATTLER_PLAYER,
    BATTLE_SCENE_BATTLER_OPPONENT,
    BATTLE_SCENE_BATTLER_COUNT,
};

// A battler picture is a native image plus an explicit tile placement list.
// Tile coordinates describe game content layout; they do not identify VRAM.
struct BattleSceneBattlerTile {
    int16_t x;
    int16_t y;
    uint16_t imageTile;
};

struct BattleSceneBattlerView {
    const uint8_t* pixels;
    size_t pixelTileCount;
    const struct BattleSceneBattlerTile* tiles;
    size_t tileCount;
    uint8_t palette;
    bool visible;
};

struct BattleAnimationTileBinding {
    uint8_t graphicsId;
    uint16_t tileOffset;
};

bool QueueBattleAnimation(void);
void DeinitBattleAnimation(struct BattleAnim* bc);
struct BattleAnim* BattleAnimationObjects(void);
size_t BattleAnimationObjectCount(void);
struct BattleAnim* BattleAnimationObjectAt(size_t index);
struct BattleAnim* BattleAnimationFirstObject(void);
struct BattleBGEffect* BattleAnimationBGEffects(void);
size_t BattleAnimationBGEffectCount(void);
struct BattleBGEffect* BattleAnimationBGEffectAt(size_t index);
struct BattleBGEffect* AllocateBattleAnimationBGEffect(void);
void SetBattleAnimationTileBinding(size_t index, uint8_t graphicsId, uint16_t tileOffset);
void AppendBattleAnimationTileBinding(uint8_t graphicsId, uint16_t tileOffset);
uint16_t BattleAnimationTileOffset(uint8_t graphicsId);
uint8_t* BattleAnimationTileWritePointer(uint16_t tileId, size_t tileCount);
uint8_t* BattleAnimationHudTileWritePointer(uint16_t tileId, size_t tileCount);
uint8_t* BattleSceneBattlerTileWritePointer(uint16_t tileId, size_t tileCount);
const uint8_t* BattleSceneBattlerPixels(void);
void SetBattleSceneBattlerImage(enum BattleSceneBattlerId battler,
    const uint8_t* pixels, size_t tileCount, uint8_t width, uint8_t height,
    int16_t x, int16_t y, uint8_t palette);
void UpdateBattleSceneBattlerImage(enum BattleSceneBattlerId battler,
    const uint8_t* pixels, size_t tileCount);
void SetBattleSceneBattlerVisible(enum BattleSceneBattlerId battler, bool visible);
void TranslateBattleSceneBattler(enum BattleSceneBattlerId battler, int16_t xDelta, int16_t yDelta);
void ClearBattleSceneBattlerTiles(enum BattleSceneBattlerId battler);
void ClearBattleSceneBattlerRegion(enum BattleSceneBattlerId battler,
    int16_t x, int16_t y, uint8_t width, uint8_t height);
void PlaceBattleSceneBattlerPattern(enum BattleSceneBattlerId battler,
    int16_t x, int16_t y, uint8_t width, uint8_t height,
    const uint8_t* imageTiles);
const struct BattleSceneBattlerView* BattleSceneBattler(enum BattleSceneBattlerId battler);
void RestoreBattleSceneBattlerPlacement(enum BattleSceneBattlerId battler);
void ClearBattleSceneBattlers(void);
const uint8_t* BattleAnimationSpritePixels(const struct BattleAnimationSprite* sprite);
struct BattleAnimationCommandState* BattleAnimationCommandState(void);
uint8_t BattleAnimationParameterGet(void);
void BattleAnimationParameterSet(uint8_t parameter);
BattleAnimationId BattleAnimationIdGet(void);
void BattleAnimationIdSet(BattleAnimationId id);
void BattleAnimationIdClearHighByte(void);
void BattleAnimationIdSetLowByte(uint8_t id);
void BeginBattleAnimationPresentation(void);
void EndBattleAnimationPresentation(void);
bool BattleAnimationPresentationActive(void);
void BeginBattleSceneDisplay(void);
void EndBattleSceneDisplay(void);
bool BattleSceneDisplayActive(void);
void BattleSceneCameraSet(int16_t x, int16_t y);
void BattleSceneCameraTranslate(int16_t xDelta, int16_t yDelta);
int16_t BattleSceneCameraX(void);
int16_t BattleSceneCameraY(void);
void BattleSceneScanlineEffectSet(enum BattleSceneScanlineEffect effect,
    uint8_t startLine, uint8_t endLine);
void BattleSceneScanlineEffectClear(void);
enum BattleSceneScanlineEffect BattleSceneScanlineEffectGet(void);
uint8_t BattleSceneScanlineEffectStart(void);
uint8_t BattleSceneScanlineEffectEnd(void);
int16_t BattleSceneHorizontalOffsetForLine(uint8_t line);
int16_t BattleSceneVerticalOffsetForLine(uint8_t line);
uint8_t* BattleAnimationScanlineOverrides(void);
uint8_t* BattleAnimationScanlineScratch(void);
uint8_t* BattleAnimationPaletteOutput(bool objectPalette);
uint8_t* BattleAnimationPaletteSource(bool objectPalette);
uint8_t BattleAnimationDMGBGPalette(void);
uint8_t BattleAnimationDMGObjectPalette0(void);
uint8_t BattleAnimationDMGObjectPalette1(void);
void BattleAnimationDMGBGPaletteSet(uint8_t palette);
void BattleAnimationDMGObjectPalette0Set(uint8_t palette);
void BattleAnimationDMGObjectPalette1Set(uint8_t palette);
struct BattleAnimationRenderState* BattleAnimationRenderState(void);
struct BattleAnimationEffectScratchState* BattleAnimationEffectScratchState(void);
uint8_t* BattleAnimationSurfWaveSamples(void);
const struct BattleAnimationSprite* BattleSceneSprites(size_t* count);
void ClearBattleSceneBaselineSprites(void);
void SetBattleScenePlayerTrainerBackpic(void);
void TranslateBattleSceneBaselineSprites(int16_t xDelta, int16_t yDelta);
void ClearBattleAnimationHudSprites(void);
void SetBattleAnimationHudSprites(size_t firstSprite, uint8_t y, uint8_t x, int8_t direction,
    const uint16_t* tileIds, size_t tileCount);
void BeginBattleAnimationRenderFrame(void);
void ClearBattleAnimationRenderSprites(void);
void SetBattleAnimationRenderSpritePalette(uint8_t paletteMask);
void ResetNativeBattleAnimationState(void);
void ClearNativeBattleAnimationObjects(void);
void IncrementBattleAnimationObjectIndex(void);
// void InitBattleAnimation(struct BattleAnim* bc);
void BattleAnimOAMUpdate(struct BattleAnim* bc);
// void InitBattleAnimBuffer(struct BattleAnim* bc);
void v_ExecuteBGEffects(void);
bool v_QueueBGEffect(void);
//#include "data/battle_anims/objects.h"

#endif
