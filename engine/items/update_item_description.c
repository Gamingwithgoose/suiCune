#include "../../constants.h"
#include "update_item_description.h"
#include "print_item_description.h"
#include "../../home/text.h"
#include "../../home/menu.h"
#include "../menus/scrolling_menu.h"

void UpdateItemDescription(void){
    // LD_A_addr(wMenuSelection);
    // LD_addr_A(wCurSpecies);
    const struct MenuData* data = GetMenuData();
    ItemId item = (data->scrollingMenu.format == SCROLLINGMENU_NATIVE_ITEMS_NORMAL
                || data->scrollingMenu.format == SCROLLINGMENU_NATIVE_ITEMS_QUANTITY)
        ? GetScrollingMenuItemSelection()
        : wram->wMenuSelection;
    // hlcoord(0, 12, wTilemap);
    // LD_B(4);
    // LD_C(SCREEN_WIDTH - 2);
    // CALL(aTextbox);
    Textbox(coord(0, 12, wram->wTilemap), 4, SCREEN_WIDTH - 2);
    // LD_A_addr(wMenuSelection);
    // CP_A(-1);
    // RET_Z ;
    if(item == ITEM_LIST_END)
        return;
    // decoord(1, 14, wTilemap);
    // FARCALL(aPrintItemDescription);
    PrintItemDescription(coord(1, 14, wram->wTilemap), item);
    // RET;
}
