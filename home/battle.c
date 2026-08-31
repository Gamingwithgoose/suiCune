#include "../constants.h"
#include "battle.h"
#include "copy.h"
#include "text.h"
#include "tilemap.h"
#include "map_objects.h"
#include "delay.h"
#include "../engine/battle/core.h"
#include "../engine/battle_anims/core.h"

//  //  unreferenced
//  Probably used in gen 1 to convert index number to dex number
//  Not required in gen 2 because index number == dex number
void GetDexNumber(void){
    // PUSH_HL;
    // LD_A_B;
    // DEC_A;
    // LD_B(0);
    // ADD_HL_BC;
    // LD_HL(mBaseData + BASE_DEX_NO);
    // LD_BC(BASE_DATA_SIZE);
    // CALL(aAddNTimes);
    // LD_A(BANK(aBaseData));
    // CALL(aGetFarWord);
    // LD_B_L;
    // LD_C_H;
    // POP_HL;
    // RET;
}

struct PartyMon* UserPartyMon(void){
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto ot;
    if(hram.hBattleTurn != TURN_PLAYER)
    {
        // JR(mOTPartyAttr);
        return wram->wOTPartyMon + wram->wCurOTMon;
    }

    // JR(mBattlePartyAttr);
    return gPokemon.partyMon + wram->wCurPartyMon;
}

struct PartyMon* OpponentPartyMon(void){
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto ot;
    if(hram.hBattleTurn == TURN_PLAYER)
    {
        // JR(mOTPartyAttr);
        return wram->wOTPartyMon + wram->wCurOTMon;
    }

    // JR(mBattlePartyAttr);
    return gPokemon.partyMon + wram->wCurPartyMon;
}

void ResetDamage(void){
    // XOR_A_A;
    // LD_addr_A(wCurDamage);
    // LD_addr_A(wCurDamage + 1);
    wram->wCurDamage = 0;
}

void SetPlayerTurn(void){
    // XOR_A_A;
    // LDH_addr_A(hBattleTurn);
    hram.hBattleTurn = TURN_PLAYER; // Player's turn
}

void SetEnemyTurn(void){
    // LD_A(1);
    // LDH_addr_A(hBattleTurn);
    hram.hBattleTurn = TURN_ENEMY; // Enemy's turn
}

void UpdateOpponentInParty(void){
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // JR_Z (mUpdateEnemyMonInParty);
    if(hram.hBattleTurn == TURN_PLAYER)
        return UpdateEnemyMonInParty();

    // JR(mUpdateBattleMonInParty);
    return UpdateBattleMonInParty();
}

void UpdateUserInParty(void){
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // JR_Z (mUpdateBattleMonInParty);
    if(hram.hBattleTurn == TURN_PLAYER)
        return UpdateBattleMonInParty();
    
    // JR(mUpdateEnemyMonInParty);
    return UpdateEnemyMonInParty();
}

//  Update level, status, current HP
void UpdateBattleMonInParty(void){
    // LD_A_addr(wCurBattleMon);
    return UpdateBattleMon(wram->wCurBattleMon);
}

void UpdateBattleMon(uint8_t a){
    // LD_HL(wPartyMon1Level);
    // CALL(aGetPartyLocation);
    // LD_D_H;
    // LD_E_L;
    // uint16_t de = GetPartyLocation(wPartyMon1Level, a);
    struct PartyMon* de = gPokemon.partyMon + a;

    // LD_HL(wBattleMonLevel);
    // LD_BC(wBattleMonMaxHP - wBattleMonLevel);
    // JP(mCopyBytes);
    de->mon.level = wram->wBattleMon.level;
    de->status = wram->wBattleMon.status[0];
    de->unused = wram->wBattleMon.status[1];
    de->HP = wram->wBattleMon.hp;
}

//  Update level, status, current HP
void UpdateEnemyMonInParty(void){
//  No wildmons.
    // LD_A_addr(wBattleMode);
    // DEC_A;
    // RET_Z ;
    if(wram->wBattleMode == WILD_BATTLE)
        return;

    // LD_A_addr(wCurOTMon);
    // LD_HL(wOTPartyMon1Level);
    // CALL(aGetPartyLocation);
    // LD_D_H;
    // LD_E_L;
    struct PartyMon* de = wram->wOTPartyMon + wram->wCurOTMon;

    // LD_HL(wEnemyMonLevel);
    // LD_BC(wEnemyMonMaxHP - wEnemyMonLevel);
    // JP(mCopyBytes);
    de->mon.level = wram->wEnemyMon.level;
    de->status = wram->wEnemyMon.status[0];
    de->unused = wram->wEnemyMon.status[1];
    de->HP = wram->wEnemyMon.hp;
}

void RefreshBattleHuds(void){
    // CALL(aUpdateBattleHuds);
    UpdateBattleHuds();
    // LD_C(3);
    // CALL(aDelayFrames);
    DelayFrames(3);
    // JP(mWaitBGMap);
    WaitBGMap();
}

void UpdateBattleHuds(void){
    // FARCALL(aUpdatePlayerHUD);
    UpdatePlayerHUD();
    // FARCALL(aUpdateEnemyHUD);
    UpdateEnemyHUD();
    // RET;

// INCLUDE "home/battle_vars.asm"

}

// For mobile link battles only.
void MobileTextBorder(void){
    // LD_A_addr(wLinkMode);
    // CP_A(LINK_MOBILE);
    // RET_C ;
    if(wram->wLinkMode < LINK_MOBILE)
        return;

// Draw a cell phone icon at the
// top right corner of the border.
    // hlcoord(19, 12, wTilemap);
    // LD_hl(0x5e);  // top
    *coord(19, 12, wram->wTilemap) = 0x5e;  // top
    // hlcoord(19, 13, wTilemap);
    // LD_hl(0x5f);  // bottom
    *coord(19, 13, wram->wTilemap) = 0x5f;  // bottom
}

//  Open a textbox and print text at hl.
void BattleTextbox(const struct TextCmd* hl){
    // PUSH_HL;
    // CALL(aSpeechTextbox);
    SpeechTextbox();

    // CALL(aMobileTextBorder);
    MobileTextBorder();

    // CALL(aUpdateSprites);
    UpdateSprites();

    // CALL(aApplyTilemap);
    ApplyTilemap();

    // POP_HL;
    // CALL(aPrintTextboxText);
    PrintTextboxText(hl);
    // RET;
}

//  Open a textbox and print battle text at 20:hl.
void StdBattleTextbox(const struct TextCmd* hl){
    // LDH_A_addr(hROMBank);
    // PUSH_AF;

    // LD_A(BANK(aBattleText));
    // RST(aBankswitch);

    // CALL(aBattleTextbox);
    BattleTextbox(hl);

    // POP_AF;
    // RST(aBankswitch);
    // RET;
}

void PushLYOverrides(void){
    // LDH_A_addr(hLCDCPointer);
    // AND_A_A;
    // RET_Z ;
    if(hram.hLCDCPointer == 0)
        return;

    // LD_A(LOW(wLYOverridesBackup));
    // LD_addr_A(wRequested2bppSource);
    // LD_A(HIGH(wLYOverridesBackup));
    // LD_addr_A(wRequested2bppSource + 1);

    // LD_A(LOW(wLYOverrides));
    // LD_addr_A(wRequested2bppDest);
    // LD_A(HIGH(wLYOverrides));
    // LD_addr_A(wRequested2bppDest + 1);

    // LD_A((wLYOverridesEnd - wLYOverrides) / LEN_2BPP_TILE);
    // LD_addr_A(wRequested2bppSize);
    if(BattleAnimationPresentationActive()) {
        CopyBytes(BattleAnimationScanlineOverrides(), BattleAnimationScanlineScratch(), SCREEN_HEIGHT_PX);
        return;
    }
    CopyBytes(wram->wLYOverrides, wram->wLYOverridesBackup, wLYOverridesEnd - wLYOverrides);
    // RET;
}
