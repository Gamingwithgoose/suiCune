#include "../../constants.h"
#include "tmhm2.h"
#include "../../home/pokemon.h"
#include "../smallflag.h"
#include "../../data/moves/tmhm_moves.h"

uint8_t CanLearnTMHMMove(SpeciesId species, MoveId move){
    // LD_A_addr(wCurPartySpecies);
    // LD_addr_A(wCurSpecies);
    // CALL(aGetBaseData);
    const struct BaseData* base = GetSpeciesBaseData(species);
    if(base == NULL)
        return 0;
    // LD_HL(wBaseTMHM);
    // PUSH_HL;

    // LD_A_addr(wPutativeTMHMMove);
    // LD_B_A;
    // LD_C(0);
    size_t c = 0;
    // LD_HL(mTMHMMoves);
    const MoveId* hl = TMHMMoves;

    while(c < TMHMMoveCount) {
    // loop:
        // LD_A_hli;
        MoveId a = *(hl++);
        // AND_A_A;
        // IF_Z goto end;
        // CP_A_B;
        // IF_Z goto found;
        if(a == move)
            break;
        // INC_C;
        c++;
        // goto loop;
    }

    if(c == TMHMMoveCount) {
        log_warn("Move %u is not a valid TMHM move\n", move);
        return 0;
    }


// found:
    // POP_HL;
    // LD_B(CHECK_FLAG);
    // PUSH_DE;
    // LD_D(0);
    // PREDEF(pSmallFarFlagAction);
    if(c > UINT16_MAX)
        return 0;
    uint8_t res = SmallFarFlagAction(base->TMHM, (uint16_t)c, CHECK_FLAG);
    // POP_DE;
    // RET;
    return res;
}

MoveId GetTMHMMove(uint8_t tmhm){
    // LD_A_addr(wTempTMHM);
    // DEC_A;
    // LD_HL(mTMHMMoves);
    // LD_B(0);
    // LD_C_A;
    // ADD_HL_BC;
    // LD_A_hl;
    // LD_addr_A(wTempTMHM);
    // RET;
    if(tmhm == 0 || tmhm > TMHMMoveCount)
        return NO_MOVE;
    return TMHMMoves[tmhm-1];
}
