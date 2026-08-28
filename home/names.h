// void NamesPointers(void);
extern const char (*const NamesPointers[])[20];
uint8_t* GetName(uint8_t type, uint16_t index);
void GetName_GB(void);
void GetNthString(void);
uint16_t GetNthString_GB(uint16_t hl, uint8_t a);
uint8_t* GetBasePokemonName(SpeciesId a);
uint8_t* GetPokemonName(SpeciesId index);
void GetPokemonName_GB(void);
uint8_t* GetItemName(ItemId a);
uint8_t* GetTMHMName(ItemId a);
uint8_t* GetMoveName(MoveId move);
#include "hm_moves.h"
