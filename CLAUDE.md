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
- Options: `-DWOWDECOMP_DISABLE_SIGNATURE_CHECKS=OFF` to restore the signature checks (default ON: modified MPQs /
  interface `.toc` files need them off),
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

## Related repos (sibling checkouts in `d:\GitHub` unless noted)

The project ships in two parallel flavours: **32-bit** race masks (stock DBC/packet layout, at most 31 playable races)
and **256-bit** race masks (this fork's layout, races up to 255). Each flavour's server, client, DBCTool and WHE must
match; a 256-bit DBC or packet is unreadable by a 32-bit build and vice versa. Shared client assets live once.

| Part | 32-bit | 256-bit |
| --- | --- | --- |
| Client | `wowdecomp` (upstream here; also the 32-bit client, carrying the same race/retail work on stock masks) | `wowdecomp-256-bit` (this repo) |
| Server | `azerothcore-wotlk-32-bit` (stock masks; git `upstream` = mod-playerbots/azerothcore-wotlk, branch `Playerbot`) for developing and testing WHE-32-bit's `patches/whe-acore.patch` (and `whe-playerbots.patch`) | `azerothcore-wotlk-256-bit` fork (256-bit `RaceMask` throughout) + WHE-256-bit's `patches/whe-acore.patch` for the module hooks |
| Server modules | stock mod-playerbots + patch | `mod-playerbots-256-bit`, `mod-transmog-256-bit`, `mod-ale-256-bit` |
| DBC tool | `DBCTool-32-bit` | `DBCTool-256-bit` (four 64-bit race-mask columns) |
| Race module (DBC SQL modpaks, world SQL, scripts) | WHE-32-bit = `mod-worgoblin-high-elf`, checked out at `C:\Users\Medviten\Desktop\WoW Mod Folder\GitHub\mod-worgoblin-high-elf` (not in `d:\GitHub`) | `WHE-256-bit` (git `upstream` = mod-worgoblin-high-elf) |
| Client assets (`patch-A.MPQ/` folder: models, textures, interface; `Optional/patch-J.MPQ`) | `WHE-common` | `WHE-common` |

- Modpaks (`modpaks/F-0xx_*`) exist in both WHE repos with the same ids; a fix to a modpak's SQL usually has to be
  made in both checkouts (only the race-mask columns differ). Race ids are assigned by manifest order
  (`tools/scripts/build-dbc.manifest`), so they can differ between checkouts — look them up, don't assume.
- WHE modpaks can carry `patches/client/*.patch` for this repo and `patches/core/` for the server; `build-dbc.sh`
  combines them per target.
- Assets are not in either WHE repo: add or fix BLP/M2 files in `WHE-common/patch-A.MPQ/`, never per flavour.
- A change to client code here usually needs porting to `wowdecomp` too (and vice versa), minus the mask layout.
