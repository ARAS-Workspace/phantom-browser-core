// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ID_CONSTANTS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ID_CONSTANTS_H_

namespace web_app {

// The URLs used to generate the app IDs MUST match the start_url field of the
// manifest served by the PWA.
// Please maintain the alphabetical order when adding new app IDs.

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://calculator.apps.chrome/"))
inline constexpr char kCalculatorAppId[] = "oabkinaljpjeilageghcdlnekhphhphl";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://canvas.apps.chrome/"))
inline constexpr char kCanvasAppId[] = "ieailfmhaghpphfffooibmlghaeopach";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://cursive.apps.chrome/"))
inline constexpr char kCursiveAppId[] = "apignacaigpffemhdbhmnajajaccbckh";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://mail.google.com/mail/?usp=installed_webapp"))
inline constexpr char kGmailAppId[] = "fmgjjmmmlfnkbppncabfkddbjimcfncm";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://calendar.google.com/calendar/r"))
inline constexpr char kGoogleCalendarAppId[] =
    "kjbdgfilnfhdoflbpgamdcdgpehopbep";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://mail.google.com/chat/"))
inline constexpr char kOldGoogleChatAppId[] =
    "mdpkiolbdkhdjpekfbkbmhigcaggjagi";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://chat.google.com/"))
inline constexpr char kGoogleChatAppId[] = "pommaclcbfghclhalboakcipcmmndhcj";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://docs.google.com/document/?usp=installed_webapp"))
inline constexpr char kGoogleDocsAppId[] = "mpnpojknpmmopombnjdcgaaiekajbnjb";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://drive.google.com/?lfhs=2"))
inline constexpr char kGoogleDriveAppId[] = "aghbiahbpaijignceidepookljebhfak";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://keep.google.com/?usp=installed_webapp"))
inline constexpr char kGoogleKeepAppId[] = "eilembjdkfgodjkcjnpgpaenohkicgjd";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://www.google.com/maps?force=tt&source=ttpwa"))
inline constexpr char kGoogleMapsAppId[] = "mnhkaebcjjhencmpkapnbdaogjamfbcj";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://meet.google.com/landing?lfhs=2"))
inline constexpr char kGoogleMeetAppId[] = "kjgfgldnnfoeklkmfkjfagphfepbbdan";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://play.google.com/store/movies?usp=installed_webapp"))
inline constexpr char kGoogleMoviesAppId[] = "aiihaadhfoadjgjcegeomiajkajbjlcn";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://docs.google.com/spreadsheets/?usp=installed_webapp"))
inline constexpr char kGoogleSheetsAppId[] = "fhihpiojkbmbpdjeoajapmgkhlnakfjf";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://docs.google.com/presentation/?usp=installed_webapp"))
inline constexpr char kGoogleSlidesAppId[] = "kefjledonklijopmnomlcbpllchaibag";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://messages.google.com/web/"))
inline constexpr char kMessagesAppId[] = "hpfldicfbfomlpcikngkocigghgafkph";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://books.google.com/ebooks/app"))
inline constexpr char kPlayBooksAppId[] = "jglfhlbohpgcbefmhdmpancnijacbbji";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://www.youtube.com/?feature=ytca"))
inline constexpr char kYoutubeAppId[] = "agimnkijcaahngcdmfeangaknmldooml";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/std::nullopt, GURL(
//     "https://music.youtube.com/?source=pwa"))
inline constexpr char kYoutubeMusicAppId[] = "cinhimbnkkaeohfgghhklpknlkffjgod";

// Generated as: web_app::GenerateAppId(/*manifest_id=*/"", GURL(
//     "chrome://password-manager/?source=pwa"))
inline constexpr char kPasswordManagerAppId[] =
    "kajebgjangihfbkjfejcanhanjmmbcfd";

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_ID_CONSTANTS_H_
