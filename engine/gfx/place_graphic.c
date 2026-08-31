#include "../../constants.h"
#include "place_graphic.h"

// Fill a wBoxAlignment-aligned box with sequential tile IDs.
void PlaceGraphicNative(uint8_t* hl, uint8_t startTile, uint8_t b, uint8_t c){
    // LD_DE(SCREEN_WIDTH);
    uint8_t a;

    // LD_A_addr(wBoxAlignment);
    // AND_A_A;
    // IF_NZ goto right;
    if(wram->wBoxAlignment == 0) {
        a = startTile;
    // x1:
        do {
            // PUSH_BC;
            // PUSH_HL;
            uint8_t* hl2 = hl;
            uint8_t c2 = c;

            do {
            // y1:
                // LD_hl_A;
                *hl = a++;
                hl += SCREEN_WIDTH;
                // ADD_HL_DE;
                // INC_A;
                // DEC_C;
                // IF_NZ goto y1;
            } while(--c != 0);

            // POP_HL;
            // INC_HL;
            hl = hl2 + 1;
            // POP_BC;
            c = c2;
            // DEC_B;
            // IF_NZ goto x1;
        } while(--b != 0);
        // RET;
    }
    else {


    // right:
    //  Right-aligned.
        // PUSH_BC;
        // LD_B(0);
        // DEC_C;
        // ADD_HL_BC;
        // POP_BC;
        hl += c - 1;

        a = startTile;

        do {
        // x2:
            // PUSH_BC;
            // PUSH_HL;
            uint8_t* hl2 = hl;
            uint8_t c2 = c;

            do {
            // y2:
                // LD_hl_A;
                *hl = a++;
                // ADD_HL_DE;
                hl += SCREEN_WIDTH;
                // INC_A;
                // DEC_C;
                // IF_NZ goto y2;
            } while(--c != 0);

            // POP_HL;
            // DEC_HL;
            hl = hl2 - 1;
            // POP_BC;
            c = c2;
            // DEC_B;
            // IF_NZ goto x2;
        } while(--b != 0);
        // RET;
    }

}

// Fill a wBoxAlignment-aligned box with sequential tile IDs, staggering by row.
void PlaceGraphicYStaggerNative(uint8_t* hl, uint8_t startTile, uint8_t b, uint8_t c){
    // LD_DE(SCREEN_WIDTH);
    uint8_t a;

    // LD_A_addr(wBoxAlignment);
    // AND_A_A;
    // IF_NZ goto right;
    if(wram->wBoxAlignment == 0) {
        a = startTile;
    // y1:
        do {
            // PUSH_BC;
            // PUSH_HL;
            uint8_t* hl2 = hl;
            uint8_t b2 = b;

            do {
            // x1:
                // LD_hl_A;
                *hl = a++;
                hl++;
                // ADD_HL_DE;
                // INC_A;
                // DEC_C;
                // IF_NZ goto x1;
            } while(--b != 0);

            // POP_HL;
            // INC_HL;
            hl = hl2 + SCREEN_WIDTH;
            // POP_BC;
            b = b2;
            // DEC_B;
            // IF_NZ goto x1;
        } while(--c != 0);
        // RET;
    }
    else {


    // right:
    //  Right-aligned.
        // PUSH_BC;
        // LD_B(0);
        // DEC_C;
        // ADD_HL_BC;
        // POP_BC;
        hl += b - 1;

        a = startTile;

        do {
        // x2:
            // PUSH_BC;
            // PUSH_HL;
            uint8_t* hl2 = hl;
            uint8_t b2 = b;

            do {
            // y2:
                // LD_hl_A;
                *hl = a++;
                // ADD_HL_DE;
                hl -= 1;
                // INC_A;
                // DEC_C;
                // IF_NZ goto y2;
            } while(--b != 0);

            // POP_HL;
            // DEC_HL;
            hl = hl2 + SCREEN_WIDTH;
            // POP_BC;
            b = b2;
            // DEC_B;
            // IF_NZ goto x2;
        } while(--c != 0);
        // RET;
    }

}

// Legacy predef entry points.  Direct C callers must use the explicit native
// variants above; these remain only while the emulated predef dispatcher exists.
void PlaceGraphic(uint8_t* hl, uint8_t b, uint8_t c){
    PlaceGraphicNative(hl, hram.hGraphicStartTile, b, c);
}

void PlaceGraphicYStagger(uint8_t* hl, uint8_t b, uint8_t c){
    PlaceGraphicYStaggerNative(hl, hram.hGraphicStartTile, b, c);
}

void PlaceGraphic_Padded(uint8_t* hl, uint8_t start, uint8_t w, uint8_t h, uint8_t w2, uint8_t h2, uint8_t fill) {
    if(w == w2 && h == h2)
        return PlaceGraphicYStaggerNative(hl, start, w, h);
    
    uint8_t x1, x2, y1, y2;
    if(wram->wBoxAlignment == 0) {
        x1 = (w2 - w) / 2;
        x2 = ((w2 - w) / 2) + w;
        y1 = (h2 - h) / 2;
        y2 = ((h2 - h) / 2) + h;

        uint8_t a = start;
        
        for(uint8_t yy = 0; yy <= h2; ++yy) {
            for(uint8_t xx = 0; xx <= w2; ++xx) {
                if((xx < x1 || xx > x2 || yy < y1 || yy > y2)) {
                    hl[coordidx(xx, yy)] = fill;
                }
                else {
                    hl[coordidx(xx, yy)] = a++;
                }
            }
        }
        return;
    }
    else {
        x1 = ((w2 - w) + 1) / 2;
        x2 = (((w2 - w) + 1) / 2) + w;
        y1 = ((h2 - h) + 1) / 2;
        y2 = (((h2 - h) + 1) / 2) + h;

        uint8_t a = start;
        
        for(int16_t yy = h2; yy >= 0; --yy) {
            for(int16_t xx = w2; xx >= 0; --xx) {
                if((xx < x1 || xx > x2 || yy < y1 || yy > y2)) {
                    hl[coordidx(xx, yy)] = fill;
                }
                else {
                    hl[coordidx(xx, yy)] = a++;
                }
            }
        }
        return;
    }
}
