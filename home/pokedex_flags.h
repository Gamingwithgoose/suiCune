uint8_t CountSetBits(const uint8_t* hl, uint8_t b);
uint8_t GetWeekday(void);
void SetSeenAndCaughtMon(DexId c);
void SetSeenMon(DexId c);
bool CheckCaughtMon(DexId c);
bool CheckSeenMon(DexId c);
bool PokedexFlagAction(uint8_t* hl, DexId c, uint8_t b);
