#include "../../constants.h"
#include "core.h"
#include "helpers.h"
#include "bg_effects.h"
#include "../../data/battle_anims/objects.h"
#include <stdlib.h>
#include <string.h>

static void InitBattleAnimation(struct BattleAnim* bc);
static void InitBattleAnimBuffer(struct BattleAnim* bc);

struct NativeBattleAnimationState {
    struct BattleAnimationTileBinding* tileBindings;
    size_t tileBindingCount;
    size_t tileBindingCapacity;
    uint8_t* tilePixels;
    size_t tilePixelCapacity;
    uint8_t* hudTilePixels;
    size_t hudTilePixelCapacity;
    struct BattleAnimationCommandState command;
    struct BattleAnimationRenderState render;
    struct BattleAnimationEffectScratchState effectScratch;
    struct BattleAnimationSprite* renderSprites;
    size_t renderSpriteCount;
    size_t renderSpriteCapacity;
    struct BattleAnimationSprite* hudSprites;
    size_t hudSpriteCount;
    size_t hudSpriteCapacity;
    struct BattleAnim* objects;
    size_t objectCount;
    size_t objectCapacity;
    struct BattleBGEffect* bgEffects;
    size_t bgEffectCount;
    size_t bgEffectCapacity;
    uint8_t lastObjectIndex;
};

static struct NativeBattleAnimationState sBattleAnimationState;

struct BattleAnim* BattleAnimationObjects(void){
    return sBattleAnimationState.objects;
}

size_t BattleAnimationObjectCount(void){
    return sBattleAnimationState.objectCount;
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
            abort();
        sBattleAnimationState.bgEffects = effects;
        sBattleAnimationState.bgEffectCapacity = newCapacity;
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
            abort();
        sBattleAnimationState.tileBindings = bindings;
        sBattleAnimationState.tileBindingCapacity = newCapacity;
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

static uint8_t* BattleRenderTileWritePointer(uint8_t** pixels, size_t* capacity, uint16_t tileId, size_t tileCount){
    if(tileCount > SIZE_MAX - (size_t)tileId)
        abort();
    size_t requiredCapacity = (size_t)tileId + tileCount;
    if(requiredCapacity == 0)
        requiredCapacity = 1;
    if(requiredCapacity > *capacity) {
        if(requiredCapacity > SIZE_MAX / LEN_2BPP_TILE)
            abort();
        uint8_t* resizedPixels = realloc(*pixels, requiredCapacity * LEN_2BPP_TILE);
        if(resizedPixels == NULL)
            abort();
        memset(resizedPixels + *capacity * LEN_2BPP_TILE, 0,
            (requiredCapacity - *capacity) * LEN_2BPP_TILE);
        *pixels = resizedPixels;
        *capacity = requiredCapacity;
    }
    return *pixels + (size_t)tileId * LEN_2BPP_TILE;
}

uint8_t* BattleAnimationTileWritePointer(uint16_t tileId, size_t tileCount){
    return BattleRenderTileWritePointer(&sBattleAnimationState.tilePixels,
        &sBattleAnimationState.tilePixelCapacity, tileId, tileCount);
}

uint8_t* BattleAnimationHudTileWritePointer(uint16_t tileId, size_t tileCount){
    return BattleRenderTileWritePointer(&sBattleAnimationState.hudTilePixels,
        &sBattleAnimationState.hudTilePixelCapacity, tileId, tileCount);
}

const uint8_t* BattleAnimationSpritePixels(const struct BattleAnimationSprite* sprite){
    const uint8_t* pixels;
    size_t capacity;
    if(sprite->resourceKind == BATTLE_RENDER_RESOURCE_HUD) {
        pixels = sBattleAnimationState.hudTilePixels;
        capacity = sBattleAnimationState.hudTilePixelCapacity;
    }
    else {
        pixels = sBattleAnimationState.tilePixels;
        capacity = sBattleAnimationState.tilePixelCapacity;
    }
    if(sprite->tileId >= capacity)
        return NULL;
    return pixels + (size_t)sprite->tileId * LEN_2BPP_TILE;
}

struct BattleAnimationCommandState* BattleAnimationCommandState(void){
    return &sBattleAnimationState.command;
}

struct BattleAnimationRenderState* BattleAnimationRenderState(void){
    return &sBattleAnimationState.render;
}

struct BattleAnimationEffectScratchState* BattleAnimationEffectScratchState(void){
    return &sBattleAnimationState.effectScratch;
}

const struct BattleAnimationSprite* BattleAnimationRenderSprites(size_t* count){
    *count = sBattleAnimationState.renderSpriteCount;
    return sBattleAnimationState.renderSprites;
}

const struct BattleAnimationSprite* BattleAnimationHudSprites(size_t* count){
    *count = sBattleAnimationState.hudSpriteCount;
    return sBattleAnimationState.hudSprites;
}

void ClearBattleAnimationHudSprites(void){
    free(sBattleAnimationState.hudTilePixels);
    free(sBattleAnimationState.hudSprites);
    sBattleAnimationState.hudTilePixels = NULL;
    sBattleAnimationState.hudTilePixelCapacity = 0;
    sBattleAnimationState.hudSprites = NULL;
    sBattleAnimationState.hudSpriteCount = 0;
    sBattleAnimationState.hudSpriteCapacity = 0;
}

void SetBattleAnimationHudSprites(size_t firstSprite, uint8_t y, uint8_t x, int8_t direction,
    const uint16_t* tileIds, size_t tileCount){
    if(tileCount > SIZE_MAX - firstSprite)
        abort();
    size_t requiredCapacity = firstSprite + tileCount;
    if(requiredCapacity > sBattleAnimationState.hudSpriteCapacity) {
        size_t newCapacity = sBattleAnimationState.hudSpriteCapacity == 0 ? 8 : sBattleAnimationState.hudSpriteCapacity;
        while(newCapacity < requiredCapacity) {
            if(newCapacity > SIZE_MAX / 2)
                abort();
            newCapacity *= 2;
        }
        if(newCapacity > SIZE_MAX / sizeof(*sBattleAnimationState.hudSprites))
            abort();
        struct BattleAnimationSprite* sprites = realloc(sBattleAnimationState.hudSprites,
            newCapacity * sizeof(*sprites));
        if(sprites == NULL)
            abort();
        sBattleAnimationState.hudSprites = sprites;
        sBattleAnimationState.hudSpriteCapacity = newCapacity;
    }
    for(size_t i = 0; i < tileCount; i++) {
        struct BattleAnimationSprite* sprite = &sBattleAnimationState.hudSprites[firstSprite + i];
        sprite->yCoord = y;
        sprite->xCoord = x;
        sprite->tileId = tileIds[i];
        sprite->attributes = PAL_BATTLE_OB_YELLOW;
        sprite->resourceKind = BATTLE_RENDER_RESOURCE_HUD;
        x += direction;
    }
    if(sBattleAnimationState.hudSpriteCount < requiredCapacity)
        sBattleAnimationState.hudSpriteCount = requiredCapacity;
}

void BeginBattleAnimationRenderFrame(void){
    sBattleAnimationState.renderSpriteCount = 0;
}

void ClearBattleAnimationRenderSprites(void){
    sBattleAnimationState.renderSpriteCount = 0;
}

void SetBattleAnimationRenderSpritePalette(uint8_t paletteMask){
    for(size_t i = 0; i < sBattleAnimationState.renderSpriteCount; i++)
        sBattleAnimationState.renderSprites[i].attributes &= paletteMask;
}

static struct BattleAnimationSprite* AppendBattleAnimationRenderSprite(void){
    if(sBattleAnimationState.renderSpriteCount == sBattleAnimationState.renderSpriteCapacity) {
        size_t newCapacity = sBattleAnimationState.renderSpriteCapacity == 0 ? 64 : sBattleAnimationState.renderSpriteCapacity * 2;
        if(newCapacity < sBattleAnimationState.renderSpriteCapacity || newCapacity > SIZE_MAX / sizeof(*sBattleAnimationState.renderSprites))
            abort();
        struct BattleAnimationSprite* sprites = realloc(sBattleAnimationState.renderSprites, newCapacity * sizeof(*sprites));
        if(sprites == NULL)
            abort();
        sBattleAnimationState.renderSprites = sprites;
        sBattleAnimationState.renderSpriteCapacity = newCapacity;
    }
    return &sBattleAnimationState.renderSprites[sBattleAnimationState.renderSpriteCount++];
}

void ResetNativeBattleAnimationState(void){
    uint8_t* hudTilePixels = sBattleAnimationState.hudTilePixels;
    size_t hudTilePixelCapacity = sBattleAnimationState.hudTilePixelCapacity;
    struct BattleAnimationSprite* hudSprites = sBattleAnimationState.hudSprites;
    size_t hudSpriteCount = sBattleAnimationState.hudSpriteCount;
    size_t hudSpriteCapacity = sBattleAnimationState.hudSpriteCapacity;

    free(sBattleAnimationState.renderSprites);
    free(sBattleAnimationState.tileBindings);
    free(sBattleAnimationState.tilePixels);
    free(sBattleAnimationState.objects);
    free(sBattleAnimationState.bgEffects);
    memset(&sBattleAnimationState, 0, sizeof(sBattleAnimationState));
    // Trainer HUD resources are battle-view state, not transient animation
    // state. Animation setup clears must not erase them mid-battle.
    sBattleAnimationState.hudTilePixels = hudTilePixels;
    sBattleAnimationState.hudTilePixelCapacity = hudTilePixelCapacity;
    sBattleAnimationState.hudSprites = hudSprites;
    sBattleAnimationState.hudSpriteCount = hudSpriteCount;
    sBattleAnimationState.hudSpriteCapacity = hudSpriteCapacity;
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
            abort();
        sBattleAnimationState.objects = objects;
        sBattleAnimationState.objectCapacity = newCapacity;
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
        uint8_t b = BattleAnimationRenderState()->yCoord + BattleAnimationRenderState()->yOffset;
        // PUSH_HL;
        // LD_A_hl;
        uint8_t y = *(hl++);
        // LD_HL(wBattleAnimTempOAMFlags);
        // BIT_hl(OAM_Y_FLIP);
        // IF_Z goto no_yflip;
        if(bit_test(BattleAnimationRenderState()->oamFlags, OAM_Y_FLIP)) {
            // ADD_A(0x8);
            // XOR_A(0xff);
            // INC_A;
            y = -(y + 0x8);
        }

    // no_yflip:
        // POP_HL;
        // ADD_A_B;
        // LD_de_A;
        struct BattleAnimationSprite* sprite = AppendBattleAnimationRenderSprite();
        sprite->yCoord = y + b;

    // X Coord
        // INC_HL;
        // INC_DE;
        // LD_A_addr(wBattleAnimTempXCoord);
        // LD_B_A;
        // LD_A_addr(wBattleAnimTempXOffset);
        // ADD_A_B;
        // LD_B_A;
        b = BattleAnimationRenderState()->xCoord + BattleAnimationRenderState()->xOffset;
        // PUSH_HL;
        // LD_A_hl;
        uint8_t x = *(hl++);
        // LD_HL(wBattleAnimTempOAMFlags);
        // BIT_hl(OAM_X_FLIP);
        // IF_Z goto no_xflip;
        if(bit_test(BattleAnimationRenderState()->oamFlags, OAM_X_FLIP)) {
            // ADD_A(0x8);
            // XOR_A(0xff);
            // INC_A;
            x = -(x + 0x8);
        }

    // no_xflip:
        // POP_HL;
        // ADD_A_B;
        // LD_de_A;
        sprite->xCoord = x + b;

    // Tile ID
        // INC_HL;
        // INC_DE;
        uint8_t tileID = *(hl++);
        // LD_A_addr(wBattleAnimTempTileID);
        // ADD_A(BATTLEANIM_BASE_TILE);
        // ADD_A_hl;
        // LD_de_A;
        sprite->tileId = BattleAnimationRenderState()->tileId + BATTLEANIM_BASE_TILE + tileID;

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
        if(wram->wFXAnimID == KINESIS || wram->wFXAnimID == SOFTBOILED || wram->wFXAnimID == MILK_DRINK) {
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
