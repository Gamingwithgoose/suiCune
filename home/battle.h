void GetDexNumber(void);
struct PartyMon* UserPartyMon(void);
struct PartyMon* OpponentPartyMon(void);
void ResetDamage(void);
void SetPlayerTurn(void);
void SetEnemyTurn(void);
void UpdateOpponentInParty(void);
void UpdateUserInParty(void);
void UpdateBattleMonInParty(void);
void UpdateBattleMon(uint8_t a);
void UpdateEnemyMonInParty(void);
void UpdateEnemyMonInParty(void);
void RefreshBattleHuds(void);
void UpdateBattleHuds(void);
void MobileTextBorder(void);
void BattleTextbox(const struct TextCmd* hl);
void StdBattleTextbox(const struct TextCmd* hl);
void PushLYOverrides(void);
#include "battle_vars.h"
