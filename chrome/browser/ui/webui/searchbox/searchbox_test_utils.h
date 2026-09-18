// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_TEST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_TEST_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/test_omnibox_edit_model.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "realbox_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/size.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/omnibox_popup/mojom/omnibox_popup.mojom.h"
#endif

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::ReturnRef;
using testing::SaveArg;

class MockSearchboxPage : public searchbox::mojom::Page {
 public:
  MockSearchboxPage();
  ~MockSearchboxPage() override;
  mojo::PendingRemote<searchbox::mojom::Page> BindAndGetRemote();
  mojo::Receiver<searchbox::mojom::Page> receiver_{this};

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void,
              AutocompleteResultChanged,
              (searchbox::mojom::AutocompleteResultPtr));
  MOCK_METHOD(void,
              UpdateSelection,
              (searchbox::mojom::OmniboxPopupSelectionPtr,
               searchbox::mojom::OmniboxPopupSelectionPtr));
  MOCK_METHOD(void,
              StepSelection,
              (searchbox::mojom::SelectionDirection,
               searchbox::mojom::SelectionStep));
  MOCK_METHOD(void, OpenCurrentSelection, (WindowOpenDisposition));
  MOCK_METHOD(void, ResetPopupToInitialState, ());
  MOCK_METHOD(void, SetInputText, (const std::string& input_text));
  MOCK_METHOD(void,
              SetThumbnail,
              (const std::string& thumbnail_url, bool is_deletable));
  MOCK_METHOD(void,
              AddFileContext,
              (const base::UnguessableToken&,
               searchbox::mojom::SelectedFileInfoPtr));
#if !BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(void, UpdateSmartTabSharingActive, (bool active), (override));
#endif
  MOCK_METHOD(void, UpdateContentSharingPolicy, (bool enabled), (override));
  MOCK_METHOD(void,
              OnPermissionPromptChanged,
              (bool, const gfx::Size&),
              (override));
  MOCK_METHOD(void,
              SetRestoredTabIds,
              (const std::vector<int32_t>& ids),
              (override));
};

#if !BUILDFLAG(IS_ANDROID)
class MockOmniboxPopupPage : public omnibox_popup::mojom::Page {
 public:
  MockOmniboxPopupPage();
  ~MockOmniboxPopupPage() override;
  mojo::PendingRemote<omnibox_popup::mojom::Page> BindAndGetRemote();
  mojo::Receiver<omnibox_popup::mojom::Page> receiver_{this};

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void, OnShow, (), (override));
  MOCK_METHOD(void, OnContextMenuClosed, (), (override));
  MOCK_METHOD(void,
              SetInputState,
              (omnibox_popup::mojom::OmniboxInputStatePtr state),
              (override));
  MOCK_METHOD(void, SetFocus, (bool is_focused), (override));
  MOCK_METHOD(void, ClearAutocompleteMatches, (), (override));
  MOCK_METHOD(void, ClearPopup, (ClearPopupCallback callback), (override));
};
#endif

class MockAutocompleteController : public AutocompleteController {
 public:
  MockAutocompleteController(
      std::unique_ptr<AutocompleteProviderClient> provider_client,
      int provider_types);
  ~MockAutocompleteController() override;
  MockAutocompleteController(const MockAutocompleteController&) = delete;
  MockAutocompleteController& operator=(const MockAutocompleteController&) =
      delete;

  // AutocompleteController:
  MOCK_METHOD(void, Start, (const AutocompleteInput&), (override));
};

class MockOmniboxEditModel : public OmniboxEditModel {
 public:
  explicit MockOmniboxEditModel(OmniboxController* omnibox_controller);
  ~MockOmniboxEditModel() override;
  MockOmniboxEditModel(const MockOmniboxEditModel&) = delete;
  MockOmniboxEditModel& operator=(const MockOmniboxEditModel&) = delete;

  // OmniboxEditModel:
  MOCK_METHOD(void, SetUserText, (const std::u16string&), (override));
  MOCK_METHOD(void, OnPaste, (), (override));
  MOCK_METHOD(bool,
              OnAfterPossibleChange,
              (const OmniboxView::StateChanges& state_changes,
               bool allow_keyword_ui_change),
              (override));
  MOCK_METHOD(void, OnChanged, (), (override));
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_SEARCHBOX_TEST_UTILS_H_
