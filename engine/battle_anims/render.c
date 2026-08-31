#include "render.h"
#include "core.h"

static void RenderBattler(const struct BattleSceneRenderLine* line,
    const struct BattleSceneBattlerView* battler) {
    if(battler == NULL || !battler->visible || battler->pixels == NULL)
        return;
    for(size_t tileIndex = 0; tileIndex < battler->tileCount; tileIndex++) {
        const struct BattleSceneBattlerTile* tile = &battler->tiles[tileIndex];
        if(tile->imageTile >= battler->pixelTileCount)
            continue;
        int16_t y = tile->y - BattleSceneVerticalOffsetForLine(line->line);
        int16_t x = tile->x - BattleSceneHorizontalOffsetForLine(line->line);
        int py = line->line - y;
        if(py < 0 || py >= TILE_WIDTH)
            continue;
        const uint8_t* source = battler->pixels + (size_t)tile->imageTile * LEN_2BPP_TILE;
        uint8_t low = source[py * 2];
        uint8_t high = source[py * 2 + 1];
        for(uint8_t bit = 0; bit < TILE_WIDTH; bit++) {
            int displayX = x + (TILE_WIDTH - 1 - bit);
            if(displayX >= 0 && displayX < SCREEN_WIDTH) {
                uint8_t color = (low & 1) | ((high & 1) << 1);
                line->pixels[displayX] = line->colorMode
                    ? (battler->palette << 2) + color
                    : line->monochromeBackgroundPalette[color] | line->monochromeBackgroundMask;
                line->priority[displayX] = 0;
            }
            low >>= 1;
            high >>= 1;
        }
    }
}

void RenderBattleSceneBattlers(const struct BattleSceneRenderLine* line) {
    if(line == NULL || line->pixels == NULL || line->priority == NULL ||
        line->monochromeBackgroundPalette == NULL)
        return;
    for(uint8_t battler = BATTLE_SCENE_BATTLER_PLAYER;
        battler < BATTLE_SCENE_BATTLER_COUNT; battler++) {
        RenderBattler(line, BattleSceneBattler(battler));
    }
}

void RenderBattleSceneSprites(const struct BattleSceneRenderLine* line,
    bool largeSprites, BattleSceneSpriteDrawFn drawSprite, void* context) {
    if(line == NULL || drawSprite == NULL)
        return;
    size_t count;
    const struct BattleAnimationSprite* sprites = BattleSceneSprites(&count);
    for(uint8_t layer = BATTLE_SCENE_LAYER_BASELINE; layer < BATTLE_SCENE_LAYER_COUNT; layer++) {
        for(size_t index = count; index != 0; index--) {
            const struct BattleAnimationSprite* sprite = &sprites[index - 1];
            if(sprite->layer != layer)
                continue;
            struct BattleAnimationSprite pixelSource = *sprite;
            if(sprite->resourceKind == BATTLE_RENDER_RESOURCE_ANIMATION)
                pixelSource.tileId &= largeSprites ? 0xfffe : 0xffff;
            drawSprite(context, line->pixels, line->priority, sprite->yCoord,
                sprite->xCoord, sprite->tileId, sprite->attributes,
                BattleAnimationSpritePixels(&pixelSource));
        }
    }
}
