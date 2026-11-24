/*
 * fontconfig/fc-fontations/opentype_langs.rs
 *
 * Copyright 2026 fontconfig Authors
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

//! PANOSE, the 'meta' design and supported languages, and the languages
//! whose GSUB/GPOS language system carries 'locl'.  Mirrors fcfreetype.c.

use font_types::Tag;
use read_fonts::tables::layout::{FeatureList, ScriptList};
use read_fonts::TableProvider;
use skrifa::FontRef;
use std::ffi::{CStr, CString};

use crate::pattern_bindings::{FcPatternBuilder, PatternElement};
use fcint_bindings::{
    FcLangFromOpenTypeTag, FC_DESIGN_LANG_OBJECT, FC_LOCL_LANG_OBJECT, FC_PANOSE_OBJECT,
    FC_SUPPORTED_LANG_OBJECT,
};

const MAX_LOCL_TAGS: usize = 256;

fn panose_string(panose: &[u8]) -> Option<CString> {
    // An all-zero PANOSE means "any" in every digit: no classification.
    if panose.len() != 10 || panose.iter().all(|&b| b == 0) {
        return None;
    }
    let s = panose
        .iter()
        .map(|b| b.to_string())
        .collect::<Vec<_>>()
        .join(" ");
    CString::new(s).ok()
}

/// Split a comma-separated ScriptLangTag list, dropping empty and
/// malformed entries.
fn script_lang_tags(data: &[u8]) -> Vec<CString> {
    data.split(|&c| c == b',' || c == b' ')
        .filter(|tag| {
            !tag.is_empty()
                && tag.len() < 64
                && tag.iter().all(|c| c.is_ascii_alphanumeric() || *c == b'-')
        })
        .filter_map(|tag| CString::new(tag).ok())
        .collect()
}

fn be_u32(data: &[u8], at: usize) -> Option<u32> {
    Some(u32::from_be_bytes(data.get(at..at + 4)?.try_into().ok()?))
}

fn meta_langs(meta: &[u8]) -> (Vec<CString>, Vec<CString>) {
    let mut design = Vec::new();
    let mut supported = Vec::new();
    if meta.len() < 16 || be_u32(meta, 0) != Some(1) {
        return (design, supported);
    }
    let count = be_u32(meta, 12).unwrap_or(0) as usize;
    if count > (meta.len() - 16) / 12 {
        return (design, supported);
    }
    for i in 0..count {
        let map = 16 + 12 * i;
        let (Some(tag), Some(off), Some(size)) = (
            be_u32(meta, map),
            be_u32(meta, map + 4),
            be_u32(meta, map + 8),
        ) else {
            continue;
        };
        let Some(data) = meta.get(off as usize..(off as usize).saturating_add(size as usize))
        else {
            continue;
        };
        match &tag.to_be_bytes() {
            b"dlng" => design.extend(script_lang_tags(data)),
            b"slng" => supported.extend(script_lang_tags(data)),
            _ => {}
        }
    }
    (design, supported)
}

/// Append to `tags` the language systems (not the default one) whose
/// required or listed features include 'locl'.
fn collect_locl_lang_sys(
    script_list: Option<ScriptList>,
    feature_list: Option<FeatureList>,
    tags: &mut Vec<Tag>,
) {
    let (Some(script_list), Some(feature_list)) = (script_list, feature_list) else {
        return;
    };
    let features = feature_list.feature_records();
    let is_locl = |idx: u16| {
        features
            .get(idx as usize)
            .is_some_and(|f| f.feature_tag() == Tag::new(b"locl"))
    };
    for script_record in script_list.script_records() {
        let Ok(script) = script_record.script(script_list.offset_data()) else {
            continue;
        };
        for lang_sys_record in script.lang_sys_records() {
            let Ok(lang_sys) = lang_sys_record.lang_sys(script.offset_data()) else {
                continue;
            };
            let has_locl = is_locl(lang_sys.required_feature_index())
                || lang_sys.feature_indices().iter().any(|i| is_locl(i.get()));
            let tag = lang_sys_record.lang_sys_tag();
            if has_locl && !tags.contains(&tag) && tags.len() < MAX_LOCL_TAGS {
                tags.push(tag);
            }
        }
    }
}

fn locl_langs(font: &FontRef) -> Vec<CString> {
    let mut tags = Vec::new();
    if let Ok(gsub) = font.gsub() {
        collect_locl_lang_sys(gsub.script_list().ok(), gsub.feature_list().ok(), &mut tags);
    }
    if let Ok(gpos) = font.gpos() {
        collect_locl_lang_sys(gpos.script_list().ok(), gpos.feature_list().ok(), &mut tags);
    }
    tags.sort();

    let mut langs: Vec<CString> = Vec::new();
    for tag in tags {
        let lang = unsafe { FcLangFromOpenTypeTag(u32::from_be_bytes(tag.to_be_bytes())) };
        if lang.is_null() {
            continue; // not a registered language system tag
        }
        let lang = unsafe { CStr::from_ptr(lang as *const libc::c_char) }.to_owned();
        if !langs.contains(&lang) {
            langs.push(lang);
        }
    }
    langs
}

pub fn add_opentype_langs(font: &FontRef, pattern: &mut FcPatternBuilder) {
    if let Some(panose) = font
        .os2()
        .ok()
        .and_then(|os2| panose_string(os2.panose_10()))
    {
        pattern.append_element(PatternElement::new(FC_PANOSE_OBJECT as i32, panose.into()));
    }

    if let Some(meta) = font.table_data(Tag::new(b"meta")) {
        let (design, supported) = meta_langs(meta.as_bytes());
        for tag in design {
            pattern.append_element(PatternElement::new(
                FC_DESIGN_LANG_OBJECT as i32,
                tag.into(),
            ));
        }
        for tag in supported {
            pattern.append_element(PatternElement::new(
                FC_SUPPORTED_LANG_OBJECT as i32,
                tag.into(),
            ));
        }
    }

    for lang in locl_langs(font) {
        pattern.append_element(PatternElement::new(FC_LOCL_LANG_OBJECT as i32, lang.into()));
    }
}

#[cfg(test)]
mod test {
    use super::{meta_langs, panose_string, script_lang_tags};

    #[test]
    fn panose() {
        assert_eq!(None, panose_string(&[0; 10]));
        assert_eq!(
            "2 11 5 2 4 5 4 2 2 4",
            panose_string(&[2, 11, 5, 2, 4, 5, 4, 2, 2, 4])
                .unwrap()
                .to_str()
                .unwrap()
        );
    }

    #[test]
    fn script_lang_tag_list() {
        let tags = script_lang_tags(b"en,ja-Jpan , zh-Hant,,x!y");
        let tags: Vec<_> = tags.iter().map(|t| t.to_str().unwrap()).collect();
        assert_eq!(vec!["en", "ja-Jpan", "zh-Hant"], tags);
    }

    #[test]
    fn truncated_meta() {
        let (d, s) = meta_langs(&[0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9]);
        assert!(d.is_empty() && s.is_empty());
    }
}
