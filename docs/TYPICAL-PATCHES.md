# typical patches: what each one moves

One section per patch, in commit order. Each lists every result that moved in
the harness (`test/typical/run.sh`, diffed against the baseline committed just
before the patch), with the reason it was meant to move. The baseline is updated
in a separate commit after each patch. Test gate for each patch: `meson test`
(31 OK, 1 skip). The meson `pytest` target cannot run here because the
pytest-tap plugin is missing, so pytest runs directly with meson's environment
instead (68 passed, 9 skipped). Background: docs/TYPICAL-MATCHING.md.

## P1: per-language Noto CJK preferences (conf.d/65-nonlatin.conf)

Size: +149 lines of XML (28 generated `<alias>` blocks plus a comment), no C.

The patch adds 28 `<alias>` blocks, one for each pair of generic
(serif, sans-serif, monospace, system-ui) and language (ja, ko, zh-cn,
zh-sg, zh-tw, zh-hk, zh-mo). Each block prefers the matching regional Noto
CJK face when the request's `lang` contains that language. The blocks sit
before the existing generic lists, because an earlier alias puts its family
earlier in the request. The unconditional KR entries stay as the default
for requests without a CJK language.
Spec: CSS Fonts 4 §2.1.5 and §5.2 step 2 (the UA may choose the font for a
generic family by the content language).

Moved results (all intended: each CJK request now gets its own region's face):

| request | before | after | why |
|---|---|---|---|
| sans-serif:lang=ja | Noto Sans CJK KR | Noto Sans CJK JP | ja → JP |
| sans-serif:lang=zh-cn | Noto Sans CJK KR | Noto Sans CJK SC | zh-cn → SC |
| sans-serif:lang=zh-tw | Noto Sans CJK KR | Noto Sans CJK TC | zh-tw → TC |
| sans-serif:lang=zh-hk | Noto Sans CJK KR | Noto Sans CJK HK | zh-hk → HK |
| serif:lang=ja | Noto Serif CJK KR | Noto Serif CJK JP | ja → JP, serif list |
| monospace:lang=ja | Noto Sans Mono CJK KR | Noto Sans Mono CJK JP | ja → JP, monospace list |
| :lang=ja | Noto Sans CJK KR | Noto Sans CJK JP | no family, so the default sans-serif → JP |
| sans-serif:lang=ja:weight=bold | Noto Sans CJK KR Bold | Noto Sans CJK JP Bold | ja → JP; weight unchanged |
| sort sans-serif:lang=ja (1st) | Noto Sans CJK KR | Noto Sans CJK JP | Japanese fallback now reaches JP |
| sort sans-serif:lang=zh-cn (1st) | Noto Sans CJK KR | Noto Sans CJK SC | same for zh-cn |

Unchanged as intended: `sans-serif:lang=ko` (KR) and
`sans-serif:lang=ko:weight=bold` (KR Bold). No non-CJK request moved.

## P2: remove icon and music-notation fonts from the fantasy list (fc-genericfamily/fantasy.txt)

Size: −12 lines of data, no C.

The patch removes Bravura, CaladingsCLM, FontAwesome, FontAwesome 7 Brands,
FontAwesome 7 Free, Foundation Icons, Leland, Material Icons, Petaluma,
entypo, mnmlicons and Finale Broadway. These are pictograph, dingbat or
SMuFL music-notation fonts, not fantasy *text* faces. Their text companions
(PetalumaScript, Finale Broadway Text) stay. Faces from the removed families
are now classified FC_FAMILY_UNKNOWN. Because GENERIC_FAMILY (slot 10)
ranks above LANG (slot 12), a font classified "fantasy" with no Latin glyphs
used to beat every text font for any fantasy request.
Spec: CSS Fonts 4 §5: a face is used only for characters it can render, so
a pictograph font is never a stand-in for a text family.

The other way to fix this was to give no generic class to faces with an
empty langset. It was rejected because 85 installed faces with an empty
langset are real text fonts for scripts that have no orthography file.

Moved results:

| request | before | after | why |
|---|---|---|---|
| Impact | FontAwesome | Noto Sans | No installed fantasy face covers Latin, so the request falls through to the sans-serif default. That is the intended outcome: a text face instead of icons. |

## P3: classify the requested family with the curated table (src/fccfg.c)

Size: +21 lines of C in `FcConfigSubstituteWithPat`, plus 3 cases in
`test/test-49-sansserif.json`. Without the patch, the Menlo and Georgia
cases fail.

When a pattern-kind substitution starts and the request has no
`genericfamily`, the patch classifies its family list with
`FcGenericFamilyGetValues`, the same curated data the scanner applies to
fonts. It stops at the first family it can classify and adds the class as
a weak value. Config rules still run afterwards. An unknown name adds
nothing. `49-sansserif.conf` then appends the matching generic name, as it
already does for classes set by config. An installed family with that name
still wins at slot 9 (strong family) before slot 10 (generic) is read.
Spec: CSS Fonts 4 §2.1.5 (reasonable default choices for generic families).
The data is the library's own curated list.

Moved results:

| request | before | after | why |
|---|---|---|---|
| Menlo | Noto Sans | Noto Sans Mono | Menlo is monospace in `monospace.txt`. Before, the request had no class and fell through to sans-serif. |
| Monaco | Noto Sans | Noto Sans Mono | same |
| Comic Sans MS | Noto Sans | Z003 Medium Italic | The config (`45-latin.conf:270`) and the curated list (`cursive.txt`) both say cursive. Before, the "sans" substring rule in `48-guessfamily.conf` set sans-serif first and hid the config's own class. Z003 is the only cursive face installed. |

**Judgement call for Joop: Comic Sans MS → Z003.** Kept for now.
* The config and the curated list both call Comic Sans cursive.
* The font's own data agrees: Comic Sans MS carries PANOSE family kind 3
  ("Latin Hand Written"), not 2 ("Latin Text"). Comic Sans is not installed
  here, so that value comes from the font as shipped by Microsoft, not from
  a scan on this machine.
* The visible result is still a calligraphic Chancery italic, because Z003
  is the only cursive face installed.
* Reverting would mean letting the "sans" substring guess win over both
  sources.

Unchanged as intended: Futura, Helvetica Neue, Roboto and Inter stay on
Noto Sans, and Georgia, Garamond and Cambria stay on Noto Serif (same
class as before). Consolas and SF Mono were already monospace.

## P4: directional weight and width matching in OpenType units, width first (src/fcmatch.c, src/fcobjs.h)

Size: +115 lines in `fcmatch.c` (two comparators, a helper, and one
priority-order move) and 2 changed lines in `fcobjs.h`. Also a new test,
`test/test-css-weight-width.json`, with 4 cases. The old library fails 3 of
them.

What changes:

* `FcCompareWeight` and `FcCompareWidth` replace `FcCompareRange` for
  WEIGHT and WIDTH. A face whose value or range contains the request still
  scores 0, and the value passed on (for variable fonts) is computed
  exactly as before. Only the ordering of misses changes. The new order
  follows CSS Fonts 4 §5.2 step 4.3 for weight:
  * below 400: lighter first, descending, then heavier;
  * 400–500: heavier up to 500, then lighter, then heavier above 500;
  * above 500: heavier first, then lighter.
  Width follows step 4.1: at or below 100 %, narrower first, otherwise
  wider first.
* Weight is compared in OpenType units (`FcWeightToOpenTypeDouble`), not
  on fontconfig's non-linear scale. Which rule applies is decided by the
  nearest OpenType weight class (a multiple of 100, ties to the lighter
  one). That way fontconfig's in-between constants keep their meaning:
  `book` (380) searches like regular, and `demilight` (350) searches like
  light.
  * Without this rounding, `DejaVu Sans:weight=book` went to ExtraLight,
    even though DejaVu's regular face is *named* Book, and 83 `book`
    requests moved. With it, none move.
* In the priority order, WIDTH moves ahead of SLANT and WEIGHT. CSS
  §5.2 step 4: "font-width is tried first", then font-style, then
  font-weight. STYLE (an explicit style string) stays first.

Moved results in the harness:

| request | before | after | why |
|---|---|---|---|
| Noto Sans:weight=extralight | Light (300) | Thin (100) | CSS: below 400, lighter first. |
| sans-serif:weight=extralight | Noto Sans Light | Noto Sans Thin | same |
| DejaVu Sans:width=condensed:weight=light | ExtraLight, normal width | Condensed (Book), 87 % | Width is tried before weight. |
| Noto Sans Sinhala:width=condensed | Regular, normal width | Thin Condensed, 63 % | Width first. Among the condensed faces, 400 has nothing at 400–500, so lighter comes next: Thin. |

Wider check: all 277 installed families × {thin, extralight, light,
demilight, book, regular, medium, semibold, bold, extrabold, black,
condensed, semiexpanded} (3,601 requests), old library against new.
82 moved, all of the kinds above:

| requests | move | rule |
|---|---|---|
| 68 × `weight=extralight` | Light → Thin | below 400, lighter first |
| 4 × `weight=extralight` | Regular → Thin | the family has only Thin, Regular and heavier, so lighter first |
| 4 × `weight=light` (Noto Sans Oriya, Syriac ×3) | Regular → Thin | same rule; these families have no Light |
| 4 × `weight=demilight` (same families) | Regular → Thin | demilight searches like light |
| 1 × `SourceCodeVF:weight=extrabold` | Bold → Black | above 500, heavier first |
| 1 × `Noto Sans Sinhala:width=condensed` | Regular → Thin Condensed | width first (as in the harness) |

**Judgement call for Joop: Light → Thin when a family has no Light.**
Kept for now.
* Affected: `weight=light` and `weight=demilight` in Noto Sans Oriya and
  the three Noto Sans Syriac families. They have Thin, Regular and heavier
  faces, but no Light.
* CSS Fonts 4 §5.2 step 4.3 searches lighter weights first below 400, so a
  300 request takes Thin (100) over Regular (400), although 400 is
  numerically nearer.
* Browsers do the same.
* Reverting means departing from the CSS rule for this one direction,
  e.g. by preferring the nearest weight when the lighter candidate is
  further away.

## P4b: pick the family first when no requested family matched (src/fcmatch.c)

Size: +62/−4 lines in `FcFontSetMatchInternal`. Also a new test,
`test/test-family-first.json`, with 3 cases. Without the patch, the bold
case fails.

While scoring, the matcher now also keeps a second winner, computed with
the style slots (STYLE, WIDTH, SLANT, WEIGHT) set to zero. That winner is
the face whose *family* would win if style did not count. The second pass
runs only when the overall winner differs from it and the request named no
family the winner has (no family element, or both family scores
"no match"). It then picks the best face, by the full score, among the
faces whose first family name equals that family. When a requested family
matched, nothing changes. FcFontSort is not changed (see below).
Spec: CSS Fonts 4 §5 step 1 (start from a family) and §5.2 step 4 (width,
style and weight are matched among *that family's* faces).

Moved results in the harness:

| request | before | after | why |
|---|---|---|---|
| sans-serif:lang=nqo:weight=bold | Noto Sans NKo Unjoined Bold | Noto Sans NKo Regular (emboldened) | Same family as the regular request. 90-synthetic.conf emboldens it. |
| :lang=ban:weight=bold | Noto Sans Balinese Bold | Noto Serif Balinese Regular (emboldened) | same, and stays serif |
| serif:lang=he:weight=black | Noto Serif Hebrew Black | Liberation Serif Bold | same family as the regular request; heaviest face of that family |
| :lang=ur:weight=light | Noto Kufi Arabic Light | Noto Nastaliq Urdu Regular | same family as the regular request (Nastaliq has no Light) |
| :lang=ks:weight=light | Noto Kufi Arabic Light | Noto Nastaliq Urdu Regular | same |

Wider check: 338 languages × {none, sans-serif, serif, monospace} ×
{regular, light, semibold, bold, black} (6,760 requests), P4 library against
P4b. 25 moved. Every one of them is a request that P4 sent to a different
family than the regular-weight request (ban, he, yi, ks, lah, nqo, pa-pk,
ps-af, ps-pk, ur), and every one now stays in the regular request's family.
After the patch, no request in the sweep changes family with weight.

Not done: FcFontSort, the fallback list. Its tail still orders families
that config does not list by style first. The sort would need a family-rank
key ahead of the style slots, which is a bigger change. Left for later.

## P5: not done (variable fonts and the `variable=False` default)

This was not attempted, because it would not stay clean.

`FcDefaultSubstitute` adds `variable=False` to every request
(`fcdefault.c:43`). That is the contract that keeps variable faces away
from clients that cannot apply `fontvariations`. Letting the variable
Cantarell face win `weight=semibold` (it covers 600 exactly, but today
loses to the Bold named instance) would need two things:

* telling a defaulted `variable` apart from one the client set, which means
  changing the binding of a default the API has always added as strong;
* trusting every client to honour `fontvariations`.

A client that ignores `fontvariations` would draw the variable face's
default instance. For Cantarell that is Regular, which is worse than Bold.
The harness keeps `Cantarell:weight=semibold` (Bold today) and the
`variable=true` variant (wght=600) as the before/after for anyone who
designs this properly. A per-client opt-in (the client sets `variable=true`
or DontCare) already gives the exact weight today.

## Rebased onto typical-scan

The 10 scan commits come first: the `panose`, `designlang`, `supportedlang`
and `locllang` objects, the OpenType-tag table, and the fontations port.
The typical series was rebased on top of them, so upstream sees the scan
side first and the matcher side second. The rebase applied without
conflicts.

* **Harness:** the rebased tip produced a result file identical to the
  P4b baseline, byte for byte. Nothing moved, so no baseline commit was
  needed for the rebase itself.
* **Cache name:** the scan branch bumps the cache snapshot version
  (`.cache-13~snap1`). The harness builds a private cache anyway.
* **Test gate:** it now reads the pytest environment from
  `meson introspect`.
* **Results:** meson 31 OK, pytest 79 passed (the scan tests included).

Then 9 requests were added under `[designlang]` (ug, ar, fa, sd, tr, pl,
ee, se), with a baseline taken *before* P6, so P6's own diff is readable.

## P6: rank by design language (`meta` dlng) inside the LANG slot (src/fcmatch.c)

Size: +185 lines in `fcmatch.c`. Most of it is the tag matcher: subtag
comparison and a 7-entry table of Han/Kana/Hangul script tags. Also a new
test, `test/test-designlang.json`, with 4 cases. Without the patch, the
`ur` and `zh-tw` cases fail.

How it ranks:

* `FcCompare` now adds an offset to the LANG score of every font that
  covers the requested language:
  * 0 when the font's `designlang` names that language;
  * 30 otherwise, including fonts that have no `designlang`.
* The offset stays below 100, the step between two requested languages, so
  it only orders fonts that already tie on coverage. A missing object
  counts as unknown, never as a penalty. TYPICAL-SCAN.md requires that.

How tags are matched:

* A ScriptLangTag matches when the primary language subtags agree and the
  region subtags agree where both have one. If the tag has a script
  subtag, it must be one of the language's scripts (checked only for the
  CJK languages, where the table knows them).
* A script-only tag (`Hans`, `Hant`, `Jpan`, `Kore`, …) matches only
  through that table, so `Hans` serves `zh-cn`/`zh-sg` and `Hant` serves
  `zh-tw`/`zh-hk`/`zh-mo`. Other script-only tags (`Dupl`, `sunu`, `latn`)
  match nothing. Without CLDR's likely-script data, a script-only `latn`
  would otherwise outrank every font for every Latin language.

Spec: OpenType `meta`. `dlng` "Indicates languages and/or scripts for the
user audiences that the font was primarily designed for." That is a
statement about intent, beyond coverage.

**Not used: `slng` and `locl`.** The brief offered them as lower tiers. I
built all three tiers and swept them, and both lower tiers are unclean.
The sweep (338 languages × 4 generics × 5 weights) moved 255 requests with
the three tiers. With dlng alone it moves 5.

* `slng` ("declared to be capable of supporting") sent `:lang=ar` and
  `:lang=fa` to Noto Nastaliq Urdu, which declares `slng ar-Arab, fa-Arab`.
  Nastaliq is not the script style Arabic or Persian readers expect. slng
  restates coverage, and ranking it above fonts without a `meta` table
  amounts to penalizing absence.
* `locl` sent `:lang=tr`, `pl`, `vi`, `kk`, `bg`, `bs` and others to
  Adwaita Mono, a monospace face that carries locl for them. It sent the
  Sami languages to DejaVu, `sd` to Noto Kufi Arabic, and `:lang=ee` to
  Noto Sans *Italic*: the Italic face has a locl system the upright one
  lacks. Whether a font has locl for a language says little about whether
  the font is right for it.

Moved results:

| request | before | after | why |
|---|---|---|---|
| :lang=ug | DejaVu Sans Book | Noto Nastaliq Urdu Regular | Its dlng lists `ug-Arab`; DejaVu has no `meta`. |

The sweep's other 4 moves are the same request at light, semibold, bold
and black. `sans-serif:lang=ug` stays DejaVu Sans, because Nastaliq is not
classified sans-serif and the generic slot decides first.

**Note for Joop:** the font's data drives this. Noto Nastaliq Urdu claims
Uyghur as a design language. Uyghur is usually set in a Naskh-style face,
so if that claim is wrong, it is a data problem in Noto, not a matcher
problem. The harness shows it if Noto changes its `meta`.

## fc-match --explain (src/fcmatch.c, fccfg.c, fcdefault.c, fcdbg.c, fc-match)

No matching change: the harness result file is identical before and after.

`fc-match --explain PATTERN` (`-e`) prints, in this order:

1. **Config substitution.** Every change to the request, with the config
   file and the rule's tests, and the values added (`+`) or removed (`-`).
   Changes made before any rule are shown first: the default languages and
   the curated generic class (P3).
2. **Default substitution.** What `FcDefaultSubstitute` filled in.
3. **The request** as it goes into matching.
4. **Candidates.** The winner, the next face of its own family, and the
   best face of each of the next four families. For each runner-up:
   the slot where it lost, with the two values compared, in words. For
   example: `lang: both cover ja; designlang: winner designed for ja, this
   face has no design-language data (score 0 vs 30)`. When the
   family-first rule (P4b) overrode a better raw score, that is what it
   says instead.
5. **A table of the score per slot**, in priority order. Rows where every
   candidate scores 0 are left out, and `*` marks the slot where each
   candidate lost.

How it works:

* It is a debug bit, `FC_DBG_EXPLAIN` (8192). `fc-match` ORs the bit into
  `FC_DEBUG` before its first library call, so the library prints
  `FC_DEBUG=…` on stderr as it does for every debug bit.
* The scores come from `FcCompare` itself, called once per font after the
  real match. The winner is the one `FcFontSetMatchInternal` chose. No
  second matcher is involved.
* The design-language offset (P6) is read back out of the LANG score. It
  is the remainder below 100.
* It explains `FcFontMatch` only, so it cannot be combined with `-s` or
  `-a`.

Test: `test/test_explain.py`, with three test fonts. Scan rules give them
family names, languages and `designlang`, and an alias prefers the "KR"
face for sans-serif. The test checks the config trace line, the `lang`
loss explained for both runners-up, and the table rows.

Found with it: the harness's requests are written `sans-serif:…`.
`FcNameParse` reads the `-` as the start of a size list, so the request's
strong family is `"sans"`, not `"sans-serif"`. `48-guessfamily.conf` then
adds sans-serif, so the results are the same either way. The family-name
form that keeps the hyphen is `:family=sans-serif`.
