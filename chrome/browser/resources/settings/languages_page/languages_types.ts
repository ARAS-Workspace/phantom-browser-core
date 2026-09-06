// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for dictionaries and interfaces used by
 * language settings.
 */

/**
 * Settings and state for a particular enabled language.
 */
export interface LanguageState {
  language: chrome.languageSettingsPrivate.Language;
  removable: boolean;
  translateEnabled: boolean;
  isManaged: boolean;
  isForced: boolean;
}

/**
 * Languages data to expose to consumers.
 * supported: an array of languages, ordered alphabetically, set once
 *     at initialization.
 * enabled: an array of enabled language states, ordered by preference.
 * translateTarget: the default language to translate into.
 * prospectiveUILanguage: the "prospective" UI language, i.e., the one to be
 *     used on next restart. Matches the current UI language preference unless
 *     the user has chosen a different language without restarting. May differ
 *     from the actually used language (navigator.language). Chrome OS and
 *     Windows only.
 */
export interface LanguagesModel {
  supported: chrome.languageSettingsPrivate.Language[];
  enabled: LanguageState[];
  translateTarget: string;
  alwaysTranslate: chrome.languageSettingsPrivate.Language[];
  neverTranslate: chrome.languageSettingsPrivate.Language[];
  neverTranslateSites: string[];
  // TODO(dpapad): Wrap prospectiveUILanguage with if expr "is_win" block.
  prospectiveUILanguage?: string;
}

/**
 * Helper methods for reading and writing language settings.
 */
export interface LanguageHelper {
  languages?: LanguagesModel|undefined;

  whenReady(): Promise<void>;

  // <if expr="is_win">
  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   */
  setProspectiveUiLanguage(languageCode: string): void;

  /**
   * True if the prospective UI language has been changed.
   */
  requiresRestart(): boolean;

  // </if>

  isLanguageEnabled(languageCode: string): boolean;

  /**
   * Enables the language, making it available for input.
   */
  enableLanguage(languageCode: string): void;

  disableLanguage(languageCode: string): void;

  /**
   * Returns true iff provided languageState can be disabled.
   */
  canDisableLanguage(languageState: LanguageState): boolean;

  canEnableLanguage(language: chrome.languageSettingsPrivate.Language): boolean;

  /**
   * Moves the language in the list of enabled languages by the given offset.
   * @param upDirection True if we need to move toward the front, false if we
   *     need to move toward the back.
   */
  moveLanguage(languageCode: string, upDirection: boolean): void;

  /**
   * Moves the language directly to the front of the list of enabled languages.
   */
  moveLanguageToFront(languageCode: string): void;

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   */
  enableTranslateLanguage(languageCode: string): void;

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   */
  disableTranslateLanguage(languageCode: string): void;

  /**
   * Sets the translate target language.
   */
  setTranslateTargetLanguage(languageCode: string): void;

  /**
   * Sets whether a given language should always be automatically translated.
   */
  setLanguageAlwaysTranslateState(
      languageCode: string, alwaysTranslate: boolean): void;


  getLanguage(languageCode: string): chrome.languageSettingsPrivate.Language
      |undefined;
}
