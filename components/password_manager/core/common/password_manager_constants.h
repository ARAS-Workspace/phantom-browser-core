// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains constants specific to the Password Manager component.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_CONSTANTS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_CONSTANTS_H_

namespace password_manager::constants {

inline constexpr char kAutocompleteUsername[] = "username";
inline constexpr char kAutocompleteOneTimePassword[] = "one-time-code";
inline constexpr char kAutocompleteWebAuthn[] = "webauthn";

// Password manager specific regexes are defined below.

// Form parsing related regexes, used to rule out irrelevant fields (SSN, OTP,
// etc) based on their label or name attribute.
inline constexpr char16_t kOneTimePwdRe[] =
    // "One time" is good signal that it is an OTP field.
    u"one.?time|"
    // The main tokens are good signals, but they are short, require word
    // boundaries around them.
    u"(?:\\b|_)(?:otp|otc|totp|sms|2fa|mfa)(?:\\b|_)|"
    // Alternatively, require companion tokens before or after the main tokens.
    u"(?:otp|otc|totp|sms|2fa|mfa).?(?:code|token|input|val|pin|login|verif|"
    u"pass|pwd|psw|auth|field)|"
    u"(?:verif(?:y|ication)?|email|phone|text|login|input|txt|user).?(?:otp|"
    u"otc|totp|sms|2fa|mfa)|"
    // Sometimes the main tokens are combined with each other.
    u"sms.?otp|mfa.?otp|"
    // "code" is not so strong signal as the main tokens, but in combination
    // with "verification" and its variations it is.
    u"verif(?:y|ication)?.?code|(?:\\b|_)vcode|"
    // 'Second factor' and its variations are good signals.
    u"(?:second|two|2).?factor|"
    // A couple of custom strings that are usually OTP fields.
    u"wfls-token|email_code";

inline constexpr char16_t kSearch[] = u"search";

// Variations of the word password. Used to enable manual password generation on
// the field.
inline constexpr char16_t kPasswordRe[] =
    // Synonyms and abbreviations of password.
    u"pass(?:word|code)|pas(?:word|code)|pswrd|psw|pswd|pwd|parole|watchword|"

    // Translations.
    u"pasahitza|parol|lozinka|sifr|contrasenya|heslo|adgangskode|losen|"
    u"wachtwoord|paswoord|salasana|passe|contrasinal|passwort|jelszo|"
    u"sandi|signum|slaptazodis|kata|passord|haslo|senha|geslo|contrasena|"
    u"khau";

}  // namespace password_manager::constants

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_PASSWORD_MANAGER_CONSTANTS_H_
