// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/test/font_fallback_test_data.h"

#include <string>

#include "build/build_config.h"

namespace gfx {

FallbackFontTestCase::FallbackFontTestCase() = default;
FallbackFontTestCase::FallbackFontTestCase(const FallbackFontTestCase& other) =
    default;

FallbackFontTestCase::FallbackFontTestCase(
    UScriptCode script_arg,
    std::string language_tag_arg,
    std::u16string text_arg,
    std::vector<std::string> fallback_fonts_arg)
    : script(script_arg),
      language_tag(language_tag_arg),
      text(text_arg),
      fallback_fonts(fallback_fonts_arg) {}

FallbackFontTestCase::~FallbackFontTestCase() = default;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// A list of script and the fallback font on the linux test environment.
// On linux, font-config configuration and fonts are mock. The config
// can be found in '${build}/etc/fonts/fonts.conf' and the test fonts
// can be found in '${build}/test_fonts/*'.
const std::vector<FallbackFontTestCase> kGetFontFallbackTests = {
    {USCRIPT_BENGALI, "bn", u"\u09B8\u09AE", {"Mukti Narrow"}},
    {USCRIPT_DEVANAGARI, "hi", u"\u0905\u0906", {"Lohit Devanagari"}},
    {USCRIPT_GURMUKHI, "pa", u"\u0A21\u0A22", {"Lohit Gurmukhi"}},
    {USCRIPT_HAN, "zh-CN", u"\u6211", {"Noto Sans CJK JP"}},
    {USCRIPT_KHMER, "km", u"\u1780\u1781", {"Noto Sans Khmer"}},
    {USCRIPT_TAMIL, "ta", u"\u0BB1\u0BB2", {"Lohit Tamil"}},
    {USCRIPT_THAI, "th", u"\u0e01\u0e02", {"Garuda"}},
};

#else

// No fallback font tests are defined on that platform.
const std::vector<FallbackFontTestCase> kGetFontFallbackTests = {};

#endif

}  // namespace gfx
