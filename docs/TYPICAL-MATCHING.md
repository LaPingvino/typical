# How fontconfig matches fonts, and where it goes wrong

Branch `typical`, on upstream main after 2.18.4 (`3e9035e5`). This document
maps the matcher and changes nothing. File:line references are to that tree.
Experiments ran against the fonts installed on this machine (853 faces), with
the library built from this tree and the system configuration in
`/etc/fonts`. `test/typical/` reproduces every case below (see the last
section).

Summary:

* Matching compares a fixed list of 30 scores in a fixed order and stops at
  the first difference. Family comes first, then language coverage, then
  everything else. Weight, width and slant come last, and are scored as plain
  numeric distances.
* The language score asks only one question: does the font's cmap cover the
  language's orthography? It never asks whether the font was designed for that
  language. A pan-CJK font covers ja, ko and zh equally, so the language score
  ties, and a config list that names only "Noto Sans CJK KR" breaks the tie.
  The result is KR glyphs for `ja`, `zh-cn`, `zh-tw` and `zh-hk`.
* Of the OpenType data that says what a font is *for* or *like*, fontconfig
  reads only usWeightClass, usWidthClass, the optical size range, the
  code-page bits (as a narrow exclusion), name IDs 1/2/16/17/21/22, fvar, and
  the GSUB/GPOS script tags (stored but never matched). It ignores `meta`
  (dlng/slng), `STAT`, PANOSE, sFamilyClass, the GSUB/GPOS language systems
  and the language tags of name records.
* The fonts installed here would not fix CJK by themselves. The Noto CJK
  2.004 faces have no `meta` table, identical GSUB language systems, and
  near-identical code-page bits. The only thing that tells the JP face from
  the KR face is the language ID on their name records. The fix for CJK
  belongs mainly in config, and CSS Fonts 4 supports that: per CSS, choosing a
  font per language for a generic family is the UA's job.

---

## 1. How matching works today

### 1.1 The pipeline

A client (here `fc-match`, `fc-match/fc-match.c:187-212`) does three things:

1. **`FcConfigSubstitute(…, FcMatchPattern)`** runs every `<match
   target="pattern">` rule and `<alias>` in config order
   (`src/fccfg.c:2001` onward). Two things happen here as well:
   * The default languages are added to `lang` as *weak* values, unless the
     request already asks for one of them: `FC_LANG`, else the locale
     (`src/fccfg.c:2030-2073`). A request `:lang=ja` under `FC_LANG=en`
     becomes `lang: ja(s) "en"(w)`.
   * Aliases expand generic names. `sans-serif` turns into a list of about
     110 family names (from `45-latin.conf`, `60-latin.conf`,
     `65-nonlatin.conf`, …), all bound **weak**, around the original strong
     `sans-serif`. Aliases with `<default>` also tag the request with a
     `genericfamily` value (the alias sugar in `src/fcxml.c:2040-2180`).
     `48-guessfamily.conf` and `49-sansserif.conf` then fill in a generic
     from substrings ("mono", "sans", "serif"). As a last resort they append
     `sans-serif`.
2. **`FcConfigSetDefaultSubstitute`** (`src/fcdefault.c:287`) fills in
   whatever is still missing: weight=regular, slant=roman, width=normal
   (`:297-304`), size and pixel size, `familylang`/`stylelang` = locale plus
   a weak `en-us` (`:366-376`), and the boolean defaults
   (`src/fcdefault.c:35-44`). One of these is **`variable=False`**.
3. **`FcFontMatch`** (`src/fcmatch.c:1082`) or **`FcFontSort`**
   (`src/fcmatch.c:1396`) scores every font in the cache against the
   request. The winner goes through `FcFontRenderPrepare`
   (`src/fcmatch.c:727`), which merges request and font and runs the
   `<match target="font">` rules. Synthetic bold and oblique come from
   `90-synthetic.conf` at this stage.

### 1.2 The score vector and its order

`FcCompare` (`src/fcmatch.c:662-725`) walks the request and the font together,
object by object (the objects are sorted). For each object both have, it adds
a number into one slot of `double score[PRI_END]`. **An object the font
lacks scores 0, the same as a perfect match.** The slots are ordered by the
`FcMatcherPriority` enum (`src/fcmatch.c:359-391`):

| idx | slot | compare fn | what it means |
|----:|------|------------|---------------|
| 0 | FILE | filename | only if the request names a file |
| 1-4 | FONT_WRAPPER, FONTFORMAT, VARIABLE, NAMED_INSTANCE | string/bool | **`variable=False` default lives here** |
| 5-7 | SCALABLE, COLOR, FOUNDRY | | |
| 8 | CHARSET | `FcCharSetSubtractCount` | only if the request carries a charset |
| 9 | FAMILY (strong) | hash lookup | index of first strong family name the font has, else 1e99 |
| 10 | GENERIC_FAMILY | equal | serif/sans/mono/… class, 0 or 1000 |
| 11 | POSTSCRIPT_NAME (strong) | | |
| 12 | **LANG** | `FcCompareLang` | 0 equal, 1000 other territory, 2000 not covered (+100 × request position) |
| 13 | FAMILY (weak) | hash lookup | index of first weak family name, else 1e99 |
| 14 | POSTSCRIPT_NAME (weak) | | |
| 15-18 | SYMBOL, SPACING, SIZE, PIXEL_SIZE | | |
| 19 | STYLE | string | only if the request names a style string |
| 20 | SLANT | number | |
| 21 | WEIGHT | range distance | in fontconfig units, **not** OpenType units |
| 22 | WIDTH | range distance | |
| 23-27 | FONT_HAS_HINT, DECORATIVE, ANTIALIAS, RASTERIZER, OUTLINE | | |
| 28-29 | ORDER, FONTVERSION | | last tie-breakers |

`FcFontSetMatchInternal` (`src/fcmatch.c:905-1051`) keeps the font whose
vector is lexicographically smallest (`:951-960`). Each slot is strictly more
important than every slot after it. Nothing is weighted or summed across
slots.

For each object, `FcCompareValueList` (`src/fcmatch.c:435-512`) tries every
request value against every font value. It keeps the best
`compare × 1000 + request_index × 100 + font_index` (`:472`). So a value
earlier in the request's list wins, and among equally good fonts, the order
of values inside the font matters a little.

Family is special-cased. `FcCompareDataInit` (`src/fcmatch.c:535-581`) hashes
the request's family list once, keeping the first position of each name for
strong and for weak bindings separately. `FcCompareFamilies`
(`src/fcmatch.c:583-624`) then writes two slots: **strong family (9)** and
**weak family (13)**. That split is the whole meaning of "binding": a family
the user typed (strong) outranks language. A family that config added (weak)
ranks after language.

`GENERIC_FAMILY` (slot 10) arrived in 2.18.0 (`21f65e1c`, issue 457). It first
sat *before* strong family and later moved after it. Fonts get the value at
scan time (§2.6). Old caches get it backfilled by `FcCompareGenericFamilyBackfill`
(`src/fcmatch.c:634-655`, `:715-722`).

### 1.3 How language is compared

* **What a font's `lang` is.** At scan time, `FcLangSetFromCharSet`
  (`src/fclang.c:96`) marks a language as supported if the font's cmap
  covers every code point in fontconfig's orthography file for it
  (`fc-lang/*.orth`). This is **coverage, not intent.** A pan-CJK font gets
  `ja`, `ko`, `zh-cn`, `zh-tw` and `zh-hk` together. The one OpenType input
  is `FcLangIsExclusiveFromOs2` (`src/fclang.c:1029-1058`). If exactly one of
  the four CJK code-page bits (JIS, GB2312, KSC, Big5; table at
  `src/fclang.c:41-46`) is set in OS/2 `ulCodePageRange1`, the other CJK
  languages are left out. When two or more are set, nothing is excluded.
* **Comparing.** `FcCompareLang` (`src/fcmatch.c:107-150`) calls
  `FcLangSetHasLang` / `FcLangSetCompare` (`src/fclang.c:590`, `:651`) and
  maps the result to 0 (equal), 1 (same language, different territory) or
  2 (different). All the request's languages are tried, and the best value
  counts.
* **In sorting**, `FcFontSetSort` adds a second pass
  (`src/fcmatch.c:1320-1357`). After a first sort, only the *first* font that
  satisfies each requested language keeps its lang score. Every other font
  gets `lang = 10000`, and the list is sorted again. This pushes one font per
  language near the top of a fallback list.

### 1.4 Fallback lists: FcFontSort plus the charset trim

`FcFontSort(trim=True)` (what `fc-match -s` and Pango use) scores every font
the same way and sorts with `FcSortCompare` (`src/fcmatch.c:1123-1138`). Then
`FcSortWalk` (`src/fcmatch.c:1140-1200`) **drops every font whose charset
adds no new code point** to the union of the fonts above it. So a fallback
list only ever names fonts that bring new characters. A renderer then takes,
for each character, the first font in the list that has it.

A consequence: once one CJK face is in the list, every sibling with the same
cmap (JP, SC, TC, HK) is **trimmed away**. For `sans-serif:lang=ja` the
trimmed list holds exactly two CJK faces, and the first is KR. Japanese text
cannot fall back to the JP face.

### 1.5 Worked example: `sans-serif:lang=ja` with Noto CJK

```
$ FONTCONFIG_PATH=/etc/fonts FC_LANG=en FC_DEBUG=3 \
    $TMPDIR/typ-build/fc-match/fc-match 'sans-serif:lang=ja'
Match Pattern has 28 elts
	family: "DejaVu LGC Sans"(w) "Noto Sans"(w) ... "Noto Sans CJK KR"(w) ... "sans-serif"(s) ...
	lang: ja(s) "en"(w)
	genericfamily: 2(i)(w)            # sans-serif
...
Score 0 0 0 0 0 0 0 0 0 1e+99 0 0 0 1e+99 0 … | "Noto Sans CJK JP" Regular
Score 0 0 0 0 0 0 0 0 0 1e+99 0 0 0 69    0 … | "Noto Sans CJK KR" Regular
Score 0 0 0 0 0 0 0 0 0 1e+99 0 0 0 1e+99 0 … | "Noto Sans CJK SC" Regular
Best score 0 0 0 0 0 0 0 0 0 1e+99 0 0 0 69 …
	family: "Noto Sans CJK KR"(w)
```

Slot by slot:

* slot 9 (strong family): nobody has a font named `sans-serif`, so 1e99 for
  all of them;
* slot 10 (generic): JP, KR and SC are all classified sans, so 0;
* slot 12 (lang): every Noto CJK face covers the `ja` orthography, so 0 for
  all. The code-page exclusion does not fire, because these faces set JIS,
  GB2312, KSC and Johab together;
* **slot 13 (weak family) decides.** `Noto Sans CJK KR` is the 69th name in
  the expanded `sans-serif` list (`conf.d/65-nonlatin.conf:130`, tagged
  `<!-- hangul (ko) -->`, added by "Update default Korean fonts", 84ae86bc).
  No JP, SC, TC or HK Noto face is in the list, so they score 1e99.

The KR line in the alias list has no language condition, so it applies to
every request. The same thing happens for `ko`, `zh-cn`, `zh-tw` and `zh-hk`,
and for `serif`/`monospace` via lines 50 and 173. With no config at all, the
built library returns **JP** (font-set order breaks the tie). The KR result
comes from the config, not from the data.

---

## 2. Which OpenType data fontconfig reads

Two scanners write the same pattern objects: FreeType (`src/fcfreetype.c`,
default) and Fontations (`fc-fontations/*.rs`, behind `-Dfontations`,
reached via `src/fcfontations.c`). Another agent is extending the scan. This
section only documents what is there now.

| OpenType data | read? | where | pattern object | used in matching? |
|---|---|---|---|---|
| name ID 21, 16, 1 (WWS, typographic, legacy family) | yes, **all** become family values, in that order | `fcfreetype.c:1062-1073`, loop `:1395-1545`; `names.rs:70` | `family`, `familylang` | yes, slots 9/13. A face matches under any of its names |
| name ID 22, 17, 2 (subfamilies) | yes | same | `style`, `stylelang` | only if the request has `style=` (slot 19) |
| name ID 4 / Mac full name | yes | same | `fullname` | no |
| name record **language ID** | yes, as `familylang` etc. | `FcSfntNameLanguage`; dedup `fcfreetype.c:1523` | `familylang` | only for *display* name choice in `FcFontRenderPrepare`. **A localized name with the same string as the English one is dropped, language and all** |
| OS/2 usWeightClass | yes | `fcfreetype.c:1764-1769` → `FcWeightFromOpenTypeDouble` (`fcweight.c:28-75`); `attributes.rs:96` | `weight` (fontconfig scale) | slot 21 |
| OS/2 usWidthClass | yes | `fcfreetype.c:1771-1785`; `attributes.rs:108` | `width` (percent) | slot 22 |
| OS/2 usLower/UpperOpticalPointSize | yes (v5) | `fcfreetype.c:1802-1822` | `size` range | slot 17 |
| OS/2 ulCodePageRange1/2 | only the 4 CJK bits, as an exclusion | `fclang.c:1029`; `lang.rs:31-52` | narrows `lang` | indirectly (slot 12) |
| OS/2 fsSelection | **not directly** in C. Italic and bold come from the style *name* first (`FcContainsSlant`/`FcContainsWeight`, `fcfreetype.c:1897-1918`), then FreeType's `style_flags` (`:1923-1937`). Fontations uses skrifa's `Attributes` (`attributes.rs:144-200`) | | `slant`, `weight` | slot 20/21 |
| fsSelection bit 8 WWS, bit 9 OBLIQUE | no | | | |
| OS/2 PANOSE, sFamilyClass | **no** | | | |
| OS/2 ulUnicodeRange | no | | | |
| fvar axes + named instances | yes (wght, wdth, opsz, ital/slnt) | `fcfreetype.c:1242-1340`; `instance_enumerate.rs` | `variable`, `namedinstance`, ranges on `weight`/`width`/`size` | slots 3/4 and 21/22 |
| **STAT** | **no** | | | |
| **meta dlng / slng** | **no** | | | |
| GSUB/GPOS ScriptList | script tags only | `FcFontCapabilities`, `fcfreetype.c:2912-2960`; `capabilities.rs:52-63` | `capability` = `"otlayout:latn otlayout:kana …"` | **no** (`capability` has no comparator, `fcobjs.h:60`) |
| GSUB/GPOS LangSys records | **no** | | | |
| cmap | yes | charset + `FcLangSetFromCharSet` | `charset`, `lang` | slots 8/12 |
| head fontRevision | yes | `fcfreetype.c:1751` | `fontversion` | last tie-break |
| (curated name list) | n/a | `fc-genericfamily/*.txt` → `FcGenericAliasGetClassification`, `fcfreetype.c:2088-2137` | `genericfamily` | slot 10 |

The installed fonts on this machine (`fontTools` survey, 805 sfnt faces):

* `STAT`: 5 faces.
* `meta`: 21 faces, all Noto: Nastaliq Urdu `dlng=ur-Arab,…`, Bengali, Toto,
  Duployan, Dives Akuru, Sunuwar.
* fsSelection WWS bit: 209.
* name ID 16: 384. Name ID 21: 4.
* PANOSE bProportion = 9 (monospaced): 40.
* usWidthClass ≠ 5: only DejaVu * Condensed (4), Nimbus Sans Narrow (4) and
  two Noto Sans Sinhala Condensed faces (2).

The Noto Sans CJK 2.004 TTC faces (JP/KR/SC/TC/HK) carry:

* no `meta` and no `STAT`;
* the same GSUB language systems (`JAN KOR ZHH ZHS ZHT`) in every face;
* the same code-page bits, except that TC/HK set Big5 where the others set
  GB2312.

**What does differ is the language ID of the family name record:** 0x0411
(Japanese) on JP, 0x0412 (Korean) on KR, and so on. That record is the one
fontconfig drops at `fcfreetype.c:1523`, because its string equals the
English one.

---

## 3. Where it goes wrong

Each case is in `test/typical/requests.txt`, with its current result in
`test/typical/baseline.txt`. Run with `FONTCONFIG_PATH=/etc/fonts FC_LANG=en`.

### 3.1 CJK: every CJK language gets the Korean face

```
sans-serif:lang=ja     -> Noto Sans CJK KR      (want JP)
sans-serif:lang=zh-cn  -> Noto Sans CJK KR      (want SC)
sans-serif:lang=zh-tw  -> Noto Sans CJK KR      (want TC)
sans-serif:lang=zh-hk  -> Noto Sans CJK KR      (want HK)
serif:lang=ja          -> Noto Serif CJK KR
monospace:lang=ja      -> Noto Sans Mono CJK KR
:lang=ja               -> Noto Sans CJK KR
fc-match -s sans-serif:lang=ja   -> KR only. JP is trimmed out of the fallback list
```

**Root cause.** It takes three things together.

1. Language means orthographic coverage (`fclang.c:96`), so all regional
   faces tie at slot 12.
2. The tie goes to the weak family slot 13, where `65-nonlatin.conf:130`
   (and `:50`, `:173`, `:229`) lists only the KR family, with no language
   condition.
3. The sort trim (`fcmatch.c:1176`) removes the siblings from the fallback
   list, so even per-character fallback cannot recover.

The design-language data that could settle it (`meta` dlng) is not read. The
installed Noto CJK does not carry it anyway.

### 3.2 Weight: a semibold request served by Bold, and other weight drift

```
Cantarell:weight=semibold                -> Cantarell "Bold" named instance (w=200)
Cantarell:weight=semibold:variable=true  -> Cantarell variable, wght=600     (what was asked)
Adwaita Sans:weight=150                  -> "SemiBold" named instance (180), not 150
Noto Sans:weight=extralight              -> Noto Sans Light   (CSS picks Thin)
sans-serif:weight=extralight             -> Noto Sans Light   (CSS picks Thin)
```

**Root causes.**

* *Variable fonts.* `FcBoolDefaults` adds `variable=False`
  (`fcdefault.c:43`), and VARIABLE is slot 3, far ahead of WEIGHT (slot 21).
  The variable face covers weight 600 exactly, but it loses to *any* named
  instance before weight is ever compared. Cantarell has no SemiBold
  instance, so semibold becomes Bold.
* *Distance in the wrong units and with no direction.* `FcCompareRange`
  (`fcmatch.c:209-260`) takes `|a−b|` on fontconfig's weight scale. That
  scale is non-linear against OpenType (`fcweight.c:28-41`: 500→100,
  600→180, 700→200, 800→205, 900→210). ExtraLight (fc 40) is 10 from Light
  (50) and 40 from Thin (0), so Light wins. CSS Fonts 4 §5.2 step 4.3 checks
  *lighter* weights first for a desired weight under 400, so it picks Thin.
  Above 500 the compressed top of the scale turns Bold/ExtraBold/Black into
  near-ties.

*Bold from another family.* An exhaustive run on this machine (277
families × light/semibold/bold/black, request `Family:weight=X`) found **no
direct request that leaves its family**. The only exceptions were icon fonts
that config rejects. Family (slot 9 or 13) always precedes weight, which is
what CSS requires too (§5 step 1, §5.2 step 4 is per family). The ways a
weight request *does* end up in another family are:

* **Families split by name.** A face without a shared name ID 16/21 is a
  different family to fontconfig, since family identity is just
  "any of IDs 21/16/1". Nimbus Sans Narrow is the case installed here (see
  3.3). A bold face named only "Foo Bold" (ID 1) would likewise never serve
  `Foo:bold`. 90-synthetic.conf would embolden Foo Regular instead.
* **Fallback.** In `FcFontSort`, characters the family lacks come from
  whichever family comes next in alias order. Within that family the weight
  is matched, so the run stays weight-coherent but mixes families. That is
  expected under CSS cluster matching (§5.3).

The user's recollection most likely comes from the variable-font case
(semibold → Bold), from a split family, or from a renderer's per-character
fallback. The harness carries all three, so a regression in any of them
shows up.

### 3.3 Condensed requests

```
DejaVu Sans:width=condensed               -> DejaVu Sans Condensed        ok
DejaVu Sans:width=condensed:weight=light  -> DejaVu Sans ExtraLight (normal width)
                                             CSS: DejaVu Sans Condensed (Book)
Noto Sans Sinhala:width=condensed         -> Regular (normal width)
                                             CSS: a condensed face (Thin Condensed)
Helvetica:width=condensed / Nimbus Sans:width=condensed
                                          -> Nimbus Sans Regular (normal width)
sans-serif:width=condensed                -> Noto Sans Regular (normal width)
```

**Root causes.**

* *Order.* Width is slot 22, after slant (20) and weight (21). CSS Fonts 4
  §5.2 step 4 narrows the family's faces by **width first** ("font-width is
  tried first"), then style, then weight. Whenever a family's condensed
  faces don't include the requested weight, fontconfig keeps the weight and
  drops the width. CSS does the opposite.
* *Split families.* Nimbus Sans Narrow has no name ID 16 tying it to Nimbus
  Sans, so it is a separate family. By the font's own data that is
  "correct". CSS would not cross families either. Config already handles it
  with an explicit alias (`Arial Narrow`/`Helvetica Narrow` → Nimbus Sans
  Narrow).
* `sans-serif:width=condensed` staying on Noto Sans (which has no condensed
  face) is CSS-conformant. CSS picks the family first and then synthesizes
  nothing for width. Not a bug.
* DejaVu "Condensed" sets usWidthClass 4 (semi-condensed, 87.5 %).
  fontconfig reports 87, which is faithful to the data.

### 3.4 Absent families landing on something unlike them

```
Impact   -> FontAwesome      (an icon font with no Latin coverage at all)
Menlo    -> Noto Sans        (Menlo is monospace)
Monaco   -> Noto Sans        (monospace)
SF Mono  -> Noto Sans Mono   ok, but only because the name contains "mono"
Helvetica Neue, Futura, Roboto, Inter, xyzzy -> Noto Sans   (fine)
Helvetica -> Nimbus Sans, Arial -> Liberation Sans, Courier New -> Liberation Mono  (fine: metric aliases)
```

**Root causes.**

* *Impact → FontAwesome.* `45-latin.conf:262-265` gives Impact the default
  `fantasy`, so the request carries `genericfamily=fantasy`. The curated
  list `fc-genericfamily/fantasy.txt:22-24` classifies FontAwesome (and
  Bravura, Leland, and other icon/music fonts) as fantasy.
  GENERIC_FAMILY is slot 10 and LANG slot 12, so a font with an **empty
  langset** (`lang=[]`, it cannot render a single word of English) beats
  every font that can. The best score shows it: lang = 2000 for the winner.
* *Menlo/Monaco → proportional.* The same curated table already knows they
  are monospace (`fc-genericfamily/monospace.txt:102,104`). But
  `FcGenericAliasGetClassification` is applied only to *installed fonts* at
  scan time (`fcfreetype.c:2103`), never to the *request*. The request gets
  a generic only from config aliases or the "mono"/"sans" substring rules
  in `48-guessfamily.conf`, so it falls through to `49-sansserif.conf`'s
  catch-all `sans-serif`.

### 3.5 Smaller observations

* `sans-serif:lang=ur` → Noto Kufi Arabic and `serif:lang=ur` → Noto Naskh
  Arabic, while `:lang=ur` → Noto Nastaliq Urdu. Nastaliq is the only one
  with `meta dlng=ur-Arab`. The generic slot decides before any notion of
  design language could. This is arguably right for an explicit generic.
  It is listed so a `meta` patch can be checked against it.
* `monospace:slant=italic` → Noto Sans Mono Regular plus a synthetic
  oblique matrix (90-synthetic.conf). This matches CSS (family first, then
  synthesize). Not a bug.
* With the build's default `--prefix=/usr/local`, the relative
  `<include>conf.d</include>` in `/etc/fonts/fonts.conf` resolves against
  `/usr/local/etc/fonts` and silently loads nothing. The harness sets
  `FONTCONFIG_PATH=/etc/fonts` for that reason. Without it, the built
  library answers `sans-serif:lang=ja` with JP. That is the clearest
  evidence that the KR result comes from config.

---

## 4. Smallest fixes, ranked by size and risk

All of these keep the config language and the public API/ABI as they are. Size is the
expected diff. Risk is the chance of changing results nobody asked to change.

### P1. Language-conditioned CJK preferences in config. Size: ~40 lines XML. Risk: low.

Replace the unconditional `Noto {Sans,Serif,Sans Mono} CJK KR` entries in
`conf.d/65-nonlatin.conf` (lines 50, 130, 173, 229) with `<alias>` blocks
that test `lang`:

* `ja` → JP;
* `ko` → KR;
* `zh-cn`/`zh-sg` → SC;
* `zh-tw` → TC;
* `zh-hk`/`zh-mo` → HK;
* for no language: keep one neutral default.

`fc-genconf` already emits exactly this shape (`src/fcconffile.c:69-78`).
Prototype run with two such rules: `sans-serif:lang=ja` → JP,
`sans-serif:lang=zh-cn` → SC, `DejaVu Sans:lang=ja` unchanged (strong family
still wins). Each generic needs its own block, because the generic slot
(10) gates a sans rule out of `serif:lang=ja`.
*Spec:* CSS Fonts 4 §2.1.5 (a generic family "may be a composite face
combining different typefaces based on … the content language") and §5.2
step 2 ("User agents may choose the generic font family to use based on the
language of the containing element"). Picking a font per language for a
generic is the UA's job, so config is the right layer. Nothing in these
fonts' OpenType data could make the choice.

### P2. Don't let a font with an empty langset win on genericfamily. Size: ~3 lines C. Risk: low.

In the genericfamily block of the scanner (`fcfreetype.c:2088-2137`, and its
Fontations twin in `names.rs:170`), skip the curated classification when the
face's langset is empty. Icon and music fonts then become
`FC_FAMILY_UNKNOWN`. The alternative is to drop FontAwesome/Bravura/Leland
from `fantasy.txt`, which is data only, but leaves the hole open for the next
icon font. Fixes Impact → FontAwesome.
*Spec:* CSS Fonts 4 §5.3 (cluster matching): a face is only used for
characters it supports. A face that supports no language's text cannot stand
in for a text family.

### P3. Classify the request's family with the curated table. Size: ~15 lines C. Risk: low.

When a pattern reaches `FcConfigSubstitute(FcMatchPattern)` with no
`genericfamily`, look up its first family name with
`FcGenericAliasGetClassification` (`src/fcgenericalias.c:25`) and add the
class weakly. The existing `49-sansserif.conf` rules then append `monospace`
or `serif` as they already do for config-classified names. An installed
Menlo still wins at slot 9 before slot 10 is read. Fixes Menlo/Monaco →
monospace.
*Spec:* CSS Fonts 4 §2.1.5 ("reasonable default choices for the generic
font families"). This is not OpenType data. It reuses data the library
already ships, symmetrically with how it classifies fonts.

### P4. CSS-ordered, directional weight and width matching. Size: ~60 lines C in fcmatch.c. Risk: medium.

Give WEIGHT and WIDTH their own comparators in place of `FcCompareRange`:

* *weight:* convert both sides to OpenType units (`FcWeightToOpenType`) and
  produce a key that follows CSS Fonts 4 §5.2 step 4.3. Below 400: lighter
  first, descending, then heavier. 400–500: up to 500, then lighter, then
  heavier. Above 500: heavier first, then lighter. A range that contains
  the target scores 0.
* *width:* the same with step 4.1 (≤100 %: narrower first, else wider
  first).
* optionally move WIDTH ahead of SLANT/WEIGHT in `FcMatcherPriority`
  (`fcmatch.c:379-382`), following "font-width is tried first", then
  font-style, then font-weight. This is the change that fixes
  `DejaVu Sans:width=condensed:weight=light`. The direction-only part fixes
  `extralight` → Thin.

The values stay the same type, so there is no ABI change. It does visibly
change some weight picks, which is why it sits at medium risk. The harness
shows exactly which ones.
*Spec:* CSS Fonts 4 §5.2 steps 4.1–4.3. OpenType OS/2 usWeightClass and
usWidthClass (the scale the comparison should run in; usWidthClass 1–9 =
50–200 %).

### P5. Variable fonts: let the axis win over a mismatching named instance. Size: ~20 lines C. Risk: medium-high.

Today `variable=False` (`fcdefault.c:43`, slot 3) hides the variable face
behind every named instance. Smallest form of the fix: in `FcCompare`, when
the request did not set `variable` explicitly and the variable face's range
contains the requested weight/width exactly while the best named instance
does not, don't count the slot-3 mismatch. This needs a flag for "default,
not user-set", for example a weak binding on the default, and a check of
that binding in `FcCompareValueList`. Renderers that ignore
`fontvariations` would draw the default instance, so this must stay
conservative.
*Spec:* CSS Fonts 4 §2.2.2 ("a user agent using a font which supports a
range of weights should behave the same as if a font is present at each
individual weight in the range") and §5.2 step 4. OpenType `fvar` and STAT
(named instances are a UI convenience, not the only faces).

### P6. Read `meta` dlng/slng and rank by design language. Size: ~80 lines C + Rust + one new object. Risk: medium.

Scan `meta` (`dlng`, `slng`) in both scanners and store `dlng` as a new
langset object (appended to `fcobjs.h`, which adds API and does not break
it). Compare it with `FcCompareLang` in a new slot right **after** LANG:

* a face whose dlng covers the requested language beats one that merely
  covers the orthography;
* faces without `meta` score as today. A missing object is neutral
  (`fcmatch.c:676-703`).

Optionally let `slng` narrow `lang` where present, the same way the
code-page exclusion does today. This is the principled answer to "use the
font's own data", but it does nothing for the Noto CJK installed here (no
`meta`). Only 21 of 805 faces here have it. Coordinate with the scan work
before taking it on.
*Spec:* OpenType `meta`: dlng "Indicates languages and/or scripts for the
user audiences that the font was primarily designed for"; slng "…that the
font is declared to be capable of supporting".

### P7 (not recommended alone). Name-record language as a CJK hint. Size: ~30 lines C. Risk: high.

Keep the language of a duplicate-string name record (`fcfreetype.c:1523`).
When the family names carry exactly one CJK language, feed that into the
existing code-page exclusion. This is the only OpenType signal that tells
the installed Noto CJK faces apart. But the `name` spec defines the
language ID as "the language in which a particular string is written", not
the font's audience. So this is a convention (Adobe/Google/Microsoft CJK
fonts follow it), not a rule. Use P1 for CJK, and P6 when fonts ship
`meta`.

**Suggested order:** P1 (fixes the complaint), P2 + P3 (small, clearly
right), then P4 behind the harness. P5 and P6 are design work.

---

## 5. Test harness

```
test/typical/requests.txt   the requests, tagged by case
test/typical/run.sh         runs them; pins FONTCONFIG_FILE/PATH, FC_LANG=en, LC_ALL
test/typical/baseline.txt   results of the unpatched build on this machine
                            (system fc-match 2.18.4 gives identical results)
```

```
meson setup $TMPDIR/typ-build ~/Projects/typical && ninja -C $TMPDIR/typ-build
~/Projects/typical/test/typical/run.sh -o $TMPDIR/after.txt
diff -u ~/Projects/typical/test/typical/baseline.txt $TMPDIR/after.txt
~/Projects/typical/test/typical/run.sh -s        # same requests via the system fc-match
```

The baseline's first line records the face count and a hash of the font
set, so a diff against a different font install shows up as such instead of
as a matching change.
