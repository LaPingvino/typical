# Copyright (C) 2026 fontconfig Authors
# SPDX-License-Identifier: HPND

# fc-match --explain: the config trace and the per-slot scores, on a CJK
# request where a config alias prefers one face and the design language
# (meta dlng) of another wins at the lang slot.

from fctest import FcTest
from pathlib import Path
import pytest


@pytest.fixture
def fctest():
    return FcTest()


def test_explain_cjk(fctest):
    srcdir = Path(fctest.srcdir) / 'test'
    fonts = {
        'no_family_name.ttf': ('Test CJK KR', '<string>ja</string><string>ko</string>', None),
        'no_family_name_serif.ttf': ('Test CJK JP', '<string>ja</string><string>ko</string>', 'ja-Jpan'),
        'opentype-langs.ttf': ('Test Latin', '<string>en</string>', None),
    }
    rules = []
    for file, (family, langs, dlng) in fonts.items():
        dl = (f'<edit name="designlang" mode="assign_replace"><string>{dlng}</string></edit>'
              if dlng else '<edit name="designlang" mode="delete_all"><string>x</string></edit>')
        rules.append(f'''
          <match target="scan">
            <test name="file" compare="contains"><string>/{file}</string></test>
            <edit name="family" mode="assign_replace"><string>{family}</string></edit>
            <edit name="lang" mode="assign_replace"><langset>{langs}</langset></edit>
            {dl}
          </match>''')
    # Config prefers the KR face for sans-serif, as upstream's
    # 65-nonlatin.conf does for every CJK language.
    rules.append('''
          <alias>
            <family>sans-serif</family>
            <prefer><family>Test CJK KR</family></prefer>
          </alias>''')
    fctest._extra += rules
    fctest.env['FC_LANG'] = 'en'
    fctest.env['LC_ALL'] = 'C'
    fctest.setup()
    fctest.install_font([srcdir / f for f in fonts], '.')

    for ret, stdout, stderr in fctest.run_match(['--explain', ':family=sans-serif:lang=ja']):
        assert ret == 0, stderr
        out = stdout
    lines = out.splitlines()
    print(out)

    # The config trace names the file and rule, and what it added.
    i = lines.index('== Config substitution: each change to the request, by file and rule ==')
    trace = lines[i:lines.index('== Request after config and default substitution ==')]
    rule = [n for n, l in enumerate(trace) if l.endswith('.conf: when family Equal(ignore blanks) "sans-serif"')]
    assert rule, trace
    assert trace[rule[0] + 1].strip() == 'family: +"Test CJK KR"(w)', trace
    assert any(l.strip() == 'lang: +"en"(w)' for l in trace), trace
    assert any(l.strip().startswith('weight: +80') for l in trace), trace

    # The winner, and the slot where each runner-up lost, with both values.
    cands = lines[lines.index('== Candidates: the winner and the best face of each of the next 2 families =='):]
    assert '"Test CJK JP"' in cands[1] and cands[1].endswith('[winner]'), cands
    assert '"Test CJK KR"' in cands[2], cands
    assert cands[3].strip() == ('lost at lang: both cover ja; designlang: winner designed for ja, '
                                'this face has no design-language data (score 0 vs 30)'), cands
    assert '"Test Latin"' in cands[4], cands
    assert cands[5].strip() == ('lost at lang: winner covers ja; this face covers en '
                                '(requested language #2) (score 0 vs 130)'), cands

    # The per-slot table marks the lang slot for both, and shows that the
    # KR face was ahead on the weak family slot, which comes later.
    lang = [l.split() for l in lines if l.startswith('lang ')]
    assert lang == [['lang', '0', '30*', '130*']], lang
    fam = [l.split() for l in lines if l.startswith('family (weak)')]
    assert fam == [['family', '(weak)', '1e+99', '0', '1e+99']], fam

    assert lines[-1].endswith('"Test CJK JP" "Regular"'), lines[-1]
