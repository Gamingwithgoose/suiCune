#pragma once

#include <SDL2/SDL.h>
#include <stdint.h>

void init_input_mapping(void);
void handle_input_event(const SDL_Event* e);
uint8_t NativeInputHeld(void);
