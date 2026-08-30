#include "../constants.h"
#include "joypad.h"
#include "delay.h"
#include "audio.h"
#include "tilemap.h"
#include "../charmap.h"
#include "time_palettes.h"
#include "../engine/events/catch_tutorial_input.h"
#include "../util/input.h"

struct AutoInputState {
    const uint8_t* cursor;
    uint8_t framesRemaining;
    bool active;
};

static struct AutoInputState s_autoInput;

static void SyncLegacyLogicalJoypadMirrors(void) {
    hram.hJoyReleased = NativeInputLogicalReleased();
    hram.hJoyPressed = NativeInputLogicalPressed();
    hram.hJoyDown = NativeInputLogicalHeld();
    hram.hJoyLast = NativeInputLogicalLast();
}

void Joypad(void) {
        //  Replaced by UpdateJoypad, called from VBlank instead of the useless
    //  joypad interrupt.

    //  This is a placeholder in case the interrupt is somehow enabled.
    // RET;
}

void ClearJoypad(void) {
    NativeInputClearLogicalPressed();
    NativeInputClearLogicalHeld();
    SyncLegacyLogicalJoypadMirrors();
}

void ResetJoypadInput(void) {
    NativeInputClearLogicalState();
    SyncLegacyLogicalJoypadMirrors();
}

void ClearJoypadPressed(void) {
    NativeInputClearLogicalPressed();
    SyncLegacyLogicalJoypadMirrors();
}

void ClearJoypadHeld(void) {
    NativeInputClearLogicalHeld();
    SyncLegacyLogicalJoypadMirrors();
}

void ClearJoypadLast(void) {
    NativeInputClearLogicalLast();
    SyncLegacyLogicalJoypadMirrors();
}

// This is called automatically every frame in VBlank. SDL input arrives as
// native action bits; legacy HRAM mirrors remain until their C consumers are
// migrated to native input state.
void UpdateJoypad(void) {
    //  Updates:

    //  hJoypadReleased: released this frame (delta)
    //  hJoypadPressed: pressed this frame (delta)
    //  hJoypadDown: currently pressed
    //  hJoypadSum: pressed so far

    //  Any of these three bits can be used to disable input.
    uint8_t disable = wram->wJoypadDisable;
    if((disable & ((1 << JOYPAD_DISABLE_MON_FAINT_F) | (1 << JOYPAD_DISABLE_SGB_TRANSFER_F) | (1 << 4))) != 0)
        return;

    //  If we're saving, input is disabled.
    if(wram->wGameLogicPaused)
        return;

    NativeInputAdvanceFrame();
    hram.hJoypadReleased = NativeInputReleased();
    hram.hJoypadPressed = NativeInputPressed();
    hram.hJoypadSum = NativeInputPressesSinceClear();
    hram.hJoypadDown = NativeInputHeld();

    //  Now that we have the input, we can do stuff with it.

    //  For example, soft reset:
    // AND_A(A_BUTTON | B_BUTTON | SELECT | START);
    // CP_A(A_BUTTON | B_BUTTON | SELECT | START);
    // JP_Z (mReset);

    // RET;
}

//  Update mirror joypad input from hJoypadDown (real input)
void GetJoypad(void) {
    //  hJoyReleased: released this frame (delta)
    //  hJoyPressed: pressed this frame (delta)
    //  hJoyDown: currently pressed

    //  bit 0 A
    //      1 B
    //      2 SELECT
    //      3 START
    //      4 RIGHT
    //      5 LEFT
    //      6 UP
    //      7 DOWN

    //PUSH_AF;
    //PUSH_HL;
    //PUSH_DE;
    //PUSH_BC;

    //  The player input can be automated using an input stream.
    //  See more below.
    if(s_autoInput.active)
    {
        //  Use a predetermined input stream (used in the catching tutorial).
        //  Stream format: [input][duration]
        //  A value of $ff will immediately end the stream.

        //  Read from the input stream.
        const uint8_t* hl = s_autoInput.cursor;

        //  We only update when the input duration has expired.
        uint8_t len = s_autoInput.framesRemaining;
        if(len == 0)
        {
            //  An input of $ff will end the stream.
            uint8_t input = *(hl++);
            if(input == 0xFF) 
            {
                StopAutoInput();
                input = NO_INPUT;
            }
            else 
            {
                //  A duration of $ff will end the stream indefinitely.
                uint8_t duration = *(hl++);
                s_autoInput.framesRemaining = duration;
                if(duration != 0xFF)
                {
                    //  On to the next input...
                    s_autoInput.cursor = hl;
                }
                else 
                {
                    //  The current input is overwritten.
                    hl -= 2;
                    input = NO_INPUT;
                }
            }
            NativeInputSetAutoLogicalFrame(input);
        }
        else 
        {
            //  Until then, don't change anything.
            s_autoInput.framesRemaining = --len;
        }
        SyncLegacyLogicalJoypadMirrors();
        return;
    }
    else 
    {

        //  To get deltas, take this and last frame's input.
        uint8_t real_input = NativeInputHeld();
        //LDH_A_addr(hJoypadDown);  // real input
        //LD_B_A;
        //LDH_A_addr(hJoyDown);  // last frame mirror
        //LD_E_A;

        //  Released this frame:
        //XOR_A_B;
        //LD_D_A;
        //AND_A_E;
        //LDH_addr_A(hJoyReleased);

        //  Pressed this frame:
        //LD_A_D;
        //AND_A_B;
        //LDH_addr_A(hJoyPressed);
        NativeInputAdvanceLogicalFrame(real_input);
        SyncLegacyLogicalJoypadMirrors();
        //LD_A_B;
        //LDH_addr_A(hJoyDown);  // frame input
    }
    //POP_BC;
    //POP_DE;
    //POP_HL;
    //POP_AF;
    //RET;

    //return StartAutoInput();
}

//  Start reading automated input stream at hl.
void StartAutoInput(const uint8_t* hl) {
    s_autoInput.cursor = hl;
    //  Start reading the stream immediately.
    s_autoInput.framesRemaining = 0;
    //  Reset input mirrors.
    NativeInputClearLogicalFrame();
    SyncLegacyLogicalJoypadMirrors();

    s_autoInput.active = true;
}

void StopAutoInput(void) {
    //  Clear variables related to automated input.
    s_autoInput.cursor = NULL;
    s_autoInput.framesRemaining = 0;
    //  Back to normal input.
    s_autoInput.active = false;
}

bool IsAutoInputActive(void) {
    return s_autoInput.active;
}

//  //  unreferenced
void JoyTitleScreenInput(void) {
// loop:
    // CALL(aDelayFrame);

    // PUSH_BC;
    // CALL(aJoyTextDelay);
    // POP_BC;

    //  Save data can be deleted by pressing Up + B + Select.
    // LDH_A_addr(hJoyDown);
    // CP_A(D_UP | SELECT | B_BUTTON);
    // IF_Z goto keycombo;

    //  Press Start or A to start the game.
    // LDH_A_addr(hJoyLast);
    // AND_A(START | A_BUTTON);
    // IF_NZ goto keycombo;

    // DEC_C;
    // IF_NZ goto loop;

    // AND_A_A;
    // RET;

// keycombo:
    // SCF;
    // RET;
}

void JoyWaitAorB(void) {
    do {
        // CALL(aDelayFrame);
        DelayFrame();

        // CALL(aGetJoypad);
        GetJoypad();

        // LDH_A_addr(hJoyPressed);
        // AND_A(A_BUTTON | B_BUTTON);
        // RET_NZ;
        if((NativeInputLogicalPressed() & (A_BUTTON | B_BUTTON)) != 0)
            break;
        
        // CALL(aUpdateTimeAndPals);
        UpdateTimeAndPals();

        // goto loop;
    } while(1);
}

void WaitButton(void) {
    // LDH_A_addr(hOAMUpdate);
    // PUSH_AF;
    uint8_t tempOAMUpdate = hram.hOAMUpdate;

    // LD_A(1);
    // LDH_addr_A(hOAMUpdate);
    hram.hOAMUpdate = 1;

    // CALL(aWaitBGMap);
    WaitBGMap();

    // CALL(aJoyWaitAorB);
    JoyWaitAorB();

    // POP_AF;
    // LDH_addr_A(hOAMUpdate);
    hram.hOAMUpdate = tempOAMUpdate;
    
    // RET;
}

void JoyTextDelay(void) {
    // CALL(aGetJoypad);
    GetJoypad();

    // LDH_A_addr(hInMenu);
    // AND_A_A;
    // LDH_A_addr(hJoyPressed);
    // IF_Z goto ok;
    if(hram.hInMenu != 0)
    {
        // LDH_A_addr(hJoyDown);
        // LDH_addr_A(hJoyLast);
        NativeInputSetLogicalLast(NativeInputLogicalHeld());
    }
    else 
    {
        // LDH_addr_A(hJoyLast);
        NativeInputSetLogicalLast(NativeInputLogicalPressed());
    }

    // LDH_A_addr(hJoyPressed);
    // AND_A_A;
    // IF_Z goto checkframedelay;
    if(NativeInputLogicalPressed() == 0)
    {
// checkframedelay:
        // LD_A_addr(wTextDelayFrames);
        // AND_A_A;
        // IF_Z goto restartframedelay;
        if(wram->wTextDelayFrames == 0)
        {
    // restartframedelay:
            // LD_A(5);
            // LD_addr_A(wTextDelayFrames);
            wram->wTextDelayFrames = 5;
            SyncLegacyLogicalJoypadMirrors();

            // RET;
            return;
        }

        // XOR_A_A;
        // LDH_addr_A(hJoyLast);
        NativeInputClearLogicalLast();
        SyncLegacyLogicalJoypadMirrors();

        // RET;
        return;
    }
    
    // LD_A(15);
    // LD_addr_A(wTextDelayFrames);
    wram->wTextDelayFrames = 15;
    SyncLegacyLogicalJoypadMirrors();

    // RET;
    return;
}

//  Show a blinking cursor in the lower right-hand
//  corner of a textbox and wait until A or B is
//  pressed.
//
//  NOTE: The cursor has to be shown before calling
//  this function or no cursor will be shown at all.
void WaitPressAorB_BlinkCursor(void) {
    // LDH_A_addr(hMapObjectIndex);
    // PUSH_AF;
    uint8_t mapObjIdx = hram.hMapObjectIndex;

    // LDH_A_addr(hObjectStructIndex);
    // PUSH_AF;
    uint8_t objStructIdx = hram.hObjectStructIndex;

    // XOR_A_A;
    // LDH_addr_A(hMapObjectIndex);
    hram.hMapObjectIndex = 0;

    // LD_A(6);
    // LDH_addr_A(hObjectStructIndex);
    hram.hObjectStructIndex = 6;

    do {
        DelayFrame();
        // PUSH_HL;
        // hlcoord(18, 17, wTilemap);
        // CALL(aBlinkCursor);
        // POP_HL;
        BlinkCursor(coord(18, 17, wram->wTilemap));

        // CALL(aJoyTextDelay);
        JoyTextDelay();

        // LDH_A_addr(hJoyLast);
        // AND_A(A_BUTTON | B_BUTTON);
        // IF_Z goto loop;
    } while((NativeInputLogicalLast() & (A_BUTTON | B_BUTTON)) == 0);

    // POP_AF;
    // LDH_addr_A(hObjectStructIndex);
    hram.hObjectStructIndex = objStructIdx;

    // POP_AF;
    // LDH_addr_A(hMapObjectIndex);
    hram.hMapObjectIndex = mapObjIdx;

    // RET;
    return;
}

void SimpleWaitPressAorB(void) {
    
    do {
        // CALL(aJoyTextDelay);
        JoyTextDelay();
        DelayFrame();
        // LDH_A_addr(hJoyLast);
        // AND_A(A_BUTTON | B_BUTTON);
    } while((NativeInputLogicalLast() & (A_BUTTON | B_BUTTON)) == 0);
    // IF_Z goto loop;
    // RET;
}

static void PromptButton_wait_input(void) {
    // PEEK("");
    // LDH_A_addr(hOAMUpdate);
    // PUSH_AF;
    uint8_t temp = hram.hOAMUpdate;

    // LD_A(0x1);
    // LDH_addr_A(hOAMUpdate);
    hram.hOAMUpdate = 1;

    // LD_A_addr(wInputType);
    // OR_A_A;
    // IF_Z goto input_wait_loop;
    if(IsAutoInputActive())
    {
        // FARCALL(av_DudeAutoInput_A);
        v_DudeAutoInput_A();
    }
    // input_wait_loop:
    do {
        // CALL(aPromptButton_blink_cursor);

        do {
        // blink_cursor:
            // LDH_A_addr(hVBlankCounter);
            // AND_A(0b00010000);  // bit 4, a
            // IF_Z goto cursor_off;
            if((hram.hVBlankCounter & 0b00010000) == 0)
            {
            // cursor_off:
                // lda_coord(17, 17, wTilemap);
                // ldcoord_a(18, 17, wTilemap);
                *coord(18, 17, wram->wTilemap) = *coord(17, 17, wram->wTilemap);
            }
            else 
            {
                // LD_A(0xee);
                // ldcoord_a(18, 17, wTilemap);
                *coord(18, 17, wram->wTilemap) = CHAR_DOWN_CURSOR;
            }

            // RET;
        } while(0);

        // CALL(aJoyTextDelay);
        JoyTextDelay();

        // LDH_A_addr(hJoyPressed);
        // AND_A(A_BUTTON | B_BUTTON);
        // IF_NZ goto received_input;
        if((NativeInputLogicalPressed() & (A_BUTTON | B_BUTTON)) != 0)
        {
            // PEEK("received_input");
        // received_input:
            // POP_AF;
            // LDH_addr_A(hOAMUpdate);
            hram.hOAMUpdate = temp;

            // RET;
            return;
        }

        // CALL(aUpdateTimeAndPals);
        UpdateTimeAndPals();

        // LD_A(0x1);
        // LDH_addr_A(hBGMapMode);
        hram.hBGMapMode = BGMAPMODE_UPDATE_TILES;

        // CALL(aDelayFrame);
        DelayFrame();

        // goto input_wait_loop;
    } while(1);
}

//  Show a blinking cursor in the lower right-hand
//  corner of a textbox and wait until A or B is
//  pressed, afterwards, play a sound.
void PromptButton(void) {
    // LD_A_addr(wLinkMode);
    // AND_A_A;
    // IF_NZ goto link;
    if(wram->wLinkMode != LINK_NULL)
    {
        // LD_C(65);
        // JP(mDelayFrames);
        return DelayFrames(65);
    }
    
    // CALL(aPromptButton_wait_input);
    PromptButton_wait_input();

    // PUSH_DE;
    // LD_DE(SFX_READ_TEXT_2);
    // CALL(aPlaySFX);
    // POP_DE;
    PlaySFX(SFX_READ_TEXT_2);
    // RET;
}

void BlinkCursor(uint8_t* hl) {
    // PUSH_BC;
    // LD_A_hl;
    // LD_B_A;
    // LD_A(0xee);
    // CP_A_B;
    // POP_BC;
    // IF_NZ goto place_arrow;
    if(*hl != CHAR_DOWN_CURSOR)
    {
// place_arrow:
        // LDH_A_addr(hMapObjectIndex);
        // AND_A_A;
        // RET_Z;
        if(hram.hMapObjectIndex == 0)
            return;
        
        // DEC_A;
        // LDH_addr_A(hMapObjectIndex);

        // RET_NZ;
        if(--hram.hMapObjectIndex != 0)
            return;
        
        // DEC_A;
        // LDH_addr_A(hMapObjectIndex);
        hram.hMapObjectIndex--;

        // LDH_A_addr(hObjectStructIndex);
        // DEC_A;
        // LDH_addr_A(hObjectStructIndex);

        // RET_NZ;
        if(--hram.hObjectStructIndex != 0)
            return;
        
        // LD_A(6);
        // LDH_addr_A(hObjectStructIndex);
        hram.hObjectStructIndex = 6;

        // LD_A(0xee);
        // LD_hl_A;
        *hl = CHAR_DOWN_CURSOR;

        // RET;
        return;
    }

    // LDH_A_addr(hMapObjectIndex);
    // DEC_A;
    // LDH_addr_A(hMapObjectIndex);

    // RET_NZ;
    if(--hram.hMapObjectIndex != 0)
        return;
    
    // LDH_A_addr(hObjectStructIndex);
    // DEC_A;
    // LDH_addr_A(hObjectStructIndex);

    // RET_NZ;
    if(--hram.hObjectStructIndex != 0)
        return;

    // LD_A(0x7a);
    // LD_hl_A;
    *hl = CHAR_FRAME_TOP;

    // LD_A(-1);
    // LDH_addr_A(hMapObjectIndex);
    hram.hMapObjectIndex = 0xff;

    // LD_A(6);
    // LDH_addr_A(hObjectStructIndex);
    hram.hObjectStructIndex = 6;
    
    // RET;
    return;
}
