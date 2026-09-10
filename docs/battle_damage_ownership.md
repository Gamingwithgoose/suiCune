# Battle damage ownership

## Dependency map

The damage closure spans home/battle, battle core, effect commands, weather and
badge modifiers, AI scoring, move effects, and multihit battle text.

- Current damage: packed WRAM working state produced by damage calculation,
  modifiers and fixed-damage moves; consumed by HP application, recoil, draining,
  Counter/Mirror Coat, delayed attacks and AI scoring.
- Damage taken: packed WRAM Bide accumulation also aliased as a byte for multihit
  counts and text. These are separate logical values and require separate owners.
- Future Sight: side-owned pending damage and a gameplay countdown; survives
  switching and is consumed by end-of-turn move dispatch.
- Substitute HP: side-owned battle health, with an accidental byte capacity;
  created by Substitute and consumed by ordinary damage interception.
- Enemy HP at player switch: snapshot consumed only by withdrawal text selection.
- Triple Kick: damage multiplier currently reads an animation parameter. The
  hit sequence must own this value and project it to presentation.
- HP buffers and animation parameters: downstream presentation compatibility.
- Persistent packed party, action flags, move metadata, script dispatch and
  shared Rollout/Bide/multihit countdowns remain authoritative legacy dependencies
  outside this numeric damage closure. Save/link formats remain unchanged.

The native model owns numeric damage and participant damage state. Retired WRAM
slots retain layout only; they have no runtime readers or writers. Entry into a
new battle clears the model. Pending Future Sight persists across switches;
Substitute follows the existing condition/Baton Pass lifecycle.

## Result

`BattleState.damage` and each participant's accumulated and pending damage are
host-native uint32 values. Scaling uses uint64 intermediates and saturates only
at UINT32_MAX; additions check overflow before adding. HP application and healing
compare against numeric uint16 HP before narrowing. HP remains limited by the
persistent party boundary; Substitute HP now supports the full maxHP/4 range.
The base damage formula's gameplay DAMAGE_CAP and turn/hit-count rules remain.

All direct producers and consumers of the ten retired WRAM fields use native
state, including AI comparisons, Counter, Mirror Coat, Bide, fixed-damage moves,
weather, badge/type modifiers, recoil, draining and delayed attacks. No damage
projection is written back to WRAM. Multihit text reads its separate bounded hit
count. Triple Kick owns its sequence and projects that count to animation only.
Future Sight consumes and clears its pending state once. BattleIntro clears the
complete model before the next battle; switching preserves pending side effects.
Bide accumulation retains the existing pre-HP-application timing, and fainting
clears the affected side's accumulation. Substitute health is meaningful only
while its condition is set, including across Baton Pass.

Numeric fixes within this closure:

- DoubleDamage now stores the doubled value instead of byte-swapping the old
  value. Triple Kick no longer mistakes bits 8-15 for overflow.
- Substitute does not truncate its cost to a byte or change stored health when
  the move fails. A depleted substitute has zero stored health.
- Draining clamps the native health result; it no longer fills to maximum through
  the old HP-buffer comparison. Recoil also mutates before display projection.
- Pain Split computes both native recipients from their original HP before
  presentation and no longer transports its average through global damage.
- Enemy healing cannot underflow withdrawal-text damage loss. Zero quarter-HP
  avoids division by zero in that presentation calculation.
- OHKO damage covers the target or its active substitute without a packed-word
  sentinel. False Swipe cannot underflow a zero-HP target.
- Fury Cutter's five-stage gameplay counter cannot wrap after 255 uses.

## Remaining boundaries and next closure

1. **Authoritative legacy gameplay:** persistent `gPokemon.partyMon` records,
   opponent party records, byte content IDs, action/move metadata, shared move
   countdowns and the battle script cursor remain. They are consumed by switching,
   rewards, move learning, items, evolution and move dispatch. Migrate persistent
   party ownership next, reusing NativePartyMon with numeric experience across
   capture, storage/daycare transfer, rewards, evolution, items and save/load.
   Then migrate action state and script execution. Packed-party round trips remain
   a limitation; this damage closure does not complete persistent-party migration.
2. **Presentation compatibility:** HP buffers are written from native old/new HP
   for AnimateHPBar; animation parameters receive the native kick count. Remove
   these when those consumers accept typed presentation events. Neither buffer
   controls the migrated HP computation or Triple Kick multiplier.
3. **Save/link compatibility:** packed PartyMon and external formats retain their
   layout. They remain necessary to unmigrated import/export and link paths.
   Native saves should follow native persistent party ownership.
4. **Historical/layout-only:** retired WRAM declarations, global.h RAMMAP symbols
   and commented ASM references retain historical addresses. There are no active
   C or header consumers of the selected addresses. Remove layout padding only
   with migration of adjoining unmigrated WRAM ranges.
5. **Other battle dependencies:** remaining gb_read/gb_write include the debug
   SRAM skip-battle flag and other unmigrated script/mobile/save paths. HRAM and
   WRAM also serve link ordering, random state, action flags and display setup.
   These remain outside numeric damage ownership, not damage compatibility caches.

## Validation

Eight source ownership guards pass, scanning C and headers for retired fields,
address transport, packed battler consumers, scalar misuse, damage endian
adapters, presentation-owned Triple Kick damage, Bide/text aliasing and HP-buffer
reads in Pain Split. Source review covers numeric widths, narrowing, all selected
field references and battle/side lifetimes. Build/runtime validation: not
performed. The source guards do not establish gameplay correctness.

Runtime acceptance remains pending: ordinary attacks and modifiers, both sides'
HP loss and fainting, Bide, Counter/Mirror Coat, recoil and draining, Substitute
creation/depletion/Baton Pass, Future Sight across switching and battle re-entry,
multihit text and Triple Kick, Pain Split with unequal max HP, PP consumption,
party writeback, rewards, level-up, battle exit and the next battle. Include
Substitute max HP above 1023, damage above 65535, zero HP, and overflow boundaries
in native arithmetic tests. Cosmetic animation parity is separate from these
state checks.

Counts are comment-stripped literal occurrences in engine/battle/**/*.c,
engine/pokemon/**/*.c and home/{battle,battle_vars,pokemon}.c, using the same scope
as the active-battler migration:

| Token | Before | After |
|---|---:|---:|
| `wram->` | 3470 | 3368 |
| `hram` | 177 | 177 |
| `gb_read` | 22 | 22 |
| `gb_write` | 19 | 19 |
| `cpu_reg` | 0 | 0 |

## Changed files

- `constants/types.h`: Own native damage, side effects and hit counts in the battle model.
- `wram.h`: Retire the ten former authoritative fields while retaining their layout.
- `wram.c`: Remove address assertions for retired damage fields.
- `home/battle.c`: Provide overflow-safe native arithmetic and bounded HP mutation; reset native damage.
- `home/battle.h`: Declare numeric damage and widened HP-operation interfaces.
- `engine/battle/core.c`: Use native reset, delayed-effect and withdrawal-snapshot state.
- `engine/battle/effect_commands.c`: Migrate damage computation, application, modifiers, multihit state, recoil and draining.
- `engine/battle/misc.c`: Use native weather/badge damage arithmetic.
- `engine/battle/ai/scoring.c`: Compare full-width native damage estimates.
- `engine/battle/move_effects/bide.c`: Accumulate and release native damage separately from multihit text.
- `engine/battle/move_effects/counter.c`: Double native damage without packed-word limits.
- `engine/battle/move_effects/mirror_coat.c`: Double native damage without packed-word limits.
- `engine/battle/move_effects/false_swipe.c`: Compare full-width damage and protect zero-HP subtraction.
- `engine/battle/move_effects/fury_cutter.c`: Scale native damage with a bounded gameplay stage.
- `engine/battle/move_effects/future_sight.c`: Own and consume pending damage in native participant state.
- `engine/battle/move_effects/pain_split.c`: Compute shared HP directly in both native battlers.
- `engine/battle/move_effects/pursuit.c`: Scale native damage while switching.
- `engine/battle/move_effects/rollout.c`: Scale native damage without artificial word overflow.
- `engine/battle/move_effects/substitute.c`: Use full-width substitute health and commit only successful creation.
- `engine/battle/move_effects/triple_kick.c`: Own the multiplier in gameplay and project its sequence to animation.
- `data/text/battle.c`: Read dedicated hit counts instead of aliased Bide damage bytes.
- `tools/tests/test_battle_ownership.py`: Extend ownership guards to headers and damage/presentation boundaries.
- `docs/battle_damage_ownership.md`: Record the damage closure, dependency classification and pending validation.
- `docs/battle_party_ownership.md`: Link the completed damage closure and retain persistent-party priorities.
- `LOCAL_CHANGELOG.txt`: Record one coherent native damage migration section.

Recommended commit title: Move battle damage and delayed effects into native state

Recommended commit body: Own damage, Bide accumulation, Future Sight, substitute
health and hit counts in the native battle model. Remove packed damage transport,
separate gameplay multipliers from presentation and use bounded numeric HP
updates. Extend source ownership guards; persistent-party and action/script
ownership remain pending. Build/runtime validation is not performed.
