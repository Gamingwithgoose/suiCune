// Native battle operations; packed PartyMon access remains a migration boundary.
uint32_t BattleScaleDamage(uint32_t damage, uint32_t numerator, uint32_t denominator);
uint32_t BattleAddDamage(uint32_t damage, uint32_t amount);
struct BattleParticipant* BattleParticipantForSide(enum BattleSide side);
uint16_t BattleApplyDamage(struct BattlePokemon* mon, uint32_t damage);
uint16_t BattleRestoreHP(struct BattlePokemon* mon, uint32_t amount);
void BattleLoadPartyPokemon(struct BattlePokemon* dest, const struct PartyMon* src);
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
