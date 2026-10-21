# CLAUDE.md

Fork of [wowdecomp](https://github.com/Medviten/wowdecomp) (C++ decompilation of the WoW 3.3.5a `Wow.exe`, build 12340)
that adds custom-race support: 256-bit race masks, race ids up to 255, altered forms (Worgen/Gilnean), and assorted
client QoL features. See `README.md` for the upstream project overview and build requirements.

Remotes: `origin` = this fork, `upstream` = wowdecomp. Upstream changes are pulled in with merge commits
("Merge upstream wowdecomp (...)").

## Build

Windows + Visual Studio 2022 + CMake only. In-source builds are refused. x64 is the target used here.

```powershell
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64
cmake --build build/x64 --config Release --target WowClient --parallel
```

- Output goes to `bin/Win/` (`WowClient64.exe`, `XvidDecoder.dll`). Game `Data/` must be copied there to run.
- `WowClient` = client + crash logger (console, `WowClient_crash.log`); `Wow` = same without it.
- Options: `-DWOWDECOMP_DISABLE_SIGNATURE_CHECKS=ON` (needed for modified MPQs / interface `.toc` files),
  `-DWOWCLIENT_CRASH_LOG=OFF`, `-DWOWCLIENT_OUTPUT_OVERRIDE=<name>`.
- Win32 still builds (needs NASM + `ThirdParty/DivxDecoder/DivxDecoder.dll`); it exists to check struct layouts
  against the 32-bit original. Arch macros: `WOWDECOMP_ARCH_64` / `WOWDECOMP_ARCH_32`.
- There is no test suite; verification is building and running the client against a server.

## Layout

- `Engine/Source/<Module>` — engine (Storm, Gx D3D9/OpenGL, Model2, Frame/FrameXML/FrameScript, SoundEngine incl.
  fmodex, lua-5.1, ...). `WoW/Source/<Module>` — game (DB, Object, Glue, Ui, UIUtil, WorldClient, Net, Magic, ...).
  `WoW/Common` — shared game code (ComponentCore = character model compositing). `WoW/Source/Win64|Win32` — WinMain.
- Each module is a static library; its `CMakeLists.txt` globs `*.cpp` with `CONFIGURE_DEPENDS`, so new `.cpp` files
  only need a CMake re-configure, not a list edit. New modules must be added to the root `CMakeLists.txt`.
- Includes are rooted at the repo root: `#include "WoW/Source/DB/RaceMask.h"`.
- `ThirdParty/` holds upstream libraries (zlib, freetype, expat, xvidcore, ...), linked not decompiled.

## Code conventions

- File and class names follow the original client (`CGlueMgr`, `Unit_C`, `*_Scripts.cpp` for Lua bindings,
  `*_CVar.cpp` for CVar registration, `DB/Rec/*Rec.h` for DBC records).
- Every reconstructed function is tagged with the original address and a verification marker:
  ```cpp
  // VERIFY: 99831e05 manual
  // OFFSET: 0x004E63A0
  void CNameGen::GenerateName(char* buffer, uint32_t size) {
  ```
  Keep these tags intact when editing a function. Do not add `OFFSET`/`VERIFY` tags to new code that has no
  counterpart in the binary.
- Struct members carry hex offset comments (`/* 0030 */`) matching the 32-bit original layout.
- Style: 4-space indent, `this->` on member access, `m_` members, `s_` statics, braces on every `if`,
  `public:` indented inside the class, fixed-width int types, `static_cast`.
- Use Storm string/memory helpers (`SStrCmp`, `SStrCmpI`, `SStrCopy`, `STORM_MAX_STR`) like surrounding code.

## Fork-specific changes

**Mark every deviation from the original with a `// Custom:` comment** explaining what changed and why. This is
how fork changes are told apart from decompiled code (and kept through upstream merges). When a change shifts a
struct, say so (e.g. "the offsets below are those of the stock 32-bit layout").

Key pieces:

- **256-bit race masks** — `WoW/Source/DB/RaceMask.h`: `RaceMask` = 8 × uint32, bit `race - 1` per race
  (`HasRace`), except HelmetGeosetVisData hide masks, which index by race id itself (`HasBit`). Replaces the int32
  masks in `FactionRec`, `SkillLineAbilityRec`, `SkillRaceClassInfoRec`, `TalentTabRec`, `HelmetGeosetVisDataRec`
  (so their `s_numColumns`/`s_rowSize` differ from stock), item `AllowableRace` (`ItemStats_C`, cache version bumped
  to 6) and the who-request mask (`FriendList`). Layout must match DBCs written by **DBCTool-256-bit** and packets
  sent by the matching **azerothcore-wotlk-256-bit** server — changing either side means changing the others.
- **Race limits removed** — race ids up to 255 (no 21-race cap); `RealmConnection::HandleCharEnum` no longer rejects
  more than 10 characters per realm.
- **Altered forms** — CVar `alteredFormRaces` ("race=alteredRace" pairs, default `22=23` Worgen = Gilnean) stands in
  for Cata's `ChrRaces.m_alteredFormRaceID`; `ClientGetAlteredFormRace()` in `Client_CVar.cpp`. Two Forms /
  Running Wild handling lives mainly in `Unit_C.cpp`, `CharacterCreation*.cpp`, `BarberShop*.cpp`,
  `DressUpModelFrame.cpp`. CVar `shapeshiftSkinColorRaces` picks races whose shapeshift models follow skin color.
- **Auto-login** — `-login -password -realmlist -realmname` command-line switches (`CMD_LOGIN`..`CMD_REALM_NAME` in
  `Client.h`, handled by `CommandLineAutoLogin` in `CGlueMgr.cpp`).
- **Names** — spaces allowed (`ValidateName.cpp`), name cache is case-sensitive (`DBCacheInstances.cpp`).
- **Graphics CVars** — `farClipLimit`, `disableFog`, `grassWind*`, `grassParting` (`WorldParam_CVar.cpp`,
  `DetailDoodad.cpp`); camera `cameraZoomLimit`/acceleration/character fade (`Camera_CVar.cpp`); Stormwind
  occluder removed (`CWorldOcclusion.cpp`).
- Guild bank fix (`GuildBankFrame*.cpp`), character-creator Worgen/human and druid-form previews.

To see the full fork delta: `git diff 4119c4b HEAD -- WoW Engine` (4119c4b is the last pure-upstream commit before
the fork's work), or grep for `// Custom`.

## Related repos (sibling checkouts in `d:\GitHub`)

- `wowdecomp` — upstream decomp.
- `DBCTool-256-bit` — DBC ⇄ MySQL tool producing the 256-bit race-mask DBCs this client reads.
- `azerothcore-wotlk-256-bit`, `mod-playerbots-256-bit` — server side with matching 256-bit protocol.
- `WHE-256-bit` / `mod-worgoblin-high-elf` — AzerothCore module adding the custom races (DBCs + world SQL).
