#include "../../constants.h"
#include "anim_commands.h"
#include "core.h"
#include "helpers.h"
#include "functions.h"
#include "pokeball_wobble.h"
#include "bg_effects.h"
#include "../battle/core.h"
#include "../battle/check_battle_scene.h"
#include "../gfx/load_pics.h"
#include "../../home/audio.h"
#include "../../home/battle.h"
#include "../../home/delay.h"
#include "../../home/text.h"
#include "../../home/video.h"
#include "../../home/palettes.h"
#include "../../home/copy.h"
#include "../../home/sram.h"
#include "../../home/tilemap.h"
#include "../../home/pokemon.h"
#include "../../data/moves/animations.h"
#include "../../data/battle_anims/object_gfx.h"
#include "../../gfx/sprites.h"
#include "../../audio/engine.h"
#include "../../util/log.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static battleanim_s sBattleAnim;

static void GetSubstitutePic(void);
static size_t BuildMinimizePic(uint8_t pixels[7 * 7 * LEN_2BPP_TILE]);
static void CopyMinimizePic(uint8_t* de);
static void BattleAnimApplyFullCGBPals(void);

//  Battle animation command interpreter.

void PlayBattleAnim(void){
    log_runtime_event("ANIMATION", "play begin id=%u", (unsigned)BattleAnimationIdGet());
    BeginBattleAnimationPresentation();
    v_PlayBattleAnim();
    EndBattleAnimationPresentation();
    log_runtime_event("ANIMATION", "play complete id=%u", (unsigned)BattleAnimationIdGet());
}

void v_PlayBattleAnim(void){
    // LD_C(6);

// wait:
    // CALL(aBattleAnimDelayFrame);
    // DEC_C;
    // IF_NZ goto wait;
    for(uint8_t c = 0; c < 6; ++c) {
        // CALL(aBattleAnimDelayFrame);
        BattleAnimDelayFrame();
    }

    // CALL(aBattleAnimAssignPals);
    BattleAnimAssignPals();
    // CALL(aBattleAnimRequestPals);
    BattleAnimRequestPals();
    // CALL(aBattleAnimDelayFrame);
    BattleAnimDelayFrame();

    // LD_C(1);
    // LDH_A_addr(rKEY1);
    // BIT_A(7);  // check CGB double speed mode
    // IF_NZ goto got_speed;
    // LD_C(3);
    uint8_t c = (bit_test(gb_read(rKEY1), 7))? 1: 3;

// got_speed:
    // LD_HL(hVBlank);
    // LD_A_hl;
    // PUSH_AF;
    uint8_t vblank = hram.hVBlank;

    // LD_hl_C;
    hram.hVBlank = c;
    // CALL(aBattleAnimRunScript);
    BattleAnimRunScript();

    // POP_AF;
    // LDH_addr_A(hVBlank);
    hram.hVBlank = vblank;

    // LD_A(1);
    // LDH_addr_A(hBGMapMode);
    hram.hBGMapMode = BGMAPMODE_UPDATE_TILES;

    // CALL(aBattleAnimDelayFrame);
    // CALL(aBattleAnimDelayFrame);
    // CALL(aBattleAnimDelayFrame);
    BattleAnimDelayFrame();
    BattleAnimDelayFrame();
    BattleAnimDelayFrame();
    // CALL(aWaitSFX);
    WaitSFX();
    // RET;
}

void BattleAnimRunScript(void){
    // LD_A_addr(wFXAnimID + 1);
    // AND_A_A;
    // IF_NZ goto hi_byte;
    if(HIGH(BattleAnimationIdGet()) == 0) {
        // FARCALL(aCheckBattleScene);
        // IF_C goto disabled;
        if(CheckBattleScene()) {
            log_runtime_event("ANIMATION", "scene animation enabled id=%u", (unsigned)BattleAnimationIdGet());
            // CALL(aBattleAnimClearHud);
            BattleAnimClearHud();
            // CALL(aRunBattleAnimScript);
            RunBattleAnimScript();

            // CALL(aBattleAnimAssignPals);
            BattleAnimAssignPals();
            // CALL(aBattleAnimRequestPals);
            BattleAnimRequestPals();

            BattleSceneCameraSet(0, 0);
            // CALL(aBattleAnimDelayFrame);
            BattleAnimDelayFrame();
            // CALL(aBattleAnimRestoreHuds);
            BattleAnimRestoreHuds();
        }
        else {
            log_runtime_event("ANIMATION", "scene animation skipped id=%u setting=disabled", (unsigned)BattleAnimationIdGet());
        }

    // disabled:
        // LD_A_addr(wNumHits);
        // AND_A_A;
        // IF_Z goto done;
        if(wram->wNumHits != 0) {
            // LD_L_A;
            // LD_H(0);
            // LD_DE(ANIM_MISS);
            // ADD_HL_DE;
            // LD_A_L;
            // LD_addr_A(wFXAnimID);
            // LD_A_H;
            // LD_addr_A(wFXAnimID + 1);
            BattleAnimationIdSet(wram->wNumHits + ANIM_MISS);
        }
        else {
        // done:
            // CALL(aBattleAnim_RevertPals);
            BattleAnim_RevertPals();
            // RET;
            return;
        }
    }

// hi_byte:
    // CALL(aWaitSFX);
    WaitSFX();
    // CALL(aPlayHitSound);
    PlayHitSound();
    // CALL(aRunBattleAnimScript);
    RunBattleAnimScript();

// done:
    // CALL(aBattleAnim_RevertPals);
    BattleAnim_RevertPals();
    // RET;
}

void RunBattleAnimScript(void){
    // CALL(aClearBattleAnims);
    ClearBattleAnims();

    do {
    // playframe:
        // CALL(aRunBattleAnimCommand);
        RunBattleAnimCommand();
        // CALL(av_ExecuteBGEffects);
        v_ExecuteBGEffects();
        // CALL(aBattleAnim_UpdateOAM_All);
        BattleAnim_UpdateOAM_All();
        // CALL(aPushLYOverrides);
        PushLYOverrides();
        // CALL(aBattleAnimRequestPals);
        BattleAnimRequestPals();

    //  Speed up Rollout's animation.
        // LD_A_addr(wFXAnimID + 1);
        // OR_A_A;
        // IF_NZ goto not_rollout;

        // LD_A_addr(wFXAnimID);
        // CP_A(ROLLOUT);
        // IF_NZ goto not_rollout;
        if(BattleAnimationIdGet() == ROLLOUT) {
            const struct BattleBGEffect* effects = BattleAnimationBGEffects();
            for(size_t i = 0; i < BattleAnimationBGEffectCount(); i++) {
                if(effects[i].function == 0)
                    break;
            }
        }
        else {
        // not_rollout:
            // CALL(aBattleAnimDelayFrame);
            BattleAnimDelayFrame();
        }

    // done:
        // LD_A_addr(wBattleAnimFlags);
        // BIT_A(BATTLEANIM_STOP_F);
        // IF_Z goto playframe;
    } while(!bit_test(sBattleAnim.flags, BATTLEANIM_STOP_F));

    // CALL(aBattleAnim_ClearOAM);
    BattleAnim_ClearOAM();
    // RET;
}

void BattleAnimClearHud(void){
    // CALL(aBattleAnimDelayFrame);
    BattleAnimDelayFrame();
    // CALL(aWaitTop);
    WaitTop();
    // CALL(aClearActorHud);
    ClearActorHud();
    // LD_A(0x1);
    // LDH_addr_A(hBGMapMode);
    hram.hBGMapMode = BGMAPMODE_UPDATE_TILES;
    // CALL(aBattleAnimDelayFrame);
    // CALL(aBattleAnimDelayFrame);
    // CALL(aBattleAnimDelayFrame);
    BattleAnimDelayFrame();
    BattleAnimDelayFrame();
    BattleAnimDelayFrame();
    // CALL(aWaitTop);
    WaitTop();
    // RET;
}

void BattleAnimRestoreHuds(void){
    // CALL(aBattleAnimDelayFrame);
    BattleAnimDelayFrame();
    // CALL(aWaitTop);
    WaitTop();

    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    // LD_A(MBANK(awCurBattleMon));  // aka BANK(wTempMon), BANK(wPartyMon1), and several others
    // LDH_addr_A(rSVBK);

//  this block should just be "call UpdateBattleHuds"
    // LD_HL(mUpdateBattleHuds);
    // LD_A(BANK(aUpdatePlayerHUD));
    // RST(aFarCall);
    UpdateBattleHUDs();

    // POP_AF;
    // LDH_addr_A(rSVBK);

    // LD_A(0x1);
    // LDH_addr_A(hBGMapMode);
    hram.hBGMapMode = BGMAPMODE_UPDATE_TILES;
    // CALL(aBattleAnimDelayFrame);
    // CALL(aBattleAnimDelayFrame);
    // CALL(aBattleAnimDelayFrame);
    BattleAnimDelayFrame();
    BattleAnimDelayFrame();
    BattleAnimDelayFrame();
    // CALL(aWaitTop);
    WaitTop();
    // RET;
}

void BattleAnimRequestPals(void){
    // LDH_A_addr(hCGB);
    // AND_A_A;
    // RET_Z ;
    if(hram.hCGB == 0)
        return;

    // LDH_A_addr(rBGP);
    // LD_B_A;
    // LD_A_addr(wBGP);
    // CP_A_B;
    // CALL_NZ (aBattleAnim_SetBGPals);
    if(gb_read(rBGP) != BattleAnimationDMGBGPalette())
        BattleAnim_SetBGPals(BattleAnimationDMGBGPalette());

    // LDH_A_addr(rOBP0);
    // LD_B_A;
    // LD_A_addr(wOBP0);
    // CP_A_B;
    // CALL_NZ (aBattleAnim_SetOBPals);
    if(gb_read(rOBP0) != BattleAnimationDMGObjectPalette0())
        BattleAnim_SetOBPals(BattleAnimationDMGObjectPalette0());
    // RET;
}

void BattleAnimDelayFrame(void){
//  Like DelayFrame but wastes battery life.

    // LD_A(1);
    // LD_addr_A(wVBlankOccurred);

// wait:
    // LD_A_addr(wVBlankOccurred);
    // AND_A_A;
    // IF_NZ goto wait;
    // RET;
    DelayFrame();
}

void ClearActorHud(void){
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto player;
    if(hram.hBattleTurn == TURN_PLAYER) {
    // player:
        // hlcoord(9, 7, wTilemap);
        // LD_BC((5 << 8) | 11);
        // CALL(aClearBox);
        ClearBox(coord(9, 7, wram->wTilemap), 11, 5);
        // RET;
        return;
    }
    else {
        // hlcoord(1, 0, wTilemap);
        // LD_BC((4 << 8) | 10);
        ClearBox(coord(1, 0, wram->wTilemap), 10, 4);
        // CALL(aClearBox);
        // RET;
        return;
    }
}

void PlaceWindowOverBattleTextbox(void){
//  //  unreferenced
    // XOR_A_A;
    // LDH_addr_A(hBGMapMode);
    hram.hBGMapMode = BGMAPMODE_NONE;
// bgcoord hBGMapAddress, 0, 20
    // LD_A(LOW(vBGMap0 + 20 * BG_MAP_WIDTH));
    // LDH_addr_A(hBGMapAddress);
    // LD_A(HIGH(vBGMap0 + 20 * BG_MAP_WIDTH));
    // LDH_addr_A(hBGMapAddress + 1);
    hram.hBGMapAddress = bgcoord(0, 20, vBGMap0);
    // CALL(aWaitBGMap2);
    WaitBGMap2();
    // LD_A((SCREEN_HEIGHT - TEXTBOX_HEIGHT) * TILE_WIDTH);
    // LDH_addr_A(hWY);
    hram.hWY = (SCREEN_HEIGHT - TEXTBOX_HEIGHT) * TILE_WIDTH;
// bgcoord hBGMapAddress, 0, 0
    // XOR_A_A;  // LOW(vBGMap0)
    // LDH_addr_A(hBGMapAddress);
    // LD_A(HIGH(vBGMap0));
    // LDH_addr_A(hBGMapAddress + 1);
    hram.hBGMapAddress = bgcoord(0, 0, vBGMap0);
    // CALL(aBattleAnimDelayFrame);
    BattleAnimDelayFrame();
    // RET;
}

void BattleAnim_ClearOAM(void){
    // LD_A_addr(wBattleAnimFlags);
    // BIT_A(BATTLEANIM_KEEPSPRITES_F);
    // IF_Z goto delete;
    if(bit_test(sBattleAnim.flags, BATTLEANIM_KEEPSPRITES_F)) {
        // Keep only temporary effect sprites and retint that semantic layer.
        // Persistent battler and HUD scene elements retain their own palettes.
        SetBattleAnimationRenderSpritePalette(~(PALETTE_MASK | VRAM_BANK_1));
        // RET;
        return;
    }
    else {
    // delete:
        // LD_HL(wVirtualOAM);
        // LD_C(wVirtualOAMEnd - wVirtualOAM);
        // XOR_A_A;

    // loop2:
        // LD_hli_A;
        // DEC_C;
        // IF_NZ goto loop2;
        ClearBattleAnimationRenderSprites();
        // RET;
        return;
    }
}

static bool RunBattleAnimCommand_CheckTimer(void) {
    // LD_A_addr(wBattleAnimDelay);
    // AND_A_A;
    // IF_Z goto done;
    if(sBattleAnim.delay == 0)
        return true;

    // DEC_A;
    // LD_addr_A(wBattleAnimDelay);
    sBattleAnim.delay--;
    // AND_A_A;
    // RET;
    return false;

// done:
    // SCF;
    // RET;
}

static void RunBattleAnimCommand_RunScript(void) {
    if(!sBattleAnim.script)
        return;
    while(1) {
    // loop:
        // CALL(aGetBattleAnimByte);
        int res = sBattleAnim.script(&sBattleAnim);

        // CP_A(anim_ret_command);
        // IF_NZ goto not_done_with_anim;

    //  Return from a subroutine.
        // LD_HL(wBattleAnimFlags);
        // BIT_hl(BATTLEANIM_IN_SUBROUTINE_F);
        // IF_NZ goto do_anim;
        if(res == 1 && !sBattleAnim.script) {
            // SET_hl(BATTLEANIM_STOP_F);
            bit_set(sBattleAnim.flags, BATTLEANIM_STOP_F);
            // RET;
            return;
        }

    // not_done_with_anim:
        // CP_A(anim_obj_command);
        // IF_NC goto do_anim;
        else if(res == 0x2) {
            return;
        }

        // LD_addr_A(wBattleAnimDelay);
        // RET;

    // do_anim:
        // CALL(aRunBattleAnimCommand_DoCommand);
        // goto loop;
    }
}

void RunBattleAnimCommand(void){
    // CALL(aRunBattleAnimCommand_CheckTimer);
    // RET_NC ;
    if(!RunBattleAnimCommand_CheckTimer())
        return;
    // CALL(aRunBattleAnimCommand_RunScript);
    RunBattleAnimCommand_RunScript();
    // RET;
    return;


// RunScript:

// loop:
    // CALL(aGetBattleAnimByte);

    // CP_A(anim_ret_command);
    // IF_NZ goto not_done_with_anim;

//  Return from a subroutine.
    // LD_HL(wBattleAnimFlags);
    // BIT_hl(BATTLEANIM_IN_SUBROUTINE_F);
    // IF_NZ goto do_anim;

    // SET_hl(BATTLEANIM_STOP_F);
    // RET;


// not_done_with_anim:
    // CP_A(anim_obj_command);
    // IF_NC goto do_anim;

    // LD_addr_A(wBattleAnimDelay);
    // RET;


// do_anim:
    // CALL(aRunBattleAnimCommand_DoCommand);

    // goto loop;


// DoCommand:
//  Execute battle animation command in [wBattleAnimByte].
    // LD_A_addr(wBattleAnimByte);
    // SUB_A(anim_obj_command);

    // LD_E_A;
    // LD_D(0);
    // LD_HL(mBattleAnimCommands);
    // ADD_HL_DE;
    // ADD_HL_DE;
    // LD_A_hli;
    // LD_H_hl;
    // LD_L_A;
    // JP_hl;
/*
    switch(REG_DE) {
        case anim_obj_command: CALL(aBattleAnimCmd_Obj); break;
        case anim_1gfx_command: CALL(aBattleAnimCmd_1GFX); break;
        case anim_2gfx_command: CALL(aBattleAnimCmd_2GFX); break;
        case anim_3gfx_command: CALL(aBattleAnimCmd_3GFX); break;
        case anim_4gfx_command: CALL(aBattleAnimCmd_4GFX); break;
        case anim_5gfx_command: CALL(aBattleAnimCmd_5GFX); break;
        case anim_incobj_command: CALL(aBattleAnimCmd_IncObj); break;
        case anim_setobj_command: CALL(aBattleAnimCmd_SetObj); break;
        case anim_incbgeffect_command: CALL(aBattleAnimCmd_IncBGEffect); break;
        case anim_battlergfx_1row_command: CALL(aBattleAnimCmd_BattlerGFX_1Row); break;
        case anim_battlergfx_2row_command: CALL(aBattleAnimCmd_BattlerGFX_2Row); break;
        case anim_checkpokeball_command: CALL(aBattleAnimCmd_CheckPokeball); break;
        case anim_transform_command: CALL(aBattleAnimCmd_Transform); break;
        case anim_raisesub_command: CALL(aBattleAnimCmd_RaiseSub); break;
        case anim_dropsub_command: CALL(aBattleAnimCmd_DropSub); break;
        case anim_resetobp0_command: CALL(aBattleAnimCmd_ResetObp0); break;
        case anim_sound_command: CALL(aBattleAnimCmd_Sound); break;
        case anim_cry_command: CALL(aBattleAnimCmd_Cry); break;
        case anim_minimizeopp_command: CALL(aBattleAnimCmd_MinimizeOpp); break;
        case anim_oamon_command: CALL(aBattleAnimCmd_OAMOn); break;
        case anim_oamoff_command: CALL(aBattleAnimCmd_OAMOff); break;
        case anim_clearobjs_command: CALL(aBattleAnimCmd_ClearObjs); break;
        case anim_beatup_command: CALL(aBattleAnimCmd_BeatUp); break;
        case anim_0xe7_command: CALL(aBattleAnimCmd_E7); break;
        case anim_updateactorpic_command: CALL(aBattleAnimCmd_UpdateActorPic); break;
        case anim_minimize_command: CALL(aBattleAnimCmd_Minimize); break;
        case anim_0xea_command: CALL(aBattleAnimCmd_EA); break;
        case anim_0xeb_command: CALL(aBattleAnimCmd_EB); break;
        case anim_0xec_command: CALL(aBattleAnimCmd_EC); break;
        case anim_0xed_command: CALL(aBattleAnimCmd_ED); break;
        case anim_if_param_and_command: CALL(aBattleAnimCmd_IfParamAnd); break;
        case anim_jumpuntil_command: CALL(aBattleAnimCmd_JumpUntil); break;
        case anim_bgeffect_command: CALL(aBattleAnimCmd_BGEffect); break;
        case anim_bgp_command: CALL(aBattleAnimCmd_BGP); break;
        case anim_obp0_command: CALL(aBattleAnimCmd_OBP0); break;
        case anim_obp1_command: CALL(aBattleAnimCmd_OBP1); break;
        case anim_keepsprites_command: CALL(aBattleAnimCmd_KeepSprites); break;
        case anim_0xf5_command: CALL(aBattleAnimCmd_F5); break;
        case anim_0xf6_command: CALL(aBattleAnimCmd_F6); break;
        case anim_0xf7_command: CALL(aBattleAnimCmd_F7); break;
        case anim_if_param_equal_command: CALL(aBattleAnimCmd_IfParamEqual); break;
        case anim_setvar_command: CALL(aBattleAnimCmd_SetVar); break;
        case anim_incvar_command: CALL(aBattleAnimCmd_IncVar); break;
        case anim_if_var_equal_command: CALL(aBattleAnimCmd_IfVarEqual); break;
        case anim_jump_command: CALL(aBattleAnimCmd_Jump); break;
        case anim_loop_command: CALL(aBattleAnimCmd_Loop); break;
        case anim_call_command: CALL(aBattleAnimCmd_Call); break;
        case anim_ret_command: CALL(aBattleAnimCmd_Ret); break;
        default: break;
    }
    RET;
    */
}

// void BattleAnimCommands(void){
//  entries correspond to anim_* constants (see macros/scripts/battle_anims.asm)
    //table_width ['2', 'BattleAnimCommands']
    //dw ['BattleAnimCmd_Obj'];
    //dw ['BattleAnimCmd_1GFX'];
    //dw ['BattleAnimCmd_2GFX'];
    //dw ['BattleAnimCmd_3GFX'];
    //dw ['BattleAnimCmd_4GFX'];
    //dw ['BattleAnimCmd_5GFX'];
    //dw ['BattleAnimCmd_IncObj'];
    //dw ['BattleAnimCmd_SetObj'];
    //dw ['BattleAnimCmd_IncBGEffect'];
    //dw ['BattleAnimCmd_BattlerGFX_1Row'];
    //dw ['BattleAnimCmd_BattlerGFX_2Row'];
    //dw ['BattleAnimCmd_CheckPokeball'];
    //dw ['BattleAnimCmd_Transform'];
    //dw ['BattleAnimCmd_RaiseSub'];
    //dw ['BattleAnimCmd_DropSub'];
    //dw ['BattleAnimCmd_ResetObp0'];
    //dw ['BattleAnimCmd_Sound'];
    //dw ['BattleAnimCmd_Cry'];
    //dw ['BattleAnimCmd_MinimizeOpp'];
    //dw ['BattleAnimCmd_OAMOn'];
    //dw ['BattleAnimCmd_OAMOff'];
    //dw ['BattleAnimCmd_ClearObjs'];
    //dw ['BattleAnimCmd_BeatUp'];
    //dw ['BattleAnimCmd_E7'];
    //dw ['BattleAnimCmd_UpdateActorPic'];
    //dw ['BattleAnimCmd_Minimize'];
    //dw ['BattleAnimCmd_EA'];  // dummy
    //dw ['BattleAnimCmd_EB'];  // dummy
    //dw ['BattleAnimCmd_EC'];  // dummy
    //dw ['BattleAnimCmd_ED'];  // dummy
    //dw ['BattleAnimCmd_IfParamAnd'];
    //dw ['BattleAnimCmd_JumpUntil'];
    //dw ['BattleAnimCmd_BGEffect'];
    //dw ['BattleAnimCmd_BGP'];
    //dw ['BattleAnimCmd_OBP0'];
    //dw ['BattleAnimCmd_OBP1'];
    //dw ['BattleAnimCmd_KeepSprites'];
    //dw ['BattleAnimCmd_F5'];
    //dw ['BattleAnimCmd_F6'];
    //dw ['BattleAnimCmd_F7'];
    //dw ['BattleAnimCmd_IfParamEqual'];
    //dw ['BattleAnimCmd_SetVar'];
    //dw ['BattleAnimCmd_IncVar'];
    //dw ['BattleAnimCmd_IfVarEqual'];
    //dw ['BattleAnimCmd_Jump'];
    //dw ['BattleAnimCmd_Loop'];
    //dw ['BattleAnimCmd_Call'];
    //dw ['BattleAnimCmd_Ret'];
    //assert_table_length ['0x100 - FIRST_BATTLE_ANIM_CMD']

    // return BattleAnimCmd_EA();
// }

void BattleAnimCmd_Wait(uint8_t a) {
    // LD_addr_A(wBattleAnimDelay);
    sBattleAnim.delay = a;
    // RET;
}

void BattleAnimCmd_EA(void){
    return BattleAnimCmd_EB();
}

void BattleAnimCmd_EB(void){
    return BattleAnimCmd_EC();
}

void BattleAnimCmd_EC(void){
    return BattleAnimCmd_ED();
}

void BattleAnimCmd_ED(void){
    // RET;
}

void BattleAnimCmd_Ret(battleanim_s* anim){
    // LD_HL(wBattleAnimFlags);
    // RES_hl(BATTLEANIM_IN_SUBROUTINE_F);
    bit_reset(anim->flags, BATTLEANIM_IN_SUBROUTINE_F);
    // LD_HL(wBattleAnimParent);
    // LD_E_hl;
    // INC_HL;
    // LD_D_hl;
    // LD_HL(wBattleAnimAddress);
    // LD_hl_E;
    // INC_HL;
    // LD_hl_D;
    anim->script = anim->parent;
    anim->pos = anim->parent_pos;
    anim->parent = NULL;
    // RET;
}

void BattleAnimCmd_Call(battleanim_s* anim, battleanim_func func){
    // CALL(aGetBattleAnimByte);
    // LD_E_A;
    // CALL(aGetBattleAnimByte);
    // LD_D_A;
    // PUSH_DE;
    // LD_HL(wBattleAnimAddress);
    // LD_E_hl;
    // INC_HL;
    // LD_D_hl;
    // LD_HL(wBattleAnimParent);
    // LD_hl_E;
    // INC_HL;
    // LD_hl_D;
    anim->parent = anim->script;
    anim->parent_pos = anim->pos;
    // POP_DE;
    // LD_HL(wBattleAnimAddress);
    // LD_hl_E;
    // INC_HL;
    // LD_hl_D;
    anim->script = func;
    anim->pos = 0;
    // LD_HL(wBattleAnimFlags);
    // SET_hl(BATTLEANIM_IN_SUBROUTINE_F);
    bit_set(anim->flags, BATTLEANIM_IN_SUBROUTINE_F);
    // RET;
}

void BattleAnimCmd_Jump(battleanim_s* anim, battleanim_func func){
    // CALL(aGetBattleAnimByte);
    // LD_E_A;
    // CALL(aGetBattleAnimByte);
    // LD_D_A;
    // LD_HL(wBattleAnimAddress);
    // LD_hl_E;
    // INC_HL;
    // LD_hl_D;
    anim->script = func;
    anim->pos = 0;
    // RET;
}

void BattleAnimCmd_Loop(battleanim_s* anim, uint8_t count, battleanim_func func){
    // CALL(aGetBattleAnimByte);
    // LD_HL(wBattleAnimFlags);
    // BIT_hl(BATTLEANIM_IN_LOOP_F);
    // IF_NZ goto continue_loop;
    if(!bit_test(anim->flags, BATTLEANIM_IN_LOOP_F)) {
        // AND_A_A;
        // IF_Z goto perpetual;
        if(count != 0) {
            // DEC_A;
            // SET_hl(BATTLEANIM_IN_LOOP_F);
            bit_set(anim->flags, BATTLEANIM_IN_LOOP_F);
            // LD_addr_A(wBattleAnimLoops);
            anim->loops = --count;
        }
        else {
            goto perpetual;
        }
    }

// continue_loop:
    // LD_HL(wBattleAnimLoops);
    // LD_A_hl;
    // AND_A_A;
    // IF_Z goto return_from_loop;
    if(anim->loops == 0) {
    // return_from_loop:
        // LD_HL(wBattleAnimFlags);
        // RES_hl(BATTLEANIM_IN_LOOP_F);
        bit_reset(anim->flags, BATTLEANIM_IN_LOOP_F);
        // LD_HL(wBattleAnimAddress);
        // LD_E_hl;
        // INC_HL;
        // LD_D_hl;
        // INC_DE;
        // INC_DE;
        // LD_hl_D;
        // DEC_HL;
        // LD_hl_E;
        // RET;
        return;
    }
    // DEC_hl;
    anim->loops--;

perpetual:
    // CALL(aGetBattleAnimByte);
    // LD_E_A;
    // CALL(aGetBattleAnimByte);
    // LD_D_A;
    // LD_HL(wBattleAnimAddress);
    // LD_hl_E;
    // INC_HL;
    // LD_hl_D;
    anim->script = func;
    anim->pos = 0;
    // RET;
}

bool BattleAnimCmd_LoopInline(uint8_t count) {
    if(!bit_test(sBattleAnim.flags, BATTLEANIM_IN_LOOP_F)) {
        if(count == 0)
            return true;
        bit_set(sBattleAnim.flags, BATTLEANIM_IN_LOOP_F);
        sBattleAnim.loops = --count;
    }
    if(sBattleAnim.loops == 0) {
        bit_reset(sBattleAnim.flags, BATTLEANIM_IN_LOOP_F);
        return false;
    }
    sBattleAnim.loops--;
    return true;
}

void BattleAnimCmd_JumpUntil(battleanim_s* anim, battleanim_func func){
    // LD_HL(wBattleAnimParam);
    // LD_A_hl;
    // AND_A_A;
    // IF_Z goto dont_jump;
    if(BattleAnimationParameterGet() == 0) {
    // dont_jump:
        // LD_HL(wBattleAnimAddress);
        // LD_E_hl;
        // INC_HL;
        // LD_D_hl;
        // INC_DE;
        // INC_DE;
        // LD_hl_D;
        // DEC_HL;
        // LD_hl_E;
        // RET;
        return;
    }

    // DEC_hl;
    BattleAnimationParameterSet(BattleAnimationParameterGet() - 1);
    // CALL(aGetBattleAnimByte);
    // LD_E_A;
    // CALL(aGetBattleAnimByte);
    // LD_D_A;
    // LD_HL(wBattleAnimAddress);
    // LD_hl_E;
    // INC_HL;
    // LD_hl_D;
    anim->script = func;
    anim->pos = 0;
    // RET;
}

void BattleAnimCmd_SetVar(uint8_t var){
    // CALL(aGetBattleAnimByte);
    // LD_addr_A(wBattleAnimVar);
    sBattleAnim.var = var;
    // RET;
}

void BattleAnimCmd_IncVar(void){
    // LD_HL(wBattleAnimVar);
    // INC_hl;
    // RET;
    sBattleAnim.var++;
}

uint8_t BattleAnimGetVar(void){
    return sBattleAnim.var;
}

void BattleAnimCmd_IfVarEqual(battleanim_s* anim, uint8_t n, battleanim_func func){
    // CALL(aGetBattleAnimByte);
    // LD_HL(wBattleAnimVar);
    // CP_A_hl;
    // IF_Z goto jump;
    if(anim->var == n) {
    // jump:
        // CALL(aGetBattleAnimByte);
        // LD_E_A;
        // CALL(aGetBattleAnimByte);
        // LD_D_A;
        // LD_HL(wBattleAnimAddress);
        // LD_hl_E;
        // INC_HL;
        // LD_hl_D;
        anim->script = func;
        anim->pos = 0;
        // RET;
        return;
    }

    // LD_HL(wBattleAnimAddress);
    // LD_E_hl;
    // INC_HL;
    // LD_D_hl;
    // INC_DE;
    // INC_DE;
    // LD_hl_D;
    // DEC_HL;
    // LD_hl_E;
    // RET;
}

void BattleAnimCmd_IfParamEqual(battleanim_s* anim, uint8_t param, battleanim_func func){
    // CALL(aGetBattleAnimByte);
    // LD_HL(wBattleAnimParam);
    // CP_A_hl;
    // IF_Z goto jump;
    if(BattleAnimationParameterGet() == param) {
    // jump:
        // CALL(aGetBattleAnimByte);
        // LD_E_A;
        // CALL(aGetBattleAnimByte);
        // LD_D_A;
        // LD_HL(wBattleAnimAddress);
        // LD_hl_E;
        // INC_HL;
        // LD_hl_D;
        anim->script = func;
        anim->pos = 0;
        // RET;
        return;
    }

    // LD_HL(wBattleAnimAddress);
    // LD_E_hl;
    // INC_HL;
    // LD_D_hl;
    // INC_DE;
    // INC_DE;
    // LD_hl_D;
    // DEC_HL;
    // LD_hl_E;
    // RET;
}

void BattleAnimCmd_IfParamAnd(battleanim_s* anim, uint8_t n, battleanim_func func){
    // CALL(aGetBattleAnimByte);
    // LD_E_A;
    // LD_A_addr(wBattleAnimParam);
    // AND_A_E;
    // IF_NZ goto jump;
    if(BattleAnimationParameterGet() & n) {
    // jump:
        // CALL(aGetBattleAnimByte);
        // LD_E_A;
        // CALL(aGetBattleAnimByte);
        // LD_D_A;
        // LD_HL(wBattleAnimAddress);
        // LD_hl_E;
        // INC_HL;
        // LD_hl_D;
        anim->script = func;
        anim->pos = 0;
        // RET;
    }

    // LD_HL(wBattleAnimAddress);
    // LD_E_hl;
    // INC_HL;
    // LD_D_hl;
    // INC_DE;
    // INC_DE;
    // LD_hl_D;
    // DEC_HL;
    // LD_hl_E;
    // RET;
}

void BattleAnimCmd_Obj(uint8_t index, uint8_t x, uint8_t y, uint8_t param){
//  index, x, y, param
    // CALL(aGetBattleAnimByte);
    // LD_addr_A(wBattleObjectTempID);
    BattleAnimationCommandState()->objectId = index;
    // CALL(aGetBattleAnimByte);
    // LD_addr_A(wBattleObjectTempXCoord);
    BattleAnimationCommandState()->objectX = x;
    // CALL(aGetBattleAnimByte);
    // LD_addr_A(wBattleObjectTempYCoord);
    BattleAnimationCommandState()->objectY = y;
    // CALL(aGetBattleAnimByte);
    // LD_addr_A(wBattleObjectTempParam);
    BattleAnimationCommandState()->objectParam = param;
    // CALL(aQueueBattleAnimation);
    QueueBattleAnimation();
    // RET;
}

void BattleAnimCmd_BGEffect(uint8_t index, uint8_t jt, uint8_t turn, uint8_t param){
    // CALL(aGetBattleAnimByte);
    // LD_addr_A(wBattleBGEffectTempID);
    BattleAnimationCommandState()->bgEffectId = index;
    // CALL(aGetBattleAnimByte);
    // LD_addr_A(wBattleBGEffectTempJumptableIndex);
    BattleAnimationCommandState()->bgEffectJumptableIndex = jt;
    // CALL(aGetBattleAnimByte);
    // LD_addr_A(wBattleBGEffectTempTurn);
    BattleAnimationCommandState()->bgEffectTurn = turn;
    // CALL(aGetBattleAnimByte);
    // LD_addr_A(wBattleBGEffectTempParam);
    BattleAnimationCommandState()->bgEffectParam = param;
    // CALL(av_QueueBGEffect);
    v_QueueBGEffect();
    // RET;
}

void BattleAnimCmd_BGP(uint8_t bgp){
    // CALL(aGetBattleAnimByte);
    // LD_addr_A(wBGP);
    BattleAnimationDMGBGPaletteSet(bgp);
    // RET;
}

void BattleAnimCmd_OBP0(uint8_t obp0){
    // CALL(aGetBattleAnimByte);
    // LD_addr_A(wOBP0);
    BattleAnimationDMGObjectPalette0Set(obp0);
    // RET;
}

void BattleAnimCmd_OBP1(uint8_t obp1){
    // CALL(aGetBattleAnimByte);
    // LD_addr_A(wOBP1);
    BattleAnimationDMGObjectPalette1Set(obp1);
    // RET;
}

void BattleAnimCmd_ResetObp0(void){
    // LDH_A_addr(hSGB);
    // AND_A_A;
    // LD_A(0xe0);
    // IF_Z goto not_sgb;
    // LD_A(0xf0);

// not_sgb:
    // LD_addr_A(wOBP0);
    BattleAnimationDMGObjectPalette0Set((hram.hSGB == 0)? 0xe0: 0xf0);
    // RET;
}

void BattleAnimCmd_ClearObjs(void){
    // The native pool has no byte-addressed object layout. Clear semantic
    // object records rather than reproducing the historical partial-byte bug.
    ClearNativeBattleAnimationObjects();
}

void BattleAnimCmd_1GFX(void){
    // return BattleAnimCmd_2GFX();
}

void BattleAnimCmd_2GFX(void){
    // return BattleAnimCmd_3GFX();
}

void BattleAnimCmd_3GFX(void){
    // return BattleAnimCmd_4GFX();
}

void BattleAnimCmd_4GFX(void){
    // return BattleAnimCmd_5GFX();
}

void BattleAnimCmd_5GFX(void){
    // LD_A_addr(wBattleAnimByte);
    // AND_A(0xf);
    // LD_C_A;
    // LD_HL(wBattleAnimTileDict);
    // XOR_A_A;
    // LD_addr_A(wBattleAnimGFXTempTileID);

// loop:
    // LD_A_addr(wBattleAnimGFXTempTileID);
    // CP_A((vTiles1 - vTiles0) / LEN_2BPP_TILE - BATTLEANIM_BASE_TILE);
    // RET_NC ;
    // CALL(aGetBattleAnimByte);
    // LD_hli_A;
    // LD_A_addr(wBattleAnimGFXTempTileID);
    // LD_hli_A;
    // PUSH_BC;
    // PUSH_HL;
    // LD_L_A;
    // LD_H(0);
    // for(int rept = 0; rept < 4; rept++){
    // ADD_HL_HL;
    // }
    // LD_DE(vTiles0 + LEN_2BPP_TILE * BATTLEANIM_BASE_TILE);
    // ADD_HL_DE;
    // LD_A_addr(wBattleAnimByte);
    // CALL(aLoadBattleAnimGFX);
    // LD_A_addr(wBattleAnimGFXTempTileID);
    // ADD_A_C;
    // LD_addr_A(wBattleAnimGFXTempTileID);
    // POP_HL;
    // POP_BC;
    // DEC_C;
    // IF_NZ goto loop;
    // RET;

}

void BattleAnimCmd_NGFX(uint8_t c, ...){
    // LD_A_addr(wBattleAnimByte);
    // AND_A(0xf);
    // LD_C_A;
    // XOR_A_A;
    // LD_addr_A(wBattleAnimGFXTempTileID);
    BattleAnimationEffectScratchState()->gfxTileId = 0;
    size_t bindingIndex = 0;
    log_debug("NGFX %d - ", c);
    va_list v;
    va_start(v, c);

    do {
    // loop:
        // Read the native graphics resource identity before allocating its
        // tile range; a native resource may exceed the former VRAM span.
        uint8_t byte = (uint8_t)va_arg(v, int);
        if(BattleAnimationEffectScratchState()->gfxTileId >
            UINT16_MAX - BATTLEANIM_BASE_TILE - (AnimObjGFX[byte].tiles - 1)) {
            va_end(v);
            log_debug("\n");
            return;
        }
        log_debug("%d, ", (int)byte);
        // LD_hli_A;
        // LD_A_addr(wBattleAnimGFXTempTileID);
        // LD_hli_A;
        SetBattleAnimationTileBinding(bindingIndex++, byte, BattleAnimationEffectScratchState()->gfxTileId);
        // PUSH_BC;
        // PUSH_HL;
        // LD_L_A;
        // LD_H(0);
        // for(int rept = 0; rept < 4; rept++){
        // ADD_HL_HL;
        // }
        // Native tile storage replaces the fixed battle-animation VRAM range.
        const struct BattleAnimGFX* gfx = AnimObjGFX + byte;
        uint8_t* de = BattleAnimationTileWritePointer(BATTLEANIM_BASE_TILE + BattleAnimationEffectScratchState()->gfxTileId, gfx->tiles);
        // LD_A_addr(wBattleAnimByte);
        // CALL(aLoadBattleAnimGFX);
        // LD_A_addr(wBattleAnimGFXTempTileID);
        // ADD_A_C;
        // LD_addr_A(wBattleAnimGFXTempTileID);
        BattleAnimationEffectScratchState()->gfxTileId += LoadBattleAnimGFX(de, byte);
        // POP_HL;
        // POP_BC;
        // DEC_C;
        // IF_NZ goto loop;
    } while(--c != 0);
    log_debug("\n");

    va_end(v);
    // RET;
}

void BattleAnimCmd_IncObj(uint8_t a){
    struct BattleAnim* objects = BattleAnimationObjects();
    for(size_t i = 0; i < BattleAnimationObjectCount(); i++) {
        if(objects[i].index == a) {
            objects[i].jumptableIndex++;
            return;
        }
    }
}

void BattleAnimCmd_IncBGEffect(uint8_t c){
    struct BattleBGEffect* effects = BattleAnimationBGEffects();
    for(size_t i = 0; i < BattleAnimationBGEffectCount(); i++) {
        if(effects[i].function == c) {
            effects[i].jumptableIndex++;
            return;
        }
    }
}

void BattleAnimCmd_SetObj(uint8_t a, uint8_t jt){
    struct BattleAnim* objects = BattleAnimationObjects();
    for(size_t i = 0; i < BattleAnimationObjectCount(); i++) {
        if(objects[i].index == a) {
            objects[i].jumptableIndex = jt;
            return;
        }
    }
}

static uint8_t* BattleAnimCmd_BattlerGFX_1Row_LoadFeet(uint8_t* hl, const uint8_t* de, uint8_t a) {
    do {
        // PUSH_AF;
        // PUSH_HL;
        // PUSH_DE;
        // LD_BC((BANK(aBattleAnimCmd_BattlerGFX_1Row) << 8) | 1);
        // CALL(aRequest2bpp);
        CopyBytes(hl, de, 1 * LEN_2BPP_TILE);
        // POP_DE;
        // LD_A_addr(wBattleAnimGFXTempPicHeight);
        // LD_L_A;
        // LD_H(0);
        // ADD_HL_DE;
        // LD_E_L;
        // LD_D_H;
        de += LEN_2BPP_TILE;
        // POP_HL;
        // LD_BC(1 * LEN_2BPP_TILE);
        // ADD_HL_BC;
        hl += 1 * LEN_2BPP_TILE;
        // POP_AF;
        // DEC_A;
        // IF_NZ goto LoadFeet;
    } while(--a != 0);
    // RET;
    return hl;
}

void BattleAnimCmd_BattlerGFX_1Row(void){
    log_debug("BattlerGFX_1Row::\n");
    // LD_A(ANIM_GFX_PLAYERHEAD);
    // LD_hli_A;
    // LD_A((0x80 - 6 - 7) - BATTLEANIM_BASE_TILE);
    // LD_hli_A;
    // LD_A(ANIM_GFX_ENEMYFEET);
    // LD_hli_A;
    // LD_A((0x80 - 6) - BATTLEANIM_BASE_TILE);
    // LD_hl_A;
    AppendBattleAnimationTileBinding(ANIM_GFX_PLAYERHEAD, (0x80 - 6 - 7) - BATTLEANIM_BASE_TILE);
    AppendBattleAnimationTileBinding(ANIM_GFX_ENEMYFEET, (0x80 - 6) - BATTLEANIM_BASE_TILE);

    // LD_HL(vTiles0 + LEN_2BPP_TILE * (0x80 - 6 - 7));
    // LD_DE(vTiles2 + LEN_2BPP_TILE * 0x06);  // Enemy feet start tile
    // LD_A(7 * LEN_2BPP_TILE);  // Enemy pic height
    // LD_addr_A(wBattleAnimGFXTempPicHeight);
    BattleAnimationEffectScratchState()->gfxPicHeight = 7 * LEN_2BPP_TILE;
    // LD_A(7);  // Copy 7x1 tiles
    // CALL(aBattleAnimCmd_BattlerGFX_1Row_LoadFeet);
    const struct BattleSceneBattlerView* opponent = BattleSceneBattler(BATTLE_SCENE_BATTLER_OPPONENT);
    const struct BattleSceneBattlerView* player = BattleSceneBattler(BATTLE_SCENE_BATTLER_PLAYER);
    if(opponent == NULL || player == NULL || opponent->pixelTileCount < 7 * 7 || player->pixelTileCount < 6 * 6)
        abort();
    uint8_t* hl2 = BattleAnimCmd_BattlerGFX_1Row_LoadFeet(
        BattleAnimationTileWritePointer(0x80 - 6 - 7, 7),
        opponent->pixels + LEN_2BPP_TILE * (6 * 7), 7);
    // LD_DE(vTiles2 + LEN_2BPP_TILE * 0x31);  // Player head start tile
    // LD_A(6 * LEN_2BPP_TILE);  // Player pic height
    // LD_addr_A(wBattleAnimGFXTempPicHeight);
    BattleAnimationEffectScratchState()->gfxPicHeight = 6 * LEN_2BPP_TILE;
    // LD_A(6);  // Copy 6x1 tiles
    // CALL(aBattleAnimCmd_BattlerGFX_1Row_LoadFeet);
    BattleAnimCmd_BattlerGFX_1Row_LoadFeet(hl2, player->pixels, 6);
    // RET;
}

static uint8_t* BattleAnimCmd_BattlerGFX_2Row_LoadHead(uint8_t* hl, const uint8_t* de, uint8_t a) {
    do {
        // PUSH_AF;
        // PUSH_HL;
        // PUSH_DE;
        // LD_BC((BANK(aBattleAnimCmd_BattlerGFX_2Row_LoadHead) << 8) | 2);
        // CALL(aRequest2bpp);
        CopyBytes(hl, de, LEN_2BPP_TILE);
        CopyBytes(hl + LEN_2BPP_TILE, de + BattleAnimationEffectScratchState()->gfxPicHeight, LEN_2BPP_TILE);
        // POP_DE;
        // LD_A_addr(wBattleAnimGFXTempPicHeight);
        // LD_L_A;
        // LD_H(0);
        // ADD_HL_DE;
        de += LEN_2BPP_TILE;
        // LD_E_L;
        // LD_D_H;
        // POP_HL;
        // LD_BC(2 * LEN_2BPP_TILE);
        // ADD_HL_BC;
        hl += 2 * LEN_2BPP_TILE;
        // POP_AF;
        // DEC_A;
        // IF_NZ goto LoadHead;
    } while(--a != 0);
    // RET;
    return hl;
}

void BattleAnimCmd_BattlerGFX_2Row(void){
    log_debug("BattlerGFX_2Row::\n");
    // LD_A(ANIM_GFX_PLAYERHEAD);
    // LD_hli_A;
    // LD_A((0x80 - 6 * 2 - 7 * 2) - BATTLEANIM_BASE_TILE);
    // LD_hli_A;
    // LD_A(ANIM_GFX_ENEMYFEET);
    // LD_hli_A;
    // LD_A((0x80 - 6 * 2) - BATTLEANIM_BASE_TILE);
    // LD_hl_A;
    AppendBattleAnimationTileBinding(ANIM_GFX_PLAYERHEAD, (0x80 - 6 * 2 - 7 * 2) - BATTLEANIM_BASE_TILE);
    AppendBattleAnimationTileBinding(ANIM_GFX_ENEMYFEET, (0x80 - 6 * 2) - BATTLEANIM_BASE_TILE);

    // LD_HL(vTiles0 + LEN_2BPP_TILE * (0x80 - 6 * 2 - 7 * 2));
    // LD_DE(vTiles2 + LEN_2BPP_TILE * 0x05);  // Enemy feet start tile
    // LD_A(7 * LEN_2BPP_TILE);  // Enemy pic height
    // LD_addr_A(wBattleAnimGFXTempPicHeight);
    BattleAnimationEffectScratchState()->gfxPicHeight = 7 * LEN_2BPP_TILE;
    // LD_A(7);  // Copy 7x2 tiles
    // CALL(aBattleAnimCmd_BattlerGFX_2Row_LoadHead);
    const struct BattleSceneBattlerView* opponent = BattleSceneBattler(BATTLE_SCENE_BATTLER_OPPONENT);
    const struct BattleSceneBattlerView* player = BattleSceneBattler(BATTLE_SCENE_BATTLER_PLAYER);
    if(opponent == NULL || player == NULL || opponent->pixelTileCount < 7 * 7 || player->pixelTileCount < 6 * 6)
        abort();
    uint8_t* hl2 = BattleAnimCmd_BattlerGFX_2Row_LoadHead(
        BattleAnimationTileWritePointer(0x80 - 6 * 2 - 7 * 2, 14),
        opponent->pixels + LEN_2BPP_TILE * 7 * 5, 7);
    // LD_DE(vTiles2 + LEN_2BPP_TILE * 0x31);  // Player head start tile
    // LD_A(6 * LEN_2BPP_TILE);  // Player pic height
    // LD_addr_A(wBattleAnimGFXTempPicHeight);
    BattleAnimationEffectScratchState()->gfxPicHeight = 6 * LEN_2BPP_TILE;
    // LD_A(6);  // Copy 6x2 tiles
    // CALL(aBattleAnimCmd_BattlerGFX_2Row_LoadHead);
    BattleAnimCmd_BattlerGFX_2Row_LoadHead(hl2, player->pixels, 6);
    // RET;
}

void BattleAnimCmd_CheckPokeball(void){
    // CALLFAR(aGetPokeBallWobble);
    // LD_A_C;
    // LD_addr_A(wBattleAnimVar);
    sBattleAnim.var = GetPokeBallWobble();
    // RET;
}

void BattleAnimCmd_E7(void){
    // RET;
}

void BattleAnimCmd_Transform(void){
    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    // LD_A(MBANK(awCurPartySpecies));
    // LDH_addr_A(rSVBK);

    // LD_A_addr(wCurPartySpecies);
    // PUSH_AF;
    uint8_t species = wram->wCurPartySpecies;

    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto player;

    if(hram.hBattleTurn != TURN_PLAYER) {
        // LD_A_addr(wTempBattleMonSpecies);
        // LD_addr_A(wCurPartySpecies);
        wram->wCurPartySpecies = wram->wTempBattleMonSpecies;
        // LD_HL(wBattleMonDVs);
        // PREDEF(pGetUnownLetter);
        GetUnownLetter(wram->wBattleMon.dvs);
        uint8_t pixels[7 * 7 * LEN_2BPP_TILE];
        if(LoadNativeFrontpicPixels(pixels, 0))
            UpdateBattleSceneBattlerImage(BATTLE_SCENE_BATTLER_OPPONENT, pixels, 7 * 7);
        // goto done;
    }
    else {
    // player:
        // LD_A_addr(wTempEnemyMonSpecies);
        // LD_addr_A(wCurPartySpecies);
        wram->wCurPartySpecies = wram->wTempEnemyMonSpecies;
        // LD_HL(wEnemyMonDVs);
        // PREDEF(pGetUnownLetter);
        GetUnownLetter(wram->wEnemyMon.dvs);
        uint8_t pixels[7 * 7 * LEN_2BPP_TILE];
        if(LoadNativeFrontpicPixels(pixels, 0))
            UpdateBattleSceneBattlerImage(BATTLE_SCENE_BATTLER_PLAYER, pixels, 7 * 7);
    }

// done:
    // POP_AF;
    // LD_addr_A(wCurPartySpecies);
    wram->wCurPartySpecies = species;

    // POP_AF;
    // LDH_addr_A(rSVBK);
    // RET;
}

void BattleAnimCmd_UpdateActorPic(void){
    // LD_DE(vTiles0 + LEN_2BPP_TILE * 0x00);
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto player;

    // LD_HL(vTiles2 + LEN_2BPP_TILE * 0x00);
    // LD_B(0);
    // LD_C(7 * 7);
    // CALL(aRequest2bpp);
    // RET;

// player:
    // LD_HL(vTiles2 + LEN_2BPP_TILE * 0x31);
    // LD_B(0);
    // LD_C(6 * 6);
    // CALL(aRequest2bpp);
    // Picture-changing commands update the native battler resource directly.
    // This retained script opcode is therefore an explicit no-op.
}

void BattleAnimCmd_RaiseSub(void){
    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    // LD_A(1);  // unnecessary bankswitch?
    // LDH_addr_A(rSVBK);

    GetSubstitutePic();
}

static void GetSubstitutePic_CopyTile(uint8_t* de, const char* hl, int start){
    // LD_BC(1 * LEN_2BPP_TILE);
    // LD_A(BANK(aMonsterSpriteGFX));
    // CALL(aFarCopyBytes);
    LoadPNG2bppAssetSectionToVRAM(de, hl, start, 1);
    // RET;
}

static void GetSubstitutePic(void){
    uint8_t pixels[7 * 7 * LEN_2BPP_TILE] = {0};

    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto player;
    if(hram.hBattleTurn != TURN_PLAYER) {
        // LD_HL(mMonsterSpriteGFX + 0 * LEN_2BPP_TILE);
        // LD_DE(sScratch + (2 * 7 + 5) * LEN_2BPP_TILE);
        // CALL(aGetSubstitutePic_CopyTile);
        GetSubstitutePic_CopyTile(pixels + (2 * 7 + 5) * LEN_2BPP_TILE, MonsterSpriteGFX, 0);
        // LD_HL(mMonsterSpriteGFX + 1 * LEN_2BPP_TILE);
        // LD_DE(sScratch + (3 * 7 + 5) * LEN_2BPP_TILE);
        // CALL(aGetSubstitutePic_CopyTile);
        GetSubstitutePic_CopyTile(pixels + (2 * 7 + 6) * LEN_2BPP_TILE, MonsterSpriteGFX, 1);
        // LD_HL(mMonsterSpriteGFX + 2 * LEN_2BPP_TILE);
        // LD_DE(sScratch + (2 * 7 + 6) * LEN_2BPP_TILE);
        // CALL(aGetSubstitutePic_CopyTile);
        GetSubstitutePic_CopyTile(pixels + (3 * 7 + 5) * LEN_2BPP_TILE, MonsterSpriteGFX, 2);
        // LD_HL(mMonsterSpriteGFX + 3 * LEN_2BPP_TILE);
        // LD_DE(sScratch + (3 * 7 + 6) * LEN_2BPP_TILE);
        // CALL(aGetSubstitutePic_CopyTile);
        GetSubstitutePic_CopyTile(pixels + (3 * 7 + 6) * LEN_2BPP_TILE, MonsterSpriteGFX, 3);

        UpdateBattleSceneBattlerImage(BATTLE_SCENE_BATTLER_OPPONENT, pixels, 7 * 7);
        // goto done;
    }
    else {
    // player:
        // LD_HL(mMonsterSpriteGFX + 4 * LEN_2BPP_TILE);
        // LD_DE(sScratch + (2 * 6 + 4) * LEN_2BPP_TILE);
        // CALL(aGetSubstitutePic_CopyTile);
        GetSubstitutePic_CopyTile(pixels + (2 * 6 + 4) * LEN_2BPP_TILE, MonsterSpriteGFX, 4);
        // LD_HL(mMonsterSpriteGFX + 5 * LEN_2BPP_TILE);
        // LD_DE(sScratch + (3 * 6 + 4) * LEN_2BPP_TILE);
        // CALL(aGetSubstitutePic_CopyTile);
        GetSubstitutePic_CopyTile(pixels + (2 * 6 + 5) * LEN_2BPP_TILE, MonsterSpriteGFX, 5);
        // LD_HL(mMonsterSpriteGFX + 6 * LEN_2BPP_TILE);
        // LD_DE(sScratch + (2 * 6 + 5) * LEN_2BPP_TILE);
        // CALL(aGetSubstitutePic_CopyTile);
        GetSubstitutePic_CopyTile(pixels + (3 * 6 + 4) * LEN_2BPP_TILE, MonsterSpriteGFX, 6);
        // LD_HL(mMonsterSpriteGFX + 7 * LEN_2BPP_TILE);
        // LD_DE(sScratch + (3 * 6 + 5) * LEN_2BPP_TILE);
        // CALL(aGetSubstitutePic_CopyTile);
        GetSubstitutePic_CopyTile(pixels + (3 * 6 + 5) * LEN_2BPP_TILE, MonsterSpriteGFX, 7);

        UpdateBattleSceneBattlerImage(BATTLE_SCENE_BATTLER_PLAYER, pixels, 6 * 6);
    }

}

void BattleAnimCmd_MinimizeOpp(void){
    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    // LD_A(1);  // unnecessary bankswitch?
    // LDH_addr_A(rSVBK);

    uint8_t pixels[7 * 7 * LEN_2BPP_TILE];
    size_t tileCount = BuildMinimizePic(pixels);
    UpdateBattleSceneBattlerImage((hram.hBattleTurn == TURN_PLAYER)
            ? BATTLE_SCENE_BATTLER_PLAYER : BATTLE_SCENE_BATTLER_OPPONENT,
        pixels, tileCount);

    // POP_AF;
    // LDH_addr_A(rSVBK);
    // RET;
}

static size_t BuildMinimizePic(uint8_t pixels[7 * 7 * LEN_2BPP_TILE]){
    memset(pixels, 0, 7 * 7 * LEN_2BPP_TILE);

    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto player;
    if(hram.hBattleTurn != TURN_PLAYER) {
        // LD_DE(sScratch + (3 * 7 + 5) * LEN_2BPP_TILE);
        // CALL(aCopyMinimizePic);
        CopyMinimizePic(pixels + (3 * 7 + 5) * LEN_2BPP_TILE);
        // LD_HL(vTiles2 + LEN_2BPP_TILE * 0x00);
        // LD_DE(sScratch);
        // LD_BC((BANK(aGetMinimizePic) << 8) | 7 * 7);
        // RET;
        return 7 * 7;
    }
    else {
    // player:
        // LD_DE(sScratch + (3 * 6 + 4) * LEN_2BPP_TILE);
        // CALL(aCopyMinimizePic);
        CopyMinimizePic(pixels + (3 * 6 + 4) * LEN_2BPP_TILE);
        // LD_HL(vTiles2 + LEN_2BPP_TILE * 0x31);
        // LD_DE(sScratch);
        // LD_BC((BANK(aGetMinimizePic) << 8) | 6 * 6);
        // RET;
        return 6 * 6;
    }

}

static const char MinimizePic[] = "gfx/battle/minimize.png";

static void CopyMinimizePic(uint8_t* de){
    // LD_HL(mMinimizePic);
    // LD_BC(0x10);
    // LD_A(BANK(aMinimizePic));
    // CALL(aFarCopyBytes);
    LoadPNG2bppAssetSectionToVRAM(de, MinimizePic, 0x0, 0x10 / LEN_2BPP_TILE);
    // RET;
}

void BattleAnimCmd_Minimize(void){
    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    // LD_A(1);  // unnecessary bankswitch?
    // LDH_addr_A(rSVBK);

    uint8_t pixels[7 * 7 * LEN_2BPP_TILE];
    size_t tileCount = BuildMinimizePic(pixels);
    UpdateBattleSceneBattlerImage((hram.hBattleTurn == TURN_PLAYER)
            ? BATTLE_SCENE_BATTLER_PLAYER : BATTLE_SCENE_BATTLER_OPPONENT,
        pixels, tileCount);

    // POP_AF;
    // LDH_addr_A(rSVBK);
    // RET;
}

void BattleAnimCmd_DropSub(void){
    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    // LD_A(MBANK(awCurPartySpecies));
    // LDH_addr_A(rSVBK);

    // LD_A_addr(wCurPartySpecies);
    // PUSH_AF;
    species_t species = wram->wCurPartySpecies;
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto player;

    if(hram.hBattleTurn != TURN_PLAYER) {
        // CALLFAR(aDropEnemySub);
        DropEnemySub();
        // goto done;
    }
    else {
    // player:
        // CALLFAR(aDropPlayerSub);
        DropPlayerSub();
    }

// done:
    // POP_AF;
    // LD_addr_A(wCurPartySpecies);
    wram->wCurPartySpecies = species;

    // POP_AF;
    // LDH_addr_A(rSVBK);
    // RET;
}

void BattleAnimCmd_BeatUp(void){
    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    // LD_A(MBANK(awCurPartySpecies));
    // LDH_addr_A(rSVBK);

    // LD_A_addr(wCurPartySpecies);
    // PUSH_AF;
    species_t species = wram->wCurPartySpecies;

    // LD_A_addr(wBattleAnimParam);
    // LD_addr_A(wCurPartySpecies);
    wram->wCurPartySpecies = BattleAnimationParameterGet();

    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto player;
    if(hram.hBattleTurn != TURN_PLAYER) {
        // LD_HL(wBattleMonDVs);
        // PREDEF(pGetUnownLetter);
        GetUnownLetter(wram->wBattleMon.dvs);
        uint8_t pixels[7 * 7 * LEN_2BPP_TILE];
        if(LoadNativeFrontpicPixels(pixels, 0))
            UpdateBattleSceneBattlerImage(BATTLE_SCENE_BATTLER_OPPONENT, pixels, 7 * 7);
        // goto done;
    }
    else {
    // player:
        // LD_HL(wEnemyMonDVs);
        // PREDEF(pGetUnownLetter);
        GetUnownLetter(wram->wEnemyMon.dvs);
        uint8_t pixels[6 * 6 * LEN_2BPP_TILE];
        if(LoadNativeBackpicPixels(pixels, wram->wCurPartySpecies))
            UpdateBattleSceneBattlerImage(BATTLE_SCENE_BATTLER_PLAYER, pixels, 6 * 6);
    }

// done:
    // POP_AF;
    // LD_addr_A(wCurPartySpecies);
    wram->wCurPartySpecies = species;
    // LD_B(SCGB_BATTLE_COLORS);
    // CALL(aGetSGBLayout);
    GetSGBLayout(SCGB_BATTLE_COLORS);

    // POP_AF;
    // LDH_addr_A(rSVBK);
    // RET;
}

void BattleAnimCmd_OAMOn(void){
    // XOR_A_A;
    // LDH_addr_A(hOAMUpdate);
    hram.hOAMUpdate = 0x0;
    // RET;
}

void BattleAnimCmd_OAMOff(void){
    // LD_A(0x1);
    // LDH_addr_A(hOAMUpdate);
    hram.hOAMUpdate = 0x1;
    // RET;
}

void BattleAnimCmd_KeepSprites(void){
    // LD_HL(wBattleAnimFlags);
    // SET_hl(BATTLEANIM_KEEPSPRITES_F);
    bit_set(sBattleAnim.flags, BATTLEANIM_KEEPSPRITES_F);
    // RET;
}

void BattleAnimCmd_F5(void){
    // RET;
}

void BattleAnimCmd_F6(void){
    // RET;
}

void BattleAnimCmd_F7(void){
    // RET;
}

void BattleAnimCmd_Sound(uint8_t duration, uint8_t tracks, uint16_t sfx){
    log_debug(":: (%d, %d, %d)\n", duration, tracks, sfx);
    static const uint8_t GetPanning[] = {0xf0, 0x0f, 0xf0, 0x0f};
    // CALL(aGetBattleAnimByte);
    // LD_E_A;
    // SRL_A;
    // SRL_A;
    // LD_addr_A(wSFXDuration);
    wram->wSFXDuration = duration;
    // CALL(aBattleAnimCmd_Sound_GetCryTrack);
    // maskbits(NUM_NOISE_CHANS, 0);
    // LD_addr_A(wCryTracks);
    wram->wCryTracks = (tracks ^ ((hram.hBattleTurn == TURN_PLAYER)? 0: 1)) & 3;

    // LD_E_A;
    // LD_D(0);
    // LD_HL(mBattleAnimCmd_Sound_GetPanning);
    // ADD_HL_DE;
    // LD_A_hl;
    // LD_addr_A(wStereoPanningMask);
    wram->wStereoPanningMask = GetPanning[wram->wCryTracks];

    // CALL(aGetBattleAnimByte);
    // LD_E_A;
    // LD_D(0);
    // CALLFAR(aPlayStereoSFX);
    // SafeCallGB(aPlayStereoSFX, &(struct cpu_registers_s){.de = sfx});
    PlayStereoSFX(sfx);

    // RET;

// GetCryTrack:
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_NZ goto enemy;

    // LD_A_E;
    // RET;


// enemy:
    // LD_A_E;
    // XOR_A(1);
    // RET;
}

void BattleAnimCmd_Cry(uint8_t cry){
    static const uint16_t CryData[] = {
    //  +pitch, +length
        0x0000, 0x00c0,
        0x0000, 0x0040,
        0x0000, 0x0000,
        0x0000, 0x0000,
    };
    // CALL(aGetBattleAnimByte);
    // maskbits(NUM_NOISE_CHANS, 0);
    // LD_E_A;
    // LD_D(0);
    // LD_HL(mBattleAnimCmd_Cry_CryData);
    // for(int rept = 0; rept < 4; rept++){
    // ADD_HL_DE;
    // }
    const uint16_t* de = CryData + ((cry & 3) * 2);

    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    // LD_A(MBANK(awEnemyMon));  // wBattleMon is in WRAM0, but wEnemyMon is in WRAMX
    // LDH_addr_A(rSVBK);

    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_NZ goto enemy;

    species_t species;
    if(hram.hBattleTurn == TURN_PLAYER) {
        // LD_A(0xf0);
        // LD_addr_A(wCryTracks);
        wram->wCryTracks = 0xf0;
        // LD_A_addr(wBattleMonSpecies);
        species = wram->wBattleMon.species;
        // goto done_cry_tracks;
    }
    else {
    // enemy:
        // LD_A(0x0f);
        // LD_addr_A(wCryTracks);
        wram->wCryTracks = 0x0f;
        // LD_A_addr(wEnemyMonSpecies);
        species = wram->wEnemyMon.species;
    }

// done_cry_tracks:
    // PUSH_HL;
    // CALL(aLoadCry);
    const struct PokemonCry* cryData = LoadCry(species);
    // POP_HL;
    // IF_C goto done;
    if(cryData) {
        // LD_A_hli;
        // LD_C_A;
        // LD_A_hli;
        // LD_B_A;

        // PUSH_HL;
        // LD_HL(wCryPitch);
        // LD_A_hli;
        // LD_H_hl;
        // LD_L_A;
        // ADD_HL_BC;
        // LD_A_L;
        // LD_addr_A(wCryPitch);
        // LD_A_H;
        // LD_addr_A(wCryPitch + 1);
        wram->wCryPitch += de[0];
        // POP_HL;

        // LD_A_hli;
        // LD_C_A;
        // LD_B_hl;
        // LD_HL(wCryLength);
        // LD_A_hli;
        // LD_H_hl;
        // LD_L_A;
        // ADD_HL_BC;

        // LD_A_L;
        // LD_addr_A(wCryLength);
        // LD_A_H;
        // LD_addr_A(wCryLength + 1);
        wram->wCryLength += de[1];
        // LD_A(1);
        // LD_addr_A(wStereoPanningMask);
        wram->wStereoPanningMask = 1;

        // CALLFAR(av_PlayCry);
        // struct cpu_registers_s reg = {.de = cryData->index};
        // SafeCallGB(av_PlayCry, &reg);
        v_PlayCry(cryData->index);
    }

// done:
    // POP_AF;
    // LDH_addr_A(rSVBK);
    // RET;
}

void PlayHitSound(void){
    // LD_A_addr(wNumHits);
    // CP_A(0x1);
    // IF_Z goto okay;
    // CP_A(0x4);
    // RET_NZ ;
    if(wram->wNumHits != 0x1 && wram->wNumHits != 0x4)
        return;

    const uint8_t typeMod = (wram->wTypeModifier & 0x7f);
// okay:
    // LD_A_addr(wTypeModifier);
    // AND_A(0x7f);
    // RET_Z ;
    if(typeMod == 0)
        return;

    // CP_A(EFFECTIVE);
    // LD_DE(SFX_DAMAGE);
    // IF_Z goto play;
    if(typeMod == EFFECTIVE)
        return PlaySFX(SFX_DAMAGE);

    // LD_DE(SFX_SUPER_EFFECTIVE);
    // IF_NC goto play;
    else if(typeMod > EFFECTIVE)
        return PlaySFX(SFX_SUPER_EFFECTIVE);

    // LD_DE(SFX_NOT_VERY_EFFECTIVE);
    else 
        return PlaySFX(SFX_NOT_VERY_EFFECTIVE);

// play:
    // CALL(aPlaySFX);
    // RET;
}

void BattleAnimAssignPals(void){
    // LDH_A_addr(hCGB);
    // AND_A_A;
    // IF_NZ goto cgb;
    if(hram.hCGB != 0) {
    // cgb:
        // LD_A(0b11100100);
        // LD_addr_A(wBGP);
        BattleAnimationDMGBGPaletteSet(0b11100100);
        // LD_addr_A(wOBP0);
        BattleAnimationDMGObjectPalette0Set(0b11100100);
        // LD_addr_A(wOBP1);
        BattleAnimationDMGObjectPalette1Set(0b11100100);
        BattleAnimApplyFullCGBPals();
        // RET;
        return;
    }
    // LDH_A_addr(hSGB);
    // AND_A_A;
    // LD_A(0b11100000);
    // IF_Z goto sgb;
    // LD_A(0b11110000);

// sgb:
    // LD_addr_A(wOBP0);
    BattleAnimationDMGObjectPalette0Set((hram.hSGB != 0)? 0b11110000: 0b11100000);
    // LD_A(0b11100100);
    // LD_addr_A(wBGP);
    BattleAnimationDMGBGPaletteSet(0b11100100);
    // LD_addr_A(wOBP1);
    BattleAnimationDMGObjectPalette1Set(0b11100100);
    // RET;
}

static void BattleAnimApplyFullCGBPals(void){
    const uint8_t bgp = BattleAnimationDMGBGPalette();
    const uint8_t obp0 = BattleAnimationDMGObjectPalette0();
    const uint8_t obp1 = BattleAnimationDMGObjectPalette1();

    gb_write(rBGP, bgp);
    gb_write(rOBP0, obp0);
    gb_write(rOBP1, obp1);
    if(hram.hCGB == 0)
        return;

    CopyPals(BattleAnimationPaletteOutput(false), BattleAnimationPaletteSource(false), bgp, 8);
    CopyPals(BattleAnimationPaletteOutput(true), BattleAnimationPaletteSource(true), obp0, 8);
    hram.hCGBPalUpdate = TRUE;
}

void ClearBattleAnims(void){
//  Clear animation block
    // LD_HL(wLYOverrides);
    memset(BattleAnimationScanlineOverrides(), 0, BATTLE_ANIMATION_SCANLINE_WORKSPACE_SIZE);
    memset(BattleAnimationScanlineScratch(), 0, BATTLE_ANIMATION_SCANLINE_WORKSPACE_SIZE);

    ResetNativeBattleAnimationState();

    // LD_HL(wFXAnimID);
    // LD_E_hl;
    // INC_HL;
    // LD_D_hl;
    // LD_HL(mBattleAnimations);
    // ADD_HL_DE;
    // ADD_HL_DE;
    // CALL(aGetBattleAnimPointer);
    sBattleAnim.script = BattleAnimations[BattleAnimationIdGet()];
    sBattleAnim.pos = 0;
    sBattleAnim.parent = NULL;
    sBattleAnim.parent_pos = 0;
    sBattleAnim.flags = 0;
    sBattleAnim.delay = 0;
    sBattleAnim.loops = 0;
    sBattleAnim.var = 0;
    // CALL(aBattleAnimAssignPals);
    BattleAnimAssignPals();
    // CALL(aBattleAnimDelayFrame);
    BattleAnimDelayFrame();
    // RET;
}

void BattleAnim_RevertPals(void){
    // CALL(aWaitTop);
    WaitTop();
    // LD_A(0b11100100);
    // LD_addr_A(wBGP);
    BattleAnimationDMGBGPaletteSet(0b11100100);
    // LD_addr_A(wOBP0);
    BattleAnimationDMGObjectPalette0Set(0b11100100);
    // LD_addr_A(wOBP1);
    BattleAnimationDMGObjectPalette1Set(0b11100100);
    BattleAnimApplyFullCGBPals();
    BattleSceneCameraSet(0, 0);
    // CALL(aBattleAnimDelayFrame);
    BattleAnimDelayFrame();
    // LD_A(0x1);
    // LDH_addr_A(hBGMapMode);
    hram.hBGMapMode = BGMAPMODE_UPDATE_TILES;
    // RET;
}

void BattleAnim_SetBGPals(uint8_t bgp){
    BattleAnimationDMGBGPaletteSet(bgp);
    // LDH_A_addr(hCGB);
    // AND_A_A;
    // RET_Z ;
    if(hram.hCGB == 0)
        return;
    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    // LD_A(MBANK(awBGPals1));
    // LDH_addr_A(rSVBK);
    // LD_HL(wBGPals2);
    // LD_DE(wBGPals1);
    // LDH_A_addr(rBGP);
    // LD_B_A;
    // LD_C(7);
    // CALL(aCopyPals);
    CopyPals(BattleAnimationPaletteOutput(false), BattleAnimationPaletteSource(false),
        BattleAnimationDMGBGPalette(), 7);
    // LD_HL(wOBPals2);
    // LD_DE(wOBPals1);
    // LDH_A_addr(rBGP);
    // LD_B_A;
    // LD_C(2);
    // CALL(aCopyPals);
    CopyPals(BattleAnimationPaletteOutput(true), BattleAnimationPaletteSource(true),
        BattleAnimationDMGBGPalette(), 2);
    // POP_AF;
    // LDH_addr_A(rSVBK);
    // LD_A(TRUE);
    // LDH_addr_A(hCGBPalUpdate);
    hram.hCGBPalUpdate = TRUE;
    // RET;
}

void BattleAnim_SetOBPals(uint8_t obp0){
    // LDH_addr_A(rOBP0);
    gb_write(rOBP0, obp0);
    // LDH_A_addr(hCGB);
    // AND_A_A;
    // RET_Z ;
    if(hram.hCGB == 0)
        return;
    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    // LD_A(MBANK(awOBPals1));
    // LDH_addr_A(rSVBK);
    // LD_HL(wOBPals2 + PALETTE_SIZE * PAL_BATTLE_OB_GRAY);
    // LD_DE(wOBPals1 + PALETTE_SIZE * PAL_BATTLE_OB_GRAY);
    // LDH_A_addr(rOBP0);
    // LD_B_A;
    // LD_C(2);
    // CALL(aCopyPals);
    CopyPals(BattleAnimationPaletteOutput(true) + PALETTE_SIZE * PAL_BATTLE_OB_GRAY,
        BattleAnimationPaletteSource(true) + PALETTE_SIZE * PAL_BATTLE_OB_GRAY, gb_read(rOBP0), 2);
    // POP_AF;
    // LDH_addr_A(rSVBK);
    // LD_A(TRUE);
    // LDH_addr_A(hCGBPalUpdate);
    hram.hCGBPalUpdate = TRUE;
    // RET;
}

void BattleAnim_UpdateOAM_All(void){
    // Rebuild only the temporary effect layer. Baseline battler and HUD scene
    // elements survive an animation frame reset.
    BeginBattleAnimationRenderFrame();
    size_t objectCount = BattleAnimationObjectCount();
    for(size_t i = 0; i < objectCount; i++) {
        struct BattleAnim* object = BattleAnimationObjectAt(i);
        if(object == NULL)
            break;
        if(object->index == 0)
            continue;
        DoBattleAnimFrame(object);
        // A callback may queue another object and grow the native pool.
        // Reacquire by index before the next operation.
        object = BattleAnimationObjectAt(i);
        if(object == NULL || object->index == 0)
            continue;
        BattleAnimOAMUpdate(object);
    }
}
