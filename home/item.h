void DoItemEffect(void);
bool CheckTossableItem(ItemId item);
bool TossItem(item_pocket_u* pocket, ItemId item, uint8_t quantity);
bool ReceiveItem(item_pocket_u* pocket, ItemId item, uint8_t quantity);
bool CheckItem(item_pocket_u *pocket, ItemId item);
item_pocket_u* GetItemPocket(uint8_t pocket);
