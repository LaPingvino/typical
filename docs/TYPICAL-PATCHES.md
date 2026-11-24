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

The Light → Thin moves for families with only Thin and Regular are what
CSS specifies. They are listed so they can be judged, not hidden.

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
