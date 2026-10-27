# Retail races: plan

Port Kalibro's retail race layer (`D:\GitHub\Kalibros-decomp`, branch `Esteria`, remote
`STRHercules/wowdecomp`) so chosen races use retail models with full retail customization, starting with
Dracthyr. Both forks start at upstream `4119c4b`, so his changes are diffs against the code we started from.
His design notes: `Kalibros-decomp/docs/retail-race-handoff.md`, `retail-casc.md`, `cdn-downloads.md`.

## Decisions (2026-10-26)

- **Per race, opt-in.** A race uses the retail model only when mapped (his `WOW_RETAIL_RACES` /
  `Data\Retail\RetailRaces.csv`, Wrath race -> retail ChrRaces id). Unmapped races, NPCs and non-retail models
  stay stock. We expose the mapping as a CVar like `alteredFormRaces`.
- **Data: files first, CDN fallback.** Read `retail/<FileDataID>` from files we ship (a pack gathered per
  mapped race); fall back to his CascLib online mode (Blizzard CDN, pinned build) when a file is missing.
- **Persistence: full choice list.** Beyond his five-byte packed codecs: server storage and packets in
  azerothcore-wotlk-256-bit so every choice persists and other players see it (his phases 2-3, unimplemented).
- **Dracthyr: dragon form** (retail's native form, his default). Visage later, possibly as an altered form.
- **Licence:** his `Db2.*` / `Appearance.*` derive from GPL-3.0 wxl-db2. Ported as-is; settle distribution
  terms before shipping.

## Phases

1. **Engine.** `Engine/Source/Retail` (RetailAssets library: CascSource, DecodedCache, Db2, Appearance,
   Compose, RetailModel, RetailShader, ClientTables, BusyNotice), `cmake/RetailAssets.cmake` (CascLib by
   pinned archive), option `WOWDECOMP_RETAIL_ASSETS`, SFile `retail/` routing, Model2 changes (shared
   skeleton splice, external AFM2/AFSB animations, >256-bone rigs), probes/tests. Add the files-first source.
   Check: build, `RetailAssetProbe` loads the Dracthyr models from files.
2. **Characters.** `RetailRace.{h,cpp}` and its hooks (CharacterComponent, ComponentUtils, Unit_C,
   CharacterSelection/Creation), without his Esteria DLL coupling (`EsteriaAppearance.cpp`). Mapping CVar.
   Check: Dracthyr in character creation and in the world.
   Client wire side done here too: opcodes `SMSG_RETAIL_LOOK` 0x524, `CMSG_RETAIL_FORM_LOOK` 0x525,
   `SMSG_RETAIL_FORM_LOOK_ACK` 0x526 (`Net/Types.h`, routed by a side table in `NetClient.cpp` since they are
   past `NUM_MSG_TYPES`; registered on `RealmConnection`), `RLE1`/`RLP1` char-enum tails
   (`RetailRaceEnumTail`), `RLK1` footer on create / customize / race / faction change. Kalibro's eye-effect and
   body-slider footers (`BSH1`, `BSE1`) are not sent.
3. **Creator UI.** Our GlueXML panel for the retail options (his `EA_*` contract under our own names),
   choice names, random, preview. Done as `RetailRace_CharacterCreate.lua` in WHE-256-bit's
   `data/patch-A.MPQ/Interface/GlueXML` (also in the squished client's patch-B): scrolling option panel in place of
   the stock rows, Two Forms tabs and "Show both forms", Randomize via `EA_RANDOM`, `RetailLoadExisting()` for
   paid customization, wheel zoom / right-drag pan for all races. Not done: hover preview of choices
   (`EA_PREVIEW_*`).
4. **Persistence.** His later commits (78ac78e7 body sliders, 566014cd Two Forms) already carry the full look:
   `Engine/Source/Retail/RetailLookWire.h`, shared verbatim with his server (`EsteriaWow`,
   `src/server/shared/RetailLookWire.h`): 132-byte payload (version, race, sex/form, count, 64 u16 choice
   indices), client footer `RLK1` on create/customize, `RLE1` block after SMSG_CHAR_ENUM, and
   SMSG_ESTERIA_RETAIL_LOOK for other players. Port that to azerothcore-wotlk-256-bit; keep the five stock
   bytes filled for bots/tools.
   Done (uncompiled): `src/server/shared/RetailLookWire.h`, opcodes 0x524-0x526 (`SMSG_RETAIL_LOOK`,
   `CMSG_RETAIL_FORM_LOOK`, `SMSG_RETAIL_FORM_LOOK_ACK`), tables `character_retail_look` / `character_form_look`
   (pending characters update), footers read on create / customize / race and faction change (a change without
   one drops the saved look), RLE1/RLP1 enum tails, look sent on login and when a player comes into view,
   `Player::ToggleRetailForm` for an in-world form spell, `RetailLook.TwoFormsRaces` (default `22=1,52=3,70=3`).
5. **Dracthyr modpaks.** WHE F-0xx for Alliance 52 / Horde 70 (70 is above 64: second race-mask word),
   retail asset pack tool, CoA's Dracthyr racials (CoA races 54/58) activated.
   Done: WHE F-073/F-074 from `tools/races/dracthyr_modpaks.py` (Home/Kin Human/Orc, their stock model and
   CharSections rows as what non-Retail clients see), CoA racials via `coa_racials.py` (ids appended after the
   others), manifest entries off by default. GlueXML already had the slots/icons/race info. Not done: retail asset
   pack tool, an in-world Visage spell calling `ToggleRetailForm`.
6. **More races** opt in (Haranir, Earthen, Mechagnome, Vulpera, Highmountain ...).

## Risks

- He renders with D3D11/Vulkan backends we do not have (we have D3D9/D3D9Ex/OpenGL); his notes say the
  Vulkan path for composed textures is untested. Retail shaders are degraded to legacy blend modes, which
  should be renderer-independent, but this needs checking early.
- Our fork changed CharacterComponent too (creatureSkinRaces, altered forms, 256-bit masks): merge by hand.
- His later commits mix in body sliders, Kyrian/Venthyr, Two Forms and skyriding; port only what retail
  races need.
