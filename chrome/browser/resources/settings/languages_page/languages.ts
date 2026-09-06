// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-languages' handles Chrome's language and input
 * method settings. The 'languages' property, which reflects the current
 * language settings, must not be changed directly. Instead, changes to
 * language settings should be made using the LanguageHelper APIs provided by
 * this class via the LanguageHelper singleton instance.
 */

import {assert} from '//resources/js/assert.js';
import {PromiseResolver} from '//resources/js/promise_resolver.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixin} from '/shared/settings/prefs2/pref_service_observer_mixin.js';

import type {LanguagesBrowserProxy} from './languages_browser_proxy.js';
import {LanguagesBrowserProxyImpl} from './languages_browser_proxy.js';
import type {LanguageHelper, LanguagesModel, LanguageState} from './languages_types.js';
import {convertLanguageCodeForChrome, convertLanguageCodeForTranslate, getBaseLanguage} from './languages_util.js';

const MoveType = chrome.languageSettingsPrivate.MoveType;

// The fake language name used for ARC IMEs. The value must be in sync with the
// one in ui/base/ime/ash/extension_ime_util.h.
const kArcImeLanguage: string = '_arc_ime_language_';

interface ModelArgs {
  supportedLanguages: chrome.languageSettingsPrivate.Language[];
  translateTarget: string;
  alwaysTranslateCodes: string[];
  neverTranslateCodes: string[];
  neverTranslateSites: string[];
  startingUILanguage: string;
  supportedInputMethods?: chrome.languageSettingsPrivate.InputMethod[];
  currentInputMethodId?: string;
}


let instance: LanguageHelper|null = null;

export function getLanguageHelperInstance(): LanguageHelper {
  assert(instance);
  return instance;
}

/**
 * Singleton element that generates the languages model on start-up and
 * updates it whenever Chrome's pref store and other settings change.
 */

const SettingsLanguagesElementBase = PrefServiceObserverMixin(PolymerElement);

class SettingsLanguagesElement extends SettingsLanguagesElementBase implements
    LanguageHelper {
  static get is() {
    return 'settings-languages';
  }

  static get properties() {
    return {
      languages: {
        type: Object,
        notify: true,
      },

      intlAcceptLanguagesPref_: Object,
      intlAppLocalePref_: Object,
      intlForcedLanguagesPref_: Object,
      translateAllowlistsPref_: Object,
      translateBlockedLanguagesPref_: Object,
      translateRecentTargetPref_: Object,
      translateSiteBlocklistPref_: Object,
    };
  }

  static get observers() {
    return [
      'alwaysTranslateLanguagesPrefChanged_(' +
          'translateAllowlistsPref_, languages)',
      'neverTranslateLanguagesPrefChanged_(' +
          'translateBlockedLanguagesPref_, languages)',
      'neverTranslateSitesPrefChanged_(translateSiteBlocklistPref_, languages)',
      // <if expr="is_win">
      'prospectiveUiLanguageChanged_(intlAppLocalePref_, languages)',
      // </if>
      'preferredLanguagesPrefChanged_(' +
          'intlAcceptLanguagesPref_, intlForcedLanguagesPref_, languages)',
      'translateLanguagesPrefChanged_(' +
          'translateBlockedLanguagesPref_, languages)',
      'translateTargetPrefChanged_(translateRecentTargetPref_, languages)',
      'updateRemovableLanguages_(' +
          'intlAppLocalePref_, translateBlockedLanguagesPref_, ' +
          'languages.enabled)',
    ];
  }

  declare languages: LanguagesModel|undefined;

  declare private intlAcceptLanguagesPref_:
      chrome.settingsPrivate.PrefObject<string>|undefined;
  declare private intlAppLocalePref_: chrome.settingsPrivate.PrefObject<string>|
      undefined;
  declare private intlForcedLanguagesPref_:
      chrome.settingsPrivate.PrefObject<string[]>|undefined;
  declare private translateAllowlistsPref_:
      chrome.settingsPrivate.PrefObject<Record<string, string>>|undefined;
  declare private translateBlockedLanguagesPref_:
      chrome.settingsPrivate.PrefObject<string[]>|undefined;
  declare private translateRecentTargetPref_:
      chrome.settingsPrivate.PrefObject<string>|undefined;
  declare private translateSiteBlocklistPref_:
      chrome.settingsPrivate.PrefObject<Record<string, string>>|undefined;

  private resolver_: PromiseResolver<void> = new PromiseResolver();
  private supportedLanguageMap_:
      Map<string, chrome.languageSettingsPrivate.Language> = new Map();
  private enabledLanguageSet_: Set<string> = new Set();

  // <if expr="is_win">
  /** Prospective UI language when the page was loaded. */
  private originalProspectiveUILanguage_: string;
  // </if>

  private browserProxy_: LanguagesBrowserProxy =
      LanguagesBrowserProxyImpl.getInstance();
  private languageSettingsPrivate_: typeof chrome.languageSettingsPrivate;

  constructor() {
    super();

    this.languageSettingsPrivate_ =
        this.browserProxy_.getLanguageSettingsPrivate();
  }

  override connectedCallback() {
    super.connectedCallback();

    assert(!instance);
    instance = this;

    this.mirrorPrefs({
      'intl.accept_languages': 'intlAcceptLanguagesPref_',
      'intl.app_locale': 'intlAppLocalePref_',
      'intl.forced_languages': 'intlForcedLanguagesPref_',
      'translate_allowlists': 'translateAllowlistsPref_',
      'translate_blocked_languages': 'translateBlockedLanguagesPref_',
      'translate_recent_target': 'translateRecentTargetPref_',
      'translate_site_blocklist_with_time': 'translateSiteBlocklistPref_',
    });

    const promises: Array<Promise<void>> = [];

    /**
     * An object passed into createModel to keep track of platform-specific
     * arguments, populated by the "promises" array.
     */
    const args: ModelArgs = {
      supportedLanguages: [],
      translateTarget: '',
      alwaysTranslateCodes: [],
      neverTranslateCodes: [],
      neverTranslateSites: [],
      startingUILanguage: '',

      // Only used by ChromeOS
      supportedInputMethods: [],
      currentInputMethodId: '',
    };

    // Wait until prefs are initialized before creating the model, so we can
    // include information about enabled languages.
    promises.push(PrefService.getInstance().whenInitialized());

    // Get the language list.
    promises.push(
        this.languageSettingsPrivate_.getLanguageList().then(result => {
          args.supportedLanguages = result;
        }));

    // Get the translate target language.
    promises.push(
        this.languageSettingsPrivate_.getTranslateTargetLanguage().then(
            result => {
              args.translateTarget = result;
            }));

    // Get the list of language-codes to always translate.
    promises.push(
        this.languageSettingsPrivate_.getAlwaysTranslateLanguages().then(
            result => {
              args.alwaysTranslateCodes = result;
            }));

    // Get the list of language-codes to never translate.
    promises.push(
        this.languageSettingsPrivate_.getNeverTranslateLanguages().then(
            result => {
              args.neverTranslateCodes = result;
            }));

    // <if expr="is_win">
    // Fetch the starting UI language, which affects which actions should be
    // enabled.
    promises.push(this.browserProxy_.getProspectiveUiLanguage().then(
        prospectiveUILanguage => {
          this.originalProspectiveUILanguage_ =
              prospectiveUILanguage || window.navigator.language;
        }));
    // </if>

    Promise.all(promises).then(() => {
      if (!this.isConnected) {
        // Return early if this element was detached from the DOM before
        // this async callback executes (can happen during testing).
        return;
      }

      this.createModel_(args);

      this.resolver_.resolve();
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    instance = null;
    this.resolver_ = new PromiseResolver();
  }

  // <if expr="is_win">
  /**
   * Updates the prospective UI language based on the new pref value.
   */
  private prospectiveUiLanguageChanged_() {
    if (this.intlAppLocalePref_ === undefined || this.languages === undefined) {
      return;
    }
    this.set(
        'languages.prospectiveUILanguage',
        this.intlAppLocalePref_.value || this.originalProspectiveUILanguage_);
  }
  // </if>

  /**
   * Updates the list of enabled languages from the preferred languages pref.
   */
  private preferredLanguagesPrefChanged_() {
    if (this.intlAcceptLanguagesPref_ === undefined ||
        this.intlForcedLanguagesPref_ === undefined ||
        this.languages === undefined) {
      return;
    }

    const enabledLanguageStates = this.getEnabledLanguageStates_(
        this.languages.translateTarget, this.languages.prospectiveUILanguage);

    // Recreate the enabled language set before updating languages.enabled.
    this.enabledLanguageSet_.clear();
    for (let i = 0; i < enabledLanguageStates.length; i++) {
      this.enabledLanguageSet_.add(enabledLanguageStates[i].language.code);
    }

    this.set('languages.enabled', enabledLanguageStates);

    // Update translate target language.
    this.languageSettingsPrivate_.getTranslateTargetLanguage().then(result => {
      this.set('languages.translateTarget', result);
    });
  }

  /**
   * Updates the list of always translate languages from translate prefs.
   */
  private alwaysTranslateLanguagesPrefChanged_() {
    if (this.translateAllowlistsPref_ === undefined ||
        this.languages === undefined) {
      return;
    }
    const alwaysTranslateCodes =
        Object.keys(this.translateAllowlistsPref_.value);
    const alwaysTranslateLanguages =
        alwaysTranslateCodes.map((code: string) => this.getLanguage(code));
    this.set('languages.alwaysTranslate', alwaysTranslateLanguages);
  }

  /**
   * Updates the list of never translate languages from translate prefs.
   */
  private neverTranslateLanguagesPrefChanged_() {
    if (this.translateBlockedLanguagesPref_ === undefined ||
        this.languages === undefined) {
      return;
    }
    const neverTranslateCodes = this.translateBlockedLanguagesPref_.value;
    const neverTranslateLanguages =
        neverTranslateCodes.map(code => this.getLanguage(code));
    this.set('languages.neverTranslate', neverTranslateLanguages);
  }

  /**
   * Updates the list of never translate sites from translate prefs.
   */
  private neverTranslateSitesPrefChanged_() {
    if (this.translateSiteBlocklistPref_ === undefined ||
        this.languages === undefined) {
      return;
    }
    const neverTranslateSites =
        Object.keys(this.translateSiteBlocklistPref_.value);
    this.set('languages.neverTranslateSites', neverTranslateSites);
  }

  private translateLanguagesPrefChanged_() {
    if (this.translateBlockedLanguagesPref_ === undefined ||
        this.languages === undefined) {
      return;
    }

    const translateBlockedPrefValue = this.translateBlockedLanguagesPref_.value;
    const translateBlockedSet =
        this.makeSetFromArray_(translateBlockedPrefValue);

    for (let i = 0; i < this.languages.enabled.length; i++) {
      const language = this.languages.enabled[i].language;
      const translateEnabled = this.isTranslateEnabled_(
          language.code, !!language.supportsTranslate, translateBlockedSet,
          this.languages.translateTarget, this.languages.prospectiveUILanguage);
      this.set(
          'languages.enabled.' + i + '.translateEnabled', translateEnabled);
    }
  }

  private translateTargetPrefChanged_() {
    if (this.translateRecentTargetPref_ === undefined ||
        this.languages === undefined) {
      return;
    }
    this.set(
        'languages.translateTarget', this.translateRecentTargetPref_.value);
  }

  /**
   * Constructs the languages model.
   * @param args used to populate the model above.
   */
  private createModel_(args: ModelArgs) {
    // Populate the hash map of supported languages.
    for (let i = 0; i < args.supportedLanguages.length; i++) {
      const language = args.supportedLanguages[i];
      language.supportsUI = !!language.supportsUI;
      language.supportsTranslate = !!language.supportsTranslate;
      language.supportsSpellcheck = !!language.supportsSpellcheck;
      language.isProhibitedLanguage = !!language.isProhibitedLanguage;
      this.supportedLanguageMap_.set(language.code, language);
    }

    let prospectiveUILanguage;
    // <if expr="is_win">
    // eslint-disable-next-line prefer-const
    prospectiveUILanguage =
        this.intlAppLocalePref_?.value || this.originalProspectiveUILanguage_;
    // </if>

    // Create a list of enabled languages from the supported languages.
    const enabledLanguageStates = this.getEnabledLanguageStates_(
        args.translateTarget, prospectiveUILanguage);
    // Populate the hash set of enabled languages.
    for (let l = 0; l < enabledLanguageStates.length; l++) {
      this.enabledLanguageSet_.add(enabledLanguageStates[l].language.code);
    }

    const alwaysTranslateLanguages =
        args.alwaysTranslateCodes.map(code => this.getLanguage(code)!);

    const neverTranslateLanguages =
        args.neverTranslateCodes.map(code => this.getLanguage(code)!);

    const model = {
      supported: args.supportedLanguages,
      enabled: enabledLanguageStates,
      translateTarget: args.translateTarget,
      alwaysTranslate: alwaysTranslateLanguages,
      neverTranslate: neverTranslateLanguages,
      neverTranslateSites: args.neverTranslateSites,
      // <if expr="is_win">
      prospectiveUILanguage: prospectiveUILanguage,
      // </if>
    };

    // Initialize the Polymer languages model.
    this.languages = model;
  }

  /**
   * Returns a list of LanguageStates for each enabled language in the supported
   * languages list.
   * @param translateTarget Language code of the default translate
   *     target language.
   * @param prospectiveUILanguage Prospective UI display language. Only defined
   *     on Windows and Chrome OS.
   */
  private getEnabledLanguageStates_(
      translateTarget: string,
      prospectiveUILanguage: string|undefined): LanguageState[] {
    const enabledLanguageCodes =
        (this.intlAcceptLanguagesPref_?.value || '').split(',');
    const languageForcedSet =
        this.makeSetFromArray_(this.intlForcedLanguagesPref_?.value || []);
    const translateBlockedPrefValue =
        this.translateBlockedLanguagesPref_?.value || [];
    const translateBlockedSet =
        this.makeSetFromArray_(translateBlockedPrefValue);

    const enabledLanguageStates: LanguageState[] = [];

    for (let i = 0; i < enabledLanguageCodes.length; i++) {
      const code = enabledLanguageCodes[i];
      const language = this.supportedLanguageMap_.get(code);
      // Skip unsupported languages.
      if (!language) {
        continue;
      }
      const languageState: LanguageState = {
        language: language,
        translateEnabled: this.isTranslateEnabled_(
            code, !!language.supportsTranslate, translateBlockedSet,
            translateTarget, prospectiveUILanguage),
        isManaged: false,
        isForced: languageForcedSet.has(code),
        removable: false,
      };
      enabledLanguageStates.push(languageState);
    }
    return enabledLanguageStates;
  }

  /**
   * True iff we translate pages that are in the given language.
   * @param code Language code.
   * @param supportsTranslate If translation supports the given language.
   * @param translateBlockedSet Set of languages for which translation is
   *     blocked.
   * @param translateTarget Language code of the default translate target
   *     language.
   * @param prospectiveUILanguage Prospective UI display language. Only define
   *     on Windows and Chrome OS.
   */
  private isTranslateEnabled_(
      code: string, supportsTranslate: boolean,
      translateBlockedSet: Set<string>, translateTarget: string,
      prospectiveUILanguage: string|undefined): boolean {
    const translateCode = convertLanguageCodeForTranslate(code);
    return supportsTranslate && !translateBlockedSet.has(translateCode) &&
        translateCode !== translateTarget &&
        (!prospectiveUILanguage || code !== prospectiveUILanguage);
  }

  /**
   * Updates the |removable| property of the enabled language states based
   * on what other languages and input methods are enabled.
   */
  private updateRemovableLanguages_() {
    if (this.intlAppLocalePref_ === undefined ||
        this.translateBlockedLanguagesPref_ === undefined ||
        this.languages === undefined) {
      return;
    }

    for (let i = 0; i < this.languages.enabled.length; i++) {
      const languageState = this.languages.enabled[i];
      this.set(
          'languages.enabled.' + i + '.removable',
          this.canDisableLanguage(languageState));
    }
  }

  /**
   * Creates a Set from the elements of the array.
   */
  private makeSetFromArray_<T>(list: T[]): Set<T> {
    return new Set(list);
  }

  // LanguageHelper implementation.
  whenReady(): Promise<void> {
    return this.resolver_.promise;
  }

  // <if expr="is_win">
  /**
   * Sets the prospective UI language to the chosen language. This won't affect
   * the actual UI language until a restart.
   */
  setProspectiveUiLanguage(languageCode: string) {
    this.browserProxy_.setProspectiveUiLanguage(languageCode);
  }

  /**
   * True if the prospective UI language was changed from its starting value.
   */
  requiresRestart(): boolean {
    return this.originalProspectiveUILanguage_ !==
        this.languages!.prospectiveUILanguage;
  }
  // </if>

  /**
   * @return True if the language is for ARC IMEs.
   */
  private isLanguageCodeForArcIme_(languageCode: string): boolean {
    return languageCode === kArcImeLanguage;
  }

  /**
   * @return True if the language is enabled.
   */
  isLanguageEnabled(languageCode: string): boolean {
    return this.enabledLanguageSet_.has(languageCode);
  }

  /**
   * Enables the language, making it available for input.
   */
  enableLanguage(languageCode: string) {
    this.languageSettingsPrivate_.enableLanguage(languageCode);
  }

  /**
   * Disables the language.
   */
  disableLanguage(languageCode: string) {
    // Remove the language from preferred languages.
    this.languageSettingsPrivate_.disableLanguage(languageCode);
  }

  canDisableLanguage(_languageState: LanguageState): boolean {
    // <if expr="is_win">
    // Cannot disable the prospective UI language.
    if (_languageState.language.code ===
        this.languages!.prospectiveUILanguage) {
      return false;
    }
    // </if>

    // Cannot disable the only enabled language.
    if (this.languages!.enabled.length === 1) {
      return false;
    }

    return true;
  }

  canEnableLanguage(language: chrome.languageSettingsPrivate.Language):
      boolean {
    return !(
        (this.isLanguageEnabled(language.code) ||
         language.isProhibitedLanguage ||
         this.isLanguageCodeForArcIme_(language.code)) /* internal use only */);
  }

  /**
   * Sets whether a given language should always be automatically translated.
   */
  setLanguageAlwaysTranslateState(
      languageCode: string, alwaysTranslate: boolean) {
    this.languageSettingsPrivate_.setLanguageAlwaysTranslateState(
        languageCode, alwaysTranslate);
  }

  /**
   * Moves the language in the list of enabled languages either up (toward the
   * front of the list) or down (toward the back).
   * @param upDirection True if we need to move up, false if we need to move
   *     down
   */
  moveLanguage(languageCode: string, upDirection: boolean) {
    if (upDirection) {
      this.languageSettingsPrivate_.moveLanguage(languageCode, MoveType.UP);
    } else {
      this.languageSettingsPrivate_.moveLanguage(languageCode, MoveType.DOWN);
    }
  }

  /**
   * Moves the language directly to the front of the list of enabled languages.
   */
  moveLanguageToFront(languageCode: string) {
    this.languageSettingsPrivate_.moveLanguage(languageCode, MoveType.TOP);
  }

  /**
   * Enables translate for the given language by removing the translate
   * language from the blocked languages preference.
   */
  enableTranslateLanguage(languageCode: string) {
    this.languageSettingsPrivate_.setEnableTranslationForLanguage(
        languageCode, true);
  }

  /**
   * Disables translate for the given language by adding the translate
   * language to the blocked languages preference.
   */
  disableTranslateLanguage(languageCode: string) {
    this.languageSettingsPrivate_.setEnableTranslationForLanguage(
        languageCode, false);
  }

  /**
   * Sets the translate target language.
   */
  setTranslateTargetLanguage(languageCode: string) {
    this.languageSettingsPrivate_.setTranslateTargetLanguage(languageCode);
  }

  getLanguage(languageCode: string): chrome.languageSettingsPrivate.Language
      |undefined {
    if (this.supportedLanguageMap_.has(languageCode)) {
      return this.supportedLanguageMap_.get(languageCode);
    }

    // If no languageCode is found, try the base Chrome format.
    const chromeLanguage =
        convertLanguageCodeForChrome(getBaseLanguage(languageCode));
    return this.supportedLanguageMap_.get(chromeLanguage);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-languages': SettingsLanguagesElement;
  }
}

customElements.define(SettingsLanguagesElement.is, SettingsLanguagesElement);
