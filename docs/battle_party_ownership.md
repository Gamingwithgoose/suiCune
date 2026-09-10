# Battle and party ownership map

## Initial dependency closure

Normal entry flows through BattleIntro/LoadEnemyMon and InitBattleMon; switching
runs UpdateBattleMonInParty and InitBattleMon; rewards run GiveExperience and
level/stat recalculation; ExitBattle returns persistent results to the party.

| Representation | Initial classification | Producers / consumers | Migration boundary |
| --- | --- | --- | --- |
| gPokemon.partyMon (packed PartyMon) | Authoritative persistent gameplay state | capture, storage withdrawal, party items, evolution, battle rewards; all party menus and save serialization | Still requires complete persistent-party producer/consumer migration to NativePartyMon. Existing native storage/daycare types must be reused. |
| WRAM wBattleMon / wEnemyMon | Authoritative active gameplay state | InitBattleMon, InitEnemyMon, LoadEnemyMon; damage, effects, AI, items, HUD, capture | Transfer battler ownership to ordinary native battle objects; persistent party boundary remains explicit. |
| HRAM hBattleTurn | Authoritative gameplay selection | SetPlayerTurn/SetEnemyTurn and temporary side swaps; effects, AI, HUD/animation | Native battle-side field; presentation reads the same selection but must not own Pokemon state. |
| WRAM active party indexes / participants | Authoritative selection and reward eligibility | entrance, switch/faint handlers; PP, status, rewards and party updates | Typed native battle selection; distinguish active identity from the menu scratch index wCurPartyMon. |
| WRAM stat stages / substatus flags | Authoritative volatile gameplay state | move effects and turn processing; AI and status calculations | Native participant state; retain gameplay bit meanings, remove address identity. |
| WRAM base-stat caches and damage scratch | Temporary working data | stat calculation, damage formula; move effects | Numeric native calculations where in the battler closure; remaining scratch must be separately classified. |
| wTempMon / packed experience bytes | Mixed temporary UI data and gameplay state | CopyMonToTempMon, rewards, summaries, move learning | Must not call a packed record native; persistent-party migration removes this boundary. |
| HP buffers, tilemaps, animation state, HRAM display controls | Rendering-only compatibility | battle simulation publishes display inputs; renderer and animation consume them | Preserve existing native battle scene; cosmetic send-out/layering issues are outside this migration. |
| Serialize_PartyMon / Deserialize_PartyMon, link party records | Serialization/save or external compatibility | save, load, link and mobile paths | Preserve exact serialized format; checked conversion required for native IDs. |
| CPU register macros in GetHalfHP / SwapBattlerLevels | Dead/obsolete callable translated helpers | no ordinary gameplay callers; registry metadata remains | Replace register transport with native typed behavior or retire obsolete entry. |
| Other gb_read/gb_write, ROM/bank dispatch | Mixed temporary data, historical translated paths and compatibility | battle scene, trainer setup, old register helpers | Inspect executable uses separately from commented ASM; no broad mechanical address replacement. |
| Audio/APU, RTC, link transport hardware | Actual emulated-hardware or external compatibility | unrelated host subsystems | Outside scope. |

## Baseline measurement

Executable C text with block and line comments removed. Scope: all C files in
engine/battle and engine/pokemon, plus home/battle.c, home/battle_vars.c and
home/pokemon.c. Counts are literal occurrences, not lines. Historical ASM and
commented translated instructions are excluded.

| Token | Before |
| --- | ---: |
| `wram->` | 4360 |
| `hram` | 425 |
| `gb_read` | 22 |
| `gb_write` | 19 |
| `cpu_reg` | 0 |

## Ownership constraints

No battle projection may be read back for ordinary damage/stat calculations.
The packed persistent party is a remaining authoritative subsystem, not a save-only
projection. Import on entrance and explicit persistent updates are transitional
boundaries until that subsystem is migrated. Battle-specific transformations and
stat modifications must never overwrite permanent party species/moves/base stats.
No renderer flags belong in the native Pokemon model. Existing libmobile changes
are outside this closure.

## Implemented ownership transfer

`gBattle` owns both active BattlePokemon objects, acting side, participant indexes,
reward-participant sets, stat stages, five volatile-condition bytes per side, and
unmodified battle stats. Active HP, maximum HP and all five calculated stats are
ordinary numeric uint16_t values. Status is a scalar bitset; padding is not state.
The native objects are not packed and have no Game Boy address identity.

BattleApplyDamage and BattleRestoreHP accept a typed BattlePokemon pointer and
numeric amounts, clamp at zero/max HP without integer wrapping, and return the
actual applied amount. Direct attacks and residual damage update native HP before
publishing HP-bar inputs. CalcBattleStats consumes numeric base stats/stages and
writes numeric result stats; wild-stat generation calls the native numeric stat
formula directly. Damage, AI, speed ordering, healing, status, Transform, Substitute,
PP, capture, rewards and presentation consumers read the native battlers.

The selected party index remains distinct from wCurPartyMon (menu/reward scratch).
UserPartyMon and OpponentPartyMon now select the active battle side/index instead
of that scratch cursor. Pursuit intentionally updates an outgoing party record
while the next participant is selected, so its explicit destination is retained.
Party entrance validates the selected slot, species and HP range. Native health
operations validate HP ownership invariants; stat calculation validates stage bounds.

BattleIntro resets the native battle object. Switch entrance replaces the battler
field by field from the selected persistent record. Cleanup clears volatile flags;
final battler values remain available for existing post-battle consumers until the
next entry. This preserves the existing post-battle lifetime without renderer flags.

Transform now copies the actual stage-array extent, eliminating the prior ten-byte
copy through an eight-byte stage array. Pain Split updates the enemy owner on its
second HP change. Sketch/Spite resolve persistent party pointers only after the
wild-battle boundary has been checked, preventing an invalid wild party index.

GetHalfHP now takes a typed Pokemon and returns a numeric result. SwapBattlerLevels
uses C values. Neither obsolete helper executes register/address macros. Historical
ASM and commented registry entries remain reference material.

## Remaining boundaries and removal conditions

| Boundary | Writer / reader / authority | Removal condition |
| --- | --- | --- |
| Packed player party and OT party | Party systems own persistent records; BattleLoadPartyPokemon imports on entrance; explicit battle updates export level/status/HP and move/PP/item changes. This is still gameplay authority outside the active battle, not a save-only projection. | Migrate capture, withdrawal/deposit, evolution, party items, move learning, egg state and rewards together to existing NativePartyMon; serialize only at load/save/link boundaries. |
| Experience and level-up rewards | GiveExperience still mutates packed party experience and temporary UI records; existing numeric species/experience calculation APIs remain intact. Level-up imports the newly calculated permanent numeric values through explicit decoding. | Persistent-party ownership closure; remove temporary packed reward inputs and their round trips. |
| Byte species/move/item IDs | BattlePokemon explicitly retains Legacy IDs because persistent party, battle move scripts and current-move variable dispatch still require them. Existing checked native-to-legacy conversion remains. | Widen the complete party/move-script consumer closure; do not silently narrow IDs or widen saved layouts in place. |
| HP-bar/HUD projection | Battle simulation writes wHPBuffer1/2/3 and explicitly encoded wTempMon display fields; HP bars and status/HUD text consume them. Native battlers own the outcome. Some remaining healing/effect routines still use HP buffers as intermediate working data. | Replace remaining working-data uses with locals, then introduce semantic presentation events and remove packed HUD inputs. |
| Wild original moves/PP and backup DVs | Entry and permanent move effects write original wild data; Mimic/Transform/capture consume it separately from transformed active moves. This remains meaningful gameplay snapshot state in WRAM. | Move original wild identity/moves into a native encounter owner with explicit transform/capture lifetime. |
| Damage and turn engine scratch | wCurDamage, move structs, action/result flags, weather, multi-hit/substitute counters, screens and AI scratch remain authoritative or temporary gameplay data. | Next battle-engine phase after persistent-party ownership; replace action/damage state and move-script commands with typed native inputs/results. |
| Retired WRAM/HRAM slots | No migrated gameplay writer or reader; renamed reserved members retain layout for unrelated address-based systems. They are not regenerated projections. | Remove the remaining global WRAM/HRAM layout consumers, then delete reserved storage and address metadata. |
| Crystal saves/link/mobile | Existing serializers and packed party exchange remain unchanged. Save bytes have not been widened. | Native party import/export adapters first; independent native-save migration later. |
| Final battler values after battle | Cleanup retains the final native values for remaining post-battle readers; next BattleIntro resets them. | Move post-battle displays/capture summaries to explicit result objects. |

Ordinary active-battler calculations no longer require wBattleMon/wEnemyMon address
reads, packed HP/stat extraction, HRAM hBattleTurn, WRAM stage/base-stat/condition
locations, or WRAM active-index/participant storage. Packed persistent-party state
has **not** been eliminated. The full requested battle-and-party slice remains
incomplete; the completed portion is active-battle ownership and its consumer closure.

## Important remaining raw memory calls

- battle_transition and trainer_huds read/write palette registers; start_battle and
  core write WY/LCDC for presentation. These are rendering compatibility, for a later
  renderer/host pass rather than a mechanics migration.
- check_battle_scene reads mobile battle-scene option state (wcd2f/s4_a60c).
  The mobile/options boundary must be migrated before those reads disappear.
- core reads/writes the SRAM trainer-ranking turn counter s5_a89a; effect_commands
  reads the debug skip-battle save flag; read_trainer_party reads the Mystery Gift
  trainer-house save flag. These remain persistence/debug compatibility inputs.
- bills_pc/caught_data/tempmon read box counts, species and caught metadata; mail
  reads/writes mailbox records; move_mon retains old item/ID/mailbox-related writes.
  These belong to persistent Pokemon/storage/save compatibility, not active damage.
- Remaining HRAM fields are display/menu state (hBGMapMode dominates), link clock
  selection, party-transfer scratch hMoveMon, and two overworld entity indexes.
  None is hBattleTurn. Rendering/link/storage/entity phases own their removal.
- Remaining common WRAM references are tilemaps, menu cursors, wCurPartyMon,
  wTempMon, species/level scratch, wCurDamage, OT party and action/result state.
  The table above distinguishes their authoritative, working and display roles.

## Validation

Four source-level ownership guards pass: no live retired field access, no migrated
address transport, no byte indexing of scalar battle stats/status, and no packed
BattleMon type in runtime consumers. The guard scans engine/home/util/tools/mobile/
data/maps C files and strips comments/reference strings. It can be run without a
build using `python tools/tests/test_battle_ownership.py`.

An intermediate build was attempted and failed during the numeric-stat/identifier
transition. The final source has not been built. No game executable was launched;
no battle mechanics or animation behavior has been observed for the final change.

Runtime acceptance remains outstanding for: wild entry/send-out; attacks and damage;
status and PP; switching; either side fainting and replacement; experience/level-up;
battle exit and overworld return; party persistence; another battle; Pursuit;
Transform; Pain Split; Sketch/Spite in wild encounters; and capture/contest display.
Source guards do not establish compilation or runtime correctness.

Known early-send-out and animation-layering issues remain presentation issues.
No cosmetic changes were attempted. No new runtime regression has been confirmed
or ruled out because runtime acceptance is pending.

## Measurement after migration

Same comment-stripped executable C scope and literal-occurrence method as baseline.

| Token | Before | After | Reduction |
| --- | ---: | ---: | ---: |
| `wram->` | 4360 | 3470 | 890 |
| `hram` | 425 | 177 | 248 |
| `gb_read` | 22 | 22 | 0 |
| `gb_write` | 19 | 19 | 0 |
| `cpu_reg` | 0 | 0 | 0 |

The literal cpu_reg count was already zero because translated helpers use macros.
The two remaining register-driven battler helper bodies were replaced with native
C. Unchanged gb_read/gb_write totals accurately reflect the retained boundaries.

## Changed files

- `constants.h`: Native battle types, scalar stats and side ownership declarations.
- `hram.c`: Native state definition and retired memory slots; unrelated legacy layout preserved.
- `hram.h`: Native state definition and retired memory slots; unrelated legacy layout preserved.
- `wram.c`: Native state definition and retired memory slots; unrelated legacy layout preserved.
- `wram.h`: Native state definition and retired memory slots; unrelated legacy layout preserved.
- `constants/battle_constants.h`: Native battle types, scalar stats and side ownership declarations.
- `constants/types.h`: Native battle types, scalar stats and side ownership declarations.
- `home/battle.c`: Typed native health/import operations, active participant targeting and explicit persistent-party updates.
- `home/battle.h`: Typed native health/import operations, active participant targeting and explicit persistent-party updates.
- `home/battle_vars.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `home/text.c`: Read the native battle side/battler fields at the existing presentation or compatibility boundary.
- `mobile/mobile_40.c`: Read the native battle side/battler fields at the existing presentation or compatibility boundary.
- `mobile/mobile_41.c`: Read the native battle side/battler fields at the existing presentation or compatibility boundary.
- `engine/battle/battle_transition.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/consume_held_item.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/core.c`: Battler entrance/switch/faint/reward consumers, numeric health/stat paths, lifecycle reset and explicit display projection.
- `engine/battle/core.h`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/effect_commands.c`: Native attack/status/stat consumers and typed stat/damage calculations.
- `engine/battle/effect_commands.h`: Native attack/status/stat consumers and typed stat/damage calculations.
- `engine/battle/hidden_power.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/misc.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/start_battle.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/used_move_text.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle_anims/anim_commands.c`: Read the native battle side/battler fields at the existing presentation or compatibility boundary.
- `engine/battle_anims/bg_effects.c`: Read the native battle side/battler fields at the existing presentation or compatibility boundary.
- `engine/battle_anims/core.c`: Read the native battle side/battler fields at the existing presentation or compatibility boundary.
- `engine/battle_anims/functions.c`: Read the native battle side/battler fields at the existing presentation or compatibility boundary.
- `engine/events/happiness_egg.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/items/item_effects.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/link/link.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/menus/debugmenu.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/pokemon/breeding.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/pokemon/learn.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/pokemon/mon_menu.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/pokemon/mon_stats.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/pokemon/move_mon.c`: Native wild-stat calculation and capture transfer; no encoded battle-stat copy.
- `engine/pokemon/move_mon.h`: Native wild-stat calculation and capture transfer; no encoded battle-stat copy.
- `engine/events/bug_contest/display_stats.c`: Explicit encoded numeric HP projection for the retained number renderer.
- `engine/battle/ai/items.c`: AI reads native battler identity, status, stats and HP; persistent updates encode HP explicitly.
- `engine/battle/ai/move.c`: AI reads native battler identity, status, stats and HP; persistent updates encode HP explicitly.
- `engine/battle/ai/redundant.c`: AI reads native battler identity, status, stats and HP; persistent updates encode HP explicitly.
- `engine/battle/ai/scoring.c`: AI reads native battler identity, status, stats and HP; persistent updates encode HP explicitly.
- `engine/battle/ai/switch.c`: AI reads native battler identity, status, stats and HP; persistent updates encode HP explicitly.
- `engine/battle/move_effects/attract.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/baton_pass.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/beat_up.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/bide.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/conversion.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/conversion2.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/curse.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/disable.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/encore.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/false_swipe.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/frustration.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/fury_cutter.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/future_sight.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/heal_bell.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/leech_seed.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/mimic.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/pain_split.c`: Numeric health access and correct enemy recipient for the second HP update.
- `engine/battle/move_effects/pay_day.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/perish_song.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/present.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/protect.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/psych_up.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/pursuit.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/rapid_spin.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/return.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/rollout.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/safeguard.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/sketch.c`: Native battler consumer and deferred party access across the wild boundary.
- `engine/battle/move_effects/sleep_talk.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/spikes.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/spite.c`: Native battler consumer and deferred party access across the wild boundary.
- `engine/battle/move_effects/substitute.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/teleport.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/thief.c`: Migrate direct consumers of the active battler, side, selection or volatile state; preserve surrounding behavior.
- `engine/battle/move_effects/transform.c`: Native battler transform and correctly bounded base-stat/stage copies.
- `tools/tests/test_battle_ownership.py`: Four source-only guards against reintroducing retired ownership.
- `docs/battle_party_ownership.md`: Dependency map, ownership changes, remaining boundaries and acceptance status.
- `LOCAL_CHANGELOG.txt`: One coherent technical migration entry.

## Next ownership slice

Prioritize **remaining party/Pokemon ownership**. Reuse NativePartyMon as the
persistent owner for the six gameplay slots, including egg state and numeric
experience. Trace capture, party creation, PC/daycare transfer, items, evolution,
move learning and save/load as one closure. This removes the current import/export
boundary and native-to-packed-to-native round trips during rewards and switching.
The [native damage closure](battle_damage_ownership.md) now owns numeric damage,
delayed effects and substitute health. Remaining battle action/script state
should follow persistent-party ownership. Native saves should
follow a native persistent party, rather than lead this migration.

Recommended commit title: Move active battler state out of Game Boy memory

Recommended commit body: Own active battlers, turn selection, participants and stat
state in a native battle model. Use numeric HP/stats with explicit party/display
boundaries and add source-level ownership guards. Retain packed persistent-party
and move-script compatibility for the next migration phase; runtime validation is pending.
