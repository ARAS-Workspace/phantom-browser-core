// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/autofill_counter.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/webdata_services/web_data_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager_test_utils.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace {

using browsing_data::AutofillCounter;
typedef base::test::TestFuture<
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result>>
    CounterFuture;
class AutofillCounterTest : public InProcessBrowserTest {
 public:
  AutofillCounterTest() = default;

  AutofillCounterTest(const AutofillCounterTest&) = delete;
  AutofillCounterTest& operator=(const AutofillCounterTest&) = delete;

  ~AutofillCounterTest() override = default;

  void SetUpOnMainThread() override {
    personal_data_manager_ =
        autofill::PersonalDataManagerFactory::GetForBrowserContext(
            browser()->GetProfile());
    web_data_service_ = WebDataServiceFactory::GetAutofillWebDataForProfile(
        browser()->GetProfile(), ServiceAccessType::IMPLICIT_ACCESS);

    SetAutofillDeletionPref(true);
    SetDeletionPeriodPref(browsing_data::TimePeriod::ALL_TIME);
  }

  void TearDownOnMainThread() override {
    // Release our refs to let browser tear down of services complete in the
    // same order as usual.
    personal_data_manager_ = nullptr;
    web_data_service_ = nullptr;
  }

  browsing_data::AutofillCounter GetCounter() {
    return {GetPersonalDataManager(), GetWebDataService(),
            GetEntityDataManager(), /*sync_service=*/nullptr};
  }

  // Autocomplete suggestions --------------------------------------------------

  void AddAutocompleteSuggestion(const std::string& name,
                                 const std::string& label,
                                 const std::string& value) {
    autofill::FormFieldData field;
    field.set_name(base::ASCIIToUTF16(name));
    field.set_label(base::ASCIIToUTF16(label));
    field.set_value(base::ASCIIToUTF16(value));

    std::vector<autofill::FormFieldData> form_fields;
    form_fields.push_back(field);
    web_data_service_->AddFormFields(form_fields);
  }

  void RemoveAutocompleteSuggestion(const std::string& name,
                                    const std::string& label,
                                    const std::string& value) {
    web_data_service_->RemoveFormValueForElementNameAndLabel(
        base::ASCIIToUTF16(name), base::ASCIIToUTF16(label),
        base::ASCIIToUTF16(value));
  }

  void ClearAutocompleteSuggestions() {
    web_data_service_->RemoveFormElementsAddedBetween(
        base::Time(), base::Time::Max());
  }

  // Other utils ---------------------------------------------------------------

  void SetAutofillDeletionPref(bool value) {
    browser()->GetProfile()->GetPrefs()->SetBoolean(
        browsing_data::prefs::kDeleteFormData, value);
  }

  void SetDeletionPeriodPref(browsing_data::TimePeriod period) {
    browser()->GetProfile()->GetPrefs()->SetInteger(
        browsing_data::prefs::kDeleteTimePeriod, static_cast<int>(period));
  }

  autofill::EntityDataManager* GetEntityDataManager() {
    return autofill::AutofillEntityDataManagerFactory::GetForProfile(
        browser()->GetProfile());
  }

  autofill::PersonalDataManager* GetPersonalDataManager() {
    return personal_data_manager_;
  }

  scoped_refptr<autofill::AutofillWebDataService> GetWebDataService() {
    return web_data_service_;
  }

  // Result retrieval ---------------------------------------------

  browsing_data::BrowsingDataCounter::ResultInt GetNumSuggestions() {
    return num_suggestions_;
  }

  browsing_data::BrowsingDataCounter::ResultInt GetNumEntities() {
    return num_entities_;
  }

  browsing_data::AutofillCounter::ResultInt GetCounterValue() {
    return counter_value_;
  }

  void WaitForResult() {
    std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result =
        future.Take();
    while (!result->Finished()) {
      future.Clear();
      result = future.Take();
    }

    AutofillCounter::AutofillResult* autofill_result =
        static_cast<AutofillCounter::AutofillResult*>(result.get());
    counter_value_ = autofill_result->Value();
    num_entities_ = autofill_result->num_entities();
  }

 protected:
  CounterFuture future;

 private:
  base::test::ScopedFeatureList feature_list_{
      autofill::features::kAutofillAiWithDataSchema};
  autofill::test::AutofillBrowserTestEnvironment autofill_test_environment_;

  std::unique_ptr<base::RunLoop> run_loop_;

  raw_ptr<autofill::PersonalDataManager> personal_data_manager_;
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;

  browsing_data::BrowsingDataCounter::ResultInt counter_value_;
  browsing_data::BrowsingDataCounter::ResultInt num_suggestions_;
  browsing_data::BrowsingDataCounter::ResultInt num_entities_;
};

// Tests that we count the correct number of autocomplete suggestions.
IN_PROC_BROWSER_TEST_F(AutofillCounterTest, AutocompleteSuggestions) {
  browsing_data::AutofillCounter counter = GetCounter();
  counter.Init(browser()->GetProfile()->GetPrefs(),
               future.GetRepeatingCallback());
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(0, GetCounterValue());

  AddAutocompleteSuggestion("email", "email", "example@example.com");
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(1, GetCounterValue());

  AddAutocompleteSuggestion("tel", "telephone", "+123456789");
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(2, GetCounterValue());

  AddAutocompleteSuggestion("tel", "telephone", "+987654321");
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(3, GetCounterValue());

  RemoveAutocompleteSuggestion("email", "email", "example@example.com");
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(2, GetCounterValue());

  ClearAutocompleteSuggestions();
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(0, GetCounterValue());
}

IN_PROC_BROWSER_TEST_F(AutofillCounterTest, Entities) {
  browsing_data::AutofillCounter counter = GetCounter();
  counter.Init(browser()->GetProfile()->GetPrefs(),
               future.GetRepeatingCallback());
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(GetNumEntities(), 0);

  autofill::EntityInstance passport =
      autofill::test::GetPassportEntityInstance();
  GetEntityDataManager()->AddOrUpdateEntityInstance(passport);
  autofill::EntityDataChangedWaiter(GetEntityDataManager()).Wait();
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(GetNumEntities(), 1);

  autofill::EntityInstance drivers_license =
      autofill::test::GetDriversLicenseEntityInstance();
  GetEntityDataManager()->AddOrUpdateEntityInstance(drivers_license);
  autofill::EntityDataChangedWaiter(GetEntityDataManager()).Wait();
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(GetNumEntities(), 2);

  GetEntityDataManager()->RemoveEntityInstance(passport.guid());
  autofill::EntityDataChangedWaiter(GetEntityDataManager()).Wait();
  counter.Restart();
  WaitForResult();
  EXPECT_EQ(GetNumEntities(), 1);
}

}  // namespace
