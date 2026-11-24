# typical: the scan side

What fontconfig records at scan time from a font's own OpenType data, and
what this branch adds so that matching can use it.

## Audit: what the scanners already read

Base: upstream main at 3e9035e5. Two scanners:

- `src/fcfreetype.c` (the FreeType path), which is always built.
- `fc-fontations/*.rs` (the Rust path), built only with `-Dfontations=enabled`.
  It needs `bindgen` and crates, and it is not built on this machine.

| Data | fcfreetype.c | fc-fontations | Pattern object |
|---|---|---|---|
| OS/2 `usWeightClass` | read (`FcWeightFromOpenTypeDouble`, ×multiplier for named instances) | read (`fc_weight_from_os2`) | `weight` |
| OS/2 `usWidthClass` | read (1–9 → `FC_WIDTH_*`) | read (`fc_width_from_os2`) | `width` |
| OS/2 `fsSelection` | read indirectly: FreeType turns bits 0/5 into `style_flags`, which becomes `slant` and the weight fallback; bit 7 (USE_TYPO_METRICS) and bit 8 (WWS) are not used | read indirectly through skrifa `Attributes` (`static_slant`) | `slant` |
| OS/2 `achVendID` | read | read | `foundry` |
| OS/2 code page ranges | read (exclusive-language guess) | read | `lang` |
| OS/2 optical size (v5) | read | read | `size` |
| OS/2 PANOSE (10 bytes) | **not read** | **not read** | none, **added**: `panose` |
| name ID 16/17 (typographic family/subfamily) | read, ahead of IDs 1/2 | read | `family`, `style` (+ `*lang`) |
| name ID 21/22 (WWS) | read | read | `family`, `style` |
| `fvar` axes and named instances | read (`wght`/`wdth`/`opsz` → ranges; instance names via `strid`) | read | `weight`, `width`, `size`, `variable`, `namedinstance` |
| `STAT` | **not read** by either | **not read** | none |
| `meta` `dlng` | **not read** | **not read** | none, **added**: `designlang` |
| `meta` `slng` | **not read** | **not read** | none, **added**: `supportedlang` |
| GSUB/GPOS script tags | read (`GetScriptTags`) | read | `capability` (`otlayout:xxxx`) |
| GSUB/GPOS language systems and `locl` | **not read**: only the ScriptList tags are walked, never the LangSys records or the FeatureList | **not read** | none, **added**: `locllang` |

Weight and width already exist, from OS/2 and from `fvar`. Nothing is added for them.

STAT is read by neither scanner. Named-instance styles come from `fvar` and `name`. STAT was not on the add list, so it stays out. A matcher that needs elidable axis values or STAT-only style linking would need it later.

## What this branch adds

Four objects, all `FcTypeString` with a `NULL` comparator. `fcmatch.c` does not change, so matching is not affected.

| Object | C name | Value |
|---|---|---|
| `panose` | `FC_PANOSE` | One string, the ten PANOSE bytes in decimal separated by spaces (CSS2 `panose-1` notation), e.g. `"2 11 5 2 4 5 4 2 2 4"`. Left out when OS/2 is missing or all ten bytes are 0 ("Any" everywhere, so no classification). It is a single value on purpose: several values on one object would read as alternatives. |
| `designlang` | `FC_DESIGN_LANG` | One value per `meta` `dlng` ScriptLangTag, verbatim (`en-Latn`, `Hans`, `ur-Arab`, `Dupl`). Separators are commas and spaces. Tags with characters other than `[A-Za-z0-9-]`, or longer than 63 bytes, are dropped. |
| `supportedlang` | `FC_SUPPORTED_LANG` | The same for `meta` `slng`. |
| `locllang` | `FC_LOCL_LANG` | One BCP 47 tag per GSUB or GPOS language system whose LangSys lists `locl` as a feature index or as its required feature. The default LangSys of a script is skipped. Tags are deduplicated across scripts and across GSUB and GPOS, and sorted by OpenType tag. Unregistered tags are dropped. |

Why strings and not a langset: `dlng`/`slng` hold ScriptLangTags, many of them script-only (`Hans`, `Dupl`, `sunu`). An `FcLangSet` would fold these into "extra" languages and lose the distinction. Strings keep the font's own words, and the matcher can parse them.

### OpenType tag → BCP 47

`src/fcotlang.h` holds a sorted table of 675 entries, searched with `bsearch`. `build-aux/gen-otlang.py` generates it from HarfBuzz's `hb-ot-tag-table.hh` (here HarfBuzz 11.2.1), which HarfBuzz in turn generates from the OpenType language-system registry and the IANA subtag registry. The rule matches `hb_ot_tag_to_language`: the explicit choice for ambiguous tags, else the first two-letter language, else the first three-letter one. About 230 entries (`MOH` → `moh`, `SAT` → `sat`, …) are ones that HarfBuzz leaves commented out in its table, because there the tag is just the uppercased ISO 639-3 code and its fallback covers them. They are real registry entries, and local fonts use them (`SAT`, `RHG`, `JCT`), so they are included on purpose. The table was cross-checked against the installed libharfbuzz 15.2 (`hb_ot_tag_to_language` on every tag). It differs only on those fallback entries, where HarfBuzz appends a private `-x-hbot-…` suffix to the same language; on `DUJ` (a newer registry); and on the overrides below.

- `JAN` → `ja`, `KOR` → `ko`.
- Overrides to fontconfig's own orthography names: `ZHS` → `zh-cn`, `ZHT` → `zh-tw`, `ZHH` → `zh-hk`, `ZHTM` → `zh-mo`. HarfBuzz would give `zh-Hans`/`zh-Hant`.
- `APPH`/`IPPH` (phonetic, used by 80 local fonts) → `und-fonapa`/`und-fonipa`. These are valid BCP 47 and never collide with a real language.
- `MOL` → `ro-md`, `PGR` → `el-polyton`, and `SYRE`/`SYRJ`/`SYRN` → `und-syre`… for the script variants.
- Everything is lowercased, like fontconfig's other languages.

### PANOSE and variable fonts

OS/2 describes the default instance, so every named instance of a variable font carries the default's PANOSE. For example, Adwaita Sans "Thin" reports weight digit 5 (Book). The value is recorded verbatim. The matcher should take weight and width from `weight`/`width` (which come from `fvar`) and use PANOSE for the instance-invariant digits (family kind, serif style, and so on).

### Parsing

`meta`, `GSUB` and `GPOS` are loaded whole with `FT_Load_Sfnt_Table`. Every count and offset is bounds-checked against the table length, so a malformed table just yields nothing. Noto Sans CJK's GSUB is 173 KB. A full `fc-cache -f` over /usr/share/fonts (743 files) took 3.7–4.4 s with and without the patch, within run-to-run noise.

## Cache

The cache format does not change: the objects are appended to `fcobjs.h`, and the 1024-slot `FC_EXT_OBJ_INDEX` gap keeps older readers safe. But a cache written by older fontconfig lacks the new objects, so the dev snapshot must not share its cache name. This branch therefore does what upstream does in development (commit 1eb9286f): `cachesnapversion` goes 0 → 1, so the files become `.cache-13~snap1`. At release, `new-version.sh` turns that into `cacheversion = 13`.

`cachemincompat` stays at 9, and that is **an open decision for Joop**. The `meson.build` comment says it MUST be raised when an object cannot be rebuilt from old caches, and these four cannot. But raising it to 11 or more makes `test_genericfamily_backfill_contract` (CI cross-version job) fail on purpose: the test demands that the genericfamily backfill in `fcmatch.c` be deleted, because it becomes dead code. That is a matcher-side edit, so it is not done here.

What staying at 9 costs, measured with a temporary cachedir holding only the test font:

- The patched `fc-cache` writes `.cache-13~snap1`, plus `.cache-9/-10/-11` symlinks to it. `fc-list` from that cache shows all four objects.
- If 2.18's `fc-cache` wrote the cachedir first (`.cache-12` + `.cache-9…11` symlinks), the patched reader probes `.cache-11`, accepts it, and lists the font **without** `panose`/`designlang`/`supportedlang`/`locllang`. It does not rescan. The new objects stay silently missing until the patched `fc-cache` runs. Any machine whose caches come from the system fontconfig is in this state.

So the matcher must treat absence as "unknown". It has to anyway, for fonts without these tables. But before upstreaming, `cachemincompat` should be raised and the backfill removed in the same change. Note also that at release, `new-version.sh` sets `cachemincompat` to the previous cache version (12), which would still let 2.18's `.cache-12` be probed. The release that ships these objects needs 13.

## Fontations path: gap

`fc-fontations` does not emit the four objects. It could not be built here (no `bindgen`), and uncompiled Rust is not committed. Where fontations is enabled, `test/test_fontations_ft_query.py` compares the two scanners' patterns, and it will differ on these lines until `fc-fontations` gains the same reads. read-fonts parses OS/2, `meta` and the GSUB/GPOS LangSys records, so the port should be mechanical.

## What fonts here carry (743 faces under /usr/share/fonts, first face of each file)

- `meta`: 21 faces, all Noto: Nastaliq Urdu, Bengali (Sans and Serif), Duployan, Sunuwar, Dives Akuru and Toto. The tags are script-only or script-qualified (`ur-Arab`, `as-Beng`, `Dupl`, `sunu, latn`).
- Noto CJK carries no `meta`, but every face carries `locl` for `JAN KOR ZHH ZHS ZHT`. That gives `locllang: ja ko zh-hk zh-cn zh-tw` on all 10 faces of each TTC.
- `locl` language systems in 176 faces. The most common are the CJK five, `APPH`/`IPPH` (80), `SRB` (78), `MKD`, `MOL`, `ROM` and `CAT`.
- A nonzero PANOSE in 643 of 743. Noto CJK says only `2 11 5 0 0 0 0 0 0 0`. Liberation, Adwaita, Cantarell and Source Code carry full classifications.
- `STAT` in 5: Adwaita Sans ×2, Cantarell VF, and Source Code VF ×2.
