/*
 * fontconfig/src/fcmatch.c
 *
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "fcint.h"

#include <locale.h>

static double
FcCompareNumber (const FcValue *value1, const FcValue *value2, FcValue *bestValue)
{
    double v1, v2, v;

    switch ((int)value1->type) {
    case FcTypeInteger:
	v1 = (double)value1->u.i;
	break;
    case FcTypeDouble:
	v1 = value1->u.d;
	break;
    default:
	return -1.0;
    }
    switch ((int)value2->type) {
    case FcTypeInteger:
	v2 = (double)value2->u.i;
	break;
    case FcTypeDouble:
	v2 = value2->u.d;
	break;
    default:
	return -1.0;
    }
    v = v2 - v1;
    if (v < 0)
	v = -v;
    *bestValue = FcValueCanonicalize (value2);
    return v;
}

static double
FcCompareString (const FcValue *v1, const FcValue *v2, FcValue *bestValue)
{
    *bestValue = FcValueCanonicalize (v2);
    return (double)FcStrCmpIgnoreCase (FcValueString (v1), FcValueString (v2)) != 0;
}

static double
FcCompareFamily (const FcValue *v1, const FcValue *v2, FcValue *bestValue)
{
    /* rely on the guarantee in FcPatternObjectAddWithBinding that
     * families are always FcTypeString. */
    const FcChar8 *v1_string = FcValueString (v1);
    const FcChar8 *v2_string = FcValueString (v2);

    *bestValue = FcValueCanonicalize (v2);

    if (FcToLower (*v1_string) != FcToLower (*v2_string) &&
        *v1_string != ' ' && *v2_string != ' ')
	return 1.0;

    return (double)FcStrCmpIgnoreBlanksAndCase (v1_string, v2_string) != 0;
}

static double
FcComparePostScript (const FcValue *v1, const FcValue *v2, FcValue *bestValue)
{
    const FcChar8 *v1_string = FcValueString (v1);
    const FcChar8 *v2_string = FcValueString (v2);
    int            n;
    size_t         len1, len2, mlen;

    *bestValue = FcValueCanonicalize (v2);

    if (FcToLower (*v1_string) != FcToLower (*v2_string) &&
        *v1_string != ' ' && *v2_string != ' ')
	return 1.0;

    n = FcStrMatchIgnoreCaseAndDelims (v1_string, v2_string, (const FcChar8 *)" -,");
    len1 = strlen ((const char *)v1_string);
    len2 = strlen ((const char *)v2_string);
    mlen = FC_MAX (len1, len2);

    return (double)(mlen - n) / (double)mlen;
}

static double
FcCompareLang (const FcValue *v1, const FcValue *v2, FcValue *bestValue)
{
    FcLangResult result;

    switch ((int)v1->type) {
    case FcTypeLangSet:
	switch ((int)v2->type) {
	case FcTypeLangSet:
	    result = FcLangSetCompare (FcValueLangSet (v1), FcValueLangSet (v2));
	    break;
	case FcTypeString:
	    result = FcLangSetHasLang (FcValueLangSet (v1), FcValueString (v2));
	    break;
	default:
	    return -1.0;
	}
	break;
    case FcTypeString:
	switch ((int)v2->type) {
	case FcTypeLangSet:
	    result = FcLangSetHasLang (FcValueLangSet (v2), FcValueString (v1));
	    break;
	case FcTypeString:
	    result = FcLangCompare (FcValueString (v1), FcValueString (v2));
	    break;
	default:
	    return -1.0;
	}
	break;
    default:
	return -1.0;
    }
    *bestValue = FcValueCanonicalize (v2);
    switch (result) {
    case FcLangEqual:
	return 0;
    case FcLangDifferentCountry:
	return 1;
    case FcLangDifferentLang:
    default:
	return 2;
    }
}

static double
FcCompareBool (const FcValue *v1, const FcValue *v2, FcValue *bestValue)
{
    if (v2->type != FcTypeBool || v1->type != FcTypeBool)
	return -1.0;

    bestValue->type = FcTypeBool;
    if (v2->u.b != FcDontCare)
	bestValue->u.b = v2->u.b;
    else
	bestValue->u.b = v1->u.b;

    return (double)((v2->u.b ^ v1->u.b) == 1);
}

static double
FcCompareEqual (const FcValue *value1, const FcValue *value2, FcValue *bestValue)
{
    double v1, v2, v;

    switch ((int)value1->type) {
    case FcTypeInteger:
	v1 = (double)value1->u.i;
	break;
    case FcTypeDouble:
	v1 = value1->u.d;
	break;
    case FcTypeBool:
	return FcCompareBool (value1, value2, bestValue);
    default:
	return -1.0;
    }
    switch ((int)value2->type) {
    case FcTypeInteger:
	v2 = (double)value2->u.i;
	break;
    case FcTypeDouble:
	v2 = value2->u.d;
	break;
    case FcTypeBool:
	return FcCompareBool (value1, value2, bestValue);
    default:
	return -1.0;
    }
    v = !(v1 == v2);
    *bestValue = FcValueCanonicalize (value2);

    return v;
}

static double
FcCompareCharSet (const FcValue *v1, const FcValue *v2, FcValue *bestValue)
{
    *bestValue = FcValueCanonicalize (v2); /* TODO Improve. */
    return (double)FcCharSetSubtractCount (FcValueCharSet (v1), FcValueCharSet (v2));
}

static double
FcCompareRange (const FcValue *v1, const FcValue *v2, FcValue *bestValue)
{
    FcValue value1 = FcValueCanonicalize (v1);
    FcValue value2 = FcValueCanonicalize (v2);
    double  b1, e1, b2, e2, d;

    switch ((int)value1.type) {
    case FcTypeInteger:
	b1 = e1 = value1.u.i;
	break;
    case FcTypeDouble:
	b1 = e1 = value1.u.d;
	break;
    case FcTypeRange:
	b1 = value1.u.r->begin;
	e1 = value1.u.r->end;
	break;
    default:
	return -1;
    }
    switch ((int)value2.type) {
    case FcTypeInteger:
	b2 = e2 = value2.u.i;
	break;
    case FcTypeDouble:
	b2 = e2 = value2.u.d;
	break;
    case FcTypeRange:
	b2 = value2.u.r->begin;
	e2 = value2.u.r->end;
	break;
    default:
	return -1;
    }

    if (e1 < b2)
	d = b2;
    else if (e2 < b1)
	d = e2;
    else
	d = (FC_MAX (b1, b2) + FC_MIN (e1, e2)) * .5;

    bestValue->type = FcTypeDouble;
    bestValue->u.d = d;

    /* If the ranges overlap, it's a match, otherwise return closest distance. */
    if (e1 < b2 || e2 < b1)
	return FC_MIN (fabs (b2 - e1), fabs (b1 - e2));
    else
	return 0.0;
}

static FcBool
FcValueGetSpan (const FcValue *v, double *b, double *e)
{
    FcValue c = FcValueCanonicalize (v);

    switch ((int)c.type) {
    case FcTypeInteger:
	*b = *e = c.u.i;
	return FcTrue;
    case FcTypeDouble:
	*b = *e = c.u.d;
	return FcTrue;
    case FcTypeRange:
	*b = c.u.r->begin;
	*e = c.u.r->end;
	return FcTrue;
    }
    return FcFalse;
}

/*
 * Directional search keys from CSS Fonts 4, section 5.2 ("Matching font
 * styles"), step 4.  A face whose value or range contains the desired value
 * scores 0.  Otherwise the faces are tried in the order the spec gives,
 * nearest first within each group, and each later group is offset by
 * FC_CSS_GROUP so that it never beats an earlier one.
 */
#define FC_CSS_GROUP 10000.0

static double
FcFaceDistanceBelow (double want, double b, double e)
{
    (void)b;
    return want - e; /* e < want: lighter/narrower, descending */
}

static double
FcFaceDistanceAbove (double want, double b, double e)
{
    (void)e;
    return b - want; /* b > want: heavier/wider, ascending */
}

static double
FcCompareWeight (const FcValue *v1, const FcValue *v2, FcValue *bestValue)
{
    double r, b1, e1, b2, e2, want, lo, hi, cls;

    /* Same overlap test and same best value (used for variable fonts) as
     * a plain range comparison; only the ordering of misses differs. */
    r = FcCompareRange (v1, v2, bestValue);
    if (r <= 0)
	return r;
    if (!FcValueGetSpan (v1, &b1, &e1) || !FcValueGetSpan (v2, &b2, &e2))
	return r;

    /* Compare in OpenType units (100..900), where the CSS thresholds
     * 400 and 500 live; fontconfig's own weight scale is not linear in
     * them. */
    want = FcWeightToOpenTypeDouble (FC_MIN (FC_MAX ((b1 + e1) * .5, 0), FC_WEIGHT_EXTRABLACK));
    lo = FcWeightToOpenTypeDouble (FC_MIN (FC_MAX (b2, 0), FC_WEIGHT_EXTRABLACK));
    hi = FcWeightToOpenTypeDouble (FC_MIN (FC_MAX (e2, 0), FC_WEIGHT_EXTRABLACK));
    if (want < 0 || lo < 0 || hi < 0)
	return r;

    /* Pick the CSS rule by the nearest OpenType weight class (multiples
     * of 100, ties to the lighter one).  fontconfig's in-between
     * constants then follow their class: book (380) is searched like
     * regular, demilight (350) like light.  The distances below still use
     * the exact value. */
    cls = ceil (want / 100. - .5) * 100.;

    if (cls < 400) {
	if (hi < want)
	    return FcFaceDistanceBelow (want, lo, hi);
	return FC_CSS_GROUP + FcFaceDistanceAbove (want, lo, hi);
    } else if (cls <= 500) {
	if (lo > want && lo <= 500)
	    return FcFaceDistanceAbove (want, lo, hi);
	if (hi < want)
	    return FC_CSS_GROUP + FcFaceDistanceBelow (want, lo, hi);
	return 2 * FC_CSS_GROUP + FcFaceDistanceAbove (want, lo, hi);
    } else {
	if (lo > want)
	    return FcFaceDistanceAbove (want, lo, hi);
	return FC_CSS_GROUP + FcFaceDistanceBelow (want, lo, hi);
    }
}

static double
FcCompareWidth (const FcValue *v1, const FcValue *v2, FcValue *bestValue)
{
    double r, b1, e1, b2, e2, want;

    r = FcCompareRange (v1, v2, bestValue);
    if (r <= 0)
	return r;
    if (!FcValueGetSpan (v1, &b1, &e1) || !FcValueGetSpan (v2, &b2, &e2))
	return r;

    /* fontconfig widths are already percentages of normal, as in CSS. */
    want = (b1 + e1) * .5;
    if (want <= FC_WIDTH_NORMAL) {
	if (e2 < want)
	    return FcFaceDistanceBelow (want, b2, e2);
	return FC_CSS_GROUP + FcFaceDistanceAbove (want, b2, e2);
    } else {
	if (b2 > want)
	    return FcFaceDistanceAbove (want, b2, e2);
	return FC_CSS_GROUP + FcFaceDistanceBelow (want, b2, e2);
    }
}

static double
FcCompareSize (const FcValue *v1, const FcValue *v2, FcValue *bestValue)
{
    FcValue value1 = FcValueCanonicalize (v1);
    FcValue value2 = FcValueCanonicalize (v2);
    double  b1, e1, b2, e2;

    switch ((int)value1.type) {
    case FcTypeInteger:
	b1 = e1 = value1.u.i;
	break;
    case FcTypeDouble:
	b1 = e1 = value1.u.d;
	break;
    case FcTypeRange:
	b1 = value1.u.r->begin;
	e1 = value1.u.r->end;
	break;
    default:
	return -1;
    }
    switch ((int)value2.type) {
    case FcTypeInteger:
	b2 = e2 = value2.u.i;
	break;
    case FcTypeDouble:
	b2 = e2 = value2.u.d;
	break;
    case FcTypeRange:
	b2 = value2.u.r->begin;
	e2 = value2.u.r->end;
	break;
    default:
	return -1;
    }

    bestValue->type = FcTypeDouble;
    bestValue->u.d = (b1 + e1) * .5;

    /* If the ranges overlap, it's a match, otherwise return closest distance. */
    if (e1 < b2 || e2 < b1)
	return FC_MIN (fabs (b2 - e1), fabs (b1 - e2));
    if (b2 != e2 && b1 == e2) /* Semi-closed interval. */
	return 1e-15;
    else
	return 0.0;
}

static double
FcCompareFilename (const FcValue *v1, const FcValue *v2, FcValue *bestValue)
{
    const FcChar8 *s1 = FcValueString (v1), *s2 = FcValueString (v2);
    *bestValue = FcValueCanonicalize (v2);
    if (FcStrCmp (s1, s2) == 0)
	return 0.0;
    else if (FcStrCmpIgnoreCase (s1, s2) == 0)
	return 1.0;
    else if (FcStrGlobMatch (s1, s2))
	return 2.0;
    else
	return 3.0;
}

/* Define priorities to -1 for objects that don't have a compare function. */

#define PRI_NULL(n)        \
    PRI_##n##_STRONG = -1, \
    PRI_##n##_WEAK = -1,
#define PRI1(n)
#define PRI_FcCompareFamily(n)     PRI1 (n)
#define PRI_FcCompareString(n)     PRI1 (n)
#define PRI_FcCompareNumber(n)     PRI1 (n)
#define PRI_FcCompareEqual(n)      PRI1 (n)
#define PRI_FcCompareBool(n)       PRI1 (n)
#define PRI_FcCompareFilename(n)   PRI1 (n)
#define PRI_FcCompareCharSet(n)    PRI1 (n)
#define PRI_FcCompareLang(n)       PRI1 (n)
#define PRI_FcComparePostScript(n) PRI1 (n)
#define PRI_FcCompareRange(n)      PRI1 (n)
#define PRI_FcCompareWeight(n)     PRI1 (n)
#define PRI_FcCompareWidth(n)      PRI1 (n)
#define PRI_FcCompareSize(n)       PRI1 (n)

#define FC_OBJECT(NAME, Type, Cmp) PRI_##Cmp (NAME)

typedef enum _FcMatcherPriorityDummy {
#include "fcobjs.h"
} FcMatcherPriorityDummy;

#undef FC_OBJECT

/* Canonical match priority order. */

#undef PRI1
#define PRI1(n)                     \
    PRI_##n,                        \
	PRI_##n##_STRONG = PRI_##n, \
	PRI_##n##_WEAK = PRI_##n

typedef enum _FcMatcherPriority {
    PRI1 (FILE),
    PRI1 (FONT_WRAPPER),
    PRI1 (FONTFORMAT),
    PRI1 (VARIABLE),
    PRI1 (NAMED_INSTANCE),
    PRI1 (SCALABLE),
    PRI1 (COLOR),
    PRI1 (FOUNDRY),
    PRI1 (CHARSET),
    PRI_FAMILY_STRONG,
    PRI1 (GENERIC_FAMILY),
    PRI_POSTSCRIPT_NAME_STRONG,
    PRI1 (LANG),
    PRI_FAMILY_WEAK,
    PRI_POSTSCRIPT_NAME_WEAK,
    PRI1 (SYMBOL),
    PRI1 (SPACING),
    PRI1 (SIZE),
    PRI1 (PIXEL_SIZE),
    PRI1 (STYLE),
    PRI1 (WIDTH), /* CSS Fonts 4 5.2 step 4: width, then style, then weight */
    PRI1 (SLANT),
    PRI1 (WEIGHT),
    PRI1 (FONT_HAS_HINT),
    PRI1 (DECORATIVE),
    PRI1 (ANTIALIAS),
    PRI1 (RASTERIZER),
    PRI1 (OUTLINE),
    PRI1 (ORDER),
    PRI1 (FONTVERSION),
    PRI_END
} FcMatcherPriority;

#undef PRI1

typedef struct _FcMatcher {
    FcObject object;
    double (*compare) (const FcValue *v1, const FcValue *v2, FcValue *bestValue);
    int strong, weak;
} FcMatcher;

/*
 * Order is significant, it defines the precedence of
 * each value, earlier values are more significant than
 * later values
 */
#define FC_OBJECT(NAME, Type, Cmp) { FC_##NAME##_OBJECT, Cmp, PRI_##NAME##_STRONG, PRI_##NAME##_WEAK },
static const FcMatcher _FcMatchers[] = {
    { FC_INVALID_OBJECT, NULL, -1, -1 },
#include "fcobjs.h"
};
#undef FC_OBJECT

static const FcMatcher *
FcObjectToMatcher (FcObject object,
                   FcBool   include_lang)
{
    if (include_lang) {
	switch (object) {
	case FC_FAMILYLANG_OBJECT:
	case FC_STYLELANG_OBJECT:
	case FC_FULLNAMELANG_OBJECT:
	    object = FC_LANG_OBJECT;
	    break;
	}
    }
    if (object > FC_MAX_BASE_OBJECT ||
        !_FcMatchers[object].compare ||
        _FcMatchers[object].strong == -1 ||
        _FcMatchers[object].weak == -1)
	return NULL;

    return _FcMatchers + object;
}

static FcBool
FcCompareValueList (FcObject         object,
                    const FcMatcher *match,
                    FcValueListPtr   v1orig, /* pattern */
                    FcValueListPtr   v2orig, /* target */
                    FcValue         *bestValue,
                    double          *value,
                    int             *n,
                    FcResult        *result)
{
    FcValueListPtr v1, v2;
    double         v, best, bestStrong, bestWeak;
    int            j, k, pos = 0;
    int            weak, strong;

    if (!match) {
	if (bestValue)
	    *bestValue = FcValueCanonicalize (&v2orig->value);
	if (n)
	    *n = 0;
	return FcTrue;
    }

    weak = match->weak;
    strong = match->strong;

    best = 1e99;
    bestStrong = 1e99;
    bestWeak = 1e99;
    for (v1 = v1orig, j = 0; v1; v1 = FcValueListNext (v1), j++) {
	for (v2 = v2orig, k = 0; v2; v2 = FcValueListNext (v2), k++) {
	    FcValue matchValue;
	    v = (match->compare) (&v1->value, &v2->value, &matchValue);
	    if (v < 0) {
		*result = FcResultTypeMismatch;
		return FcFalse;
	    }
	    v = v * 1000 + j * 100 + k * (v2->value.type == FcTypeString ? 1 : 0);
	    if (v < best) {
		if (bestValue)
		    *bestValue = matchValue;
		best = v;
		pos = k;
	    }
	    if (weak == strong) {
		/* found the best possible match */
		if (best < 1000)
		    goto done;
	    } else if (v1->binding == FcValueBindingStrong) {
		if (v < bestStrong)
		    bestStrong = v;
	    } else {
		if (v < bestWeak)
		    bestWeak = v;
	    }
	}
    }
done:
    if (FcDebug() & FC_DBG_MATCHV) {
	printf (" %s: %g ", FcObjectName (object), best);
	FcValueListPrint (v1orig);
	printf (", ");
	FcValueListPrint (v2orig);
	printf ("\n");
    }
    if (value) {
	if (weak == strong)
	    value[strong] += best;
	else {
	    value[weak] += bestWeak;
	    value[strong] += bestStrong;
	}
    }
    if (n)
	*n = pos;

    return FcTrue;
}

/* The bulk of the time in FcFontMatch and FcFontSort goes to
 * walking long lists of family names. We speed this up with a
 * hash table.
 */
typedef struct
{
    double strong_value;
    double weak_value;
} FamilyEntry;

typedef struct
{
    FcHashTable *family_hash;
} FcCompareData;

static void
FcCompareDataClear (FcCompareData *data)
{
    FcHashTableDestroy (data->family_hash);
}

static FcBool
FcCompareDataInit (FcPattern     *pat,
                   FcCompareData *data)
{
    FcHashTable   *table;
    FcPatternElt  *elt;
    FcValueListPtr l;
    int            i;
    const void    *key;
    FamilyEntry   *e;

    table = FcHashTableCreate ((FcHashFunc)FcStrHashIgnoreBlanksAndCase,
                               (FcCompareFunc)FcStrCmpIgnoreBlanksAndCase,
                               NULL,
                               NULL,
                               NULL,
                               free);
    if (!table)
	return FcFalse;

    elt = FcPatternObjectFindElt (pat, FC_FAMILY_OBJECT);
    if (elt) {
	for (l = FcPatternEltValues (elt), i = 0; l; l = FcValueListNext (l), i++) {
	    key = FcValueString (&l->value);
	    if (!FcHashTableFind (table, key, (void **)&e)) {
		e = malloc (sizeof (FamilyEntry));
		if (!e) {
		    FcHashTableDestroy (table);
		    return FcFalse;
		}
		e->strong_value = 1e99;
		e->weak_value = 1e99;
		FcHashTableAdd (table, (void *)key, e);
	    }
	    if (l->binding == FcValueBindingWeak) {
		if (i < e->weak_value)
		    e->weak_value = i;
	    } else {
		if (i < e->strong_value)
		    e->strong_value = i;
	    }
	}
    }

    data->family_hash = table;
    return FcTrue;
}

static FcBool
FcCompareFamilies (FcPattern     *pat,
                   FcValueListPtr v1orig,
                   FcPattern     *fnt,
                   FcValueListPtr v2orig,
                   double        *value,
                   FcResult      *result,
                   FcHashTable   *table)
{
    FcValueListPtr v2;
    double         strong_value;
    double         weak_value;
    const void    *key;
    FamilyEntry   *e;

    assert (table != NULL);

    strong_value = 1e99;
    weak_value = 1e99;

    for (v2 = v2orig; v2; v2 = FcValueListNext (v2)) {
	key = FcValueString (&v2->value);
	if (FcHashTableFind (table, key, (void **)&e)) {
	    if (e->strong_value < strong_value)
		strong_value = e->strong_value;
	    if (e->weak_value < weak_value)
		weak_value = e->weak_value;
	}
    }
    if (FcDebug() & FC_DBG_MATCHV) {
	printf ("%s: %g (%g) ", FcObjectName (FC_FAMILY_OBJECT), strong_value, weak_value);
	FcValueListPrint (v1orig);
	printf (", ");
	FcValueListPrint (v2orig);
	printf ("\n");
    }

    value[PRI_FAMILY_STRONG] = strong_value;
    value[PRI_FAMILY_WEAK] = weak_value;

    return FcTrue;
}

/*
 * Fonts loaded from caches that predate the genericfamily object
 * (issue #562) carry no FC_GENERIC_FAMILY element, so the merge-join
 * below would skip that dimension and hand them an unearned perfect
 * score.  Re-derive the value from the font's family (curated
 * classification only) and score it as if the scanner had stored it,
 * so an old-cache font matches like a freshly scanned one.
 */
static FcBool
FcCompareGenericFamilyBackfill (FcValueListPtr query, /* pattern genericfamily */
                                FcPattern     *fnt,
                                double        *value,
                                FcResult      *result)
{
    const FcMatcher      *match = FcObjectToMatcher (FC_GENERIC_FAMILY_OBJECT, FcFalse);
    FcPatternElt         *fam = FcPatternObjectFindElt (fnt, FC_FAMILY_OBJECT);
    FcGenericFamilyValues g;
    FcValueList           vl[FC_GENERIC_FAMILY_MAX_VALUES];
    int                   k;

    g = FcGenericFamilyGetValues (fam ? FcPatternEltValues (fam) : NULL);
    for (k = 0; k < g.n; k++) {
	vl[k].value.type = FcTypeInteger;
	vl[k].value.u.i = g.values[k];
	vl[k].binding = FcValueBindingStrong;
	vl[k].next = (k + 1 < g.n) ? &vl[k + 1] : NULL;
    }
    return FcCompareValueList (FC_GENERIC_FAMILY_OBJECT, match,
                               query, vl, NULL, value, NULL, result);
}

/*
 * Design language inside the LANG slot.  The OpenType 'meta' table's
 * 'dlng' lists "languages and/or scripts for the user audiences that the
 * font was primarily designed for".  Among fonts that cover the requested
 * language equally, one whose dlng names it ranks first.  A font without
 * dlng (no 'meta' table, a non-SFNT font, an older cache) is "unknown" and
 * ranks with every other covering font: absence is never a penalty.
 *
 * 'slng' ("declared to be capable of supporting") and 'locl' language
 * systems are deliberately not used: both restate coverage, and ranking
 * them above fonts that simply lack the table moved Arabic and Persian to
 * Noto Nastaliq Urdu (slng ar, fa) and Turkish, Polish and others to a
 * monospace face that happens to carry locl for them.
 *
 * The offset stays below 100, the step between two requested languages in
 * FcCompareValueList, so it only orders fonts that already tie on coverage.
 */
#define FC_LANG_TIER_DESIGN  0.0
#define FC_LANG_TIER_COVERED 30.0

/* Compare the first @n bytes of two subtags, ignoring ASCII case. */
static int
FcLangSubtagCmp (const FcChar8 *a, const FcChar8 *b, int n)
{
    int i;

    for (i = 0; i < n; i++) {
	FcChar8 ca = FcToLower (a[i]), cb = FcToLower (b[i]);

	if (ca != cb)
	    return ca < cb ? -1 : 1;
	if (!ca)
	    break;
    }
    return 0;
}

/* Script subtags for the languages whose script is not implied by
 * fontconfig's language name alone (ScriptLangTags may be script-only). */
static const struct {
    const char *lang;
    const char *scripts; /* space separated, lower case */
} fcLangScripts[] = {
    { "ja",    "jpan hani hira kana" },
    { "ko",    "kore hang hani" },
    { "zh-cn", "hans hani" },
    { "zh-sg", "hans hani" },
    { "zh-tw", "hant hani" },
    { "zh-hk", "hant hani" },
    { "zh-mo", "hant hani" },
};

static FcBool
FcLangHasScript (const FcChar8 *lang, const FcChar8 *script, int len)
{
    size_t i;

    for (i = 0; i < sizeof (fcLangScripts) / sizeof (fcLangScripts[0]); i++) {
	const char *p;

	if (FcStrCmpIgnoreCase (lang, (const FcChar8 *)fcLangScripts[i].lang) != 0)
	    continue;
	for (p = fcLangScripts[i].scripts; *p; p += (p[4] ? 5 : 4))
	    if (!FcLangSubtagCmp ((const FcChar8 *)p, script, len) && len == 4)
		return FcTrue;
    }
    return FcFalse;
}

/*
 * Does the ScriptLangTag @tag (e.g. "ur-Arab", "as-Beng", "Hans", "zh-TW")
 * name the fontconfig language @lang (e.g. "ur", "zh-tw")?  The primary
 * language subtags must agree; a region in both must agree; a script in
 * the tag must be one @lang is written in, when we know it.  A script-only
 * tag matches only through the table above.
 */
static FcBool
FcLangMatchesTag (const FcChar8 *lang, const FcChar8 *tag)
{
    const FcChar8 *sub, *lsub;
    int            n, ln;

    for (n = 0; tag[n] && tag[n] != '-'; n++)
	;
    if (n == 4 && !tag[n]) /* script-only */
	return FcLangHasScript (lang, tag, 4);
    for (ln = 0; lang[ln] && lang[ln] != '-'; ln++)
	;
    if (n != ln || FcLangSubtagCmp (tag, lang, n) != 0)
	return FcFalse;
    lsub = lang[ln] ? lang + ln + 1 : NULL;
    for (sub = tag[n] ? tag + n + 1 : NULL; sub; ) {
	int m;

	for (m = 0; sub[m] && sub[m] != '-'; m++)
	    ;
	if (m == 4) {
	    /* script subtag: reject only when we know the language's
	     * scripts and this is not one of them */
	    FcBool known = FcFalse;
	    size_t i;

	    for (i = 0; i < sizeof (fcLangScripts) / sizeof (fcLangScripts[0]); i++)
		if (!FcStrCmpIgnoreCase (lang, (const FcChar8 *)fcLangScripts[i].lang))
		    known = FcTrue;
	    if (known && !FcLangHasScript (lang, sub, 4))
		return FcFalse;
	} else if (m == 2 || m == 3) {
	    /* region subtag */
	    if (lsub && (FcLangSubtagCmp (sub, lsub, m) != 0 || (lsub[m] && lsub[m] != '-')))
		return FcFalse;
	}
	sub = sub[m] ? sub + m + 1 : NULL;
    }
    return FcTrue;
}

static FcBool
FcFontLangObjectMatches (FcPattern *fnt, FcObject object, FcStrSet *langs)
{
    FcPatternElt  *e = FcPatternObjectFindElt (fnt, object);
    FcValueListPtr l;
    int            i;

    if (!e)
	return FcFalse;
    for (l = FcPatternEltValues (e); l; l = FcValueListNext (l)) {
	FcValue v = FcValueCanonicalize (&l->value);

	if (v.type != FcTypeString)
	    continue;
	for (i = 0; i < langs->num; i++)
	    if (FcLangMatchesTag (langs->strs[i], v.u.s))
		return FcTrue;
    }
    return FcFalse;
}

/* The offset to add to a font's LANG score (see the comment above). */
static double
FcCompareLangTier (FcPattern *pat, FcPattern *fnt, double langscore)
{
    FcPatternElt  *pe = FcPatternObjectFindElt (pat, FC_LANG_OBJECT);
    FcPatternElt  *fe = FcPatternObjectFindElt (fnt, FC_LANG_OBJECT);
    FcValueListPtr l;
    FcStrSet      *langs = NULL;
    double         tier = FC_LANG_TIER_COVERED;
    int            j;

    if (!pe || !fe || langscore >= 2000)
	return 0;
    if (!FcPatternObjectFindElt (fnt, FC_DESIGN_LANG_OBJECT))
	return tier;

    /* Find the requested language that produced the LANG score, with the
     * same formula as FcCompareValueList. */
    for (l = FcPatternEltValues (pe), j = 0; l && !langs; l = FcValueListNext (l), j++) {
	FcValue        v = FcValueCanonicalize (&l->value);
	FcValueListPtr fl;
	int            k;

	for (fl = FcPatternEltValues (fe), k = 0; fl; fl = FcValueListNext (fl), k++) {
	    FcValue fv = FcValueCanonicalize (&fl->value), best;
	    double  c = FcCompareLang (&v, &fv, &best);

	    if (c < 0 || c * 1000 + j * 100 + (fv.type == FcTypeString ? k : 0) != langscore)
		continue;
	    if (v.type == FcTypeLangSet)
		langs = FcLangSetGetLangs (v.u.l);
	    else if (v.type == FcTypeString && (langs = FcStrSetCreate()))
		FcStrSetAdd (langs, v.u.s);
	    break;
	}
    }
    if (!langs)
	return tier;
    if (FcFontLangObjectMatches (fnt, FC_DESIGN_LANG_OBJECT, langs))
	tier = FC_LANG_TIER_DESIGN;
    FcStrSetDestroy (langs);
    return tier;
}

/*
 * Return a value indicating the distance between the two lists of
 * values
 */

static FcBool
FcCompare (FcPattern     *pat,
           FcPattern     *fnt,
           double        *value,
           FcResult      *result,
           FcCompareData *data)
{
    int i, i1, i2;

    for (i = 0; i < PRI_END; i++)
	value[i] = 0.0;

    i1 = 0;
    i2 = 0;
    while (i1 < pat->num && i2 < fnt->num) {
	FcPatternElt *elt_i1 = &FcPatternElts (pat)[i1];
	FcPatternElt *elt_i2 = &FcPatternElts (fnt)[i2];

	i = FcObjectCompare (elt_i1->object, elt_i2->object);
	if (i > 0)
	    i2++;
	else if (i < 0)
	    i1++;
	else if (elt_i1->object == FC_FAMILY_OBJECT && data->family_hash) {
	    if (!FcCompareFamilies (pat, FcPatternEltValues (elt_i1),
	                            fnt, FcPatternEltValues (elt_i2),
	                            value, result,
	                            data->family_hash))
		return FcFalse;
	    i1++;
	    i2++;
	} else {
	    const FcMatcher *match = FcObjectToMatcher (elt_i1->object, FcFalse);
	    if (!FcCompareValueList (elt_i1->object, match,
	                             FcPatternEltValues (elt_i1),
	                             FcPatternEltValues (elt_i2),
	                             NULL, value, NULL, result))
		return FcFalse;
	    i1++;
	    i2++;
	}
    }

    /* Design-language tiers inside the LANG slot (see above). */
    value[PRI_LANG] += FcCompareLangTier (pat, fnt, value[PRI_LANG]);

    /*
     * genericfamily is derivable from the family name, so reconstruct it
     * for fonts loaded from caches that predate the object rather than
     * leaving it unscored (issue #562).  This is done outside the
     * merge-join because the loop can exit before reaching a trailing
     * query object that the font lacks.
     * NOTE: when missing-object vs not-matched scoring is unified, this
     * reconstruction must run before the missing-object policy so
     * derivable objects are treated as present, not absent.
     */
    {
	FcPatternElt *q_gf = FcPatternObjectFindElt (pat, FC_GENERIC_FAMILY_OBJECT);

	if (q_gf && !FcPatternObjectFindElt (fnt, FC_GENERIC_FAMILY_OBJECT) &&
	    !FcCompareGenericFamilyBackfill (FcPatternEltValues (q_gf),
	                                     fnt, value, result))
	    return FcFalse;
    }

    return FcTrue;
}

FcPattern *
FcFontRenderPrepare (FcConfig  *config,
                     FcPattern *pat,
                     FcPattern *font)
{
    FcPattern    *newp;
    int           i;
    FcPatternElt *fe, *pe;
    FcValue       v;
    FcResult      result;
    FcBool        variable = FcFalse;
    FcStrBuf      variations;

    assert (pat != NULL);
    assert (font != NULL);

    FcPatternObjectGetBool (font, FC_VARIABLE_OBJECT, 0, &variable);
    assert (variable != FcDontCare);
    if (variable)
	FcStrBufInit (&variations, NULL, 0);

    newp = FcPatternCreate();
    if (!newp)
	return NULL;
    for (i = 0; i < font->num; i++) {
	fe = &FcPatternElts (font)[i];
	if (fe->object == FC_FAMILYLANG_OBJECT ||
	    fe->object == FC_STYLELANG_OBJECT ||
	    fe->object == FC_FULLNAMELANG_OBJECT) {
	    /* ignore those objects. we need to deal with them
	     * another way */
	    continue;
	}
	if (fe->object == FC_FAMILY_OBJECT ||
	    fe->object == FC_STYLE_OBJECT ||
	    fe->object == FC_FULLNAME_OBJECT) {
	    FcPatternElt *fel, *pel;

	    FC_ASSERT_STATIC ((FC_FAMILY_OBJECT + 1) == FC_FAMILYLANG_OBJECT);
	    FC_ASSERT_STATIC ((FC_STYLE_OBJECT + 1) == FC_STYLELANG_OBJECT);
	    FC_ASSERT_STATIC ((FC_FULLNAME_OBJECT + 1) == FC_FULLNAMELANG_OBJECT);

	    fel = FcPatternObjectFindElt (font, fe->object + 1);
	    pel = FcPatternObjectFindElt (pat, fe->object + 1);

	    if (fel && pel) {
		/* The font has name languages, and pattern asks for specific language(s).
		 * Match on language and and prefer that result.
		 * Note:  Currently the code only give priority to first matching language.
		 */
		int              n = 1, j;
		FcValueListPtr   l1, l2, ln = NULL, ll = NULL;
		const FcMatcher *match = FcObjectToMatcher (pel->object, FcTrue);

		if (!FcCompareValueList (pel->object, match,
		                         FcPatternEltValues (pel),
		                         FcPatternEltValues (fel), NULL, NULL, &n, &result)) {
		    FcPatternDestroy (newp);
		    return NULL;
		}

		for (j = 0, l1 = FcPatternEltValues (fe), l2 = FcPatternEltValues (fel);
		     l1 != NULL || l2 != NULL;
		     j++, l1 = l1 ? FcValueListNext (l1) : NULL, l2 = l2 ? FcValueListNext (l2) : NULL) {
		    FcValueListPtr (*func) (FcValueListPtr, FcValue, FcValueBinding);
		    FcValueBinding binding = FcValueBindingEnd;

		    if (j == n) {
			binding = FcValueBindingStrong;
			func = FcValueListPrepend;
		    } else
			func = FcValueListAppend;
		    if (l1) {
			ln = func (ln,
			           FcValueCanonicalize (&l1->value),
			           l1->binding);
		    }
		    if (l2) {
			if (binding == FcValueBindingEnd)
			    binding = l2->binding;
			ll = func (ll,
			           FcValueCanonicalize (&l2->value),
			           binding);
		    }
		}
		FcPatternObjectListAdd (newp, fe->object, ln, FcFalse);
		FcPatternObjectListAdd (newp, fel->object, ll, FcFalse);

		continue;
	    } else if (fel) {
		/* Pattern doesn't ask for specific language.  Copy all for name and
		 * lang. */
		FcValueListPtr l1, l2;

		l1 = FcValueListDuplicate (FcPatternEltValues (fe));
		l2 = FcValueListDuplicate (FcPatternEltValues (fel));
		FcPatternObjectListAdd (newp, fe->object, l1, FcFalse);
		FcPatternObjectListAdd (newp, fel->object, l2, FcFalse);

		continue;
	    }
	}

	pe = FcPatternObjectFindElt (pat, fe->object);
	if (pe) {
	    const FcMatcher *match = FcObjectToMatcher (pe->object, FcFalse);
	    if (!FcCompareValueList (pe->object, match,
	                             FcPatternEltValues (pe),
	                             FcPatternEltValues (fe), &v, NULL, NULL, &result)) {
		FcPatternDestroy (newp);
		return NULL;
	    }
	    FcPatternObjectAdd (newp, fe->object, v, FcFalse);

	    /* Set font-variations settings for standard axes in variable fonts. */
	    if (variable &&
	        FcPatternEltValues (fe)->value.type == FcTypeRange &&
	        (fe->object == FC_WEIGHT_OBJECT ||
	         fe->object == FC_WIDTH_OBJECT ||
	         fe->object == FC_SIZE_OBJECT)) {
		double      num;
		const char *tag = "    ";

		assert (v.type == FcTypeDouble);
		num = v.u.d;
		if (variations.len)
		    FcStrBufChar (&variations, ',');
		switch (fe->object) {
		case FC_WEIGHT_OBJECT:
		    tag = "wght";
		    num = FcWeightToOpenType (num);
		    break;

		case FC_WIDTH_OBJECT:
		    tag = "wdth";
		    break;

		case FC_SIZE_OBJECT:
		    tag = "opsz";
		    break;
		}
		FcStrBufFormat (&variations, "%4s=%g", tag, num);
	    }
	} else {
	    FcPatternObjectListAdd (newp, fe->object,
	                            FcValueListDuplicate (FcPatternEltValues (fe)),
	                            FcTrue);
	}
    }
    for (i = 0; i < pat->num; i++) {
	pe = &FcPatternElts (pat)[i];
	fe = FcPatternObjectFindElt (font, pe->object);
	if (!fe &&
	    pe->object != FC_FAMILYLANG_OBJECT &&
	    pe->object != FC_STYLELANG_OBJECT &&
	    pe->object != FC_FULLNAMELANG_OBJECT) {
	    FcPatternObjectListAdd (newp, pe->object,
	                            FcValueListDuplicate (FcPatternEltValues (pe)),
	                            FcFalse);
	}
    }

    if (variable && variations.len) {
	FcChar8 *vars = NULL;
	if (FcPatternObjectGetString (newp, FC_FONT_VARIATIONS_OBJECT, 0, &vars) == FcResultMatch) {
	    FcStrBufChar (&variations, ',');
	    FcStrBufString (&variations, vars);
	    FcPatternObjectDel (newp, FC_FONT_VARIATIONS_OBJECT);
	}

	FcPatternObjectAddString (newp, FC_FONT_VARIATIONS_OBJECT, FcStrBufDoneStatic (&variations));
	FcStrBufDestroy (&variations);
    }

    FcConfigSubstituteWithPat (config, newp, pat, FcMatchFont);
    return newp;
}

/*
 * FC_DBG_EXPLAIN (fc-match --explain): say why the winner won.  Every font
 * is scored with FcCompare, exactly as above; the winner and the best face
 * of each of the next few families are listed with their score per slot,
 * and each runner-up is shown the slot where it lost, with the two values
 * compared.
 */
#define FC_EXPLAIN_RUNNERS_UP 4

/* The object scored in @slot (FC_INVALID_OBJECT if none). */
static FcObject
FcExplainSlotObject (int slot)
{
    size_t o;

    for (o = 1; o < sizeof (_FcMatchers) / sizeof (_FcMatchers[0]); o++)
	if (_FcMatchers[o].compare && (_FcMatchers[o].strong == slot || _FcMatchers[o].weak == slot))
	    return _FcMatchers[o].object;
    return FC_INVALID_OBJECT;
}

static const char *
FcExplainSlotName (int slot)
{
    FcObject object;

    switch (slot) {
    case PRI_FAMILY_STRONG: return "family (strong)";
    case PRI_FAMILY_WEAK: return "family (weak)";
    case PRI_POSTSCRIPT_NAME_STRONG: return "postscriptname (strong)";
    case PRI_POSTSCRIPT_NAME_WEAK: return "postscriptname (weak)";
    }
    object = FcExplainSlotObject (slot);
    return object != FC_INVALID_OBJECT ? FcObjectName (object) : "?";
}

static void
FcExplainValues (FcPattern *pat, FcObject object)
{
    FcPatternElt *e = FcPatternObjectFindElt (pat, object);

    if (e)
	FcValueListPrint (FcPatternEltValues (e));
    else
	printf (" (none)");
}

/* Print the @n'th value of @pat's @object as plain text. */
static void
FcExplainNth (FcPattern *pat, FcObject object, int n)
{
    FcPatternElt  *e = FcPatternObjectFindElt (pat, object);
    FcValueListPtr l;

    for (l = e ? FcPatternEltValues (e) : NULL; l && n > 0; l = FcValueListNext (l))
	n--;
    if (!l) {
	printf ("?");
	return;
    }
    {
	FcValue v = FcValueCanonicalize (&l->value);

	if (v.type == FcTypeString)
	    printf ("%s", v.u.s);
	else if (v.type == FcTypeLangSet) {
	    FcStrSet  *langs = FcLangSetGetLangs (v.u.l);
	    FcStrList *sl = langs ? FcStrListCreate (langs) : NULL;
	    FcChar8   *s;
	    const char *sep = "";

	    while (sl && (s = FcStrListNext (sl))) {
		printf ("%s%s", sep, s);
		sep = "/";
	    }
	    if (sl)
		FcStrListDone (sl);
	    if (langs)
		FcStrSetDestroy (langs);
	} else
	    FcValuePrint (v);
    }
}

static const char *
FcExplainGenericName (int g)
{
    static const char *names[] = {
	"unknown", "serif", "sans-serif", "monospace", "cursive", "fantasy",
	"system-ui", "ui-serif", "ui-sans-serif", "ui-monospace", "ui-rounded",
	"emoji", "math", "fangsong"
    };

    return g >= 0 && g < (int)(sizeof (names) / sizeof (names[0])) ? names[g] : "?";
}

static void
FcExplainGeneric (FcPattern *pat)
{
    FcPatternElt  *e = FcPatternObjectFindElt (pat, FC_GENERIC_FAMILY_OBJECT);
    FcValueListPtr l;
    const char    *sep = "";

    if (!e) {
	printf ("unclassified");
	return;
    }
    for (l = FcPatternEltValues (e); l; l = FcValueListNext (l)) {
	FcValue v = FcValueCanonicalize (&l->value);

	printf ("%s%s", sep, v.type == FcTypeInteger ? FcExplainGenericName (v.u.i) : "?");
	sep = "/";
    }
}

/* LANG = coverage * 1000 + request position * 100 + design-language tier */
static void
FcExplainLangSide (FcPattern *pat, double v, FcBool plural)
{
    int cov = (int)(v / 1000), j = ((int)v % 1000) / 100;

    if (cov >= 2) {
	printf ("%s not cover any requested language", plural ? "do" : "does");
	return;
    }
    printf ("%s ", plural ? "cover" : "covers");
    FcExplainNth (pat, FC_LANG_OBJECT, j);
    if (cov == 1)
	printf (" (another territory)");
    if (j > 0)
	printf (" (requested language #%d)", j + 1);
}

static void
FcExplainLang (FcPattern *pat, FcPattern *win, double wv, FcPattern *fnt, double fv)
{
    double wtier = wv - (double)((long)wv / 100 * 100), ftier = fv - (double)((long)fv / 100 * 100);

    if (wv - wtier == fv - ftier) {
	printf ("both ");
	FcExplainLangSide (pat, wv, FcTrue);
	printf ("; designlang: ");
	if (wtier < ftier) {
	    printf ("winner designed for ");
	    FcExplainNth (pat, FC_LANG_OBJECT, ((int)wv % 1000) / 100);
	    printf (", this face %s", FcPatternObjectFindElt (fnt, FC_DESIGN_LANG_OBJECT) ? "not" : "has no design-language data");
	} else
	    printf ("this face designed for it, the winner not");
	(void)win;
	return;
    }
    printf ("winner ");
    FcExplainLangSide (pat, wv, FcFalse);
    printf ("; this face ");
    FcExplainLangSide (pat, fv, FcFalse);
}

static void
FcExplainFamily (FcPattern *pat, double v)
{
    if (v >= 1e99)
	printf ("not in the list");
    else {
	printf ("\"");
	FcExplainNth (pat, FC_FAMILY_OBJECT, (int)v);
	printf ("\" at position %d", (int)v + 1);
    }
}

/* Why @fnt lost to @win at @slot. */
static void
FcExplainSlot (FcPattern *pat, int slot, FcPattern *win, double wv, FcPattern *fnt, double fv)
{
    FcObject object;

    printf ("%s: ", FcExplainSlotName (slot));
    switch (slot) {
    case PRI_FAMILY_STRONG:
    case PRI_FAMILY_WEAK:
	printf ("%s names in the request's family list: winner ", slot == PRI_FAMILY_STRONG ? "strong" : "weak");
	FcExplainFamily (pat, wv);
	printf (", this face ");
	FcExplainFamily (pat, fv);
	break;
    case PRI_GENERIC_FAMILY:
	printf ("request wants ");
	FcExplainGeneric (pat);
	printf (", winner is ");
	FcExplainGeneric (win);
	printf (", this face ");
	FcExplainGeneric (fnt);
	break;
    case PRI_LANG:
	FcExplainLang (pat, win, wv, fnt, fv);
	break;
    default:
	object = FcExplainSlotObject (slot);
	if (object == FC_INVALID_OBJECT)
	    break;
	printf ("request");
	FcExplainValues (pat, object);
	printf (", winner");
	FcExplainValues (win, object);
	printf (", this face");
	FcExplainValues (fnt, object);
	break;
    }
    printf (" (score %g vs %g)\n", wv, fv);
}

typedef struct {
    FcPattern     *font;
    const FcChar8 *family;
    double         score[PRI_END];
} FcExplainEntry;

static int
FcExplainScoreCmp (const double *a, const double *b)
{
    int i;

    for (i = 0; i < PRI_END; i++)
	if (a[i] != b[i])
	    return a[i] < b[i] ? -1 : 1;
    return 0;
}

static int
FcExplainEntryCmp (const void *a, const void *b)
{
    return FcExplainScoreCmp (((const FcExplainEntry *)a)->score, ((const FcExplainEntry *)b)->score);
}

static void
FcExplainMatch (FcFontSet    **sets,
                int            nsets,
                FcPattern     *p,
                FcPattern     *best,
                const FcChar8 *famfirst,
                FcCompareData *data)
{
    FcExplainEntry *e = NULL, *t;
    int             n = 0, size = 0, set, f, i, k, nshow, own;
    FcResult        result;
    int             lost[2 + FC_EXPLAIN_RUNNERS_UP];

    printf ("== Request after config and default substitution ==\n");
    FcPatternPrint (p);

    /* Entry 0 is the winner; then the best face of every other family. */
    for (set = 0; set < nsets; set++) {
	if (!sets[set])
	    continue;
	for (f = 0; f < sets[set]->nfont; f++) {
	    FcPattern *font = sets[set]->fonts[f];
	    FcChar8   *fam = NULL;
	    double     score[PRI_END];

	    if (!FcCompare (p, font, score, &result, data))
		goto bail;
	    if (FcPatternObjectGetString (font, FC_FAMILY_OBJECT, 0, &fam) != FcResultMatch)
		fam = (FcChar8 *)"(no family)";
	    if (n == 0) {
		t = realloc (e, (size = 64) * sizeof (*e));
		if (!t)
		    goto bail;
		e = t;
		e[0].font = NULL;
		n = 1;
	    }
	    if (font == best) {
		k = 0;
	    } else {
		for (k = 1; k < n; k++)
		    if (!FcStrCmpIgnoreBlanksAndCase (e[k].family, fam))
			break;
		if (k < n && FcExplainScoreCmp (score, e[k].score) >= 0)
		    continue;
		if (k == n) {
		    if (n == size) {
			t = realloc (e, (size *= 2) * sizeof (*e));
			if (!t)
			    goto bail;
			e = t;
		    }
		    n++;
		}
	    }
	    e[k].font = font;
	    e[k].family = fam;
	    memcpy (e[k].score, score, sizeof (score));
	}
    }
    if (!e || !e[0].font)
	goto bail;
    /* The best other face of the winner's own family comes first, then
     * the other families by score. */
    for (k = 1, own = 0; k < n; k++)
	if (!FcStrCmpIgnoreBlanksAndCase (e[k].family, e[0].family)) {
	    FcExplainEntry tmp = e[1];

	    e[1] = e[k];
	    e[k] = tmp;
	    own = 1;
	    break;
	}
    qsort (e + 1 + own, n - 1 - own, sizeof (*e), FcExplainEntryCmp);
    nshow = FC_MIN (n, 1 + own + FC_EXPLAIN_RUNNERS_UP);

    printf ("== Candidates: the winner%s the best face of each of the next %d families ==\n",
            own ? ", the next face of its family, and" : " and", nshow - 1 - own);
    /* Say so only when it mattered: some family scored better overall. */
    for (k = 1; famfirst && k < n && FcExplainScoreCmp (e[k].score, e[0].score) >= 0; k++)
	;
    if (famfirst && k < n)
	printf ("No requested family matched, so the family came first: %s (the best face with style ignored), then its best face.\n", famfirst);
    for (k = 0; k < nshow; k++) {
	FcChar8 *s = FcPatternFormat (e[k].font, (const FcChar8 *)"%{=fcmatch}");

	printf ("#%d %s%s\n", k + 1, s ? (const char *)s : "?", k ? "" : "  [winner]");
	if (s)
	    FcStrFree (s);
	lost[k] = -1;
	if (!k)
	    continue;
	for (i = 0; i < PRI_END && e[k].score[i] == e[0].score[i]; i++)
	    ;
	printf ("   lost at ");
	if (i == PRI_END)
	    printf ("no slot: tied on every slot, the font order decided\n");
	else if (e[k].score[i] < e[0].score[i])
	    printf ("family first: its score is better (from %s on), but it is not in the chosen family %s\n",
	            FcExplainSlotName (i), famfirst ? (const char *)famfirst : "?");
	else {
	    lost[k] = i;
	    FcExplainSlot (p, i, e[0].font, e[0].score[i], e[k].font, e[k].score[i]);
	}
    }

    printf ("== Score per slot, in priority order (lower is better; * marks where that candidate lost) ==\n");
    printf ("%-24s", "slot");
    for (k = 0; k < nshow; k++) {
	char col[8];

	snprintf (col, sizeof (col), "#%d", k + 1);
	printf (" %10s ", col);
    }
    printf ("\n");
    for (i = 0; i < PRI_END; i++) {
	for (k = 0; k < nshow && e[k].score[i] == 0; k++)
	    ;
	if (k == nshow)
	    continue;
	printf ("%-24s", FcExplainSlotName (i));
	for (k = 0; k < nshow; k++)
	    printf (" %10g%s", e[k].score[i], lost[k] == i ? "*" : " ");
	printf ("\n");
    }
    printf ("(slots not shown are 0 for every candidate)\n");
    printf ("== Result ==\n");
bail:
    free (e);
}

static FcPattern *
FcFontSetMatchInternal (FcFontSet **sets,
                        int         nsets,
                        FcPattern  *p,
                        FcResult   *result)
{
    double              score[PRI_END], bestscore[PRI_END], famscore[PRI_END];
    int                 f;
    FcFontSet          *s;
    FcPattern          *best, *famBest, *pat = NULL;
    int                 i;
    int                 set;
    FcCompareData       data;
    const FcPatternElt *elt;
    FcChar8            *family, *famfirst = NULL;

    for (i = 0; i < PRI_END; i++)
	bestscore[i] = famscore[i] = 0;
    best = famBest = 0;
    if (FcDebug() & FC_DBG_MATCH) {
	printf ("Match ");
	FcPatternPrint (p);
    }

    if (!FcCompareDataInit (p, &data))
	return 0;

    for (set = 0; set < nsets; set++) {
	s = sets[set];
	if (!s)
	    continue;
	for (f = 0; f < s->nfont; f++) {
	    if (FcDebug() & FC_DBG_MATCHV) {
		printf ("Font %d ", f);
		FcPatternPrint (s->fonts[f]);
	    }
	    if (!FcCompare (p, s->fonts[f], score, result, &data)) {
		FcCompareDataClear (&data);
		return 0;
	    }
	    if (FcDebug() & FC_DBG_MATCHV) {
		printf ("Score");
		for (i = 0; i < PRI_END; i++) {
		    printf (" %g", score[i]);
		}
		printf ("\n");
	    }
	    for (i = 0; i < PRI_END; i++) {
		if (best && bestscore[i] < score[i])
		    break;
		if (!best || score[i] < bestscore[i]) {
		    for (i = 0; i < PRI_END; i++)
			bestscore[i] = score[i];
		    best = s->fonts[f];
		    break;
		}
	    }
	    /* The same comparison with the style slots left out: the face
	     * whose family would win if style did not count. */
	    score[PRI_STYLE] = score[PRI_WIDTH] = score[PRI_SLANT] = score[PRI_WEIGHT] = 0;
	    for (i = 0; i < PRI_END; i++) {
		if (famBest && famscore[i] < score[i])
		    break;
		if (!famBest || score[i] < famscore[i]) {
		    for (i = 0; i < PRI_END; i++)
			famscore[i] = score[i];
		    famBest = s->fonts[f];
		    break;
		}
	    }
	}
    }

    /*
     * When the request named no family the winner has (no family at all,
     * or both family scores are "no match"), nothing above keeps a family together: the style
     * slots would pick the family, e.g. a bold request for a language
     * served by "Foo" (regular only) would land on "Foo Unjoined Bold".
     * CSS Fonts 4 section 5 picks the family first and section 5.2 step 4
     * matches width, style and weight within it.  So pick the family as
     * if style did not count, then the best face of that family.
     */
    if (best && famBest && best != famBest &&
        (!FcPatternObjectFindElt (p, FC_FAMILY_OBJECT) ||
         (bestscore[PRI_FAMILY_STRONG] >= 1e99 && bestscore[PRI_FAMILY_WEAK] >= 1e99)) &&
        FcPatternObjectGetString (famBest, FC_FAMILY_OBJECT, 0, &family) == FcResultMatch) {
	best = 0;
	for (set = 0; set < nsets; set++) {
	    s = sets[set];
	    if (!s)
		continue;
	    for (f = 0; f < s->nfont; f++) {
		FcChar8 *ff;

		if (FcPatternObjectGetString (s->fonts[f], FC_FAMILY_OBJECT, 0, &ff) != FcResultMatch ||
		    FcStrCmpIgnoreBlanksAndCase (ff, family) != 0)
		    continue;
		if (!FcCompare (p, s->fonts[f], score, result, &data)) {
		    FcCompareDataClear (&data);
		    return 0;
		}
		for (i = 0; i < PRI_END; i++) {
		    if (best && bestscore[i] < score[i])
			break;
		    if (!best || score[i] < bestscore[i]) {
			for (i = 0; i < PRI_END; i++)
			    bestscore[i] = score[i];
			best = s->fonts[f];
			break;
		    }
		}
	    }
	}
	if (FcDebug() & FC_DBG_MATCH)
	    printf ("No requested family matched; family taken from %s\n", family);
	famfirst = family;
    }

    if (best && (FcDebug() & FC_DBG_EXPLAIN))
	FcExplainMatch (sets, nsets, p, best, famfirst, &data);
    FcCompareDataClear (&data);

    /* Update the binding according to the score to indicate how exactly values matches on. */
    if (best) {
	pat = FcPatternCreate();
	elt = FcPatternElts (best);
	for (i = 0; i < FcPatternObjectCount (best); i++) {
	    const FcMatcher *match = FcObjectToMatcher (elt[i].object, FcFalse);
	    FcValueListPtr   l = FcPatternEltValues (&elt[i]);

	    if (!match)
		FcPatternObjectListAdd (pat, elt[i].object,
		                        FcValueListDuplicate (l), FcTrue);
	    else {
		FcValueBinding binding = FcValueBindingWeak;
		FcValueListPtr newp = NULL, ll, t = NULL;
		FcValue        v;

		/* If the value was matched exactly, update the binding to Strong. */
		if (bestscore[match->strong] < 1000)
		    binding = FcValueBindingStrong;

		for (ll = l; ll != NULL; ll = FcValueListNext (ll)) {
		    if (!newp) {
			t = newp = FcValueListCreate();
		    } else {
			t->next = FcValueListCreate();
			t = FcValueListNext (t);
		    }
		    v = FcValueCanonicalize (&ll->value);
		    t->value = FcValueSave (v);
		    t->binding = binding;
		    t->next = NULL;
		}
		FcPatternObjectListAdd (pat, elt[i].object, newp, FcTrue);
	    }
	}
    }
    if (FcDebug() & FC_DBG_MATCH) {
	printf ("Best score");
	for (i = 0; i < PRI_END; i++)
	    printf (" %g", bestscore[i]);
	printf ("\n");
	FcPatternPrint (pat);
    }
    if (FcDebug() & FC_DBG_MATCH2) {
	char        *env = getenv ("FC_DBG_MATCH_FILTER");
	FcObjectSet *os = NULL;

	if (env) {
	    char  *ss, *s;
	    char  *p;
	    FcBool f = FcTrue;

	    ss = s = (char *)FcStrCopy ((const FcChar8 *)env);
	    os = FcObjectSetCreate();
	    while (f) {
		size_t len;
		char  *x;

		if (!(p = strchr (s, ','))) {
		    f = FcFalse;
		    len = strlen (s);
		} else {
		    len = (p - s);
		}
		x = malloc (sizeof (char) * (len + 1));
		if (x) {
		    strcpy (x, s);
		    if (FcObjectFromName (x) > 0)
			FcObjectSetAdd (os, x);
		    s = p + 1;
		    free (x);
		}
	    }
	    free (ss);
	}
	FcPatternPrint2 (p, pat, os);
	if (os)
	    FcObjectSetDestroy (os);
    }
    /* assuming that 'result' is initialized with FcResultNoMatch
     * outside this function */
    if (pat)
	*result = FcResultMatch;

    return pat;
}

FcPattern *
FcFontSetMatch (FcConfig   *config,
                FcFontSet **sets,
                int         nsets,
                FcPattern  *p,
                FcResult   *result)
{
    FcPattern *best, *ret = NULL;

    assert (sets != NULL);
    assert (p != NULL);
    assert (result != NULL);

    *result = FcResultNoMatch;

    config = FcConfigReference (config);
    if (!config)
	return NULL;
    best = FcFontSetMatchInternal (sets, nsets, p, result);
    if (best) {
	ret = FcFontRenderPrepare (config, p, best);
	FcPatternDestroy (best);
    }

    FcConfigDestroy (config);

    return ret;
}

FcPattern *
FcFontMatch (FcConfig  *config,
             FcPattern *p,
             FcResult  *result)
{
    FcFontSet *sets[2];
    int        nsets;
    FcPattern *best, *ret = NULL;

    assert (p != NULL);
    assert (result != NULL);

    *result = FcResultNoMatch;

    config = FcConfigReference (config);
    if (!config)
	return NULL;
    FcRwLockReadLock (&config->fonts_lock);
    nsets = 0;
    if (config->fonts[FcSetSystem])
	sets[nsets++] = config->fonts[FcSetSystem];
    if (config->fonts[FcSetApplication])
	sets[nsets++] = config->fonts[FcSetApplication];

    best = FcFontSetMatchInternal (sets, nsets, p, result);
    FcRwLockUnlockRead (&config->fonts_lock);
    if (best) {
	ret = FcFontRenderPrepare (config, p, best);
	FcPatternDestroy (best);
    }

    FcConfigDestroy (config);

    return ret;
}

typedef struct _FcSortNode {
    FcPattern *pattern;
    double     score[PRI_END];
} FcSortNode;

static int
FcSortCompare (const void *aa, const void *ab)
{
    FcSortNode *a = *(FcSortNode **)aa;
    FcSortNode *b = *(FcSortNode **)ab;
    double     *as = &a->score[0];
    double     *bs = &b->score[0];
    double      ad = 0, bd = 0;
    int         i;

    i = PRI_END;
    while (i-- && (ad = *as++) == (bd = *bs++))
	;
    return ad < bd ? -1 : ad > bd ? 1
                                  : 0;
}

static FcBool
FcSortWalk (FcSortNode **n, int nnode, FcFontSet *fs, FcCharSet **csp, FcBool trim)
{
    FcBool     ret = FcFalse;
    FcCharSet *cs;
    int        i;

    cs = 0;
    if (trim || csp) {
	cs = FcCharSetCreate();
	if (cs == NULL)
	    goto bail;
    }

    for (i = 0; i < nnode; i++) {
	FcSortNode *node = *n++;
	FcBool      adds_chars = FcFalse;

	/*
	 * Only fetch node charset if we'd need it
	 */
	if (cs) {
	    FcCharSet *ncs;

	    if (FcPatternGetCharSet (node->pattern, FC_CHARSET, 0, &ncs) !=
	        FcResultMatch)
		continue;

	    if (!FcCharSetMerge (cs, ncs, &adds_chars))
		goto bail;
	}

	/*
	 * If this font isn't a subset of the previous fonts,
	 * add it to the list
	 */
	if (!i || !trim || adds_chars) {
	    FcPatternReference (node->pattern);
	    if (FcDebug() & FC_DBG_MATCHV) {
		printf ("Add ");
		FcPatternPrint (node->pattern);
	    }
	    if (!FcFontSetAdd (fs, node->pattern)) {
		FcPatternDestroy (node->pattern);
		goto bail;
	    }
	}
    }
    if (csp) {
	*csp = cs;
	cs = 0;
    }

    ret = FcTrue;

bail:
    if (cs)
	FcCharSetDestroy (cs);

    return ret;
}

void
FcFontSetSortDestroy (FcFontSet *fs)
{
    FcFontSetDestroy (fs);
}

FcFontSet *
FcFontSetSort (FcConfig   *config,
               FcFontSet **sets,
               int         nsets,
               FcPattern  *p,
               FcBool      trim,
               FcCharSet **csp,
               FcResult   *result)
{
    FcFontSet    *ret;
    FcFontSet    *s;
    FcSortNode   *nodes;
    FcSortNode  **nodeps, **nodep;
    int           nnodes;
    FcSortNode   *newp;
    int           set;
    int           f;
    int           i;
    int           nPatternLang;
    FcBool       *patternLangSat;
    FcValue       patternLang;
    FcCompareData data;

    assert (sets != NULL);
    assert (p != NULL);
    assert (result != NULL);

    /* There are some implementation that relying on the result of
     * "result" to check if the return value of FcFontSetSort
     * is valid or not.
     * So we should initialize it to the conservative way since
     * this function doesn't return NULL anymore.
     */
    if (result)
	*result = FcResultNoMatch;

    if (FcDebug() & FC_DBG_MATCH) {
	printf ("Sort ");
	FcPatternPrint (p);
    }
    nnodes = 0;
    for (set = 0; set < nsets; set++) {
	s = sets[set];
	if (!s)
	    continue;
	nnodes += s->nfont;
    }
    if (!nnodes)
	return FcFontSetCreate();

    if (!config)
	config = FcConfigGetCurrent();
    FcConfigReference (config);

    for (nPatternLang = 0;
         FcPatternGet (p, FC_LANG, nPatternLang, &patternLang) == FcResultMatch;
         nPatternLang++)
	;

    /* freed below */
    nodes = malloc (nnodes * sizeof (FcSortNode) +
                    nnodes * sizeof (FcSortNode *) +
                    nPatternLang * sizeof (FcBool));
    if (!nodes)
	goto bail0;
    nodeps = (FcSortNode **)(nodes + nnodes);
    patternLangSat = (FcBool *)(nodeps + nnodes);

    if (!FcCompareDataInit (p, &data))
	goto bail1;

    newp = nodes;
    nodep = nodeps;
    for (set = 0; set < nsets; set++) {
	s = sets[set];
	if (!s)
	    continue;
	for (f = 0; f < s->nfont; f++) {
	    if (FcDebug() & FC_DBG_MATCHV) {
		printf ("Font %d ", f);
		FcPatternPrint (s->fonts[f]);
	    }
	    newp->pattern = s->fonts[f];
	    if (!FcCompare (p, newp->pattern, newp->score, result, &data))
		goto bail1;
	    /* TODO: Should we check a FcPattern in FcFontSet?
	     * This way may not work if someone has own list of application fonts
	     * That said, just to reduce the cost for lookup so far.
	     */
	    if (config->prefer_app_fonts && s != config->fonts[FcSetApplication]) {
		newp->score[PRI_ORDER] += 1000;
	    }
	    if (FcDebug() & FC_DBG_MATCHV) {
		printf ("Score");
		for (i = 0; i < PRI_END; i++) {
		    printf (" %g", newp->score[i]);
		}
		printf ("\n");
	    }
	    *nodep = newp;
	    newp++;
	    nodep++;
	}
    }

    FcCompareDataClear (&data);

    nnodes = newp - nodes;

    qsort (nodeps, nnodes, sizeof (FcSortNode *),
           FcSortCompare);

    for (i = 0; i < nPatternLang; i++)
	patternLangSat[i] = FcFalse;

    for (f = 0; f < nnodes; f++) {
	FcBool satisfies = FcFalse;
	/*
	 * If this node matches any language, go check
	 * which ones and satisfy those entries
	 */
	if (nodeps[f]->score[PRI_LANG] < 2000) {
	    for (i = 0; i < nPatternLang; i++) {
		FcValue nodeLang;

		if (!patternLangSat[i] &&
		    FcPatternGet (p, FC_LANG, i, &patternLang) == FcResultMatch &&
		    FcPatternGet (nodeps[f]->pattern, FC_LANG, 0, &nodeLang) == FcResultMatch) {
		    FcValue matchValue;
		    double  compare = FcCompareLang (&patternLang, &nodeLang, &matchValue);
		    if (compare >= 0 && compare < 2) {
			if (FcDebug() & FC_DBG_MATCHV) {
			    FcChar8 *family;
			    FcChar8 *style;

			    if (FcPatternGetString (nodeps[f]->pattern, FC_FAMILY, 0, &family) == FcResultMatch &&
			        FcPatternGetString (nodeps[f]->pattern, FC_STYLE, 0, &style) == FcResultMatch)
				printf ("Font %s:%s matches language %d\n", family, style, i);
			}
			patternLangSat[i] = FcTrue;
			satisfies = FcTrue;
			break;
		    }
		}
	    }
	}
	if (!satisfies) {
	    nodeps[f]->score[PRI_LANG] = 10000.0;
	}
    }

    /*
     * Re-sort once the language issues have been settled
     */
    qsort (nodeps, nnodes, sizeof (FcSortNode *),
           FcSortCompare);

    ret = FcFontSetCreate();
    if (!ret)
	goto bail1;

    if (!FcSortWalk (nodeps, nnodes, ret, csp, trim))
	goto bail2;

    free (nodes);

    if (ret->nfont > 0) {
	*result = FcResultMatch;
	if (FcDebug() & FC_DBG_MATCH) {
	    printf ("First font ");
	    FcPatternPrint (ret->fonts[0]);
	}
    }
    if (config)
	FcConfigDestroy (config);

    return ret;

bail2:
    FcFontSetDestroy (ret);
bail1:
    free (nodes);
bail0:
    if (config)
	FcConfigDestroy (config);
    return 0;
}

FcFontSet *
FcFontSort (FcConfig   *config,
            FcPattern  *p,
            FcBool      trim,
            FcCharSet **csp,
            FcResult   *result)
{
    FcFontSet *sets[2], *ret;
    int        nsets;

    assert (p != NULL);
    assert (result != NULL);

    *result = FcResultNoMatch;

    config = FcConfigReference (config);
    if (!config)
	return NULL;
    FcRwLockReadLock (&config->fonts_lock);
    nsets = 0;
    if (config->fonts[FcSetSystem])
	sets[nsets++] = config->fonts[FcSetSystem];
    if (config->fonts[FcSetApplication])
	sets[nsets++] = config->fonts[FcSetApplication];
    ret = FcFontSetSort (config, sets, nsets, p, trim, csp, result);
    FcRwLockUnlockRead (&config->fonts_lock);
    FcConfigDestroy (config);

    return ret;
}
#define __fcmatch__
#include "fcaliastail.h"
#undef __fcmatch__
