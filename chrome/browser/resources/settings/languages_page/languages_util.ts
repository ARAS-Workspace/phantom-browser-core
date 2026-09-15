// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Just the languages code that translate uses but Chrome has a different
// code for.
const kTranslateToChromeCode: Map<string, string> = new Map([
  ['gom', 'kok'],
  ['iw', 'he'],
  ['jw', 'jv'],
  ['no', 'nb'],
  ['tl', 'fil'],
]);

/**
 * Given a language code, returns just the base language without sub-codes. For
 * example, converts 'en-GB' to 'en'.
 */
export function getBaseLanguage(languageCode: string): string {
  return languageCode.split('-')[0];
}

/**
 * Converts deprecated ISO 639 language codes to Chrome format.
 */
export function convertLanguageCodeForChrome(languageCode: string): string {
  return kTranslateToChromeCode.get(languageCode) || languageCode;
}

/**
 * @return the [displayName] - [nativeDisplayName] if displayName and
 *     nativeDisplayName are different. If they're the same than only returns
 *     the displayName.
 */
export function getFullName(language: chrome.languageSettingsPrivate.Language):
    string {
  let fullName = language.displayName;
  if (language.displayName !== language.nativeDisplayName) {
    fullName += ' - ' + language.nativeDisplayName;
  }
  return fullName;
}
