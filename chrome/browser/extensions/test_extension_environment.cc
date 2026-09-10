// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_environment.h"

#include <utility>

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

using content::BrowserThread;

namespace {

base::DictValue MakeExtensionManifest(const base::DictValue& manifest_extra) {
  base::DictValue manifest = base::DictValue()
                                 .Set("name", "Extension")
                                 .Set("version", "1.0")
                                 .Set("manifest_version", 2);
  manifest.Merge(manifest_extra.Clone());
  return manifest;
}

base::DictValue MakePackagedAppManifest() {
  return base::DictValue()
      .Set("name", "Test App Name")
      .Set("version", "2.0")
      .Set("manifest_version", 2)
      .Set("app", base::DictValue().Set(
                      "background",
                      base::DictValue().Set("scripts", base::ListValue().Append(
                                                           "background.js"))));
}

}  // namespace

// static
ExtensionService* TestExtensionEnvironment::CreateExtensionServiceForProfile(
    TestingProfile* profile) {
  TestExtensionSystem* extension_system =
      static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile));
  return extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
}

TestExtensionEnvironment::TestExtensionEnvironment(
    Type type,
    ProfileCreationType profile_creation_mode
    )
    : task_environment_(
          type == Type::kWithTaskEnvironment
              ? std::make_unique<content::BrowserTaskEnvironment>()
              : nullptr),
      profile_(profile_creation_mode != ProfileCreationType::kCreate
                   ? nullptr
                   : std::make_unique<TestingProfile>()),
      profile_ptr_(profile_.get()) {
}

TestExtensionEnvironment::~TestExtensionEnvironment() = default;

void TestExtensionEnvironment::SetProfile(TestingProfile* profile) {
  profile_ptr_ = profile;
}

TestingProfile* TestExtensionEnvironment::profile() const {
  return profile_ptr_.get();
}

TestExtensionSystem* TestExtensionEnvironment::GetExtensionSystem() {
  return static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
}

ExtensionService* TestExtensionEnvironment::GetExtensionService() {
  if (!extension_service_) {
    extension_service_ = CreateExtensionServiceForProfile(profile());
  }
  return extension_service_;
}

ExtensionPrefs* TestExtensionEnvironment::GetExtensionPrefs() {
  return ExtensionPrefs::Get(profile());
}

ExtensionRegistrar* TestExtensionEnvironment::GetExtensionRegistrar() {
  // TODO(crbug.com/40355585): This is necessary to set up ExtensionService,
  // due to dependencies it initializes. Revisit this once that's no longer
  // the case.
  GetExtensionService();
  return ExtensionRegistrar::Get(profile());
}

const Extension* TestExtensionEnvironment::MakeExtension(
    const base::DictValue& manifest_extra) {
  base::DictValue manifest = MakeExtensionManifest(manifest_extra);
  scoped_refptr<const Extension> result =
      ExtensionBuilder().SetManifest(std::move(manifest)).Build();
  GetExtensionRegistrar()->AddExtension(result.get());
  return result.get();
}

const Extension* TestExtensionEnvironment::MakeExtension(
    const base::DictValue& manifest_extra,
    const std::string& id) {
  base::DictValue manifest = MakeExtensionManifest(manifest_extra);
  scoped_refptr<const Extension> result =
      ExtensionBuilder().SetManifest(std::move(manifest)).SetID(id).Build();
  GetExtensionRegistrar()->AddExtension(result.get());
  return result.get();
}

scoped_refptr<const Extension> TestExtensionEnvironment::MakePackagedApp(
    const std::string& id,
    bool install) {
  scoped_refptr<const Extension> result =
      ExtensionBuilder()
          .SetManifest(MakePackagedAppManifest())
          .AddFlags(Extension::FROM_WEBSTORE)
          .SetID(id)
          .Build();
  if (install) {
    GetExtensionRegistrar()->AddExtension(result.get());
  }
  return result;
}

std::unique_ptr<content::WebContents> TestExtensionEnvironment::MakeTab()
    const {
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  // Create a tab id.
  CreateSessionServiceTabHelper(contents.get());
  return contents;
}

void TestExtensionEnvironment::DeleteProfile() {
  profile_ptr_ = nullptr;
  profile_.reset();
  extension_service_ = nullptr;
}

void TestExtensionEnvironment::ProfileMarkedForPermanentDeletionForTest() {
  GetExtensionService()->ProfileMarkedForPermanentDeletionForTest();
}

}  // namespace extensions
