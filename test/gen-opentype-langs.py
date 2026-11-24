#!/usr/bin/env python3
# Copyright (C) 2026 fontconfig Authors
# SPDX-License-Identifier: HPND
#
# Generate test/opentype-langs.ttf, a tiny font carrying the OpenType data
# that test_opentype_langs.py checks: an OS/2 PANOSE, a 'meta' table with
# dlng and slng, and GSUB language systems with and without 'locl'.
#
# Usage: gen-opentype-langs.py test/opentype-langs.ttf   (needs fontTools)

import sys

from fontTools.feaLib.builder import addOpenTypeFeaturesFromString
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import newTable

FEATURES = """
languagesystem DFLT dflt;
languagesystem latn dflt;
languagesystem latn NLD;
languagesystem latn TRK;
languagesystem latn QQQ;
languagesystem hani dflt;
languagesystem hani JAN;
languagesystem hani KOR;
languagesystem hani ZHS;
languagesystem hani ZHT;
languagesystem hani ZHH;
languagesystem kana JAN;

feature locl {
    script latn;
    language NLD exclude_dflt; sub A by B;
    language QQQ exclude_dflt; sub A by B;
    script hani;
    language JAN exclude_dflt; sub A by B;
    language KOR exclude_dflt; sub A by B;
    language ZHS exclude_dflt; sub A by B;
    language ZHT exclude_dflt; sub A by B;
    language ZHH exclude_dflt; sub A by B;
    script kana;
    language JAN exclude_dflt; sub A by B;
} locl;

feature liga {
    sub A A by B;
} liga;
"""


def box():
    pen = TTGlyphPen(None)
    pen.moveTo((100, 0))
    pen.lineTo((100, 700))
    pen.lineTo((500, 700))
    pen.lineTo((500, 0))
    pen.closePath()
    return pen.glyph()


def main(out):
    glyphs = ['.notdef', 'A', 'B']
    fb = FontBuilder(1000, isTTF=True)
    fb.setupGlyphOrder(glyphs)
    fb.setupCharacterMap({0x41: 'A', 0x42: 'B'})
    fb.setupGlyf({g: box() for g in glyphs})
    fb.setupHorizontalMetrics({g: (600, 100) for g in glyphs})
    fb.setupHorizontalHeader(ascent=800, descent=-200)
    fb.setupNameTable({'familyName': 'OpenType Langs Test', 'styleName': 'Regular'})
    fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200,
                usWeightClass=400, usWidthClass=5)
    panose = fb.font['OS/2'].panose
    for attr, v in zip(('bFamilyType', 'bSerifStyle', 'bWeight', 'bProportion', 'bContrast',
                        'bStrokeVariation', 'bArmStyle', 'bLetterForm', 'bMidline', 'bXHeight'),
                       (2, 11, 5, 2, 4, 5, 4, 2, 2, 4)):
        setattr(panose, attr, v)
    fb.setupPost()
    meta = newTable('meta')
    meta.data = {'dlng': 'en-Latn, Hans', 'slng': 'en,ja-Jpan , zh-Hant,,x!y'}
    fb.font['meta'] = meta
    addOpenTypeFeaturesFromString(fb.font, FEATURES)
    fb.save(out)


if __name__ == '__main__':
    main(sys.argv[1])
