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
