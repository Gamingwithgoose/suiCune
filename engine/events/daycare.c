#include "../../constants.h"
#include "daycare.h"
#include "money.h"
#include "../../home/text.h"
#include "../../home/menu.h"
#include "../../home/pokemon.h"
#include "../../home/copy.h"
#include "../../home/audio.h"
#include "../../home/delay.h"
#include "../../home/random.h"
#include "../../home/copy_name.h"
#include "../pokemon/breeding.h"
#include "../pokemon/breedmon_level_growth.h"
#include "../pokemon/bills_pc_top.h"
#include "../pokemon/mail_2.h"
#include "../pokemon/party_menu.h"
#include "../pokemon/move_mon.h"
#include "../pokemon/mon_stats.h"
#include "../pokemon/evolve.h"
#include "../pokemon/experience.h"
#include "../../data/text/common.h"

//  PrintDayCareText.TextTable indexes
enum {
    DAYCARETEXT_MAN_INTRO,
    DAYCARETEXT_MAN_INTRO_EGG,
    DAYCARETEXT_LADY_INTRO,
    DAYCARETEXT_LADY_INTRO_EGG,
    DAYCARETEXT_WHICH_ONE,
    DAYCARETEXT_DEPOSIT,
    DAYCARETEXT_CANT_BREED_EGG,
    DAYCARETEXT_LAST_MON,
    DAYCARETEXT_LAST_ALIVE_MON,
    DAYCARETEXT_COME_BACK_LATER,
    DAYCARETEXT_REMOVE_MAIL,
    DAYCARETEXT_GENIUSES,
    DAYCARETEXT_ASK_WITHDRAW,
    DAYCARETEXT_WITHDRAW,
    DAYCARETEXT_GOT_BACK,
    DAYCARETEXT_TOO_SOON,
    DAYCARETEXT_PARTY_FULL,
    DAYCARETEXT_NOT_ENOUGH_MONEY,
    DAYCARETEXT_OH_FINE,
    DAYCARETEXT_COME_AGAIN,
};

void DayCareMan(void){
    // LD_HL(wDayCareMan);
    // BIT_hl(DAYCAREMAN_HAS_MON_F);
    // IF_NZ goto AskWithdrawMon;
    if(!bit_test(gPokemon.dayCareMan, DAYCAREMAN_HAS_MON_F)) {
        // LD_HL(wDayCareMan);
        // LD_A(DAYCARETEXT_MAN_INTRO);
        // CALL(aDayCareManIntroText);
        // IF_C goto cancel;
        if(DayCareManIntroText(&gPokemon.dayCareMan, DAYCARETEXT_MAN_INTRO)) {
            // CALL(aDayCareAskDepositPokemon);
            u8_flag_s res = DayCareAskDepositPokemon();
            // IF_C goto print_text;
            if(res.flag) {
            // print_text:
                // CALL(aPrintDayCareText);
                PrintDayCareText(res.a);
            }
            else {
                // FARCALL(aDepositMonWithDayCareMan);
                DepositMonWithDayCareMan(res.a);
                // LD_HL(wDayCareMan);
                // SET_hl(DAYCAREMAN_HAS_MON_F);
                bit_set(gPokemon.dayCareMan, DAYCAREMAN_HAS_MON_F);
                // CALL(aDayCare_DepositPokemonText);
                DayCare_DepositPokemonText();
                // CALL(aDayCare_InitBreeding);
                DayCare_InitBreeding();
                // RET;
                return;
            }
        }
    }
    else {
    // AskWithdrawMon:
        // FARCALL(aGetBreedMon1LevelGrowth);
        u8_pair_s levelGrowth = GetBreedMon1LevelGrowth();
        // LD_HL(wBreedMon1Nickname);
        // CALL(aGetPriceToRetrieveBreedmon);
        GetPriceToRetrieveBreedmon(gPokemon.breedMon1Nickname, levelGrowth);
        // CALL(aDayCare_AskWithdrawBreedMon);
        u8_flag_s res = DayCare_AskWithdrawBreedMon();
        // IF_C goto print_text;
        if(res.flag) {
            PrintDayCareText(res.a);
        }
        else {
            // FARCALL(aRetrieveMonFromDayCareMan);
            RetrieveMonFromDayCareMan();
            // CALL(aDayCare_GetBackMonForMoney);
            DayCare_GetBackMonForMoney();
            // LD_HL(wDayCareMan);
            // RES_hl(DAYCAREMAN_HAS_MON_F);
            bit_reset(gPokemon.dayCareMan, DAYCAREMAN_HAS_MON_F);
            // RES_hl(DAYCAREMAN_MONS_COMPATIBLE_F);
            bit_reset(gPokemon.dayCareMan, DAYCAREMAN_MONS_COMPATIBLE_F);
            // goto cancel;
        }
    }

// cancel:
    // LD_A(DAYCARETEXT_COME_AGAIN);
    // CALL(aPrintDayCareText);
    PrintDayCareText(DAYCARETEXT_COME_AGAIN);
    // RET;
}

void DayCareLady(void){
    // LD_HL(wDayCareLady);
    // BIT_hl(DAYCARELADY_HAS_MON_F);
    // IF_NZ goto AskWithdrawMon;
    if(!bit_test(gPokemon.dayCareLady, DAYCARELADY_HAS_MON_F)) {
        // LD_HL(wDayCareLady);
        // LD_A(DAYCARETEXT_LADY_INTRO);
        // CALL(aDayCareLadyIntroText);
        // IF_C goto cancel;
        if(DayCareLadyIntroText(&gPokemon.dayCareLady, DAYCARETEXT_LADY_INTRO)) {
            // CALL(aDayCareAskDepositPokemon);
            u8_flag_s res = DayCareAskDepositPokemon();
            // IF_C goto print_text;
            if(res.flag) {
                PrintDayCareText(res.a);
            }
            else {
                // FARCALL(aDepositMonWithDayCareLady);
                DepositMonWithDayCareLady(res.a);
                // LD_HL(wDayCareLady);
                // SET_hl(DAYCARELADY_HAS_MON_F);
                bit_set(gPokemon.dayCareLady, DAYCARELADY_HAS_MON_F);
                // CALL(aDayCare_DepositPokemonText);
                DayCare_DepositPokemonText();
                // CALL(aDayCare_InitBreeding);
                DayCare_InitBreeding();
                // RET;
                return;
            }
        }
    }
    else {
    // AskWithdrawMon:
        // FARCALL(aGetBreedMon2LevelGrowth);
        u8_pair_s lvlGrowth = GetBreedMon2LevelGrowth();
        // LD_HL(wBreedMon2Nickname);
        // CALL(aGetPriceToRetrieveBreedmon);
        GetPriceToRetrieveBreedmon(gPokemon.breedMon2Nickname, lvlGrowth);
        // CALL(aDayCare_AskWithdrawBreedMon);
        u8_flag_s res = DayCare_AskWithdrawBreedMon();
        // IF_C goto print_text;
        if(res.flag) {
            PrintDayCareText(res.a);
        }
        else {
            // FARCALL(aRetrieveMonFromDayCareLady);
            RetrieveMonFromDayCareLady();
            // CALL(aDayCare_GetBackMonForMoney);
            DayCare_GetBackMonForMoney();
            // LD_HL(wDayCareLady);
            // RES_hl(DAYCARELADY_HAS_MON_F);
            bit_reset(gPokemon.dayCareLady, DAYCARELADY_HAS_MON_F);
            // LD_HL(wDayCareMan);
            // RES_hl(DAYCAREMAN_MONS_COMPATIBLE_F);
            bit_reset(gPokemon.dayCareMan, DAYCARELADY_HAS_MON_F);
            // goto cancel;
        }
    // print_text:
        // CALL(aPrintDayCareText);
    }

// cancel:
    // LD_A(DAYCARETEXT_COME_AGAIN);
    // CALL(aPrintDayCareText);
    PrintDayCareText(DAYCARETEXT_COME_AGAIN);
    // RET;
}

bool DayCareLadyIntroText(uint8_t* hl, uint8_t a){
    // BIT_hl(DAYCARELADY_ACTIVE_F);
    // IF_NZ goto okay;
    if(!bit_test(*hl, DAYCARELADY_ACTIVE_F)) {
        // SET_hl(DAYCARELADY_ACTIVE_F);
        bit_set(*hl, DAYCARELADY_ACTIVE_F);
        // INC_A;
        a++;
    }

// okay:
    // CALL(aPrintDayCareText);
    PrintDayCareText(a);
    // CALL(aYesNoBox);
    // RET;
    return YesNoBox();
}

bool DayCareManIntroText(uint8_t* hl, uint8_t a){
    // SET_hl(DAYCAREMAN_ACTIVE_F);
    bit_set(*hl, DAYCAREMAN_ACTIVE_F);
    // CALL(aPrintDayCareText);
    PrintDayCareText(a);
    // CALL(aYesNoBox);
    // RET;
    return YesNoBox();
}

u8_flag_s DayCareAskDepositPokemon(void){
    // LD_A_addr(wPartyCount);
    // CP_A(2);
    // IF_C goto OnlyOneMon;
    if(gPokemon.partyCount < 2) {
    // OnlyOneMon:
        // LD_A(DAYCARETEXT_LAST_MON);
        // SCF;
        // RET;
        return u8_flag(DAYCARETEXT_LAST_MON, true);
    }
    // LD_A(DAYCARETEXT_WHICH_ONE);
    // CALL(aPrintDayCareText);
    PrintDayCareText(DAYCARETEXT_WHICH_ONE);
    // LD_B(PARTYMENUACTION_GIVE_MON);
    // FARCALL(aSelectTradeOrDayCareMon);
    // IF_C goto Declined;
    u8_flag_s res = SelectTradeOrDayCareMon(PARTYMENUACTION_GIVE_MON);
    if(res.flag) {
    // Declined:
        // LD_A(DAYCARETEXT_OH_FINE);
        // SCF;
        // RET;
        return u8_flag(DAYCARETEXT_OH_FINE, true);
    }
    // LD_A_addr(wCurPartySpecies);
    // CP_A(EGG);
    // IF_Z goto Egg;
    if(wram->wCurPartySpecies == EGG) {
    // Egg:
        // LD_A(DAYCARETEXT_CANT_BREED_EGG);
        // SCF;
        // RET;
        return u8_flag(DAYCARETEXT_CANT_BREED_EGG, true);
    }
    // FARCALL(aCheckCurPartyMonFainted);
    // IF_C goto OutOfUsableMons;
    if(CheckCurPartyMonFainted()) {
    // OutOfUsableMons:
        // LD_A(DAYCARETEXT_LAST_ALIVE_MON);
        // SCF;
        // RET;
        return u8_flag(DAYCARETEXT_LAST_ALIVE_MON, true);
    }
    // LD_HL(wPartyMon1Item);
    // LD_BC(PARTYMON_STRUCT_LENGTH);
    // LD_A_addr(wCurPartyMon);
    // CALL(aAddNTimes);
    // LD_D_hl;
    // FARCALL(aItemIsMail);
    // IF_C goto HoldingMail;
    if(ItemIsMail(gPokemon.partyMon[res.a].mon.item)) {
    // HoldingMail:
        // LD_A(DAYCARETEXT_REMOVE_MAIL);
        // SCF;
        // RET;
        return u8_flag(DAYCARETEXT_REMOVE_MAIL, true);
    }
    // LD_HL(wPartyMonNicknames);
    // LD_A_addr(wCurPartyMon);
    // CALL(aGetNickname);
    GetNickname(gPokemon.partyMonNickname[0], res.a);
    // AND_A_A;
    // RET;
    return u8_flag(res.a, false);


// DaycareDummyText
//   //  unreferenced
    //text_far(v_DaycareDummyText)
    //text_end
}

void DayCare_DepositPokemonText(void){
    // LD_A(DAYCARETEXT_DEPOSIT);
    // CALL(aPrintDayCareText);
    PrintDayCareText(DAYCARETEXT_DEPOSIT);
    // LD_A_addr(wCurPartySpecies);
    // CALL(aPlayMonCry);
    PlayMonCry(wram->wCurPartySpecies);
    // LD_A(DAYCARETEXT_COME_BACK_LATER);
    // CALL(aPrintDayCareText);
    PrintDayCareText(DAYCARETEXT_COME_BACK_LATER);
    // RET;
}

u8_flag_s DayCare_AskWithdrawBreedMon(void){
    // LD_A_addr(wStringBuffer2 + 1);
    // AND_A_A;
    // IF_NZ goto grew_at_least_one_level;
    if(wram->wStringBuffer2[1] != 0) {
    // grew_at_least_one_level:
        // LD_A(DAYCARETEXT_GENIUSES);
        // CALL(aPrintDayCareText);
        PrintDayCareText(DAYCARETEXT_GENIUSES);
        // CALL(aYesNoBox);
        // IF_C goto refused;
        if(!YesNoBox()) {
        // refused:
            // LD_A(DAYCARETEXT_OH_FINE);
            // SCF;
            // RET;
            return u8_flag(DAYCARETEXT_OH_FINE, true);
        }
        // LD_A(DAYCARETEXT_ASK_WITHDRAW);
        // CALL(aPrintDayCareText);
        PrintDayCareText(DAYCARETEXT_ASK_WITHDRAW);
        // CALL(aYesNoBox);
        // IF_C goto refused;
        if(!YesNoBox()) {
        // refused:
            // LD_A(DAYCARETEXT_OH_FINE);
            // SCF;
            // RET;
            return u8_flag(DAYCARETEXT_OH_FINE, true);
        }
    }
    else {
        // LD_A(DAYCARETEXT_TOO_SOON);
        // CALL(aPrintDayCareText);
        PrintDayCareText(DAYCARETEXT_TOO_SOON);
        // CALL(aYesNoBox);
        // IF_C goto refused;
        // goto check_money;
        if(!YesNoBox()) {
        // refused:
            // LD_A(DAYCARETEXT_OH_FINE);
            // SCF;
            // RET;
            return u8_flag(DAYCARETEXT_OH_FINE, true);
        }
    }

// check_money:
    // LD_DE(wMoney);
    // LD_BC(wStringBuffer2 + 2);
    // FARCALL(aCompareMoney);
    u8_flag_s res = CompareMoney(wram->wStringBuffer2 + 2, gPlayer.money);
    // IF_C goto not_enough_money;
    if(res.flag) {
    // not_enough_money:
        // LD_A(DAYCARETEXT_NOT_ENOUGH_MONEY);
        // SCF;
        // RET;
        return u8_flag(DAYCARETEXT_NOT_ENOUGH_MONEY, true);
    }
    // LD_A_addr(wPartyCount);
    // CP_A(PARTY_LENGTH);
    // IF_NC goto party_full;
    if(gPokemon.partyCount >= PARTY_LENGTH) {
    // party_full:
        // LD_A(DAYCARETEXT_PARTY_FULL);
        // SCF;
        // RET;
        return u8_flag(DAYCARETEXT_PARTY_FULL, true);
    }
    // AND_A_A;
    // RET;
    return u8_flag(0, false);
}

void DayCare_GetBackMonForMoney(void){
    // LD_BC(wStringBuffer2 + 2);
    // LD_DE(wMoney);
    // FARCALL(aTakeMoney);
    TakeMoney(gPlayer.money, wram->wStringBuffer2 + 2);
    // LD_A(DAYCARETEXT_WITHDRAW);
    // CALL(aPrintDayCareText);
    PrintDayCareText(DAYCARETEXT_WITHDRAW);
    // LD_A_addr(wCurPartySpecies);
    // CALL(aPlayMonCry);
    PlayMonCry(wram->wCurPartySpecies);
    // LD_A(DAYCARETEXT_GOT_BACK);
    // CALL(aPrintDayCareText);
    PrintDayCareText(DAYCARETEXT_GOT_BACK);
    // RET;
}

void GetPriceToRetrieveBreedmon(const uint8_t* hl, u8_pair_s lvlGrowth){
    // LD_A_B;
    // LD_addr_A(wStringBuffer2);
    wram->wStringBuffer2[0] = lvlGrowth.a;
    // LD_A_D;
    // LD_addr_A(wStringBuffer2 + 1);
    wram->wStringBuffer2[1] = lvlGrowth.b;
    // LD_DE(wStringBuffer1);
    // LD_BC(NAME_LENGTH);
    // CALL(aCopyBytes);
    CopyBytes(wram->wStringBuffer1, hl, NAME_LENGTH);
    // LD_HL(0);
    // LD_BC(100);
    // LD_A_addr(wStringBuffer2 + 1);
    // CALL(aAddNTimes);
    // LD_DE(100);
    // ADD_HL_DE;
    uint16_t money = 100 + (lvlGrowth.b * 100);
    // XOR_A_A;
    // LD_addr_A(wStringBuffer2 + 2);
    wram->wStringBuffer2[2] = 0;
    // LD_A_H;
    // LD_addr_A(wStringBuffer2 + 3);
    wram->wStringBuffer2[3] = HIGH(money);
    // LD_A_L;
    // LD_addr_A(wStringBuffer2 + 4);
    wram->wStringBuffer2[4] = LOW(money);
    // RET;
}

void PrintDayCareText(uint8_t a){
    static const txt_cmd_s DayCareManIntroText[] = {
        text_far(v_DayCareManIntroText)
        text_end
    };

    static const txt_cmd_s DayCareManIntroEggText[] = {
        text_far(v_DayCareManIntroEggText)
        text_end
    };

    static const txt_cmd_s DayCareLadyIntroText[] = {
        text_far(v_DayCareLadyIntroText)
        text_end
    };

    static const txt_cmd_s DayCareLadyIntroEggText[] = {
        text_far(v_DayCareLadyIntroEggText)
        text_end
    };

    static const txt_cmd_s WhatShouldIRaiseText[] = {
        text_far(v_WhatShouldIRaiseText)
        text_end
    };

    static const txt_cmd_s OnlyOneMonText[] = {
        text_far(v_OnlyOneMonText)
        text_end
    };

    static const txt_cmd_s CantAcceptEggText[] = {
        text_far(v_CantAcceptEggText)
        text_end
    };

    static const txt_cmd_s RemoveMailText[] = {
        text_far(v_RemoveMailText)
        text_end
    };

    static const txt_cmd_s LastHealthyMonText[] = {
        text_far(v_LastHealthyMonText)
        text_end
    };

    static const txt_cmd_s IllRaiseYourMonText[] = {
        text_far(v_IllRaiseYourMonText)
        text_end
    };

    static const txt_cmd_s ComeBackLaterText[] = {
        text_far(v_ComeBackLaterText)
        text_end
    };

    static const txt_cmd_s AreWeGeniusesText[] = {
        text_far(v_AreWeGeniusesText)
        text_end
    };

    static const txt_cmd_s YourMonHasGrownText[] = {
        text_far(v_YourMonHasGrownText)
        text_end
    };

    static const txt_cmd_s PerfectHeresYourMonText[] = {
        text_far(v_PerfectHeresYourMonText)
        text_end
    };

    static const txt_cmd_s GotBackMonText[] = {
        text_far(v_GotBackMonText)
        text_end
    };

    static const txt_cmd_s BackAlreadyText[] = {
        text_far(v_BackAlreadyText)
        text_end
    };

    static const txt_cmd_s HaveNoRoomText[] = {
        text_far(v_HaveNoRoomText)
        text_end
    };

    static const txt_cmd_s NotEnoughMoneyText[] = {
        text_far(v_NotEnoughMoneyText)
        text_end
    };

    static const txt_cmd_s OhFineThenText[] = {
        text_far(v_OhFineThenText)
        text_end
    };

    static const txt_cmd_s ComeAgainText[] = {
        text_far(v_ComeAgainText)
        text_end
    };

    static const txt_cmd_s *TextTable[] = {
    //  entries correspond to DAYCARETEXT_* constants
        [DAYCARETEXT_MAN_INTRO]         = DayCareManIntroText,  // 00
        [DAYCARETEXT_MAN_INTRO_EGG]     = DayCareManIntroEggText,  // 01
        [DAYCARETEXT_LADY_INTRO]        = DayCareLadyIntroText,  // 02
        [DAYCARETEXT_LADY_INTRO_EGG]    = DayCareLadyIntroEggText,  // 03
        [DAYCARETEXT_WHICH_ONE]         = WhatShouldIRaiseText,  // 04
        [DAYCARETEXT_DEPOSIT]           = IllRaiseYourMonText,  // 05
        [DAYCARETEXT_CANT_BREED_EGG]    = CantAcceptEggText,  // 06
        [DAYCARETEXT_LAST_MON]          = OnlyOneMonText,  // 07
        [DAYCARETEXT_LAST_ALIVE_MON]    = LastHealthyMonText,  // 08
        [DAYCARETEXT_COME_BACK_LATER]   = ComeBackLaterText,  // 09
        [DAYCARETEXT_REMOVE_MAIL]       = RemoveMailText,  // 0a
        [DAYCARETEXT_GENIUSES]          = AreWeGeniusesText,  // 0b
        [DAYCARETEXT_ASK_WITHDRAW]      = YourMonHasGrownText,  // 0c
        [DAYCARETEXT_WITHDRAW]          = PerfectHeresYourMonText,  // 0d
        [DAYCARETEXT_GOT_BACK]          = GotBackMonText,  // 0e
        [DAYCARETEXT_TOO_SOON]          = BackAlreadyText,  // 0f
        [DAYCARETEXT_PARTY_FULL]        = HaveNoRoomText,  // 10
        [DAYCARETEXT_NOT_ENOUGH_MONEY]  = NotEnoughMoneyText,  // 11
        [DAYCARETEXT_OH_FINE]           = OhFineThenText,  // 12
        [DAYCARETEXT_COME_AGAIN]        = ComeAgainText,  // 13
    };
    // LD_E_A;
    // LD_D(0);
    // LD_HL(mPrintDayCareText_TextTable);
    // ADD_HL_DE;
    // ADD_HL_DE;
    // LD_A_hli;
    // LD_H_hl;
    // LD_L_A;
    // CALL(aPrintText);
    // RET;
    PrintText(TextTable[a]);
}

void DayCareManOutside(void){
static const txt_cmd_s NotYetText[] = {
    text_far(v_NotYetText)
    text_end
};

static const txt_cmd_s FoundAnEggText[] = {
    text_far(v_FoundAnEggText)
    text_end
};

static const txt_cmd_s ReceivedEggText[] = {
    text_far(v_ReceivedEggText)
    text_end
};

static const txt_cmd_s TakeGoodCareOfEggText[] = {
    text_far(v_TakeGoodCareOfEggText)
    text_end
};

static const txt_cmd_s IllKeepItThanksText[] = {
    text_far(v_IllKeepItThanksText)
    text_end
};

static const txt_cmd_s NoRoomForEggText[] = {
    text_far(v_NoRoomForEggText)
    text_end
};
    // LD_HL(wDayCareMan);
    // BIT_hl(DAYCAREMAN_HAS_EGG_F);
    // IF_NZ goto AskGiveEgg;
    if(!bit_test(gPokemon.dayCareMan, DAYCAREMAN_HAS_EGG_F)) {
        // LD_HL(mDayCareManOutside_NotYetText);
        // CALL(aPrintText);
        PrintText(NotYetText);
        // RET;
        return;
    }

// AskGiveEgg:
    // LD_HL(mDayCareManOutside_FoundAnEggText);
    // CALL(aPrintText);
    PrintText(FoundAnEggText);
    // CALL(aYesNoBox);
    // IF_C goto Declined;
    if(!YesNoBox()) {
        PrintText(IllKeepItThanksText);
        wram->wScriptVar = FALSE;
        return;
    }
    // LD_A_addr(wPartyCount);
    // CP_A(PARTY_LENGTH);
    // IF_NC goto PartyFull;
    if(gPokemon.partyCount >= PARTY_LENGTH) {
    // PartyFull:
        // LD_HL(mDayCareManOutside_NoRoomForEggText);
        // CALL(aPrintText);
        PrintText(NoRoomForEggText);
        // LD_A(TRUE);
        // LD_addr_A(wScriptVar);
        wram->wScriptVar = TRUE;
        // RET;
        return;
    }
    // CALL(aDayCare_GiveEgg);
    DayCare_GiveEgg();
    // LD_HL(wDayCareMan);
    // RES_hl(DAYCAREMAN_HAS_EGG_F);
    bit_reset(gPokemon.dayCareMan, DAYCAREMAN_HAS_EGG_F);
    // CALL(aDayCare_InitBreeding);
    DayCare_InitBreeding();
    // LD_HL(mDayCareManOutside_ReceivedEggText);
    // CALL(aPrintText);
    PrintText(ReceivedEggText);
    // LD_DE(SFX_GET_EGG);
    // CALL(aPlaySFX);
    PlaySFX(SFX_GET_EGG);
    // LD_C(120);
    // CALL(aDelayFrames);
    DelayFrames(120);
    // LD_HL(mDayCareManOutside_TakeGoodCareOfEggText);
    // goto Load0;

// Declined:
    // LD_HL(mDayCareManOutside_IllKeepItThanksText);

// Load0:
    // CALL(aPrintText);
    PrintText(TakeGoodCareOfEggText);
    // XOR_A_A;  // FALSE
    // LD_addr_A(wScriptVar);
    wram->wScriptVar = FALSE;
    // RET;
}

bool DayCare_GiveEgg(void){
    if(gPokemon.partyCount >= PARTY_LENGTH)
        return true;

    // Party ownership is still legacy. Validate the full native egg projection
    // before changing party state so an expanded ID can never be truncated.
    struct BoxMon legacyEgg;
    if(!ConvertNativeBoxMonToLegacy(&legacyEgg, &gPokemon.eggMon)) {
        log_err("Day-Care egg cannot enter the temporary legacy party record.\n");
        return true;
    }

    uint8_t c = gPokemon.partyCount;
    gPokemon.partyCount++;
    gPokemon.legacyPartySpecies[c] = EGG;
    if(c + 1 < PARTY_LENGTH)
        gPokemon.legacyPartySpecies[c + 1] = LEGACY_SPECIES_LIST_END;
    else
        gPokemon.partyEnd = LEGACY_SPECIES_LIST_END;

    wram->wCurSpecies = legacyEgg.species;
    wram->wCurPartySpecies = legacyEgg.species;
    wram->wCurPartyLevel = gPokemon.eggMon.level;

    CopyBytes(gPokemon.partyMonNickname[c], gPokemon.eggMonNickname, MON_NAME_LENGTH);
    CopyBytes(gPokemon.partyMonOT[c], gPokemon.eggMonOT, NAME_LENGTH);
    CopyBytes(&gPokemon.partyMon[c].mon, &legacyEgg, sizeof(legacyEgg));

    struct PartyMon* partyMon = gPokemon.partyMon + c;
    CalcMonStats_PartyMon(partyMon, FALSE);
    partyMon->HP = 0;
    return false;
}

// DEPRECATED: Use gPokemon.partyMon[gPokemon.partyCount] instead
// void DayCare_GetCurrentPartyMember(void){
    // LD_A_addr(wPartyCount);
    // DEC_A;
    // CALL(aAddNTimes);
    // LD_D_H;
    // LD_E_L;
    // RET;
// }

void DayCare_InitBreeding(void){
    if(!bit_test(gPokemon.dayCareLady, DAYCARELADY_HAS_MON_F)
    || !bit_test(gPokemon.dayCareMan, DAYCAREMAN_HAS_MON_F))
        return;

    CheckBreedmonCompatibility();
    if(wram->wBreedingCompatibility == 0 || wram->wBreedingCompatibility == 0xff)
        return;

    bit_set(gPokemon.dayCareMan, DAYCAREMAN_MONS_COMPATIBLE_F);
    do {
        gPokemon.stepsToEgg = Random();
    } while(gPokemon.stepsToEgg < 150);

    ByteFill(&gPokemon.eggMon, sizeof(gPokemon.eggMon), 0);
    ByteFill(gPokemon.eggMonNickname, sizeof(gPokemon.eggMonNickname), 0);
    ByteFill(gPokemon.eggMonOT, sizeof(gPokemon.eggMonOT), 0);

    u8_flag_s breed1Gender = GetGenderForSpeciesDVs(gPokemon.breedMon1.species, gPokemon.breedMon1.DVs);
    gPokemon.breedMotherOrNonDitto =
        (gPokemon.breedMon1.species == DITTO
      || (gPokemon.breedMon2.species != DITTO && !breed1Gender.flag && breed1Gender.a != 0))? 1: 0;

    SpeciesId motherSpecies = gPokemon.breedMotherOrNonDitto
        ? gPokemon.breedMon2.species
        : gPokemon.breedMon1.species;
    SpeciesId eggSpecies = GetPreEvolutionNative(GetPreEvolutionNative(motherSpecies));

    // Nidoran female can produce either Nidoran sex.
    if(eggSpecies == NIDORAN_F)
        eggSpecies = (Random() >= 50 percent + 1)? NIDORAN_M: NIDORAN_F;

    const struct BaseData* eggBase = GetSpeciesBaseData(eggSpecies);
    if(eggBase == NULL)
        return;

    gPokemon.eggMon.species = eggSpecies;
    gPokemon.eggMon.item = NO_ITEM;
    gPokemon.eggMon.id = gPlayer.playerID;
    gPokemon.eggMon.level = EGG_LEVEL;
    gPokemon.eggMon.exp = CalcExpAtLevelWithGrowthRate(eggBase->growthRate, EGG_LEVEL);
    ByteFill(gPokemon.eggMon.statExp, sizeof(gPokemon.eggMon.statExp), 0);

    CopyName2(gPokemon.eggMonNickname, U82C("EGG@"));
    CopyBytes(gPokemon.eggMonOT, gPlayer.playerName, NAME_LENGTH);

    wram->wCurPartyLevel = EGG_LEVEL;
    wram->wSkipMovesBeforeLevelUp = FALSE;
    FillNativeMoves(gPokemon.eggMon.moves, gPokemon.eggMon.PP, eggSpecies, EGG_LEVEL);
    InitEggMoves();

    uint16_t randomDVs = (uint16_t)(Random() | (Random() << 8));
    gPokemon.eggMon.DVs = randomDVs;

    uint16_t inheritedDVs;
    if(gPokemon.breedMon1.species == DITTO) {
        inheritedDVs = gPokemon.breedMon1.DVs;
    }
    else if(gPokemon.breedMon2.species == DITTO) {
        inheritedDVs = gPokemon.breedMon2.DVs;
    }
    else {
        u8_flag_s eggGender = GetGenderForSpeciesDVs(eggSpecies, randomDVs);
        if(eggGender.flag)
            goto SkipInheritedDVs;

        if(eggGender.a != 0) {
            inheritedDVs = gPokemon.breedMotherOrNonDitto
                ? gPokemon.breedMon2.DVs
                : gPokemon.breedMon1.DVs;
        }
        else {
            inheritedDVs = gPokemon.breedMotherOrNonDitto
                ? gPokemon.breedMon1.DVs
                : gPokemon.breedMon2.DVs;
        }
    }

    {
        uint8_t low = (uint8_t)((randomDVs & 0xf0) + (inheritedDVs & 0x0f));
        uint8_t high = (uint8_t)(((randomDVs >> 8) & 0xf8) + ((inheritedDVs >> 8) & 0x07));
        gPokemon.eggMon.DVs = (uint16_t)((high << 8) | low);
    }

SkipInheritedDVs:
    FillNativePP(gPokemon.eggMon.PP, gPokemon.eggMon.moves);
    gPokemon.eggMon.happiness = eggBase->eggSteps;
    gPokemon.eggMon.pokerusStatus = 0;
    gPokemon.eggMon.caughtData[0] = 0;
    gPokemon.eggMon.caughtData[1] = 0;
}
