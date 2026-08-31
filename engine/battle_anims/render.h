#ifndef SUICUNE_ENGINE_BATTLE_ANIMS_RENDER_H
#define SUICUNE_ENGINE_BATTLE_ANIMS_RENDER_H

#include <stdbool.h>
#include <stdint.h>

struct BattleSceneRenderLine {
    uint8_t* pixels;
    uint8_t* priority;
    uint8_t line;
    bool colorMode;
    const uint8_t* monochromeBackgroundPalette;
    uint8_t monochromeBackgroundMask;
};

typedef void (*BattleSceneSpriteDrawFn)(void* context, uint8_t* pixels,
    uint8_t* priority, int16_t y, int16_t x, uint16_t tile, uint8_t attributes,
    const uint8_t* tilePixels);

void RenderBattleSceneBattlers(const struct BattleSceneRenderLine* line);
void RenderBattleSceneSprites(const struct BattleSceneRenderLine* line,
    bool largeSprites, BattleSceneSpriteDrawFn drawSprite, void* context);

#endif
