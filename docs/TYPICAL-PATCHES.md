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
