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
