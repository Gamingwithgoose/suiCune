#include "../constants.h"
#include "hm_moves.h"
#include "array.h"

//  HM moves can't be forgotten

bool IsHM(ItemId item){
    return item >= HM01;
}

bool IsHMMove(move_t a){
    static const move_t HMMoves[] = {
        CUT,
        FLY,
        SURF,
        STRENGTH,
        FLASH,
        WATERFALL,
        WHIRLPOOL,
        (move_t)-1,
    };
    return IsInMoveArray(HMMoves, a);
}
