#include "../../constants.h"
#include "menu.h"
#include "../../home/menu.h"
#include "../../home/print_text.h"
#include "../battle_anims/core.h"
#include "../../util/log.h"

static void PrintParkBallsRemaining(void);
static void PrintSafariBallsRemaining(void);

const struct MenuHeader BattleMenuHeader = {
    .flags = MENU_BACKUP_TILES,
    .coord = menu_coords(8, 12, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1),
    .data = &(struct MenuData) {
        .flags = STATICMENU_CURSOR | STATICMENU_DISABLE_B,
        .type = 0,
        ._2dMenu = {
            .rows = 2, .cols = 2,
            .spacing = 6,
            .options = (const char*[]){
                "FIGHT@",
                "<PKMN>@",
                "PACK@",
                "RUN@",
            }
        },
        .function = NULL,
    }
};

bool LoadBattleMenu(void){
    log_runtime_event("MENU", "battle command menu open cursor=%u tilemapBackup=1", (unsigned)wram->wBattleMenuCursorPosition);
    SetBattleSceneForegroundUIRegion(BATTLE_SCENE_FOREGROUND_UI_COMMAND_MENU,
        8 * TILE_WIDTH, 12 * TILE_WIDTH, 12 * TILE_WIDTH, 6 * TILE_WIDTH);
    // LD_HL(mBattleMenuHeader);
    // CALL(aLoadMenuHeader);
    LoadMenuHeader(&BattleMenuHeader);
    // LD_A_addr(wBattleMenuCursorPosition);
    // LD_addr_A(wMenuCursorPosition);
    wram->wMenuCursorPosition = wram->wBattleMenuCursorPosition;
    // CALL(aInterpretBattleMenu);
    u8_flag_s res = InterpretBattleMenu();
    // LD_A_addr(wMenuCursorPosition);
    // LD_addr_A(wBattleMenuCursorPosition);
    wram->wBattleMenuCursorPosition = wram->wMenuCursorPosition;
    // CALL(aExitMenu);
    ExitMenu();
    ClearBattleSceneForegroundUIRegion(BATTLE_SCENE_FOREGROUND_UI_COMMAND_MENU);
    log_runtime_event("MENU", "battle command menu close cursor=%u cancelled=%u",
        (unsigned)wram->wBattleMenuCursorPosition, (unsigned)res.flag);
    // RET;
    return res.flag;
}

static const struct MenuHeader ContestBattleMenuHeader = {
    .flags = MENU_BACKUP_TILES,  // flags
    .coord = menu_coords(2, 12, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1),
    .defaultOption = 1,  // default option
    .data = &(struct MenuData) {
        .flags = STATICMENU_CURSOR | STATICMENU_DISABLE_B,  // flags
        ._2dMenu = {
            .rows=2, .cols=2,  // rows, columns
            .spacing=12,  // spacing
            .options = (const char*[]) {
                "FIGHT@",
                "<PKMN>@",
                "PARKBALL×  @",
                "RUN@",
            }
        },
        .function = PrintParkBallsRemaining,
    }
};

void ContestBattleMenu(void){
    SetBattleSceneForegroundUIRegion(BATTLE_SCENE_FOREGROUND_UI_COMMAND_MENU,
        2 * TILE_WIDTH, 12 * TILE_WIDTH, 18 * TILE_WIDTH, 6 * TILE_WIDTH);
    // LD_HL(mContestBattleMenuHeader);
    // CALL(aLoadMenuHeader);
    LoadMenuHeader(&ContestBattleMenuHeader);
// fallthrough

    return CommonBattleMenu();
}

void CommonBattleMenu(void){
    log_runtime_event("MENU", "battle alternate menu open cursor=%u", (unsigned)wram->wBattleMenuCursorPosition);
    // LD_A_addr(wBattleMenuCursorPosition);
    // LD_addr_A(wMenuCursorPosition);
    wram->wMenuCursorPosition = wram->wBattleMenuCursorPosition;
    // CALL(av_2DMenu);
    u8_flag_s res = v_2DMenu();
    // LD_A_addr(wMenuCursorPosition);
    // LD_addr_A(wBattleMenuCursorPosition);
    wram->wBattleMenuCursorPosition = res.a;
    // CALL(aExitMenu);
    ExitMenu();
    ClearBattleSceneForegroundUIRegion(BATTLE_SCENE_FOREGROUND_UI_COMMAND_MENU);
    log_runtime_event("MENU", "battle alternate menu close cursor=%u cancelled=%u",
        (unsigned)wram->wBattleMenuCursorPosition, (unsigned)res.flag);
    // RET;
}

// void BattleMenuHeader(void){
    //db ['MENU_BACKUP_TILES'];  // flags
    //menu_coords ['8', '12', 'SCREEN_WIDTH - 1', 'SCREEN_HEIGHT - 1'];
    //dw ['.MenuData'];
    //db ['1'];  // default option


// MenuData:
    //db ['STATICMENU_CURSOR | STATICMENU_DISABLE_B'];  // flags
    //dn ['2', '2'];  // rows, columns
    //db ['6'];  // spacing
    //dba ['.Text']
    //dbw ['BANK(@)', 'NULL']


// Text:
    //db ['"FIGHT@"'];
    //db ['"<PKMN>@"'];
    //db ['"PACK@"'];
    //db ['"RUN@"'];

    // return SafariBattleMenuHeader();
// }

static const struct MenuHeader SafariBattleMenuHeader = {
    //db ['MENU_BACKUP_TILES'];  // flags
    .flags = MENU_BACKUP_TILES,
    .coord = menu_coords(0, 12, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1),
    //dw ['.MenuData'];
    .data = &(struct MenuData) {
    // MenuData:
        .flags = STATICMENU_CURSOR | STATICMENU_DISABLE_B,  // flags
        ._2dMenu = {
            .rows=2, .cols=2,  // rows, columns
            .spacing=11,  // spacing
            //dba ['.Text']

    // Text:
        //db ['"サファりボール×\u3000\u3000@"'];  // "SAFARI BALL×  @"
        //db ['"エサをなげる@"'];  // "THROW BAIT"
        //db ['"いしをなげる@"'];  // "THROW ROCK"
        //db ['"にげる@"'];  // "RUN"
            .options = (const char*[]) {
                "SAFARI BALL×  @",
                "THROW BAIT@",
                "THROW ROCK@",
                "RUN@",
            },
        },
        .function = PrintSafariBallsRemaining,
    },
    .defaultOption = 1,  // default option
};

void SafariBattleMenu(void){
//  //  unreferenced
    SetBattleSceneForegroundUIRegion(BATTLE_SCENE_FOREGROUND_UI_COMMAND_MENU,
        0, 12 * TILE_WIDTH, SCREEN_WIDTH_PX, 6 * TILE_WIDTH);
    // LD_HL(mSafariBattleMenuHeader);
    // CALL(aLoadMenuHeader);
    LoadMenuHeader(&SafariBattleMenuHeader);
    // JR(mCommonBattleMenu);
    return CommonBattleMenu();
}

static void PrintSafariBallsRemaining(void) {
    // hlcoord(17, 13, wTilemap);
    // LD_DE(wSafariBallsRemaining);
    // LD_BC((PRINTNUM_LEADINGZEROS | 1 << 8) | 2);
    // CALL(aPrintNum);
    PrintNum(coord(13, 16, wram->wTilemap), &gPlayer.safariBallsRemaining, PRINTNUM_LEADINGZEROS | 1, 2);
    // RET;
}

static void PrintParkBallsRemaining(void) {
    // hlcoord(13, 16, wTilemap);
    // LD_DE(wParkBallsRemaining);
    // LD_BC((PRINTNUM_LEADINGZEROS | 1 << 8) | 2);
    // CALL(aPrintNum);
    // RET;
    PrintNum(coord(13, 16, wram->wTilemap), &gPlayer.parkBallsRemaining, PRINTNUM_LEADINGZEROS | 1, 2);
}
