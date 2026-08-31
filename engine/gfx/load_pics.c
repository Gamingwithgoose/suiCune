#include "../../constants.h"
#include "load_pics.h"
#include "../../home/pokemon.h"
#include "../../home/copy.h"
#include "../../home/tilemap.h"
#include "../../data/pokemon/pic_pointers.h"
#include "../../data/pokemon/unown_pic_pointers.h"
#include "../../data/trainers/pic_pointers.h"
#include <stdlib.h>
#include <string.h>

static void CopyAnimatedFrontpicTiles(uint8_t* dest, const uint8_t* source, size_t tileCount);
static void FixBackpicAlignment(uint8_t* hl, uint8_t c);
static void PadFrontpic(uint8_t* hl, uint8_t* de, uint8_t b);
static void LoadOrientedFrontpic(uint8_t** hl, uint8_t** de, uint8_t c);
static uint8_t* LoadFrontpicPixels(uint8_t* de, uint8_t frame, uint8_t paddedPixels[7 * 7 * LEN_2BPP_TILE]);
static void BuildAnimatedEnemyFrontpic(uint8_t* hl, const uint8_t paddedPixels[7 * 7 * LEN_2BPP_TILE], const uint8_t* rawPixels);

struct NativeFrontpicSource {
    uint8_t* pixels;
    size_t tileCapacity;
};

static struct NativeFrontpicSource sNativeFrontpicSource;

static uint8_t* NativeFrontpicSourcePixels(size_t requiredTiles){
    if(requiredTiles > sNativeFrontpicSource.tileCapacity) {
        size_t capacity = sNativeFrontpicSource.tileCapacity == 0 ? 128 : sNativeFrontpicSource.tileCapacity;
        while(capacity < requiredTiles) {
            if(capacity > SIZE_MAX / 2)
                abort();
            capacity *= 2;
        }
        if(capacity > SIZE_MAX / LEN_2BPP_TILE)
            abort();
        uint8_t* pixels = realloc(sNativeFrontpicSource.pixels, capacity * LEN_2BPP_TILE);
        if(pixels == NULL)
            abort();
        memset(pixels + sNativeFrontpicSource.tileCapacity * LEN_2BPP_TILE, 0,
            (capacity - sNativeFrontpicSource.tileCapacity) * LEN_2BPP_TILE);
        sNativeFrontpicSource.pixels = pixels;
        sNativeFrontpicSource.tileCapacity = capacity;
    }
    return sNativeFrontpicSource.pixels;
}

//  Return Unown letter in wUnownLetter based on DVs at hl
uint8_t GetUnownLetter(uint16_t dvs){
//  Take the middle 2 bits of each DV and place them in order:
//     atk  def  spd  spc
//     .ww..xx.  .yy..zz.
// atk
    // LD_A_hl;
    // AND_A(0b01100000);
    // SLA_A;
    // LD_B_A;
    uint8_t b = (LOW(dvs) & 0b01100000) << 1;
// def
    // LD_A_hli;
    // AND_A(0b00000110);
    // SWAP_A;
    // SRL_A;
    // OR_A_B;
    // LD_B_A;
    b |= (LOW(dvs) & 0b00000110) << 3;
// spd
    // LD_A_hl;
    // AND_A(0b01100000);
    // SWAP_A;
    // SLA_A;
    // OR_A_B;
    // LD_B_A;
    b |= (HIGH(dvs) & 0b01100000) >> 3;
// spc
    // LD_A_hl;
    // AND_A(0b00000110);
    // SRL_A;
    // OR_A_B;
    b |= (HIGH(dvs) & 0b00000110) >> 1;

//  Divide by 10 to get 0-25
    // LDH_addr_A(hDividend + 3);
    // XOR_A_A;
    // LDH_addr_A(hDividend);
    // LDH_addr_A(hDividend + 1);
    // LDH_addr_A(hDividend + 2);
    // LD_A(0xff / NUM_UNOWN + 1);
    // LDH_addr_A(hDivisor);
    // LD_B(4);
    // CALL(aDivide);
    uint8_t quot = b / 10;

//  Increment to get 1-26
    // LDH_A_addr(hQuotient + 3);
    // INC_A;
    // LD_addr_A(wUnownLetter);
    wram->wUnownLetter = quot + 1;
    // RET;
    return quot + 1;
}

void GetMonFrontpic(uint8_t* de){
    // LD_A_addr(wCurPartySpecies);
    // LD_addr_A(wCurSpecies);
    wram->wCurSpecies = wram->wCurPartySpecies;
    // CALL(aIsAPokemon);
    // RET_C ;
    if(!IsAPokemon(wram->wCurSpecies))
        return;
    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    uint8_t svbk = gb_read(rSVBK);
    // CALL(av_GetFrontpic);
    v_GetFrontpic(de, 0);
    // POP_AF;
    // LDH_addr_A(rSVBK);
    gb_write(rSVBK, svbk);
    // RET;
}

void GetAnimatedFrontpic(uint8_t* de, uint8_t frame){
    // LD_A_addr(wCurPartySpecies);
    // LD_addr_A(wCurSpecies);
    wram->wCurSpecies = wram->wCurPartySpecies;
    // CALL(aIsAPokemon);
    // RET_C ;
    if(!IsAPokemon(wram->wCurSpecies))
        return;
    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    uint8_t svbk = gb_read(rSVBK);
    // XOR_A_A;
    // LDH_addr_A(hBGMapMode);
    hram.hBGMapMode = BGMAPMODE_NONE;
    uint8_t paddedPixels[7 * 7 * LEN_2BPP_TILE];
    uint8_t* rawPixels = LoadFrontpicPixels(de, frame, paddedPixels);
    BuildAnimatedEnemyFrontpic(de, paddedPixels, rawPixels);
    // POP_AF;
    // LDH_addr_A(rSVBK);
    gb_write(rSVBK, svbk);
    // RET;
}

bool LoadNativeFrontpicPixels(uint8_t* de, uint8_t frame){
    if(!IsAPokemon(wram->wCurSpecies))
        return false;
    uint8_t paddedPixels[7 * 7 * LEN_2BPP_TILE];
    LoadFrontpicPixels(de, frame, paddedPixels);
    return true;
}

void v_GetFrontpic(uint8_t* de, uint8_t frame){
    uint8_t paddedPixels[7 * 7 * LEN_2BPP_TILE];
    LoadFrontpicPixels(de, frame, paddedPixels);
}

static uint8_t* LoadFrontpicPixels(uint8_t* de, uint8_t frame, uint8_t paddedPixels[7 * 7 * LEN_2BPP_TILE]){
    // PUSH_DE;
    // CALL(aGetBaseData);
    GetBaseData(wram->wCurSpecies);
    // LD_A_addr(wBasePicSize);
    // AND_A(0xf);
    // LD_B_A;
    uint8_t b = wram->wBasePicSize & 0xf;
    uint8_t c = (wram->wBasePicSize & 0xf0) >> 4;
    // PUSH_BC;
    // CALL(aGetFrontpicPointer);
    int size = c * b;
    const char* de2 = GetFrontpicPointer();
    size_t sourceTiles = size;
    if(sourceTiles < 128)
        sourceTiles = 128;
    uint8_t* rawPixels = NativeFrontpicSourcePixels(sourceTiles);
    // log_debug("Loading frame %d (tile %d, base %d,%d) of %s.\n", frame, size * frame, b, c, de2);
    LoadPNG2bppAssetSectionToVRAM(rawPixels, de2, size * frame, size);
    // POP_BC;
    // The padded form is transient native data rather than shared WRAM.
    // CALL(aPadFrontpic);
    PadFrontpic(paddedPixels, rawPixels, b);
    // POP_HL;
    // PUSH_HL;
    // LD_DE(wDecompressScratch);
    // LD_C(7 * 7);
    // LDH_A_addr(hROMBank);
    // LD_B_A;
    // CALL(aGet2bpp);
    // POP_HL;
    // RET;
    CopyBytes(de, paddedPixels, 7 * 7 * LEN_2BPP_TILE);
    return rawPixels;
}

const char* GetFrontpicPointer(void){
    // LD_A_addr(wCurPartySpecies);
    // CP_A(UNOWN);
    // IF_Z goto unown;
    const char *(*hl)[2];

    uint8_t a = wram->wCurPartySpecies;
    if(a != UNOWN) {
        // LD_A_addr(wCurPartySpecies);
        // LD_D(BANK(aPokemonPicPointers));
        // goto ok;
        hl = PokemonPicPointers;
    }
    else {
    // unown:
        // LD_A_addr(wUnownLetter);
        // LD_D(BANK(aUnownPicPointers));
        a = wram->wUnownLetter;
        hl = UnownPicPointers;
    }

// ok:
// These are assumed to be at the same address in their respective banks.
    //assert ['PokemonPicPointers == UnownPicPointers'];
    // LD_HL(mPokemonPicPointers);
    // DEC_A;
    // LD_BC(6);
    // CALL(aAddNTimes);
    // LD_A_D;
    // CALL(aGetFarByte);
    // CALL(aFixPicBank);
    const char* p = hl[a - 1][0];
    // PUSH_AF;
    // INC_HL;
    // LD_A_D;
    // CALL(aGetFarWord);
    // POP_BC;
    // RET;
    return p;
}

static void BuildAnimatedEnemyFrontpic(uint8_t* hl, const uint8_t paddedPixels[7 * 7 * LEN_2BPP_TILE], const uint8_t* rawPixels){
    // LD_A(BANK(vTiles3));
    // LDH_addr_A(rVBK);
    // gb_write(rVBK, MBANK(vTiles3));
    hl += VRAM_BANK_SIZE;
    // PUSH_HL;
    // LD_DE(wDecompressScratch);
    // LD_C(7 * 7);
    // LDH_A_addr(hROMBank);
    // LD_B_A;
    // CALL(aGet2bpp);
    // LoadPNG2bppAssetSectionToVRAM(wram->wDecompressScratch, GetFrontpicPointer(), 0, 7 * 7);
    CopyBytes(hl, paddedPixels, 7 * 7 * LEN_2BPP_TILE);
    // POP_HL;
    // LD_DE(7 * 7 * LEN_2BPP_TILE);
    // ADD_HL_DE;
    // PUSH_HL;
    hl += 7 * 7 * LEN_2BPP_TILE;
    // LD_A(BANK(wBasePicSize));
    // LD_HL(wBasePicSize);
    // CALL(aGetFarWRAMByte);
    // POP_HL;
    // AND_A(0xf);
    uint8_t a = wram->wBasePicSize & 0xf;
    size_t tileCount = (size_t)a * a;
    uint8_t animatedPixels[7 * 7 * LEN_2BPP_TILE];
    memcpy(animatedPixels, paddedPixels, sizeof(animatedPixels));
    CopyAnimatedFrontpicTiles(animatedPixels, rawPixels, tileCount);
    // POP_BC;
    // POP_HL;
    // LD_DE(wDecompressScratch);
    // LDH_A_addr(hROMBank);
    // LD_B_A;
    // CALL(aGet2bpp);
    CopyBytes(hl, animatedPixels, 7 * 7 * LEN_2BPP_TILE);
    // XOR_A_A;
    // LDH_addr_A(rVBK);
    // gb_write(rVBK, 0);
    // RET;

}

static void CopyAnimatedFrontpicTiles(uint8_t* dest, const uint8_t* source, size_t tileCount){
    for(size_t tile = 0; tile < tileCount; tile++) {
        uint8_t* write = dest + tile * LEN_2BPP_TILE;
        const uint8_t* read = source + tile * LEN_2BPP_TILE;
        for(size_t byte = 0; byte < LEN_2BPP_TILE; byte++) {
            uint8_t pixelByte = *(read++);
            if(wram->wBoxAlignment != 0) {
                uint8_t reversed = 0;
                for(uint8_t bit = 0; bit < 8; bit++) {
                    reversed = (uint8_t)((reversed << 1) | (pixelByte & 1));
                    pixelByte >>= 1;
                }
                pixelByte = reversed;
            }
            *(write++) = pixelByte;
        }
    }
}

void GetMonBackpic(uint8_t* de, species_t species){
    // LD_A_addr(wCurPartySpecies);
    // CALL(aIsAPokemon);
    // RET_C ;
    LoadNativeBackpicPixels(de, species);
}

bool LoadNativeBackpicPixels(uint8_t* de, species_t species){
    if(!IsAPokemon(species))
        return false;

    // LD_A_addr(wCurPartySpecies);
    // LD_B_A;
    species_t b = species;
    // LD_A_addr(wUnownLetter);
    // LD_C_A;
    unown_letter_t c = wram->wUnownLetter;
    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    // LD_A(MBANK(awDecompressScratch));
    // LDH_addr_A(rSVBK);
    // PUSH_DE;

// These are assumed to be at the same address in their respective banks.
    //assert ['PokemonPicPointers == UnownPicPointers'];
    // LD_HL(mPokemonPicPointers);
    // LD_A_B;
    // LD_D(BANK(aPokemonPicPointers));
    // CP_A(UNOWN);
    // IF_NZ goto ok;
    // LD_A_C;
    // LD_D(BANK(aUnownPicPointers));
    const char* (*const hl)[2] = ((b == UNOWN)? UnownPicPointers: PokemonPicPointers);
    uint16_t a = (b == UNOWN)? c: b;

// ok:
    // DEC_A;
    const char *path = hl[a - 1][1];
    // LD_BC(6);
    // CALL(aAddNTimes);
    // LD_BC(3);
    // ADD_HL_BC;
    // LD_A_D;
    // CALL(aGetFarByte);
    // CALL(aFixPicBank);
    // PUSH_AF;
    // INC_HL;
    // LD_A_D;
    // CALL(aGetFarWord);
    // LD_DE(native backpic pixels);
    uint8_t backpicPixels[6 * 6 * LEN_2BPP_TILE];
    // POP_AF;
    // CALL(aFarDecompress);
    LoadPNG2bppAssetToVRAM(backpicPixels, path);
    // LD_HL(native backpic pixels);
    // LD_C(6 * 6);
    // CALL(aFixBackpicAlignment);
    FixBackpicAlignment(backpicPixels, 6 * 6);
    // POP_HL;
    // LD_DE(wDecompressScratch);
    // LDH_A_addr(hROMBank);
    // LD_B_A;
    // CALL(aGet2bpp);
    CopyBytes(de, backpicPixels, 6 * 6 * LEN_2BPP_TILE);
    // POP_AF;
    // LDH_addr_A(rSVBK);
    // RET;
    return true;
}

void FixPicBank(void){
//  This is a thing for some reason.

#define PICS_FIX (0x36)

    // PUSH_HL;
    // PUSH_BC;
    // SUB_A(BANK(aHoOhFrontpic) - PICS_FIX);
    // LD_C_A;
    // LD_B(0);
    // LD_HL(mFixPicBank_PicsBanks);
    // ADD_HL_BC;
    // LD_A_hl;
    // POP_BC;
    // POP_HL;
    // RET;


// PicsBanks:
    //db ['BANK("Pics 1")'];  // BANK("Pics 1") + 0
    //db ['BANK("Pics 2")'];  // BANK("Pics 1") + 1
    //db ['BANK("Pics 3")'];  // BANK("Pics 1") + 2
    //db ['BANK("Pics 4")'];  // BANK("Pics 1") + 3
    //db ['BANK("Pics 5")'];  // BANK("Pics 1") + 4
    //db ['BANK("Pics 6")'];  // BANK("Pics 1") + 5
    //db ['BANK("Pics 7")'];  // BANK("Pics 1") + 6
    //db ['BANK("Pics 8")'];  // BANK("Pics 1") + 7
    //db ['BANK("Pics 9")'];  // BANK("Pics 1") + 8
    //db ['BANK("Pics 10")'];  // BANK("Pics 1") + 9
    //db ['BANK("Pics 11")'];  // BANK("Pics 1") + 10
    //db ['BANK("Pics 12")'];  // BANK("Pics 1") + 11
    //db ['BANK("Pics 13")'];  // BANK("Pics 1") + 12
    //db ['BANK("Pics 14")'];  // BANK("Pics 1") + 13
    //db ['BANK("Pics 15")'];  // BANK("Pics 1") + 14
    //db ['BANK("Pics 16")'];  // BANK("Pics 1") + 15
    //db ['BANK("Pics 17")'];  // BANK("Pics 1") + 16
    //db ['BANK("Pics 18")'];  // BANK("Pics 1") + 17
    //db ['BANK("Pics 19")'];  // BANK("Pics 1") + 18
    //db ['BANK("Pics 20")'];  // BANK("Pics 1") + 19
    //db ['BANK("Pics 21")'];  // BANK("Pics 1") + 20
    //db ['BANK("Pics 22")'];  // BANK("Pics 1") + 21
    //db ['BANK("Pics 23")'];  // BANK("Pics 1") + 22
    //db ['BANK("Pics 24")'];  // BANK("Pics 1") + 23

    // return GSIntro_GetMonFrontpic();
}

//  //  unreferenced
void GSIntro_GetMonFrontpic(void){
    // LD_A_C;
    // PUSH_DE;
    // LD_HL(mPokemonPicPointers);
    // DEC_A;
    // LD_BC(6);
    // CALL(aAddNTimes);
    // LD_A(BANK(aPokemonPicPointers));
    // CALL(aGetFarByte);
    // CALL(aFixPicBank);
    // PUSH_AF;
    // INC_HL;
    // LD_A(BANK(aPokemonPicPointers));
    // CALL(aGetFarWord);
    // POP_AF;
    // POP_DE;
    // CALL(aFarDecompress);
    // RET;
}

void GetTrainerPic(uint8_t* de, uint8_t tclass){
    // LD_A_addr(wTrainerClass);
    // AND_A_A;
    // RET_Z ;
    // CP_A(NUM_TRAINER_CLASSES + 1);
    // RET_NC ;
    if(tclass == 0 || tclass >= NUM_TRAINER_CLASSES + 1)
        return;
    // CALL(aWaitBGMap);
    WaitBGMap();
    // XOR_A_A;
    // LDH_addr_A(hBGMapMode);
    hram.hBGMapMode = BGMAPMODE_NONE;
    // LD_HL(mTrainerPicPointers);
    // LD_A_addr(wTrainerClass);
    // DEC_A;
    // LD_BC(3);
    // CALL(aAddNTimes);
    const char* hl = TrainerPicPointers[tclass - 1];
    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    // LD_A(MBANK(awDecompressScratch));
    // LDH_addr_A(rSVBK);
    // PUSH_DE;
    // LD_A(BANK(aTrainerPicPointers));
    // CALL(aGetFarByte);
    // CALL(aFixPicBank);
    // PUSH_AF;
    // INC_HL;
    // LD_A(BANK(aTrainerPicPointers));
    // CALL(aGetFarWord);
    // POP_AF;
    // LD_DE(wDecompressScratch);
    // CALL(aFarDecompress);
    // POP_HL;
    // LD_DE(wDecompressScratch);
    // LD_C(7 * 7);
    // LDH_A_addr(hROMBank);
    // LD_B_A;
    // CALL(aGet2bpp);
    LoadPNG2bppAssetSectionToVRAM(de, hl, 0, 7 * 7);
    // POP_AF;
    // LDH_addr_A(rSVBK);
    // CALL(aWaitBGMap);
    WaitBGMap();
    // LD_A(1);
    // LDH_addr_A(hBGMapMode);
    hram.hBGMapMode = BGMAPMODE_UPDATE_TILES;
    // RET;
}

//  Decompress lz data from b:hl to wDecompressScratch, then copy it to address de.
//  Unused since we just copy the pixels from the PNG and format them directly.
void DecompressGet2bpp(void){
    // LDH_A_addr(rSVBK);
    // PUSH_AF;
    // LD_A(MBANK(awDecompressScratch));
    // LDH_addr_A(rSVBK);

    // PUSH_DE;
    // PUSH_BC;
    // LD_A_B;
    // LD_DE(wDecompressScratch);
    // CALL(aFarDecompress);
    // POP_BC;
    // LD_DE(wDecompressScratch);
    // POP_HL;
    // LDH_A_addr(hROMBank);
    // LD_B_A;
    // CALL(aGet2bpp);

    // POP_AF;
    // LDH_addr_A(rSVBK);
    // RET;
}

static void FixBackpicAlignment(uint8_t* hl, uint8_t c){
    // PUSH_DE;
    // PUSH_BC;
    // LD_A_addr(wBoxAlignment);
    // AND_A_A;
    // IF_Z goto keep_dims;
    if(wram->wBoxAlignment == 0)
        return;
    
    uint16_t de;
    // LD_A_C;
    // CP_A(7 * 7);
    // LD_DE(7 * 7 * LEN_2BPP_TILE);
    // IF_Z goto got_dims;
    if(c == 7 * 7) {
        de = 7 * 7 * LEN_2BPP_TILE;
    }
    // CP_A(6 * 6);
    // LD_DE(6 * 6 * LEN_2BPP_TILE);
    // IF_Z goto got_dims;
    else if(c == 6 * 6) {
        de = 6 * 6 * LEN_2BPP_TILE;
    }
    // LD_DE(5 * 5 * LEN_2BPP_TILE);
    else {
        de = 5 * 5 * LEN_2BPP_TILE;
    }

    do {
    // got_dims:
        // LD_A_hl;
        uint8_t a = *hl;
        // LD_B(0);
        uint8_t b = 0;
        // LD_C(8);
        uint8_t c = 8;

        uint8_t carry = 0;
    
        do {
        // loop:
            // RRA;
            carry = a & 1;
            a = (a >> 1);
            b = (b << 1) | carry;
            // RL_B;
            // DEC_C;
            // IF_NZ goto loop;
        } while(--c != 0);
        // LD_A_B;
        // LD_hli_A;
        *hl = b;
        hl++;
        // DEC_DE;
        // LD_A_E;
        // OR_A_D;
        // IF_NZ goto got_dims;
    } while(--de != 0);

// keep_dims:
    // POP_BC;
    // POP_DE;
    // RET;
}

static uint8_t* PadFrontpic_Fill(uint8_t* hl, uint8_t c, uint8_t a) {
    do {
        // LD_hli_A;
        *(hl++) = a;
        // DEC_C;
        // IF_NZ goto Fill;
    } while(--c != 0);
    // RET;
    return hl;
}

//  pads frontpic to fill 7x7 box
static void PadFrontpic(uint8_t* hl, uint8_t* de, uint8_t b){
    // LD_A_B;
    // CP_A(6);
    // IF_Z goto six;
    if(b == 6) {
    // six:
        // LD_C(7 << 4);
        // XOR_A_A;
        // CALL(aPadFrontpic_Fill);
        hl = PadFrontpic_Fill(hl, 7 << 4, 0);

        do {
        // six_loop:
            // LD_C((7 - 6) << 4);
            // XOR_A_A;
            // CALL(aPadFrontpic_Fill);
            hl = PadFrontpic_Fill(hl, (7 - 6) << 4, 0);
            // LD_C(6 << 4);
            // CALL(aLoadOrientedFrontpic);
            LoadOrientedFrontpic(&hl, &de, 6 << 4);
            // DEC_B;
            // IF_NZ goto six_loop;
        } while(--b != 0);
        // RET;
        return;
    }
    // CP_A(5);
    // IF_Z goto five;
    else if(b == 5) {
    // five:
        // LD_C(7 << 4);
        // XOR_A_A;
        // CALL(aPadFrontpic_Fill);
        hl = PadFrontpic_Fill(hl, 7 << 4, 0);
        hl = PadFrontpic_Fill(hl, 7 << 4, 0);

        do {
        // five_loop:
            // LD_C((7 - 5) << 4);
            // XOR_A_A;
            // CALL(aPadFrontpic_Fill);
            hl = PadFrontpic_Fill(hl, (7 - 6) << 4, 0);
            // LD_C(5 << 4);
            // CALL(aLoadOrientedFrontpic);
            LoadOrientedFrontpic(&hl, &de, 5 << 4);
            
            hl = PadFrontpic_Fill(hl, (7 - 6) << 4, 0);
            // DEC_B;
            // IF_NZ goto five_loop;
        } while(--b != 0);
        // LD_C(7 << 4);
        // XOR_A_A;
        // CALL(aPadFrontpic_Fill);
        // RET;
        return;
    }

    do {
    // seven_loop:
        // LD_C(7 << 4);
        // CALL(aLoadOrientedFrontpic);
        LoadOrientedFrontpic(&hl, &de, 7 << 4);
        // DEC_B;
        // IF_NZ goto seven_loop;
    } while(--b != 0);
    // RET;
    return;
}

static void LoadOrientedFrontpic(uint8_t** hl, uint8_t** de, uint8_t c){
    // LD_A_addr(wBoxAlignment);
    // AND_A_A;
    // IF_NZ goto x_flip;
    uint8_t* hl_ = *hl;
    uint8_t* de_ = *de;
    if(wram->wBoxAlignment == 0) {
        do {
        // left_loop:
            // LD_A_de;
            // INC_DE;
            // LD_hli_A;
            *(hl_++) = *(de_++);
            // DEC_C;
            // IF_NZ goto left_loop;
        } while(--c != 0);
        // RET;
        *hl = hl_;
        *de = de_;
        return;
    }
    else {
    // x_flip:

        // PUSH_BC;

        do {
        // right_loop:
            // LD_A_de;
            // INC_DE;
            // LD_B_A;
            // XOR_A_A;
            uint8_t b = *(de_++);
            uint8_t a = 0;
            uint8_t cy = 0;
            uint8_t temp;
            for(int rept = 0; rept < 8; rept++){
                // RR_B;
                {
                    temp = b & 1;
                    b >>= 1;
                    b |= (cy << 7);
                    cy = temp;
                }
                // RLA;
                {
                    temp = a;
                    a = (a << 1) | cy;
                    cy = (temp >> 7) & 1;
                }
            }
            // LD_hli_A;
            *(hl_++) = a;
            // DEC_C;
            // IF_NZ goto right_loop;
        } while(--c != 0);
        // POP_BC;
        // RET;
        *hl = hl_;
        *de = de_;
        return;
    }

}
