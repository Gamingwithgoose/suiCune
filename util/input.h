#pragma once

#include <stdint.h>

// Keep SDL out of this public header. constants.h defines a legacy `percent`
// macro, and including SDL after constants.h lets that macro rewrite identifiers
// inside SDL/Windows headers. Only a pointer to SDL_Event is needed here.
typedef union SDL_Event SDL_Event;

void init_input_mapping(void);
void handle_input_event(const SDL_Event* e);
uint8_t NativeInputHeld(void);
uint8_t NativeInputPressed(void);
uint8_t NativeInputReleased(void);
uint8_t NativeInputPressesSinceClear(void);
void NativeInputAdvanceFrame(void);
void NativeInputClearFrame(void);
void NativeInputClearPressesSinceClear(void);
void NativeInputOverridePressed(uint8_t pressed);
uint8_t NativeInputLogicalHeld(void);
uint8_t NativeInputLogicalPressed(void);
uint8_t NativeInputLogicalReleased(void);
uint8_t NativeInputLogicalLast(void);
void NativeInputAdvanceLogicalFrame(uint8_t held);
void NativeInputSetAutoLogicalFrame(uint8_t held);
void NativeInputClearLogicalFrame(void);
void NativeInputClearLogicalPressed(void);
void NativeInputClearLogicalHeld(void);
void NativeInputClearLogicalLast(void);
void NativeInputClearLogicalState(void);
void NativeInputSetLogicalLast(uint8_t input);