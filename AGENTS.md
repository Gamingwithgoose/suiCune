# Native runtime migration

This project targets a native C99/SDL2 runtime. C source alone does not make a
subsystem native: authoritative state, control flow, identities, resources, and
capacities must not depend on Game Boy registers, banks, address translation,
WRAM/HRAM/VRAM/OAM ownership, emulator dispatch, or hardware limits.

- Use semantic native structs, IDs, direct C calls, and dynamic resources.
- Keep legacy formats only at explicit, narrow compatibility boundaries with
  checked conversions. Do not silently narrow native values.
- Migrate ownership rather than maintaining synchronized native/legacy
  sidecars. Do not add wrappers that merely access the former hardware state.
- Preserve existing gameplay unless removing a hardware restriction requires a
  deliberate documented change.
- Preserve local modifications. Do not commit, push, stage, reset, restore,
  clean, rebase, or modify `lib/libmobile` without explicit scope.
- Do not delete ASM/reference material until native ownership is complete and
  source/build/runtime evidence shows it has no remaining use.
- Maintain `LOCAL_CHANGELOG.txt` as concise technical history only; record
  ownership changes, retained boundaries, static validation, and unperformed
  build/runtime validation.
- Do not build, launch, or runtime-test unless explicitly requested.

## Project history and documentation

Project history and durable technical documentation describe the software, not
the development conversation or authorship process. `LOCAL_CHANGELOG.txt`,
commit recommendations, migration documentation, and durable source comments
must never identify AI, artificial intelligence, Codex, an agent, assistant,
model, prompt, conversation, chat, session, user, user request, user
instruction, or similar development-process/authorship wording as the reason
work was performed.

Record runtime observations as software facts. For example, write “Runtime
regression: opponent Tackle crashes during its battle-animation sequence,” not
an account of who observed it; write “Build/runtime validation: not performed,”
not why it was omitted; and write “Migrated battle-animation state to native
runtime ownership,” not who performed the migration.

Documentation cleanup must preserve legitimate engineering history: technical
changes, affected files, subsystem names, regressions, fixes, failed technical
experiments, reversions, architectural decisions, compatibility boundaries,
validation status, known limitations, and remaining technical work. Remove or
rewrite only conversational/authorship/process framing. These rules apply to
all future work.
