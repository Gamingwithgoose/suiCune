#include "../constants.h"
#include "pokemon.h"
#include "array.h"
#include "print_text.h"
#include "audio.h"
#include "copy.h"
#include "../engine/gfx/place_graphic.h"
#include "../engine/gfx/load_pics.h"
#include "../engine/pokemon/correct_nick_errors.h"
#include "../charmap.h"
#include "../data/pokemon/base_stats.h"
#include "../data/pokemon/pic_pointers.h"
#include "../data/pokemon/unown_pic_pointers.h"
#include "../data/pokemon/cries.h"
#include "../util/misc.h"
#include <stdlib.h>

bool IsAPokemon(SpeciesId species) {
    // Pokemon index 0 is not a pokemon
    if(species == 0) return false;

    // Eggs are pokemon
    if(species == EGG) return true;

    // Pokemon indicies in range are pokemon
    if(species <= NUM_POKEMON) return true;

    // Everything else is not a pokemon
    return false;
}

//  Draw an HP bar d tiles long at hl
//  Fill it up to e pixels
void DrawBattleHPBar(uint8_t* hl, uint8_t d, uint8_t e, uint8_t b, uint8_t c){
    // PUSH_HL;
    // PUSH_DE;
    // PUSH_BC;

//  Place 'HP:'
    // LD_A(0x60);
    // LD_hli_A;
    *(hl++) = 0x60;
    // LD_A(0x61);
    // LD_hli_A;
    *(hl++) = 0x61;

//  Draw a template
    // PUSH_HL;
    uint8_t* hl2 = hl;
    // LD_A(0x62);  // empty bar

    do {
    // template:
        // LD_hli_A;
        *(hl2++) = 0x62;
        // DEC_D;
        // IF_NZ goto template;
    } while(--d != 0);
    // LD_A(0x6b);  // bar end
    // ADD_A_B;
    // LD_hl_A;
    *(hl2) = 0x6b + b;
    // POP_HL;

//  Safety check # pixels
    // LD_A_E;
    // AND_A_A;
    // IF_NZ goto fill;
    if(e == 0) {
        // LD_A_C;
        // AND_A_A;
        // IF_Z goto done;
        if(c == 0)
            return;
        // LD_E(1);
        e = 1;
    }

    do {
    // fill:
        //  Keep drawing tiles until pixel length is reached
        // LD_A_E;
        // SUB_A(TILE_WIDTH);
        // IF_C goto lastbar;
        if(e < TILE_WIDTH) {
        // lastbar:
            // LD_A(0x62);  // empty bar
            // ADD_A_E;  // + e
            // LD_hl_A;
            *hl = 0x62 + e;
            break;
        }

        // LD_E_A;
        e -= TILE_WIDTH;
        // LD_A(0x6a);  // full bar
        // LD_hli_A;
        *(hl++) = 0x6a;
        // LD_A_E;
        // AND_A_A;
        // IF_Z goto done;
        // goto fill;
    } while(e != 0);

// done:
    // POP_BC;
    // POP_DE;
    // POP_HL;
    // RET;
}

void PrepMonFrontpic(tile_t* hl){
    // LD_A(0x1);
    // LD_addr_A(wBoxAlignment);
    wram->wBoxAlignment = 0x1;
    return v_PrepMonFrontpic(hl);
}

void v_PrepMonFrontpic(tile_t* hl){
    // LD_A_addr(wCurPartySpecies);
    // CALL(aIsAPokemon);
    // IF_C goto not_pokemon;
    if(IsAPokemon(wram->wCurPartySpecies)) {
        // PUSH_HL;
        // LD_DE(vTiles2);
        // PREDEF(pGetMonFrontpic);
        GetMonFrontpic(vram->vTiles2);
        // POP_HL;
        // XOR_A_A;
        // LDH_addr_A(hGraphicStartTile);
        hram.hGraphicStartTile = 0;
        // LD_BC((7 << 8) | 7);
        // PREDEF(pPlaceGraphic);
        PlaceGraphicYStagger(hl, 7, 7);
        // XOR_A_A;
        // LD_addr_A(wBoxAlignment);
        wram->wBoxAlignment = 0;
        // RET;
        return;
    }

// not_pokemon:
    // XOR_A_A;
    // LD_addr_A(wBoxAlignment);
    wram->wBoxAlignment = 0;
    // INC_A;
    // LD_addr_A(wCurPartySpecies);
    // RET;
    wram->wCurPartySpecies = 1;
}

void PlayStereoCry(SpeciesId species){
    // PUSH_AF;
    // LD_A(1);
    // LD_addr_A(wStereoPanningMask);
    wram->wStereoPanningMask = 1;
    // POP_AF;
    // CALL(av_PlayMonCry);
    v_PlayMonCry(species);
    // CALL(aWaitSFX);
    WaitSFX();
    // RET;
}

//  Don't wait for the cry to end.
//  Used during pic animations.
void PlayStereoCry2(SpeciesId species){
    // PUSH_AF;
    // LD_A(1);
    // LD_addr_A(wStereoPanningMask);
    wram->wStereoPanningMask = 1;
    // POP_AF;
    // JP(mv_PlayMonCry);
    return v_PlayMonCry(species);
}

void PlayMonCry(SpeciesId species){
    // CALL(aPlayMonCry2);
    PlayMonCry2(species);
    // CALL(aWaitSFX);
    WaitSFX();
    // RET;
}

//  Don't wait for the cry to end.
void PlayMonCry2(SpeciesId species){
    // PUSH_AF;
    // XOR_A_A;
    // LD_addr_A(wStereoPanningMask);
    wram->wStereoPanningMask = 0;
    // LD_addr_A(wCryTracks);
    wram->wCryTracks = 0;
    // POP_AF;
    // CALL(av_PlayMonCry);
    v_PlayMonCry(species);
    // RET;
}

void v_PlayMonCry(SpeciesId species){
    // PUSH_HL;
    // PUSH_DE;
    // PUSH_BC;

    // CALL(aGetCryIndex);
    // IF_C goto done;

    int16_t idx = GetCryIndex(species);
    if(idx == -1)
        return;

    // LD_E_C;
    // LD_D_B;
    // CALL(aPlayCry);

    // PUSH_DE;
    // REG_DE = (uint16_t)idx;
    PlayCry((uint16_t)idx);
    // POP_DE;


// done:
    // POP_BC;
    // POP_DE;
    // POP_HL;
    // RET;
}

//  Load cry bc.
const struct PokemonCry* LoadCry(SpeciesId species){
    int16_t index = GetCryIndex(species);
    if(index < 0) 
        return NULL;
    uint16_t i = (uint16_t)index;

    const struct PokemonCry* hl = PokemonCries + i;

    wram->wCryPitch = hl->pitch;
    wram->wCryLength = hl->length;

    return hl;
}

int16_t GetCryIndex(SpeciesId species){
    if(species == 0 || species >= (NUM_POKEMON + 1))
        return -1;
    return (int16_t)species;
}

//  Print wTempMonLevel at hl
void PrintLevel(tile_t* hl, uint8_t lvl){
    // LD_A_addr(wTempMonLevel);
    // LD_hl(0x6e);
    // INC_HL;
    uint8_t a = lvl;
    *(hl++) = CHAR_LV;

//  How many digits?
    if(a < 100) {
        // LD_C(2);
        // CP_A(100);  // This is distinct from MAX_LEVEL.
        // JR_C (mPrint8BitNumLeftAlign);
        return Print8BitNumLeftAlign(hl, a, 2);
    }

//  3-digit numbers overwrite the :L.
    // DEC_HL;
    // INC_C;
    // JR(mPrint8BitNumLeftAlign);
    return Print8BitNumLeftAlign(hl - 1, a, 3);
}

void PrintLevel_Force3Digits(tile_t* hl, uint8_t a){
    //  Print :L and all 3 digits
    // LD_hl(0x6e);
    *hl = 0x6e;
    // INC_HL;
    // LD_C(3);

    return Print8BitNumLeftAlign(hl + 1, a, 3);
}

void Print8BitNumLeftAlign(tile_t* hl, uint8_t a, uint8_t c){
    // LD_addr_A(wTextDecimalByte);
    wram->wTextDecimalByte = a;
    // LD_DE(wTextDecimalByte);
    // LD_B(PRINTNUM_LEFTALIGN | 1);
    // JP(mPrintNum);
    PrintNum(hl, &wram->wTextDecimalByte, PRINTNUM_LEFTALIGN | 1, c);
    return;
}

//  //  unreferenced
void GetNthMove(void){
    // LD_HL(wListMoves_MoveIndicesBuffer);
    // LD_C_A;
    // LD_B(0);
    // ADD_HL_BC;
    // LD_A_hl;
    // RET;
}

const struct BaseData* GetSpeciesBaseData(SpeciesId species) {
    if(species == 0 || species == EGG || species > NUM_POKEMON)
        return NULL;
    return BasePokemonData + (species - 1);
}

bool ConvertBoxMonToNative(struct NativeBoxMon* dest, const struct BoxMon* src) {
    if(dest == NULL || src == NULL)
        return false;

    dest->species = src->species;
    dest->item = src->item;
    for(size_t i = 0; i < NUM_MOVES; ++i)
        dest->moves[i] = src->moves[i];
    dest->id = src->id;
    dest->exp = ((uint32_t)src->exp[0] << 16) | ((uint32_t)src->exp[1] << 8) | src->exp[2];
    for(size_t i = 0; i < lengthof(dest->statExp); ++i)
        dest->statExp[i] = BigEndianToNative16(src->statExp[i]);
    dest->DVs = src->DVs;
    CopyBytes(dest->PP, src->PP, sizeof(dest->PP));
    dest->happiness = src->happiness;
    dest->pokerusStatus = src->pokerusStatus;
    CopyBytes(dest->caughtData, src->caughtData, sizeof(dest->caughtData));
    dest->level = src->level;
    return true;
}

bool ConvertNativeBoxMonToLegacy(struct BoxMon* dest, const struct NativeBoxMon* src) {
    if(dest == NULL || src == NULL || src->exp > 0x00ffffffu)
        return false;

    LegacySpeciesId species;
    LegacyItemId item;
    LegacyMoveId moves[NUM_MOVES];
    if(!TrySpeciesIdToLegacy(src->species, &species)
    || !TryItemIdToLegacy(src->item, &item))
        return false;
    for(size_t i = 0; i < NUM_MOVES; ++i) {
        if(!TryMoveIdToLegacy(src->moves[i], &moves[i]))
            return false;
    }

    // All narrowing is validated before the legacy destination is mutated.
    dest->species = species;
    dest->item = item;
    CopyBytes(dest->moves, moves, sizeof(dest->moves));
    dest->id = src->id;
    dest->exp[0] = (uint8_t)(src->exp >> 16);
    dest->exp[1] = (uint8_t)(src->exp >> 8);
    dest->exp[2] = (uint8_t)src->exp;
    for(size_t i = 0; i < lengthof(src->statExp); ++i)
        dest->statExp[i] = NativeToBigEndian16(src->statExp[i]);
    dest->DVs = src->DVs;
    CopyBytes(dest->PP, src->PP, sizeof(dest->PP));
    dest->happiness = src->happiness;
    dest->pokerusStatus = src->pokerusStatus;
    CopyBytes(dest->caughtData, src->caughtData, sizeof(dest->caughtData));
    dest->level = src->level;
    return true;
}

bool ConvertPartyMonToNative(struct NativePartyMon* dest, const struct PartyMon* src) {
    if(dest == NULL || src == NULL || !ConvertBoxMonToNative(&dest->mon, &src->mon))
        return false;

    dest->status = src->status;
    dest->unused = src->unused;
    dest->isEgg = false;
    dest->HP = BigEndianToNative16(src->HP);
    dest->maxHP = BigEndianToNative16(src->maxHP);
    for(size_t i = 0; i < lengthof(dest->stats); ++i)
        dest->stats[i] = BigEndianToNative16(src->stats[i]);
    return true;
}

bool ConvertNativePartyMonToLegacy(struct PartyMon* dest, const struct NativePartyMon* src) {
    if(dest == NULL || src == NULL || !ConvertNativeBoxMonToLegacy(&dest->mon, &src->mon))
        return false;

    dest->status = src->status;
    dest->unused = src->unused;
    dest->HP = NativeToBigEndian16(src->HP);
    dest->maxHP = NativeToBigEndian16(src->maxHP);
    for(size_t i = 0; i < lengthof(src->stats); ++i)
        dest->stats[i] = NativeToBigEndian16(src->stats[i]);
    return true;
}

bool PlayerPartyMonIsEgg(uint8_t partyIndex) {
    return partyIndex < gPokemon.partyCount
        && gPokemon.legacyPartySpecies[partyIndex] == EGG;
}

bool ConvertPlayerPartyMonToNative(struct NativePartyMon* dest, uint8_t partyIndex) {
    if(dest == NULL || partyIndex >= gPokemon.partyCount)
        return false;
    if(!ConvertPartyMonToNative(dest, &gPokemon.partyMon[partyIndex]))
        return false;
    dest->isEgg = PlayerPartyMonIsEgg(partyIndex);
    return true;
}

bool ConvertBaseDataToLegacy(struct LegacyBaseData* dest, const struct BaseData* src) {
    if(!TryDexIdToLegacy(src->dexNo, &dest->dexNo)
    || !TryItemIdToLegacy(src->item1, &dest->items[0])
    || !TryItemIdToLegacy(src->item2, &dest->items[1]))
        return false;

    CopyBytes(dest->stats, src->stats, sizeof(dest->stats));
    CopyBytes(dest->types, src->types, sizeof(dest->types));
    dest->catchRate = src->catchRate;
    dest->exp = src->exp;
    dest->gender = src->gender;
    dest->unknown1 = src->unknown1;
    dest->eggSteps = src->eggSteps;
    dest->unknown2 = src->unknown2;
    dest->picSize = src->picSize;
    dest->unusedFrontpic = src->unusedFrontpic;
    dest->unusedBackpic = src->unusedBackpic;
    dest->growthRate = src->growthRate;
    dest->eggGroups = src->eggGroups;
    CopyBytes(dest->TMHM, src->TMHM, sizeof(dest->TMHM));
    return true;
}

void GetBaseData(SpeciesId species){
    // printf("%s:: %d\n", __func__, species);
//  Egg doesn't have BaseData
    if(species == EGG)
    {
        //LD_DE(mUnusedEggPic);

    //  Sprite dimensions
        wram->wBasePicSize = 0x55;  // 5x5

    //  Beta front and back sprites
    //  (see pokegold-spaceworld's data/pokemon/base_stats/*)
        // gb_write16(wBaseUnusedFrontpic,     mUnusedEggPic);
        // gb_write16(wBaseUnusedFrontpic + 2, mUnusedEggPic);
    }
    else 
    {
    // If the species value is OOB, get Bulbasaur's data as a failsafe.
        if(species == 0 || species > NUM_POKEMON)
            species = BULBASAUR;
        const struct BaseData* hl = GetSpeciesBaseData(species);
    //  Get BaseData
        //DEC_A;
        //LD_BC(BASE_DATA_SIZE);
        //LD_HL(mBaseData);
        //CALL(aAddNTimes);
        //LD_DE(wCurBaseData);
        //LD_BC(BASE_DATA_SIZE);
        //CALL(aCopyBytes);
        struct LegacyBaseData legacy;
        if(!ConvertBaseDataToLegacy(&legacy, hl)) {
            log_err("Species %u base data cannot enter the legacy WRAM cache.\n", species);
            abort();
        }
        CopyBytes(&wram->wBaseDexNo, &legacy, sizeof(legacy));
        // If our pic size is 0, we haven't initialized it yet.
        if(hl->picSize == 0) {
            // We dynamically get the base pic size by loading the frontpic and counting how many tiles
            // horizontally the pic takes up and then cache the result in BasePokemonData.

            // If our species is unown, we use the Unown pic table.
            const char* (*const hl)[2] = ((species == UNOWN)? UnownPicPointers: PokemonPicPointers);
            // unown_letter_t c = wram->wUnownLetter;

            // We either get the species or an unown letter, which will be the index into the table.
            uint16_t a = (species == UNOWN)? 1: species;
            const char *path = hl[a - 1][0];
            int w, h;
            LoadDimensionsFromPNG(path, &w, &h);

            // We assume pics have a square proportion (if a pic has a non-square proportion, we must
            // do something more involved since a frontpic image has multiple frames, stacked vertically)
            uint8_t size = (uint8_t)((((w / TILE_WIDTH) & 0xf) << 4) | ((w / TILE_WIDTH) & 0xf));
            wram->wBasePicSize = size;

            // We cache it back into the table.
            BasePokemonData[species - 1].picSize = size;
        }
    }

    //  Replace Pokedex # with species
    if(!TrySpeciesIdToLegacy(species, &wram->wBaseDexNo)) {
        log_err("Species %u cannot enter the legacy WRAM base-data cache.\n", species);
        abort();
    }
}

//  Get nickname a from list hl.
void GetNickname(const uint8_t* hl, uint8_t a){
    // PUSH_HL;
    // PUSH_BC;

    // CALL(aSkipNames);
    hl = SkipNames(hl, a);

    // LD_DE(wStringBuffer1);

    CopyBytes(wram->wStringBuffer1, hl, MON_NAME_LENGTH);

    //PUSH_DE;
    //LD_BC(MON_NAME_LENGTH);
    //CALL(aCopyBytes);
    //POP_DE;

    // CALLFAR(aCorrectNickErrors);
    CorrectNickErrors(wram->wStringBuffer1);

    // POP_BC;
    // POP_HL;
    // RET;
}

uint8_t* GetCurNickname(void){
    CopyBytes(wram->wStringBuffer1, gPokemon.partyMonNickname[wram->wCurPartyMon], MON_NAME_LENGTH);
    CorrectNickErrors(wram->wStringBuffer1);
    return wram->wStringBuffer1;
}
