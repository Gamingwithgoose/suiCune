#include "../../constants.h"
#include "core.h"
#include "helpers.h"
#include "bg_effects.h"
#include "../../data/battle_anims/objects.h"
#include <stdio.h>
#include "../../util/log.h"
#include <stdlib.h>
#include <string.h>

#define LEGACY_OBJECT_X_ORIGIN TILE_WIDTH
#define LEGACY_OBJECT_Y_ORIGIN (2 * TILE_WIDTH)

static void InitBattleAnimation(struct BattleAnim* bc);
static void InitBattleAnimBuffer(struct BattleAnim* bc);

struct NativeBattleSceneBattler {
    uint8_t* pixels;
    size_t pixelTileCount;
    size_t pixelTileCapacity;
    uint8_t* basePixels;
    size_t basePixelTileCount;
    size_t basePixelTileCapacity;
    struct BattleSceneBattlerTile* tiles;
    size_t tileCount;
    size_t tileCapacity;
    // Resizing scripts are temporary animation presentation.  Their tile
    // maps must never replace the persistent native picture placement.
    struct BattleSceneBattlerTile* presentationTiles;
    size_t presentationTileCount;
    size_t presentationTileCapacity;
    bool presentationPlacementActive;
    int16_t presentationOffsetX;
    int16_t presentationOffsetY;
    bool presentationClipEnabled;
    int16_t presentationClipX;
    int16_t presentationClipY;
    uint8_t presentationClipWidth;
    uint8_t presentationClipHeight;
    int16_t defaultX;
    int16_t defaultY;
    uint8_t defaultWidth;
    uint8_t defaultHeight;
    bool defaultMirrorTileColumns;
    uint8_t palette;
    bool persistentVisible;
    bool presentationVisible;
    uint32_t generation;
};

struct NativeBattleAnimationState {
    struct BattleAnimationTileBinding* tileBindings;
    size_t tileBindingCount;
    size_t tileBindingCapacity;
    uint8_t* tilePixels;
    size_t tilePixelCapacity;
    uint8_t* hudTilePixels;
    size_t hudTilePixelCapacity;
    uint8_t* battlerTilePixels;
    size_t battlerTilePixelCapacity;
    struct NativeBattleSceneBattler battlers[BATTLE_SCENE_BATTLER_COUNT];
    struct BattleAnimationCommandState command;
    struct BattleAnimationRenderState render;
    struct BattleAnimationEffectScratchState effectScratch;
    uint8_t surfWaveSamples[BATTLE_ANIMATION_SURF_WAVE_SAMPLE_COUNT];
    struct BattleAnimationSprite* sceneSprites;
    size_t sceneSpriteCount;
    size_t sceneSpriteCapacity;
    struct BattleAnim* objects;
    size_t objectCount;
    size_t objectCapacity;
    struct BattleBGEffect* bgEffects;
    size_t bgEffectCount;
    size_t bgEffectCapacity;
    uint8_t lastObjectIndex;
};

struct NativeSpriteValidationRepeat {
    bool active;
    uint8_t resourceKind;
    uint16_t tileId;
    size_t tileSpan;
    size_t capacity;
    BattleAnimationId animationId;
    size_t repeats;
};

struct BattleAnimationPresentationState {
    bool active;
    size_t depth;
    uint8_t dmgBGPalette;
    uint8_t dmgObjectPalette0;
    uint8_t dmgObjectPalette1;
    uint8_t bgPaletteSource[8 * PALETTE_SIZE];
    uint8_t objectPaletteSource[8 * PALETTE_SIZE];
    uint8_t bgPaletteOutput[8 * PALETTE_SIZE];
    uint8_t objectPaletteOutput[8 * PALETTE_SIZE];
    uint8_t scanlineOverrides[BATTLE_ANIMATION_SCANLINE_WORKSPACE_SIZE];
    uint8_t scanlineScratch[BATTLE_ANIMATION_SCANLINE_WORKSPACE_SIZE];
};

struct NativeBattleSceneDisplayState {
    bool active;
    int16_t cameraX;
    int16_t cameraY;
    enum BattleSceneScanlineEffect scanlineEffect;
    uint8_t scanlineStart;
    uint8_t scanlineEnd;
};

static struct NativeBattleAnimationState sBattleAnimationState;
static struct NativeSpriteValidationRepeat sNativeSpriteValidationRepeats[16];
static struct BattleAnimationPresentationState sBattleAnimationPresentation;
static struct NativeBattleSceneDisplayState sBattleSceneDisplay;
// The parameter is supplied by battle logic before an animation begins and
// may be consumed by its script. It intentionally survives per-animation
// renderer resets just as the former shared battle-state byte did.
static uint8_t sBattleAnimationParameter;
// Battle animation selection is transient native battle state. It is kept
// separate from the legacy WRAM layout so content IDs can outlive that layout.
static BattleAnimationId sBattleAnimationId;

static void BattleAnimationFlushSpriteValidationRepeats(const char* boundary){
    for(size_t index = 0; index < lengthof(sNativeSpriteValidationRepeats); index++) {
        struct NativeSpriteValidationRepeat* repeat = &sNativeSpriteValidationRepeats[index];
        if(!repeat->active)
            continue;
        if(repeat->repeats != 0) {
            log_runtime_event("ERROR", "native sprite resource rejection repeated=%zu boundary=%s kind=%u tile=%u span=%zu capacity=%zu animation=%u",
                repeat->repeats, boundary == NULL ? "unspecified" : boundary,
                (unsigned)repeat->resourceKind, (unsigned)repeat->tileId,
                repeat->tileSpan, repeat->capacity, (unsigned)repeat->animationId);
        }
        memset(repeat, 0, sizeof(*repeat));
    }
}

static const char* BattleSceneBattlerName(enum BattleSceneBattlerId battler){
    return battler == BATTLE_SCENE_BATTLER_PLAYER ? "player" : "opponent";
}

static uint32_t BattleSceneHashBytes(const uint8_t* bytes, size_t count){
    uint32_t hash = 2166136261u;
    for(size_t index = 0; index < count; index++) {
        hash ^= bytes[index];
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t BattleSceneHashPlacement(const struct BattleSceneBattlerTile* tiles, size_t count){
    uint32_t hash = 2166136261u;
    for(size_t index = 0; index < count; index++) {
        uint16_t values[3] = {(uint16_t)tiles[index].x, (uint16_t)tiles[index].y,
            tiles[index].imageTile};
        for(size_t value = 0; value < lengthof(values); value++) {
            hash ^= (uint8_t)values[value];
            hash *= 16777619u;
            hash ^= (uint8_t)(values[value] >> 8);
            hash *= 16777619u;
        }
    }
    return hash;
}

static void BattleSceneLogContent(const char* boundary, enum BattleSceneBattlerId battler,
    const struct NativeBattleSceneBattler* state){
    log_runtime_event("BATTLE_SCENE", "content boundary=%s battler=%s pixelHash=%08x basePixelHash=%08x placementHash=%08x presentationPlacementHash=%08x persistentTiles=%zu presentationTiles=%zu presentationPlacement=%u",
        boundary, BattleSceneBattlerName(battler),
        (unsigned)(state->pixels == NULL ? 0u : BattleSceneHashBytes(state->pixels, state->pixelTileCount * LEN_2BPP_TILE)),
        (unsigned)(state->basePixels == NULL ? 0u : BattleSceneHashBytes(state->basePixels, state->basePixelTileCount * LEN_2BPP_TILE)),
        (unsigned)(state->tiles == NULL ? 0u : BattleSceneHashPlacement(state->tiles, state->tileCount)),
        (unsigned)(state->presentationTiles == NULL ? 0u : BattleSceneHashPlacement(state->presentationTiles, state->presentationTileCount)),
        state->tileCount, state->presentationTileCount, (unsigned)state->presentationPlacementActive);
}

static void BattleResourceAbort(const char* pool, const char* reason, size_t requested){
    log_runtime_mark_fatal(reason);
    log_runtime_event("FATAL", "pool=%s requested=%zu", pool, requested);
    abort();
}

void BattleSceneDiagnosticSnapshot(const char* reason){
    for(uint8_t battler = BATTLE_SCENE_BATTLER_PLAYER;
        battler < BATTLE_SCENE_BATTLER_COUNT; battler++) {
        const struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
        size_t maskedTiles = 0;
        if(state->pixelTileCount > state->pixelTileCapacity ||
            state->basePixelTileCount > state->basePixelTileCapacity ||
            state->tileCount > state->tileCapacity ||
            state->presentationTileCount > state->presentationTileCapacity ||
            (state->tileCount != 0 && state->tiles == NULL) ||
            (state->presentationTileCount != 0 && state->presentationTiles == NULL)) {
            log_runtime_event("ERROR", "snapshot invariant failed battler=%s pixels=%zu/%zu base=%zu/%zu tiles=%zu/%zu presentationTiles=%zu/%zu tilesPresent=%u",
                BattleSceneBattlerName(battler), state->pixelTileCount, state->pixelTileCapacity,
                state->basePixelTileCount, state->basePixelTileCapacity, state->tileCount,
                state->tileCapacity, state->presentationTileCount, state->presentationTileCapacity,
                (unsigned)(state->tiles != NULL));
        }
        else {
            for(size_t tile = 0; tile < state->tileCount; tile++) {
                if(state->tiles[tile].masked)
                    maskedTiles++;
                if(state->tiles[tile].imageTile >= state->pixelTileCount) {
                    log_runtime_event("ERROR", "snapshot invariant failed battler=%s placementIndex=%zu sourceTile=%u validRange=0..%zu",
                        BattleSceneBattlerName(battler), tile, (unsigned)state->tiles[tile].imageTile,
                        state->pixelTileCount == 0 ? 0 : state->pixelTileCount - 1);
                    break;
                }
            }
        }
        BattleSceneLogContent(reason == NULL ? "snapshot" : reason, battler, state);
        log_runtime_event("BATTLE_SCENE",
            "snapshot=%s battler=%s generation=%u persistentVisible=%u presentationVisible=%u effectiveVisible=%u maskedTiles=%zu pixels=%zu/%zu base=%zu/%zu tiles=%zu/%zu origin=%d,%d presentationOffset=%d,%d clipEnabled=%u clip=%d,%d %ux%u palette=%u",
            reason == NULL ? "unspecified" : reason, BattleSceneBattlerName(battler),
            state->generation, (unsigned)state->persistentVisible, (unsigned)state->presentationVisible,
            (unsigned)(state->persistentVisible && state->presentationVisible),
            maskedTiles, state->pixelTileCount, state->pixelTileCapacity,
            state->basePixelTileCount, state->basePixelTileCapacity, state->tileCount,
            state->tileCapacity, state->tileCount == 0 ? 0 : state->tiles[0].x,
            state->tileCount == 0 ? 0 : state->tiles[0].y, state->presentationOffsetX,
            state->presentationOffsetY, (unsigned)state->presentationClipEnabled,
            state->presentationClipX, state->presentationClipY,
            (unsigned)state->presentationClipWidth, (unsigned)state->presentationClipHeight,
            (unsigned)state->palette);
    }
    size_t activeObjects = 0;
    for(size_t index = 0; index < sBattleAnimationState.objectCount; index++)
        activeObjects += sBattleAnimationState.objects[index].index != 0;
    size_t activeEffects = 0;
    for(size_t index = 0; index < sBattleAnimationState.bgEffectCount; index++)
        activeEffects += sBattleAnimationState.bgEffects[index].function != 0;
    log_runtime_event("BATTLE_SCENE",
        "snapshot=%s sceneSprites=active:%zu/capacity:%zu objects=allocated:%zu active:%zu capacity:%zu bgEffects=allocated:%zu active:%zu capacity:%zu tileBindings=allocated:%zu active:%zu capacity:%zu",
        reason == NULL ? "unspecified" : reason,
        sBattleAnimationState.sceneSpriteCount, sBattleAnimationState.sceneSpriteCapacity,
        sBattleAnimationState.objectCount, activeObjects, sBattleAnimationState.objectCapacity,
        sBattleAnimationState.bgEffectCount, activeEffects, sBattleAnimationState.bgEffectCapacity,
        sBattleAnimationState.tileBindingCount, sBattleAnimationState.tileBindingCount,
        sBattleAnimationState.tileBindingCapacity);
}

struct BattleAnim* BattleAnimationObjects(void){
    return sBattleAnimationState.objects;
}

size_t BattleAnimationObjectCount(void){
    return sBattleAnimationState.objectCount;
}

struct BattleAnim* BattleAnimationObjectAt(size_t index){
    if(index >= sBattleAnimationState.objectCount)
        return NULL;
    return &sBattleAnimationState.objects[index];
}

struct BattleAnim* BattleAnimationFirstObject(void){
    if(sBattleAnimationState.objectCount == 0)
        return NULL;
    return &sBattleAnimationState.objects[0];
}

struct BattleBGEffect* BattleAnimationBGEffects(void){
    return sBattleAnimationState.bgEffects;
}

size_t BattleAnimationBGEffectCount(void){
    return sBattleAnimationState.bgEffectCount;
}

struct BattleBGEffect* BattleAnimationBGEffectAt(size_t index){
    if(index >= sBattleAnimationState.bgEffectCount)
        return NULL;
    return &sBattleAnimationState.bgEffects[index];
}

struct BattleBGEffect* AllocateBattleAnimationBGEffect(void){
    for(size_t i = 0; i < sBattleAnimationState.bgEffectCount; i++) {
        if(sBattleAnimationState.bgEffects[i].function == 0)
            return &sBattleAnimationState.bgEffects[i];
    }

    if(sBattleAnimationState.bgEffectCount == sBattleAnimationState.bgEffectCapacity) {
        size_t newCapacity = sBattleAnimationState.bgEffectCapacity == 0 ? 8 : sBattleAnimationState.bgEffectCapacity;
        if(newCapacity > SIZE_MAX / 2 || newCapacity * 2 > SIZE_MAX / sizeof(*sBattleAnimationState.bgEffects))
            abort();
        newCapacity *= 2;
        struct BattleBGEffect* effects = realloc(sBattleAnimationState.bgEffects,
            newCapacity * sizeof(*effects));
        if(effects == NULL)
            BattleResourceAbort("bgEffects", "native battle background-effect allocation failed", newCapacity);
        sBattleAnimationState.bgEffects = effects;
        sBattleAnimationState.bgEffectCapacity = newCapacity;
        log_runtime_event("RESOURCE", "pool=bgEffects oldCount=%zu oldCapacity=%zu requested=%zu newCapacity=%zu",
            sBattleAnimationState.bgEffectCount, newCapacity / 2, sBattleAnimationState.bgEffectCount + 1, newCapacity);
    }

    struct BattleBGEffect* effect = &sBattleAnimationState.bgEffects[sBattleAnimationState.bgEffectCount++];
    memset(effect, 0, sizeof(*effect));
    return effect;
}

void SetBattleAnimationTileBinding(size_t index, uint8_t graphicsId, uint16_t tileOffset){
    if(index >= sBattleAnimationState.tileBindingCapacity) {
        size_t newCapacity = sBattleAnimationState.tileBindingCapacity == 0 ? 8 : sBattleAnimationState.tileBindingCapacity;
        while(newCapacity <= index) {
            if(newCapacity > SIZE_MAX / 2 / sizeof(*sBattleAnimationState.tileBindings))
                abort();
            newCapacity *= 2;
        }
        struct BattleAnimationTileBinding* bindings = realloc(sBattleAnimationState.tileBindings,
            newCapacity * sizeof(*bindings));
        if(bindings == NULL)
            BattleResourceAbort("tileBindings", "native battle tile-binding allocation failed", newCapacity);
        sBattleAnimationState.tileBindings = bindings;
        sBattleAnimationState.tileBindingCapacity = newCapacity;
        log_runtime_event("RESOURCE", "pool=tileBindings oldCount=%zu oldCapacity=%zu requested=%zu newCapacity=%zu",
            sBattleAnimationState.tileBindingCount, newCapacity / 2, index + 1, newCapacity);
    }
    sBattleAnimationState.tileBindings[index].graphicsId = graphicsId;
    sBattleAnimationState.tileBindings[index].tileOffset = tileOffset;
    if(index >= sBattleAnimationState.tileBindingCount)
        sBattleAnimationState.tileBindingCount = index + 1;
}

void AppendBattleAnimationTileBinding(uint8_t graphicsId, uint16_t tileOffset){
    SetBattleAnimationTileBinding(sBattleAnimationState.tileBindingCount, graphicsId, tileOffset);
}

uint16_t BattleAnimationTileOffset(uint8_t graphicsId){
    for(size_t i = 0; i < sBattleAnimationState.tileBindingCount; i++) {
        if(sBattleAnimationState.tileBindings[i].graphicsId == graphicsId)
            return sBattleAnimationState.tileBindings[i].tileOffset;
    }
    return 0;
}

static uint8_t* BattleRenderTileWritePointer(const char* pool, uint8_t** pixels, size_t* capacity, uint16_t tileId, size_t tileCount){
    if(tileCount > SIZE_MAX - (size_t)tileId)
        abort();
    size_t requiredCapacity = (size_t)tileId + tileCount;
    if(requiredCapacity == 0)
        requiredCapacity = 1;
    if(requiredCapacity > *capacity) {
        size_t oldCapacity = *capacity;
        if(requiredCapacity > SIZE_MAX / LEN_2BPP_TILE)
            abort();
        uint8_t* resizedPixels = realloc(*pixels, requiredCapacity * LEN_2BPP_TILE);
        if(resizedPixels == NULL)
            BattleResourceAbort(pool, "native battle tile allocation failed", requiredCapacity);
        memset(resizedPixels + *capacity * LEN_2BPP_TILE, 0,
            (requiredCapacity - *capacity) * LEN_2BPP_TILE);
        *pixels = resizedPixels;
        *capacity = requiredCapacity;
        log_runtime_event("RESOURCE", "pool=%s oldCapacity=%zu requested=%zu newCapacity=%zu tileId=%u",
            pool, oldCapacity, requiredCapacity, requiredCapacity, (unsigned)tileId);
    }
    return *pixels + (size_t)tileId * LEN_2BPP_TILE;
}

uint8_t* BattleAnimationTileWritePointer(uint16_t tileId, size_t tileCount){
    return BattleRenderTileWritePointer("animationTiles", &sBattleAnimationState.tilePixels,
        &sBattleAnimationState.tilePixelCapacity, tileId, tileCount);
}

uint8_t* BattleAnimationHudTileWritePointer(uint16_t tileId, size_t tileCount){
    return BattleRenderTileWritePointer("hudTiles", &sBattleAnimationState.hudTilePixels,
        &sBattleAnimationState.hudTilePixelCapacity, tileId, tileCount);
}

uint8_t* BattleSceneBattlerTileWritePointer(uint16_t tileId, size_t tileCount){
    return BattleRenderTileWritePointer("battlerTiles", &sBattleAnimationState.battlerTilePixels,
        &sBattleAnimationState.battlerTilePixelCapacity, tileId, tileCount);
}

const uint8_t* BattleSceneBattlerPixels(void){
    return sBattleAnimationState.battlerTilePixels;
}

static void BattleSceneEnsurePixelBuffer(const char* pool, uint8_t** pixels, size_t* capacity, size_t tileCount){
    if(pixels == NULL || capacity == NULL || tileCount == 0)
        abort();
    if(tileCount <= *capacity)
        return;
    if(tileCount > SIZE_MAX / LEN_2BPP_TILE)
        abort();
    uint8_t* resizedPixels = realloc(*pixels, tileCount * LEN_2BPP_TILE);
    if(resizedPixels == NULL)
        BattleResourceAbort(pool, "native battler pixel allocation failed", tileCount);
    *pixels = resizedPixels;
    log_runtime_event("RESOURCE", "pool=%s oldCapacity=%zu requested=%zu newCapacity=%zu",
        pool, *capacity, tileCount, tileCount);
    *capacity = tileCount;
}

static void BattleSceneEnsureBattlerPixels(enum BattleSceneBattlerId battler, size_t tileCount){
    if(battler >= BATTLE_SCENE_BATTLER_COUNT)
        abort();
    struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
    BattleSceneEnsurePixelBuffer("battlerPixels", &state->pixels, &state->pixelTileCapacity, tileCount);
}

static void BattleSceneEnsureBattlerBasePixels(enum BattleSceneBattlerId battler, size_t tileCount){
    if(battler >= BATTLE_SCENE_BATTLER_COUNT)
        abort();
    struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
    BattleSceneEnsurePixelBuffer("battlerBasePixels", &state->basePixels, &state->basePixelTileCapacity, tileCount);
}

static void BattleSceneEnsureBattlerTiles(enum BattleSceneBattlerId battler,
    struct BattleSceneBattlerTile** tiles, size_t* capacity, size_t tileCount, const char* pool){
    if(battler >= BATTLE_SCENE_BATTLER_COUNT || tileCount == 0)
        abort();
    if(tileCount <= *capacity)
        return;
    if(tileCount > SIZE_MAX / sizeof(**tiles))
        abort();
    struct BattleSceneBattlerTile* resized = realloc(*tiles, tileCount * sizeof(*resized));
    if(resized == NULL)
        BattleResourceAbort(pool, "native battler placement allocation failed", tileCount);
    *tiles = resized;
    log_runtime_event("RESOURCE", "pool=%s oldCapacity=%zu requested=%zu newCapacity=%zu battler=%s",
        pool, *capacity, tileCount, tileCount, BattleSceneBattlerName(battler));
    *capacity = tileCount;
}

void ClearBattleSceneBattlerPresentationTiles(enum BattleSceneBattlerId battler){
    if(battler >= BATTLE_SCENE_BATTLER_COUNT)
        return;
    struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
    state->presentationTileCount = 0;
    state->presentationPlacementActive = true;
    log_runtime_event("BATTLE_SCENE", "clear temporary battler placement battler=%s", BattleSceneBattlerName(battler));
}

void ClearBattleSceneBattlerRegion(enum BattleSceneBattlerId battler,
    int16_t x, int16_t y, uint8_t width, uint8_t height){
    if(battler >= BATTLE_SCENE_BATTLER_COUNT)
        return;
    struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
    size_t masked = 0;
    int16_t right = x + width * TILE_WIDTH;
    int16_t bottom = y + height * TILE_WIDTH;
    for(size_t index = 0; index < state->tileCount; index++) {
        struct BattleSceneBattlerTile* tile = &state->tiles[index];
        if(tile->x >= x && tile->x < right && tile->y >= y && tile->y < bottom) {
            tile->masked = true;
            masked++;
        }
    }
    log_runtime_event("BATTLE_SCENE", "mask battler region battler=%s region=%d,%d grid=%ux%u maskedTiles=%zu persistentTiles=%zu",
        BattleSceneBattlerName(battler), x, y, (unsigned)width, (unsigned)height, masked, state->tileCount);
}

void ClearBattleSceneBattlerPresentationMasks(void){
    for(uint8_t battler = BATTLE_SCENE_BATTLER_PLAYER;
        battler < BATTLE_SCENE_BATTLER_COUNT; battler++) {
        struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
        size_t cleared = 0;
        for(size_t tile = 0; tile < state->tileCount; tile++) {
            if(state->tiles[tile].masked) {
                state->tiles[tile].masked = false;
                cleared++;
            }
        }
        if(cleared != 0)
            log_runtime_event("BATTLE_SCENE", "unmask battler presentation battler=%s restoredTiles=%zu",
                BattleSceneBattlerName(battler), cleared);
    }
}

void PlaceBattleSceneBattlerPattern(enum BattleSceneBattlerId battler,
    int16_t x, int16_t y, uint8_t width, uint8_t height,
    const uint8_t* imageTiles){
    if(battler >= BATTLE_SCENE_BATTLER_COUNT || imageTiles == NULL || width == 0 || height == 0)
        abort();
    size_t tileCount = (size_t)width * height;
    struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
    BattleSceneEnsureBattlerTiles(battler, &state->tiles, &state->tileCapacity, tileCount,
        "battlerPlacement");
    state->tileCount = tileCount;
    for(uint8_t row = 0; row < height; row++) {
        for(uint8_t column = 0; column < width; column++) {
            size_t index = (size_t)row * width + column;
            if(imageTiles[index] >= state->pixelTileCount) {
                log_runtime_event("ERROR", "battler=%s placement sourceTile=%u validRange=0..%zu",
                    BattleSceneBattlerName(battler), (unsigned)imageTiles[index],
                    state->pixelTileCount == 0 ? 0 : state->pixelTileCount - 1);
                abort();
            }
            state->tiles[index].x = x + column * TILE_WIDTH;
            state->tiles[index].y = y + row * TILE_WIDTH;
            state->tiles[index].imageTile = imageTiles[index];
            state->tiles[index].masked = false;
        }
    }
    BattleSceneLogContent("persistent-placement", battler, state);
}

void PlaceBattleSceneBattlerPresentationLegacyPattern(enum BattleSceneBattlerId battler,
    int16_t x, int16_t y, uint8_t width, uint8_t height, const uint8_t* imageTiles){
    if(battler >= BATTLE_SCENE_BATTLER_COUNT || imageTiles == NULL || width == 0 || height == 0)
        abort();
    struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
    size_t tileCount = (size_t)width * height;
    BattleSceneEnsureBattlerTiles(battler, &state->presentationTiles,
        &state->presentationTileCapacity, tileCount, "battlerPresentationPlacement");
    for(uint8_t row = 0; row < height; row++) {
        for(uint8_t column = 0; column < width; column++) {
            size_t index = (size_t)row * width + column;
            // BGSquare data names tiles using the former column-major picture
            // layout. Convert that compatibility identity at this boundary;
            // the native image store remains row-major regardless of the
            // temporary destination grid dimensions.
            uint8_t legacyTile = imageTiles[index];
            if(state->defaultWidth == 0 || state->defaultHeight == 0 ||
                legacyTile >= (size_t)state->defaultWidth * state->defaultHeight)
                abort();
            uint8_t sourceX = legacyTile / state->defaultHeight;
            uint8_t sourceY = legacyTile % state->defaultHeight;
            if(sourceX >= state->defaultWidth)
                abort();
            uint8_t sourceColumn = state->defaultMirrorTileColumns
                ? state->defaultWidth - 1 - sourceX : sourceX;
            uint16_t imageTile = (uint16_t)sourceY * state->defaultWidth + sourceColumn;
            if(imageTile >= state->pixelTileCount)
                abort();
            state->presentationTiles[index] = (struct BattleSceneBattlerTile){
                .x = x + column * TILE_WIDTH, .y = y + row * TILE_WIDTH,
                .imageTile = imageTile, .masked = false,
            };
        }
    }
    state->presentationTileCount = tileCount;
    state->presentationPlacementActive = true;
    uint32_t presentationHash = BattleSceneHashPlacement(state->presentationTiles, state->presentationTileCount);
    log_runtime_event("BATTLE_SCENE", "temporary placement battler=%s grid=%ux%u destination=%d,%d persistentPlacementHash=%08x presentationPlacementHash=%08x sourceLayout=legacy-column-major",
        BattleSceneBattlerName(battler), (unsigned)width, (unsigned)height, x, y,
        (unsigned)BattleSceneHashPlacement(state->tiles, state->tileCount), (unsigned)presentationHash);
    for(uint8_t row = 0; row < height; row++) {
        char mapping[96] = "";
        size_t offset = 0;
        for(uint8_t column = 0; column < width; column++) {
            int written = snprintf(mapping + offset, sizeof(mapping) - offset, "%s%u",
                column == 0 ? "" : ",", (unsigned)state->presentationTiles[(size_t)row * width + column].imageTile);
            if(written < 0 || (size_t)written >= sizeof(mapping) - offset)
                break;
            offset += (size_t)written;
        }
        log_runtime_event("BATTLE_SCENE", "temporary placement battler=%s row=%u sourceTiles=%s",
            BattleSceneBattlerName(battler), (unsigned)row, mapping);
    }
    if(width == state->defaultWidth && height == state->defaultHeight &&
        x == state->defaultX && y == state->defaultY && !state->defaultMirrorTileColumns) {
        bool identical = state->presentationTileCount == state->tileCount;
        for(size_t index = 0; identical && index < state->tileCount; index++) {
            const struct BattleSceneBattlerTile* persistent = &state->tiles[index];
            const struct BattleSceneBattlerTile* presentation = &state->presentationTiles[index];
            identical = persistent->x == presentation->x && persistent->y == presentation->y &&
                persistent->imageTile == presentation->imageTile;
        }
        if(!identical) {
            log_runtime_event("ERROR", "full-size temporary placement differs from persistent battler=%s persistentPlacementHash=%08x presentationPlacementHash=%08x",
                BattleSceneBattlerName(battler), (unsigned)BattleSceneHashPlacement(state->tiles, state->tileCount),
                (unsigned)presentationHash);
            abort();
        }
    }
}

void ClearBattleSceneBattlerPresentationPlacements(void){
    for(uint8_t battler = BATTLE_SCENE_BATTLER_PLAYER;
        battler < BATTLE_SCENE_BATTLER_COUNT; battler++) {
        struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
        if(!state->presentationPlacementActive)
            continue;
        state->presentationTileCount = 0;
        state->presentationPlacementActive = false;
        log_runtime_event("BATTLE_SCENE", "restore persistent battler placement after presentation battler=%s",
            BattleSceneBattlerName(battler));
    }
}

void SetBattleSceneBattlerImageAligned(enum BattleSceneBattlerId battler,
    const uint8_t* pixels, size_t tileCount, uint8_t width, uint8_t height,
    int16_t x, int16_t y, uint8_t palette, bool mirrorTileColumns){
    if(battler >= BATTLE_SCENE_BATTLER_COUNT || pixels == NULL || width == 0 || height == 0 ||
        tileCount < (size_t)width * height) {
        log_runtime_event("ERROR", "invalid battler image battler=%u pixelsPresent=%u tileCount=%zu grid=%ux%u",
            (unsigned)battler, (unsigned)(pixels != NULL), tileCount, (unsigned)width, (unsigned)height);
        abort();
    }
    BattleSceneEnsureBattlerPixels(battler, tileCount);
    BattleSceneEnsureBattlerBasePixels(battler, tileCount);
    struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
    memcpy(state->pixels, pixels, tileCount * LEN_2BPP_TILE);
    memcpy(state->basePixels, pixels, tileCount * LEN_2BPP_TILE);
    state->pixelTileCount = tileCount;
    state->basePixelTileCount = tileCount;
    state->palette = palette;
    state->defaultX = x;
    state->defaultY = y;
    state->defaultWidth = width;
    state->defaultHeight = height;
    state->defaultMirrorTileColumns = mirrorTileColumns;
    state->generation++;
    if(state->generation == 0)
        state->generation = 1;
    uint8_t imageTiles[7 * 7];
    if((size_t)width * height > lengthof(imageTiles))
        abort();
    for(uint8_t row = 0; row < height; row++) {
        for(uint8_t column = 0; column < width; column++) {
            // Native PNG loaders decode tiles left-to-right then top-to-bottom.
            // The scene compositor consumes this explicit placement list in
            // that same row-major order. Alignment mirrors display columns
            // only; FixBackpicAlignment already mirrors bits in each tile.
            uint8_t sourceColumn = mirrorTileColumns ? width - 1 - column : column;
            imageTiles[(size_t)row * width + column] =
                (uint8_t)((size_t)row * width + sourceColumn);
        }
    }
    PlaceBattleSceneBattlerPattern(battler, x, y, width, height, imageTiles);
    state->presentationTileCount = 0;
    state->presentationPlacementActive = false;
    state->presentationOffsetX = 0;
    state->presentationOffsetY = 0;
    state->presentationClipEnabled = false;
    state->presentationClipWidth = 0;
    state->presentationClipHeight = 0;
    state->persistentVisible = true;
    state->presentationVisible = true;
    log_runtime_event("PICTURE", "replace battler=%s generation=%u sourceTiles=%zu baseTiles=%zu tileByteStride=%u bytes=%zu grid=%ux%u destination=%d,%d palette=%u order=row-major mirrorColumns=%u sourceFirst=%u sourceLast=%u",
        BattleSceneBattlerName(battler), state->generation, tileCount,
        state->basePixelTileCount, LEN_2BPP_TILE, tileCount * LEN_2BPP_TILE,
        (unsigned)width, (unsigned)height, x, y,
        (unsigned)palette, (unsigned)mirrorTileColumns, (unsigned)imageTiles[0],
        (unsigned)imageTiles[(size_t)width * height - 1]);
    for(uint8_t row = 0; row < height; row++) {
        char mapping[96] = "";
        size_t offset = 0;
        for(uint8_t column = 0; column < width; column++) {
            int written = snprintf(mapping + offset, sizeof(mapping) - offset,
                "%s%u", column == 0 ? "" : ",", (unsigned)imageTiles[(size_t)row * width + column]);
            if(written < 0 || (size_t)written >= sizeof(mapping) - offset)
                break;
            offset += (size_t)written;
        }
        log_runtime_event("PICTURE", "placement battler=%s row=%u sourceTiles=%s",
            BattleSceneBattlerName(battler), (unsigned)row, mapping);
    }
    BattleSceneLogContent("initial-image-load", battler, state);
}

void SetBattleSceneBattlerImage(enum BattleSceneBattlerId battler,
    const uint8_t* pixels, size_t tileCount, uint8_t width, uint8_t height,
    int16_t x, int16_t y, uint8_t palette){
    SetBattleSceneBattlerImageAligned(battler, pixels, tileCount, width, height,
        x, y, palette, false);
}

void UpdateBattleSceneBattlerImage(enum BattleSceneBattlerId battler,
    const uint8_t* pixels, size_t tileCount){
    if(battler >= BATTLE_SCENE_BATTLER_COUNT || pixels == NULL || tileCount == 0)
        abort();
    BattleSceneEnsureBattlerPixels(battler, tileCount);
    struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
    for(size_t i = 0; i < state->tileCount; i++) {
        if(state->tiles[i].imageTile >= tileCount)
            abort();
    }
    for(size_t i = 0; i < state->presentationTileCount; i++) {
        if(state->presentationTiles[i].imageTile >= tileCount)
            abort();
    }
    memcpy(state->pixels, pixels, tileCount * LEN_2BPP_TILE);
    state->pixelTileCount = tileCount;
    log_runtime_event("PICTURE", "update battler=%s generation=%u sourceTiles=%zu bytes=%zu",
        BattleSceneBattlerName(battler), state->generation, tileCount,
        tileCount * LEN_2BPP_TILE);
    BattleSceneLogContent("current-image-update", battler, state);
}

void SetBattleSceneBattlerPersistentVisible(enum BattleSceneBattlerId battler, bool visible){
    if(battler < BATTLE_SCENE_BATTLER_COUNT) {
        sBattleAnimationState.battlers[battler].persistentVisible = visible;
        log_runtime_event("BATTLE_SCENE", "battler=%s persistentVisible=%u presentationVisible=%u effectiveVisible=%u generation=%u",
            BattleSceneBattlerName(battler), (unsigned)visible,
            (unsigned)sBattleAnimationState.battlers[battler].presentationVisible,
            (unsigned)(visible && sBattleAnimationState.battlers[battler].presentationVisible),
            sBattleAnimationState.battlers[battler].generation);
    }
}

void SetBattleSceneBattlerPresentationVisible(enum BattleSceneBattlerId battler, bool visible){
    if(battler < BATTLE_SCENE_BATTLER_COUNT) {
        struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
        state->presentationVisible = visible;
        log_runtime_event("BATTLE_SCENE", "battler=%s persistentVisible=%u presentationVisible=%u effectiveVisible=%u generation=%u",
            BattleSceneBattlerName(battler), (unsigned)state->persistentVisible, (unsigned)visible,
            (unsigned)(state->persistentVisible && visible), state->generation);
    }
}

void TranslateBattleSceneBattler(enum BattleSceneBattlerId battler, int16_t xDelta, int16_t yDelta){
    if(battler >= BATTLE_SCENE_BATTLER_COUNT)
        return;
    struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
    state->presentationOffsetX += xDelta;
    state->presentationOffsetY += yDelta;
    log_runtime_event("BATTLE_SCENE", "translate battler presentation battler=%s dx=%d dy=%d offset=%d,%d persistentPlacementHash=%08x",
        BattleSceneBattlerName(battler), xDelta, yDelta, state->presentationOffsetX,
        state->presentationOffsetY, (unsigned)BattleSceneHashPlacement(state->tiles, state->tileCount));
}

void BeginBattleSceneBattlerFaintPresentation(enum BattleSceneBattlerId battler){
    if(battler >= BATTLE_SCENE_BATTLER_COUNT)
        return;
    struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
    if(state->defaultWidth == 0 || state->defaultHeight == 0)
        return;
    state->presentationOffsetX = 0;
    state->presentationOffsetY = 0;
    state->presentationClipEnabled = true;
    state->presentationClipX = state->defaultX;
    state->presentationClipY = state->defaultY;
    state->presentationClipWidth = state->defaultWidth * TILE_WIDTH;
    state->presentationClipHeight = state->defaultHeight * TILE_WIDTH;
    log_runtime_event("BATTLE_SCENE", "begin faint presentation battler=%s clip=%d,%d %ux%u",
        BattleSceneBattlerName(battler), state->presentationClipX, state->presentationClipY,
        (unsigned)state->presentationClipWidth, (unsigned)state->presentationClipHeight);
}

void ClearBattleSceneBattlerPresentationTransforms(void){
    for(uint8_t battler = BATTLE_SCENE_BATTLER_PLAYER;
        battler < BATTLE_SCENE_BATTLER_COUNT; battler++) {
        struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
        if(state->presentationOffsetX == 0 && state->presentationOffsetY == 0 &&
            !state->presentationClipEnabled)
            continue;
        state->presentationOffsetX = 0;
        state->presentationOffsetY = 0;
        state->presentationClipEnabled = false;
        state->presentationClipWidth = 0;
        state->presentationClipHeight = 0;
        log_runtime_event("BATTLE_SCENE", "clear battler presentation transform battler=%s",
            BattleSceneBattlerName(battler));
    }
}

const struct BattleSceneBattlerView* BattleSceneBattler(enum BattleSceneBattlerId battler){
    static struct BattleSceneBattlerView views[BATTLE_SCENE_BATTLER_COUNT];
    if(battler >= BATTLE_SCENE_BATTLER_COUNT)
        return NULL;
    struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
    struct BattleSceneBattlerView* view = &views[battler];
    view->pixels = state->pixels;
    view->pixelTileCount = state->pixelTileCount;
    view->tiles = state->presentationPlacementActive ? state->presentationTiles : state->tiles;
    view->tileCount = state->presentationPlacementActive ? state->presentationTileCount : state->tileCount;
    view->palette = state->palette;
    view->persistentVisible = state->persistentVisible;
    view->presentationVisible = state->presentationVisible;
    view->visible = state->persistentVisible && state->presentationVisible;
    view->presentationOffsetX = state->presentationOffsetX;
    view->presentationOffsetY = state->presentationOffsetY;
    view->presentationClipEnabled = state->presentationClipEnabled;
    view->presentationClipX = state->presentationClipX;
    view->presentationClipY = state->presentationClipY;
    view->presentationClipWidth = state->presentationClipWidth;
    view->presentationClipHeight = state->presentationClipHeight;
    return view;
}

void RestoreBattleSceneBattlerPlacement(enum BattleSceneBattlerId battler){
    if(battler >= BATTLE_SCENE_BATTLER_COUNT)
        return;
    struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
    if(state->defaultWidth == 0 || state->defaultHeight == 0)
        return;
    uint8_t imageTiles[7 * 7];
    size_t count = (size_t)state->defaultWidth * state->defaultHeight;
    if(count > lengthof(imageTiles))
        abort();
    for(uint8_t row = 0; row < state->defaultHeight; row++) {
        for(uint8_t column = 0; column < state->defaultWidth; column++) {
            uint8_t sourceColumn = state->defaultMirrorTileColumns
                ? state->defaultWidth - 1 - column : column;
            imageTiles[(size_t)row * state->defaultWidth + column] =
                (uint8_t)((size_t)row * state->defaultWidth + sourceColumn);
        }
    }
    PlaceBattleSceneBattlerPattern(battler, state->defaultX, state->defaultY,
        state->defaultWidth, state->defaultHeight, imageTiles);
    log_runtime_event("PICTURE", "restore placement battler=%s generation=%u grid=%ux%u order=row-major mirrorColumns=%u",
        BattleSceneBattlerName(battler), state->generation, (unsigned)state->defaultWidth,
        (unsigned)state->defaultHeight, (unsigned)state->defaultMirrorTileColumns);
}

void RestoreBattleSceneBattlerBaseImage(enum BattleSceneBattlerId battler){
    if(battler >= BATTLE_SCENE_BATTLER_COUNT)
        return;
    struct NativeBattleSceneBattler* state = &sBattleAnimationState.battlers[battler];
    if(state->basePixels == NULL || state->basePixelTileCount == 0)
        return;
    BattleSceneEnsureBattlerPixels(battler, state->basePixelTileCount);
    memcpy(state->pixels, state->basePixels, state->basePixelTileCount * LEN_2BPP_TILE);
    state->pixelTileCount = state->basePixelTileCount;
    log_runtime_event("PICTURE", "restore base image battler=%s generation=%u sourceTiles=%zu",
        BattleSceneBattlerName(battler), state->generation, state->basePixelTileCount);
    BattleSceneLogContent("restore-base-image", battler, state);
}

void ClearBattleSceneBattlers(void){
    BattleAnimationFlushSpriteValidationRepeats("battle-scene-clear");
    for(size_t i = 0; i < BATTLE_SCENE_BATTLER_COUNT; i++) {
        free(sBattleAnimationState.battlers[i].pixels);
        free(sBattleAnimationState.battlers[i].basePixels);
        free(sBattleAnimationState.battlers[i].tiles);
        free(sBattleAnimationState.battlers[i].presentationTiles);
        memset(&sBattleAnimationState.battlers[i], 0, sizeof(sBattleAnimationState.battlers[i]));
    }
    log_runtime_event("BATTLE_SCENE", "clear all battler resources");
}

const uint8_t* BattleAnimationSpritePixels(const struct BattleAnimationSprite* sprite,
    size_t tileSpan){
    if(sprite == NULL || tileSpan == 0)
        return NULL;
    const uint8_t* pixels;
    size_t capacity;
    if(sprite->resourceKind == BATTLE_RENDER_RESOURCE_HUD) {
        pixels = sBattleAnimationState.hudTilePixels;
        capacity = sBattleAnimationState.hudTilePixelCapacity;
    }
    else if(sprite->resourceKind == BATTLE_RENDER_RESOURCE_BATTLER) {
        pixels = sBattleAnimationState.battlerTilePixels;
        capacity = sBattleAnimationState.battlerTilePixelCapacity;
    }
    else if(sprite->resourceKind == BATTLE_RENDER_RESOURCE_ANIMATION) {
        pixels = sBattleAnimationState.tilePixels;
        capacity = sBattleAnimationState.tilePixelCapacity;
    }
    else {
        log_runtime_event("ERROR", "native sprite resource rejected invalidKind=%u tile=%u span=%zu",
            (unsigned)sprite->resourceKind, (unsigned)sprite->tileId, tileSpan);
        return NULL;
    }
    // In 8x16 mode the draw host reads two complete native tiles, including
    // when Y-flipped. Validate the span before exposing a source pointer.
    if(pixels == NULL || sprite->tileId > capacity || tileSpan > capacity - sprite->tileId) {
        BattleAnimationId animationId = BattleAnimationIdGet();
        struct NativeSpriteValidationRepeat* available = NULL;
        for(size_t index = 0; index < lengthof(sNativeSpriteValidationRepeats); index++) {
            struct NativeSpriteValidationRepeat* repeat = &sNativeSpriteValidationRepeats[index];
            if(repeat->active && repeat->resourceKind == sprite->resourceKind &&
                repeat->tileId == sprite->tileId && repeat->tileSpan == tileSpan &&
                repeat->capacity == capacity && repeat->animationId == animationId) {
                if(repeat->repeats != SIZE_MAX)
                    repeat->repeats++;
                return NULL;
            }
            if(!repeat->active && available == NULL)
                available = repeat;
        }
        if(available == NULL) {
            BattleAnimationFlushSpriteValidationRepeats("validation-capacity");
            available = &sNativeSpriteValidationRepeats[0];
        }
        struct NativeSpriteValidationRepeat* repeat = available;
        repeat->active = true;
        repeat->resourceKind = sprite->resourceKind;
        repeat->tileId = sprite->tileId;
        repeat->tileSpan = tileSpan;
        repeat->capacity = capacity;
        repeat->animationId = animationId;
        repeat->repeats = 0;
        log_runtime_event("ERROR", "native sprite resource rejected kind=%u tile=%u span=%zu capacity=%zu animation=%u",
            (unsigned)sprite->resourceKind, (unsigned)sprite->tileId, tileSpan, capacity,
            (unsigned)animationId);
        return NULL;
    }
    return pixels + (size_t)sprite->tileId * LEN_2BPP_TILE;
}

struct BattleAnimationCommandState* BattleAnimationCommandState(void){
    return &sBattleAnimationState.command;
}

uint8_t BattleAnimationParameterGet(void){
    return sBattleAnimationParameter;
}

void BattleAnimationParameterSet(uint8_t parameter){
    sBattleAnimationParameter = parameter;
}

BattleAnimationId BattleAnimationIdGet(void){
    return sBattleAnimationId;
}

void BattleAnimationIdSet(BattleAnimationId id){
    sBattleAnimationId = id;
}

void BattleAnimationIdClearHighByte(void){
    sBattleAnimationId &= 0xff;
}

void BattleAnimationIdSetLowByte(uint8_t id){
    sBattleAnimationId = (sBattleAnimationId & 0xff00) | id;
}

void BeginBattleAnimationPresentation(void){
    struct BattleAnimationPresentationState* presentation = &sBattleAnimationPresentation;
    if(presentation->depth == 0) {
        memset(presentation, 0, sizeof(*presentation));
        presentation->active = true;
        presentation->dmgBGPalette = wram->wBGP;
        presentation->dmgObjectPalette0 = wram->wOBP0;
        presentation->dmgObjectPalette1 = wram->wOBP1;
        memcpy(presentation->bgPaletteSource, wram->wBGPals1, sizeof(presentation->bgPaletteSource));
        memcpy(presentation->objectPaletteSource, wram->wOBPals1, sizeof(presentation->objectPaletteSource));
        memcpy(presentation->bgPaletteOutput, wram->wBGPals2, sizeof(presentation->bgPaletteOutput));
        memcpy(presentation->objectPaletteOutput, wram->wOBPals2, sizeof(presentation->objectPaletteOutput));
        ClearBattleSceneBattlerPresentationMasks();
        for(uint8_t battler = BATTLE_SCENE_BATTLER_PLAYER;
            battler < BATTLE_SCENE_BATTLER_COUNT; battler++)
            SetBattleSceneBattlerPresentationVisible(battler, true);
    }
    presentation->depth++;
    log_runtime_event("ANIMATION", "presentation begin animation=%u depth=%zu", (unsigned)BattleAnimationIdGet(), presentation->depth);
    BattleSceneDiagnosticSnapshot("presentation-begin");
}

void EndBattleAnimationPresentation(void){
    struct BattleAnimationPresentationState* presentation = &sBattleAnimationPresentation;
    if(presentation->depth == 0) {
        log_runtime_event("ERROR", "presentation end without begin animation=%u", (unsigned)BattleAnimationIdGet());
        return;
    }
    presentation->depth--;
    if(presentation->depth != 0) {
        log_runtime_event("ANIMATION", "presentation end animation=%u depth=%zu", (unsigned)BattleAnimationIdGet(), presentation->depth);
        return;
    }
    ClearBattleSceneBattlerPresentationMasks();
    ClearBattleSceneBattlerPresentationPlacements();
    ClearBattleSceneBattlerPresentationTransforms();
    for(uint8_t battler = BATTLE_SCENE_BATTLER_PLAYER;
        battler < BATTLE_SCENE_BATTLER_COUNT; battler++)
        SetBattleSceneBattlerPresentationVisible(battler, true);
    BattleAnimationFlushSpriteValidationRepeats("animation-end");
    presentation->active = false;
    log_runtime_event("ANIMATION", "presentation end animation=%u depth=0", (unsigned)BattleAnimationIdGet());
    BattleSceneDiagnosticSnapshot("presentation-end");
}

bool BattleAnimationPresentationActive(void){
    return sBattleAnimationPresentation.active;
}

void BeginBattleSceneDisplay(void){
    memset(&sBattleSceneDisplay, 0, sizeof(sBattleSceneDisplay));
    sBattleSceneDisplay.active = true;
    log_runtime_event("BATTLE_SCENE", "display begin");
    BattleSceneDiagnosticSnapshot("display-begin");
}

void EndBattleSceneDisplay(void){
    BattleSceneDiagnosticSnapshot("display-end");
    memset(&sBattleSceneDisplay, 0, sizeof(sBattleSceneDisplay));
    log_runtime_event("BATTLE_SCENE", "display end");
}

bool BattleSceneDisplayActive(void){
    return sBattleSceneDisplay.active;
}

void BattleSceneCameraSet(int16_t x, int16_t y){
    sBattleSceneDisplay.cameraX = x;
    sBattleSceneDisplay.cameraY = y;
}

void BattleSceneCameraTranslate(int16_t xDelta, int16_t yDelta){
    sBattleSceneDisplay.cameraX += xDelta;
    sBattleSceneDisplay.cameraY += yDelta;
}

int16_t BattleSceneCameraX(void){
    return sBattleSceneDisplay.cameraX;
}

int16_t BattleSceneCameraY(void){
    return sBattleSceneDisplay.cameraY;
}

void BattleSceneScanlineEffectSet(enum BattleSceneScanlineEffect effect,
    uint8_t startLine, uint8_t endLine){
    sBattleSceneDisplay.scanlineEffect = effect;
    sBattleSceneDisplay.scanlineStart = startLine;
    sBattleSceneDisplay.scanlineEnd = endLine;
}

void BattleSceneScanlineEffectClear(void){
    sBattleSceneDisplay.scanlineEffect = BATTLE_SCENE_SCANLINE_NONE;
    sBattleSceneDisplay.scanlineStart = 0;
    sBattleSceneDisplay.scanlineEnd = 0;
}

enum BattleSceneScanlineEffect BattleSceneScanlineEffectGet(void){
    return sBattleSceneDisplay.scanlineEffect;
}

uint8_t BattleSceneScanlineEffectStart(void){
    return sBattleSceneDisplay.scanlineStart;
}

uint8_t BattleSceneScanlineEffectEnd(void){
    return sBattleSceneDisplay.scanlineEnd;
}

int16_t BattleSceneHorizontalOffsetForLine(uint8_t line){
    if((sBattleSceneDisplay.scanlineEffect == BATTLE_SCENE_SCANLINE_HORIZONTAL_OFFSET ||
        sBattleSceneDisplay.scanlineEffect == BATTLE_SCENE_SCANLINE_HORIZONTAL_DISPLACEMENT) &&
        line >= sBattleSceneDisplay.scanlineStart && line <= sBattleSceneDisplay.scanlineEnd) {
        if(sBattleSceneDisplay.scanlineEffect == BATTLE_SCENE_SCANLINE_HORIZONTAL_DISPLACEMENT)
            return (int8_t)BattleAnimationScanlineOverrides()[line];
        return BattleAnimationScanlineOverrides()[line];
    }
    return sBattleSceneDisplay.cameraX;
}

int16_t BattleSceneVerticalOffsetForLine(uint8_t line){
    if(sBattleSceneDisplay.scanlineEffect == BATTLE_SCENE_SCANLINE_VERTICAL_OFFSET &&
        line >= sBattleSceneDisplay.scanlineStart && line <= sBattleSceneDisplay.scanlineEnd) {
        return BattleAnimationScanlineOverrides()[line];
    }
    return sBattleSceneDisplay.cameraY;
}

uint8_t* BattleAnimationScanlineOverrides(void){
    return sBattleAnimationPresentation.scanlineOverrides;
}

uint8_t* BattleAnimationScanlineScratch(void){
    return sBattleAnimationPresentation.scanlineScratch;
}

uint8_t* BattleAnimationPaletteOutput(bool objectPalette){
    return objectPalette ? sBattleAnimationPresentation.objectPaletteOutput
                         : sBattleAnimationPresentation.bgPaletteOutput;
}

uint8_t* BattleAnimationPaletteSource(bool objectPalette){
    return objectPalette ? sBattleAnimationPresentation.objectPaletteSource
                         : sBattleAnimationPresentation.bgPaletteSource;
}

uint8_t BattleAnimationDMGBGPalette(void){
    return sBattleAnimationPresentation.dmgBGPalette;
}

uint8_t BattleAnimationDMGObjectPalette0(void){
    return sBattleAnimationPresentation.dmgObjectPalette0;
}

uint8_t BattleAnimationDMGObjectPalette1(void){
    return sBattleAnimationPresentation.dmgObjectPalette1;
}

void BattleAnimationDMGBGPaletteSet(uint8_t palette){
    sBattleAnimationPresentation.dmgBGPalette = palette;
}

void BattleAnimationDMGObjectPalette0Set(uint8_t palette){
    sBattleAnimationPresentation.dmgObjectPalette0 = palette;
}

void BattleAnimationDMGObjectPalette1Set(uint8_t palette){
    sBattleAnimationPresentation.dmgObjectPalette1 = palette;
}

struct BattleAnimationRenderState* BattleAnimationRenderState(void){
    return &sBattleAnimationState.render;
}

struct BattleAnimationEffectScratchState* BattleAnimationEffectScratchState(void){
    return &sBattleAnimationState.effectScratch;
}

uint8_t* BattleAnimationSurfWaveSamples(void){
    return sBattleAnimationState.surfWaveSamples;
}

const struct BattleAnimationSprite* BattleSceneSprites(size_t* count){
    *count = sBattleAnimationState.sceneSpriteCount;
    return sBattleAnimationState.sceneSprites;
}

static void RemoveBattleSceneSprites(uint8_t category){
    size_t writeIndex = 0;
    for(size_t readIndex = 0; readIndex < sBattleAnimationState.sceneSpriteCount; readIndex++) {
        struct BattleAnimationSprite sprite = sBattleAnimationState.sceneSprites[readIndex];
        if(sprite.category != category)
            sBattleAnimationState.sceneSprites[writeIndex++] = sprite;
    }
    sBattleAnimationState.sceneSpriteCount = writeIndex;
}

static struct BattleAnimationSprite* AppendBattleSceneSprite(uint8_t category, uint8_t layer){
    if(sBattleAnimationState.sceneSpriteCount == sBattleAnimationState.sceneSpriteCapacity) {
        size_t newCapacity = sBattleAnimationState.sceneSpriteCapacity == 0 ? 64 : sBattleAnimationState.sceneSpriteCapacity;
        if(newCapacity > SIZE_MAX / 2 || newCapacity * 2 > SIZE_MAX / sizeof(*sBattleAnimationState.sceneSprites))
            abort();
        newCapacity *= 2;
        struct BattleAnimationSprite* sprites = realloc(sBattleAnimationState.sceneSprites,
            newCapacity * sizeof(*sprites));
        if(sprites == NULL)
            BattleResourceAbort("sceneSprites", "native battle scene-sprite allocation failed", newCapacity);
        sBattleAnimationState.sceneSprites = sprites;
        sBattleAnimationState.sceneSpriteCapacity = newCapacity;
        log_runtime_event("RESOURCE", "pool=sceneSprites oldCount=%zu oldCapacity=%zu requested=%zu newCapacity=%zu",
            sBattleAnimationState.sceneSpriteCount, newCapacity / 2,
            sBattleAnimationState.sceneSpriteCount + 1, newCapacity);
    }
    struct BattleAnimationSprite* sprite = &sBattleAnimationState.sceneSprites[sBattleAnimationState.sceneSpriteCount++];
    memset(sprite, 0, sizeof(*sprite));
    sprite->category = category;
    sprite->layer = layer;
    return sprite;
}

void ClearBattleSceneBaselineSprites(void){
    RemoveBattleSceneSprites(BATTLE_SCENE_SPRITE_BASELINE);
}

void SetBattleScenePlayerTrainerBackpic(void){
    ClearBattleSceneBaselineSprites();
    for(uint8_t column = 0; column < 6; column++) {
        for(uint8_t row = 0; row < 3; row++) {
            struct BattleAnimationSprite* sprite = AppendBattleSceneSprite(
                BATTLE_SCENE_SPRITE_BASELINE, BATTLE_SCENE_LAYER_BASELINE);
            sprite->yCoord = 6 * TILE_WIDTH + row * 2 * TILE_WIDTH;
            sprite->xCoord = SCREEN_WIDTH * TILE_WIDTH + column * TILE_WIDTH;
            // Each semantic trainer sprite is an explicit vertical 8x16
            // pair.  Source tiles are contiguous in each column: 0/1,
            // 2/3, 4/5, then 6/7, and so on.  It must not inherit OAM's
            // low-bit masking rule at the native resource boundary.
            sprite->tileId = column * 6 + row * 2;
            sprite->resourceTileId = sprite->tileId;
            sprite->attributes = PAL_BATTLE_OB_PLAYER;
            sprite->resourceKind = BATTLE_RENDER_RESOURCE_BATTLER;
            sprite->tileSpan = 2;
            log_runtime_event("TRAINER_SCENE", "sprite=%zu kind=battler resourceTile=%u tile=%u span=2 resourceTiles=%zu x=%d y=%d size=8x16 flipX=%u flipY=%u layer=%u category=%u",
                sBattleAnimationState.sceneSpriteCount - 1, (unsigned)sprite->resourceTileId,
                (unsigned)sprite->tileId, sBattleAnimationState.battlerTilePixelCapacity,
                sprite->xCoord, sprite->yCoord, (unsigned)((sprite->attributes & OBJ_FLIP_X) != 0),
                (unsigned)((sprite->attributes & OBJ_FLIP_Y) != 0), (unsigned)sprite->layer,
                (unsigned)sprite->category);
        }
    }
}

void TranslateBattleSceneBaselineSprites(int16_t xDelta, int16_t yDelta){
    for(size_t i = 0; i < sBattleAnimationState.sceneSpriteCount; i++) {
        struct BattleAnimationSprite* sprite = &sBattleAnimationState.sceneSprites[i];
        if(sprite->category != BATTLE_SCENE_SPRITE_BASELINE)
            continue;
        sprite->xCoord += xDelta;
        sprite->yCoord += yDelta;
    }
}

void ClearBattleAnimationHudSprites(void){
    free(sBattleAnimationState.hudTilePixels);
    sBattleAnimationState.hudTilePixels = NULL;
    sBattleAnimationState.hudTilePixelCapacity = 0;
    RemoveBattleSceneSprites(BATTLE_SCENE_SPRITE_HUD);
}

static struct BattleAnimationSprite* BattleSceneHudSpriteAt(size_t index){
    size_t hudIndex = 0;
    for(size_t sceneIndex = 0; sceneIndex < sBattleAnimationState.sceneSpriteCount; sceneIndex++) {
        struct BattleAnimationSprite* sprite = &sBattleAnimationState.sceneSprites[sceneIndex];
        if(sprite->category != BATTLE_SCENE_SPRITE_HUD)
            continue;
        if(hudIndex == index)
            return sprite;
        hudIndex++;
    }
    while(hudIndex <= index) {
        struct BattleAnimationSprite* sprite = AppendBattleSceneSprite(
            BATTLE_SCENE_SPRITE_HUD, BATTLE_SCENE_LAYER_HUD);
        if(hudIndex == index)
            return sprite;
        hudIndex++;
    }
    abort();
}

void SetBattleAnimationHudSprites(size_t firstSprite, uint8_t y, uint8_t x, int8_t direction,
    const uint16_t* tileIds, size_t tileCount){
    int16_t positionX = x;
    for(size_t i = 0; i < tileCount; i++) {
        if(i > SIZE_MAX - firstSprite)
            abort();
        struct BattleAnimationSprite* sprite = BattleSceneHudSpriteAt(firstSprite + i);
        sprite->yCoord = (int16_t)y - LEGACY_OBJECT_Y_ORIGIN;
        sprite->xCoord = positionX - LEGACY_OBJECT_X_ORIGIN;
        sprite->tileId = tileIds[i];
        sprite->resourceTileId = sprite->tileId;
        sprite->attributes = PAL_BATTLE_OB_YELLOW;
        sprite->resourceKind = BATTLE_RENDER_RESOURCE_HUD;
        sprite->category = BATTLE_SCENE_SPRITE_HUD;
        positionX += direction;
    }
}

void BeginBattleAnimationRenderFrame(void){
    RemoveBattleSceneSprites(BATTLE_SCENE_SPRITE_EFFECT);
}

void ClearBattleAnimationRenderSprites(void){
    RemoveBattleSceneSprites(BATTLE_SCENE_SPRITE_EFFECT);
}

void SetBattleAnimationRenderSpritePalette(uint8_t paletteMask){
    for(size_t i = 0; i < sBattleAnimationState.sceneSpriteCount; i++) {
        struct BattleAnimationSprite* sprite = &sBattleAnimationState.sceneSprites[i];
        if(sprite->category == BATTLE_SCENE_SPRITE_EFFECT)
            sprite->attributes &= paletteMask;
    }
}

static struct BattleAnimationSprite* AppendBattleAnimationRenderSprite(void){
    return AppendBattleSceneSprite(BATTLE_SCENE_SPRITE_EFFECT, BATTLE_SCENE_LAYER_EFFECT);
}

void ResetNativeBattleAnimationState(void){
    BattleAnimationFlushSpriteValidationRepeats("animation-reset");
    RemoveBattleSceneSprites(BATTLE_SCENE_SPRITE_EFFECT);
    uint8_t* hudTilePixels = sBattleAnimationState.hudTilePixels;
    size_t hudTilePixelCapacity = sBattleAnimationState.hudTilePixelCapacity;
    uint8_t* battlerTilePixels = sBattleAnimationState.battlerTilePixels;
    size_t battlerTilePixelCapacity = sBattleAnimationState.battlerTilePixelCapacity;
    struct BattleAnimationSprite* sceneSprites = sBattleAnimationState.sceneSprites;
    size_t sceneSpriteCount = sBattleAnimationState.sceneSpriteCount;
    size_t sceneSpriteCapacity = sBattleAnimationState.sceneSpriteCapacity;
    struct NativeBattleSceneBattler battlers[BATTLE_SCENE_BATTLER_COUNT];
    memcpy(battlers, sBattleAnimationState.battlers, sizeof(battlers));

    free(sBattleAnimationState.tileBindings);
    free(sBattleAnimationState.tilePixels);
    free(sBattleAnimationState.objects);
    free(sBattleAnimationState.bgEffects);
    memset(&sBattleAnimationState, 0, sizeof(sBattleAnimationState));
    // Trainer HUD resources are battle-view state, not transient animation
    // state. Animation setup clears must not erase them mid-battle.
    sBattleAnimationState.hudTilePixels = hudTilePixels;
    sBattleAnimationState.hudTilePixelCapacity = hudTilePixelCapacity;
    sBattleAnimationState.battlerTilePixels = battlerTilePixels;
    sBattleAnimationState.battlerTilePixelCapacity = battlerTilePixelCapacity;
    sBattleAnimationState.sceneSprites = sceneSprites;
    sBattleAnimationState.sceneSpriteCount = sceneSpriteCount;
    sBattleAnimationState.sceneSpriteCapacity = sceneSpriteCapacity;
    memcpy(sBattleAnimationState.battlers, battlers, sizeof(battlers));
}

void ClearNativeBattleAnimationObjects(void){
    // Inactive records are reused on demand; the native pool has no fixed
    // hardware object-array span to clear.
    sBattleAnimationState.objectCount = 0;
}

void IncrementBattleAnimationObjectIndex(void){
    sBattleAnimationState.lastObjectIndex++;
}

bool QueueBattleAnimation(void){
    for(size_t i = 0; i < sBattleAnimationState.objectCount; i++) {
        if(sBattleAnimationState.objects[i].index == 0) {
            IncrementBattleAnimationObjectIndex();
            InitBattleAnimation(&sBattleAnimationState.objects[i]);
            return false;
        }
    }

    if(sBattleAnimationState.objectCount == sBattleAnimationState.objectCapacity) {
        size_t newCapacity = sBattleAnimationState.objectCapacity == 0 ? 16 : sBattleAnimationState.objectCapacity;
        if(newCapacity > SIZE_MAX / 2 || newCapacity * 2 > SIZE_MAX / sizeof(*sBattleAnimationState.objects))
            abort();
        newCapacity *= 2;
        struct BattleAnim* objects = realloc(sBattleAnimationState.objects,
            newCapacity * sizeof(*objects));
        if(objects == NULL)
            BattleResourceAbort("animationObjects", "native battle animation-object allocation failed", newCapacity);
        sBattleAnimationState.objects = objects;
        sBattleAnimationState.objectCapacity = newCapacity;
        log_runtime_event("RESOURCE", "pool=animationObjects oldCount=%zu oldCapacity=%zu requested=%zu newCapacity=%zu",
            sBattleAnimationState.objectCount, newCapacity / 2,
            sBattleAnimationState.objectCount + 1, newCapacity);
    }

    struct BattleAnim* object = &sBattleAnimationState.objects[sBattleAnimationState.objectCount++];
    memset(object, 0, sizeof(*object));
    IncrementBattleAnimationObjectIndex();
    InitBattleAnimation(object);
    return false;
}

void DeinitBattleAnimation(struct BattleAnim* bc){
    // LD_HL(BATTLEANIMSTRUCT_INDEX);
    // ADD_HL_BC;
    // LD_hl(0x0);
    bc->index = 0x0;
    // RET;
}

static void InitBattleAnimation(struct BattleAnim* bc){
    // LD_A_addr(wBattleObjectTempID);
    // LD_E_A;
    // LD_D(0);
    // LD_HL(mBattleAnimObjects);
    // for(int rept = 0; rept < BATTLEANIMOBJ_LENGTH; rept++){
    // ADD_HL_DE;
    // }
    // LD_E_L;
    // LD_D_H;
    const struct BattleAnimObj* de = BattleAnimObjects + BattleAnimationCommandState()->objectId;
    // LD_HL(BATTLEANIMSTRUCT_INDEX);
    // ADD_HL_BC;
    // LD_A_addr(wLastAnimObjectIndex);
    // LD_hli_A;  // BATTLEANIMSTRUCT_INDEX
    bc->index = sBattleAnimationState.lastObjectIndex;
    // LD_A_de;
    // INC_DE;
    // LD_hli_A;  // BATTLEANIMSTRUCT_OAMFLAGS
    bc->oamFlags = de->flags;
    // LD_A_de;
    // INC_DE;
    // LD_hli_A;  // BATTLEANIMSTRUCT_FIX_Y
    bc->fixY = de->enemyAnim;
    // LD_A_de;
    // INC_DE;
    // LD_hli_A;  // BATTLEANIMSTRUCT_FRAMESET_ID
    bc->framesetId = de->videoSeq;
    // LD_A_de;
    // INC_DE;
    // LD_hli_A;  // BATTLEANIMSTRUCT_FUNCTION
    bc->function = de->callback;
    // LD_A_de;
    // INC_DE;
    // LD_hli_A;  // BATTLEANIMSTRUCT_PALETTE
    bc->palette = de->palette;
    // LD_A_de;
    // Resolve the animation object's graphics key to its native tile offset.
    // LD_hli_A;  // BATTLEANIMSTRUCT_TILEID
    bc->tileId = BattleAnimationTileOffset(de->tileOffset);
    // LD_A_addr(wBattleObjectTempXCoord);
    // LD_hli_A;  // BATTLEANIMSTRUCT_XCOORD
    bc->xCoord = BattleAnimationCommandState()->objectX;
    // LD_A_addr(wBattleObjectTempYCoord);
    // LD_hli_A;  // BATTLEANIMSTRUCT_YCOORD
    bc->yCoord = BattleAnimationCommandState()->objectY;
    // XOR_A_A;
    // LD_hli_A;  // BATTLEANIMSTRUCT_XOFFSET
    bc->xOffset = 0;
    // LD_hli_A;  // BATTLEANIMSTRUCT_YOFFSET
    bc->yOffset = 0;
    // LD_A_addr(wBattleObjectTempParam);
    // LD_hli_A;  // BATTLEANIMSTRUCT_PARAM
    bc->param = BattleAnimationCommandState()->objectParam;
    // XOR_A_A;
    // LD_hli_A;  // BATTLEANIMSTRUCT_DURATION
    bc->duration = 0;
    // DEC_A;
    // LD_hli_A;  // BATTLEANIMSTRUCT_FRAME
    bc->frame = (uint8_t)-1;
    // XOR_A_A;
    // LD_hli_A;  // BATTLEANIMSTRUCT_JUMPTABLE_INDEX
    bc->jumptableIndex = 0;
    // LD_hli_A;  // BATTLEANIMSTRUCT_VAR1
    bc->var1 = 0;
    // LD_hl_A;  // BATTLEANIMSTRUCT_VAR2
    bc->var2 = 0;
    // RET;
}

// #define delanim_command 0xFC
// #define dowait_command 0xFD

void BattleAnimOAMUpdate(struct BattleAnim* bc){
    // CALL(aInitBattleAnimBuffer);
    InitBattleAnimBuffer(bc);
    // CALL(aGetBattleAnimFrame);
    uint8_t a = GetBattleAnimFrame(bc);
    // CP_A(dowait_command);
    // JP_Z (mBattleAnimOAMUpdate_done);
    if(a == dowait_command)
        return;
    // CP_A(delanim_command);
    // JP_Z (mBattleAnimOAMUpdate_delete);
    if(a == delanim_command) {
    // delete:
        // CALL(aDeinitBattleAnimation);
        DeinitBattleAnimation(bc);

    // done:
        // AND_A_A;
        // RET;
        return;
    }

    // PUSH_AF;
    // LD_HL(wBattleAnimTempOAMFlags);
    // LD_A_addr(wBattleAnimTempFrameOAMFlags);
    // XOR_A_hl;
    // AND_A(PRIORITY | Y_FLIP | X_FLIP);
    // LD_hl_A;
    BattleAnimationRenderState()->oamFlags = (BattleAnimationRenderState()->oamFlags ^ BattleAnimationRenderState()->frameOamFlags) & (PRIORITY | Y_FLIP | X_FLIP);
    // POP_AF;

    // PUSH_BC;
    // CALL(aGetBattleAnimOAMPointer);
    const struct BattleOAMData* oam = GetBattleAnimOAMPointer(a);
    // LD_A_addr(wBattleAnimTempTileID);
    // ADD_A_hl;  // tile offset
    // LD_addr_A(wBattleAnimTempTileID);
    int32_t frameTileId = (int32_t)BattleAnimationRenderState()->tileId + (int8_t)oam->vtile_offset;
    if(frameTileId < 0 || frameTileId > UINT16_MAX)
        abort();
    BattleAnimationRenderState()->tileId = (uint16_t)frameTileId;
    // INC_HL;
    // LD_A_hli;  // oam data length
    // LD_C_A;
    uint8_t c = oam->data_length;
    // LD_A_hli;  // oam data pointer
    // LD_H_hl;
    // LD_L_A;
    const uint8_t* hl = oam->ptr;
    do {
    // loop:
    // Y Coord
        // LD_A_addr(wBattleAnimTempYCoord);
        // LD_B_A;
        // LD_A_addr(wBattleAnimTempYOffset);
        // ADD_A_B;
        // LD_B_A;
        int16_t b = (int16_t)BattleAnimationRenderState()->yCoord +
            BattleAnimationRenderState()->yOffset;
        // PUSH_HL;
        // LD_A_hl;
        int16_t y = *(hl++);
        // LD_HL(wBattleAnimTempOAMFlags);
        // BIT_hl(OAM_Y_FLIP);
        // IF_Z goto no_yflip;
        if(bit_test(BattleAnimationRenderState()->oamFlags, OAM_Y_FLIP)) {
            // ADD_A(0x8);
            // XOR_A(0xff);
            // INC_A;
            y = -(y + TILE_WIDTH);
        }

    // no_yflip:
        // POP_HL;
        // ADD_A_B;
        // LD_de_A;
        struct BattleAnimationSprite* sprite = AppendBattleAnimationRenderSprite();
        sprite->yCoord = y + b - LEGACY_OBJECT_Y_ORIGIN;

    // X Coord
        // INC_HL;
        // INC_DE;
        // LD_A_addr(wBattleAnimTempXCoord);
        // LD_B_A;
        // LD_A_addr(wBattleAnimTempXOffset);
        // ADD_A_B;
        // LD_B_A;
        b = (int16_t)BattleAnimationRenderState()->xCoord +
            BattleAnimationRenderState()->xOffset;
        // PUSH_HL;
        // LD_A_hl;
        int16_t x = *(hl++);
        // LD_HL(wBattleAnimTempOAMFlags);
        // BIT_hl(OAM_X_FLIP);
        // IF_Z goto no_xflip;
        if(bit_test(BattleAnimationRenderState()->oamFlags, OAM_X_FLIP)) {
            // ADD_A(0x8);
            // XOR_A(0xff);
            // INC_A;
            x = -(x + TILE_WIDTH);
        }

    // no_xflip:
        // POP_HL;
        // ADD_A_B;
        // LD_de_A;
        sprite->xCoord = x + b - LEGACY_OBJECT_X_ORIGIN;

    // Tile ID
        // INC_HL;
        // INC_DE;
        uint8_t tileID = *(hl++);
        // LD_A_addr(wBattleAnimTempTileID);
        // ADD_A(BATTLEANIM_BASE_TILE);
        // ADD_A_hl;
        // LD_de_A;
        sprite->tileId = BattleAnimationRenderState()->tileId + BATTLEANIM_BASE_TILE + tileID;
        sprite->resourceTileId = sprite->tileId;
        sprite->legacyOamTilePair = true;

    // Attributes
        // INC_HL;
        // INC_DE;
        // LD_A_addr(wBattleAnimTempOAMFlags);
        // LD_B_A;
        // LD_A_hl;
        uint8_t attr = *(hl++);
        // XOR_A_B;
        // AND_A(PRIORITY | Y_FLIP | X_FLIP);
        // LD_B_A;
        // LD_A_hl;
        // AND_A(OBP_NUM);
        // OR_A_B;
        // LD_B_A;
        // LD_A_addr(wBattleAnimTempPalette);
        // AND_A(PALETTE_MASK | VRAM_BANK_1);
        // OR_A_B;
        // LD_de_A;
        sprite->attributes = ((attr ^ BattleAnimationRenderState()->oamFlags) & (PRIORITY | Y_FLIP | X_FLIP)) | (attr & OBP_NUM) | (BattleAnimationRenderState()->palette & (PALETTE_MASK | VRAM_BANK_1));
        sprite->resourceKind = BATTLE_RENDER_RESOURCE_ANIMATION;

        // INC_HL;
        // INC_DE;
        // Native render storage grows as required; no OAM-capacity exit.
        // DEC_C;
        // IF_NZ goto loop;
    } while(--c != 0);
    // POP_BC;
    // goto done;

// delete:
    // CALL(aDeinitBattleAnimation);

// done:
    // AND_A_A;
    // RET;
    return;
}

static void InitBattleAnimBuffer(struct BattleAnim* bc){
    // LD_HL(BATTLEANIMSTRUCT_OAMFLAGS);
    // ADD_HL_BC;
    // LD_A_hl;

    // AND_A(PRIORITY);
    // LD_addr_A(wBattleAnimTempOAMFlags);
    BattleAnimationRenderState()->oamFlags = bc->oamFlags & PRIORITY;
    // XOR_A_A;
    // LD_addr_A(wBattleAnimTempFrameOAMFlags);
    BattleAnimationRenderState()->frameOamFlags = 0;
    // LD_HL(BATTLEANIMSTRUCT_PALETTE);
    // ADD_HL_BC;
    // LD_A_hl;
    // LD_addr_A(wBattleAnimTempPalette);
    BattleAnimationRenderState()->palette = bc->palette;
    // LD_HL(BATTLEANIMSTRUCT_FIX_Y);
    // ADD_HL_BC;
    // LD_A_hl;
    // LD_addr_A(wBattleAnimTempFixY);
    BattleAnimationRenderState()->fixY = bc->fixY;
    // LD_HL(BATTLEANIMSTRUCT_TILEID);
    // ADD_HL_BC;
    // LD_A_hli;
    // LD_addr_A(wBattleAnimTempTileID);
    BattleAnimationRenderState()->tileId = bc->tileId;
    // LD_A_hli;
    // LD_addr_A(wBattleAnimTempXCoord);
    BattleAnimationRenderState()->xCoord = bc->xCoord;
    // LD_A_hli;
    // LD_addr_A(wBattleAnimTempYCoord);
    BattleAnimationRenderState()->yCoord = bc->yCoord;
    // LD_A_hli;
    // LD_addr_A(wBattleAnimTempXOffset);
    BattleAnimationRenderState()->xOffset = bc->xOffset;
    // LD_A_hli;
    // LD_addr_A(wBattleAnimTempYOffset);
    BattleAnimationRenderState()->yOffset = bc->yOffset;

    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // RET_Z ;
    if(hram.hBattleTurn == TURN_PLAYER)
        return;

    // LD_HL(BATTLEANIMSTRUCT_OAMFLAGS);
    // ADD_HL_BC;
    // LD_A_hl;
    // LD_addr_A(wBattleAnimTempOAMFlags);
    BattleAnimationRenderState()->oamFlags = bc->oamFlags;
    // BIT_hl(0);
    // RET_Z ;
    if(!bit_test(bc->oamFlags, 0))
        return;

    // LD_HL(BATTLEANIMSTRUCT_XCOORD);
    // ADD_HL_BC;
    // LD_A_hli;
    // LD_D_A;
    // LD_A((-10 * 8) + 4);
    // SUB_A_D;
    // LD_addr_A(wBattleAnimTempXCoord);
    BattleAnimationRenderState()->xCoord = (uint8_t)(((-10 * 8) + 4) - bc->xCoord);
    // LD_A_hli;
    // LD_D_A;
    // LD_A_addr(wBattleAnimTempFixY);
    // CP_A(0xff);
    // IF_NZ goto check_kinesis_softboiled_milkdrink;
    if(BattleAnimationRenderState()->fixY != 0xff) {
    // check_kinesis_softboiled_milkdrink:
        // SUB_A_D;
        // PUSH_AF;
        uint8_t d = BattleAnimationRenderState()->fixY - bc->yCoord;
        // LD_A_addr(wFXAnimID + 1);
        // OR_A_A;
        // IF_NZ goto no_sub;
        // LD_A_addr(wFXAnimID);
        // CP_A(KINESIS);
        // IF_Z goto do_sub;
        // CP_A(SOFTBOILED);
        // IF_Z goto do_sub;
        // CP_A(MILK_DRINK);
        // IF_NZ goto no_sub;
        if(BattleAnimationIdGet() == KINESIS || BattleAnimationIdGet() == SOFTBOILED || BattleAnimationIdGet() == MILK_DRINK) {
        // do_sub:
            // POP_AF;
            // SUB_A(1 * 8);
            BattleAnimationRenderState()->yCoord = d - (1 * 8);
            // goto done;
        }
        else {
        // no_sub:
            // POP_AF;
            BattleAnimationRenderState()->yCoord = d;
        }
    }
    else {
        // LD_A(5 * 8);
        // ADD_A_D;
        // goto done;
        BattleAnimationRenderState()->yCoord = bc->yCoord + (5 * 8);
    }

// done:
    // LD_addr_A(wBattleAnimTempYCoord);
    // LD_A_hli;
    // XOR_A(0xff);
    // INC_A;
    // LD_addr_A(wBattleAnimTempXOffset);
    BattleAnimationRenderState()->xOffset = -bc->xOffset;
    // RET;
}


void v_ExecuteBGEffects(void){
    // CALLFAR(aExecuteBGEffects);
    // RET;
    ExecuteBGEffects();
}

bool v_QueueBGEffect(void){
    // CALLFAR(aQueueBGEffect);
    // RET;
    return QueueBGEffect();
}
