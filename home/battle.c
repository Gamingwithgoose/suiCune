#include "../constants.h"
#include "battle.h"
#include "pokemon.h"
#include <assert.h>
#include "copy.h"
#include "text.h"
#include "tilemap.h"
#include "map_objects.h"
#include "delay.h"
#include "../engine/battle/core.h"
#include "../engine/battle_anims/core.h"

struct BattleParticipant* BattleParticipantForSide(enum BattleSide side){
    assert(side == TURN_PLAYER || side == TURN_ENEMY);
    return side == TURN_PLAYER ? &gBattle.player : &gBattle.enemy;
}

uint16_t BattleApplyDamage(struct BattlePokemon* mon, uint16_t damage){
    assert(mon != NULL && mon->hp <= mon->maxHP);
    uint16_t applied = damage < mon->hp ? damage : mon->hp;
    mon->hp -= applied;
    return applied;
}

uint16_t BattleRestoreHP(struct BattlePokemon* mon, uint16_t amount){
    assert(mon != NULL && mon->hp <= mon->maxHP);
    uint16_t available = mon->maxHP - mon->hp;
    uint16_t restored = amount < available ? amount : available;
    mon->hp += restored;
    return restored;
}

// Import from the still-authoritative packed persistent party on entrance.
// Damage/stat calculations never read this record back during a turn.
void BattleLoadPartyPokemon(struct BattlePokemon* dest, const struct PartyMon* src){
    assert(dest != NULL && src != NULL);
    assert(GetSpeciesBaseData(src->mon.species) != NULL);
    *dest = (struct BattlePokemon){0};
    dest->species = src->mon.species;
    dest->item = src->mon.item;
    for(size_t i = 0; i < NUM_MOVES; ++i) {
        dest->moves[i] = src->mon.moves[i];
        dest->pp[i] = src->mon.PP[i];
    }
    dest->dvs = src->mon.DVs;
    dest->happiness = src->mon.happiness;
    dest->level = src->mon.level;
    dest->status = src->status;
    dest->hp = BigEndianToNative16(src->HP);
    dest->maxHP = BigEndianToNative16(src->maxHP);
    for(size_t i = 0; i < lengthof(dest->stats); ++i)
        dest->stats[i] = BigEndianToNative16(src->stats[i]);
    assert(dest->hp <= dest->maxHP);
}

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
    if(gBattle.turn != TURN_PLAYER)
    {
        // JR(mOTPartyAttr);
        assert(gBattle.enemy.partyIndex < PARTY_LENGTH);
        return wram->wOTPartyMon + gBattle.enemy.partyIndex;
    }

    // JR(mBattlePartyAttr);
    assert(gBattle.player.partyIndex < gPokemon.partyCount);
    return gPokemon.partyMon + gBattle.player.partyIndex;
}

struct PartyMon* OpponentPartyMon(void){
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // IF_Z goto ot;
    if(gBattle.turn == TURN_PLAYER)
    {
        // JR(mOTPartyAttr);
        assert(gBattle.enemy.partyIndex < PARTY_LENGTH);
        return wram->wOTPartyMon + gBattle.enemy.partyIndex;
    }

    // JR(mBattlePartyAttr);
    assert(gBattle.player.partyIndex < gPokemon.partyCount);
    return gPokemon.partyMon + gBattle.player.partyIndex;
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
    gBattle.turn = TURN_PLAYER; // Player's turn
}

void SetEnemyTurn(void){
    // LD_A(1);
    // LDH_addr_A(hBattleTurn);
    gBattle.turn = TURN_ENEMY; // Enemy's turn
}

void UpdateOpponentInParty(void){
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // JR_Z (mUpdateEnemyMonInParty);
    if(gBattle.turn == TURN_PLAYER)
        return UpdateEnemyMonInParty();

    // JR(mUpdateBattleMonInParty);
    return UpdateBattleMonInParty();
}

void UpdateUserInParty(void){
    // LDH_A_addr(hBattleTurn);
    // AND_A_A;
    // JR_Z (mUpdateBattleMonInParty);
    if(gBattle.turn == TURN_PLAYER)
        return UpdateBattleMonInParty();
    
    // JR(mUpdateEnemyMonInParty);
    return UpdateEnemyMonInParty();
}

//  Update level, status, current HP
void UpdateBattleMonInParty(void){
    // LD_A_addr(wCurBattleMon);
    return UpdateBattleMon(gBattle.player.partyIndex);
}

void UpdateBattleMon(uint8_t a){
    // Pursuit can finish updating the outgoing owner after selection changes.
    assert(a < gPokemon.partyCount);
    assert(gBattle.player.mon.hp <= gBattle.player.mon.maxHP);
    // LD_HL(wPartyMon1Level);
    // CALL(aGetPartyLocation);
    // LD_D_H;
    // LD_E_L;
    // uint16_t de = GetPartyLocation(wPartyMon1Level, a);
    struct PartyMon* de = gPokemon.partyMon + a;

    // LD_HL(wBattleMonLevel);
    // LD_BC(wBattleMonMaxHP - wBattleMonLevel);
    // JP(mCopyBytes);
    de->mon.level = gBattle.player.mon.level;
    de->status = gBattle.player.mon.status;

    de->HP = NativeToBigEndian16(gBattle.player.mon.hp);
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
    struct PartyMon* de = wram->wOTPartyMon + gBattle.enemy.partyIndex;

    // LD_HL(wEnemyMonLevel);
    // LD_BC(wEnemyMonMaxHP - wEnemyMonLevel);
    // JP(mCopyBytes);
    de->mon.level = gBattle.enemy.mon.level;
    de->status = gBattle.enemy.mon.status;

    de->HP = NativeToBigEndian16(gBattle.enemy.mon.hp);
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
    SetBattleSceneForegroundUIRegion(BATTLE_SCENE_FOREGROUND_UI_TEXTBOX,
        TEXTBOX_X * TILE_WIDTH, TEXTBOX_Y * TILE_WIDTH,
        TEXTBOX_WIDTH * TILE_WIDTH, TEXTBOX_HEIGHT * TILE_WIDTH);
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
    ClearBattleSceneForegroundUIRegion(BATTLE_SCENE_FOREGROUND_UI_TEXTBOX);
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
    if(BattleSceneDisplayActive()) {
        if(BattleSceneScanlineEffectGet() == BATTLE_SCENE_SCANLINE_NONE)
            return;
        // Background effects author the next scanline frame in scratch. This
        // commit makes the absolute samples visible to both native battlers
        // and the remaining generic background-plane projection.
        CopyBytes(BattleAnimationScanlineOverrides(), BattleAnimationScanlineScratch(), SCREEN_HEIGHT_PX);
        return;
    }
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
    CopyBytes(wram->wLYOverrides, wram->wLYOverridesBackup, wLYOverridesEnd - wLYOverrides);
    // RET;
}
