#include "../../constants.h"
#include "tmhm2.h"
#include "../../home/pokemon.h"
#include "../smallflag.h"
#include "../../data/moves/tmhm_moves.h"
#include "../../data/moves/names.h"

uint8_t CanLearnTMHMMove(species_t species, move_t move){
    // LD_A_addr(wCurPartySpecies);
    // LD_addr_A(wCurSpecies);
    // CALL(aGetBaseData);
    GetBaseData(species);
    // LD_HL(wBaseTMHM);
    // PUSH_HL;

    // LD_A_addr(wPutativeTMHMMove);
    // LD_B_A;
    // LD_C(0);
    uint8_t c = 0;
    // LD_HL(mTMHMMoves);
    const move_t* hl = TMHMMoves;

    while(1) {
    // loop:
        // LD_A_hli;
        move_t a = *(hl++);
        // AND_A_A;
        // IF_Z goto end;
        if(a == NO_MOVE) {
        // end:
            // POP_HL;
            // LD_C(0);
            // RET;
            const char* move_name = (move == NO_MOVE)? "NO_MOVE": MoveNames[move-1];
            log_warn("Move %s is not a valid TMHM move\n", move_name);
            return 0;
        }
        // CP_A_B;
        // IF_Z goto found;
        if(a == move)
            break;
        // INC_C;
        c++;
        // goto loop;
    }


// found:
    // POP_HL;
    // LD_B(CHECK_FLAG);
    // PUSH_DE;
    // LD_D(0);
    // PREDEF(pSmallFarFlagAction);
    uint8_t res = SmallFarFlagAction(wram->wBaseTMHM, c, CHECK_FLAG);
    // POP_DE;
    // RET;
    return res;
}

move_t GetTMHMMove(uint8_t tmhm){
    // LD_A_addr(wTempTMHM);
    // DEC_A;
    // LD_HL(mTMHMMoves);
    // LD_B(0);
    // LD_C_A;
    // ADD_HL_BC;
    // LD_A_hl;
    // LD_addr_A(wTempTMHM);
    // RET;
    return TMHMMoves[tmhm-1];
}
