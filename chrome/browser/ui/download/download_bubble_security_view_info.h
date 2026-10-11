// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_SECURITY_VIEW_INFO_H_
#define CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_SECURITY_VIEW_INFO_H_

#include "base/scoped_observation.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/ui/download/download_bubble_info.h"
#include "chrome/browser/ui/download/download_bubble_info_utils.h"
#include "components/offline_items_collection/core/fail_state.h"

class DownloadBubbleSecurityViewInfoObserver : public base::CheckedObserver {
 public:
  DownloadBubbleSecurityViewInfoObserver();
  ~DownloadBubbleSecurityViewInfoObserver() override;

  virtual void OnInfoChanged() {}
  virtual void OnContentIdChanged() {}
};

class DownloadBubbleSecurityViewInfo
    : public DownloadBubbleInfo<DownloadBubbleSecurityViewInfoObserver>,
      public download::DownloadItem::Observer {
 public:
  struct SubpageButton {
    DownloadCommands::Command command;
    std::u16string label;
    bool is_prominent = false;

    // Controls the text color of the button. Only applied for some secondary
    // buttons.
    std::optional<ui::ColorId> text_color;

    SubpageButton(DownloadCommands::Command command,
                  std::u16string label,
                  bool is_prominent,
                  std::optional<ui::ColorId> text_color = std::nullopt);
  };

  DownloadBubbleSecurityViewInfo();
  ~DownloadBubbleSecurityViewInfo() override;

  // Update the security view when a subpage is opened for a particular
  // download. Initializes this view, and associates it with `model`'s download.
  // If it is already associated with the same download, this will update the
  // view if the danger type has changed since the last time it was initialized.
  // It is not an error to initialize this with a download when it is already
  // initialized, either with the same download or a different download.
  void InitializeForDownload(DownloadUIModel& model);

  void SetSubpageButtonsForTesting(std::vector<SubpageButton> buttons);

  // Accessors
  const std::optional<offline_items_collection::ContentId>& content_id() const {
    return content_id_;
  }
  download::DownloadDangerType danger_type() const { return danger_type_; }
  const std::u16string& title_text() const { return title_text_; }
  const gfx::VectorIcon* icon_model_override() const {
    return icon_and_color_.icon;
  }
  ui::ColorId secondary_color() const { return icon_and_color_.color; }
  const std::u16string& warning_summary() const { return warning_summary_; }
  bool has_primary_button() const { return subpage_buttons_.size() > 0; }
  bool has_secondary_button() const { return subpage_buttons_.size() > 1; }
  const SubpageButton& primary_button() const { return subpage_buttons_[0]; }
  const SubpageButton& secondary_button() const { return subpage_buttons_[1]; }
  bool HasSubpage() const;

  // Returns this to an uninitialized state, where this is not associated with
  // a particular download. Called when navigating away from the security view.
  void Reset();

 private:
  friend class DownloadBubbleSecurityViewInfoTest;

  // download::DownloadItem::Observer
  void OnDownloadUpdated(download::DownloadItem* download) override;
  void OnDownloadRemoved(download::DownloadItem* download) override;

  // Returns this to the default state, while maintaining the
  // association with the download given by `content_id_`. This allows
  // the PopulateFor* methods to only set fields that differ from
  // default
  void ClearForUpdate();

  // Populate all the fields of this class based the current state of
  // the download associated with `content_id_`.
  void PopulateForDownload(download::DownloadItem* download);
  void PopulateForSuspiciousUi(
      const std::u16string& subpage_summary,
      const std::u16string& secondary_subpage_button_label);
  void PopulateForInterrupted(const DownloadUIModel& model);
  void PopulateForInProgressOrComplete(const DownloadUIModel& model);

  // The subpage of the bubble supports at most 2 buttons. The primary one must
  // be populated first, then the secondary.
  void PopulatePrimarySubpageButton(const std::u16string& label,
                                    DownloadCommands::Command command,
                                    bool is_prominent = true);
  void PopulateSecondarySubpageButton(
      const std::u16string& label,
      DownloadCommands::Command command,
      std::optional<ui::ColorId> color = std::nullopt);

  // ContentId of the download this refers to, if initialized.
  std::optional<offline_items_collection::ContentId> content_id_;

  // The text for the title (i.e. filename) that this view was last
  // created/updated with.
  std::u16string title_text_;

  // Cached danger type for the current download
  download::DownloadDangerType danger_type_ =
      download::DOWNLOAD_DANGER_TYPE_MAX;

  // This is non-null if the view should display an icon other than the system
  // icon for the filetype.
  IconAndColor icon_and_color_{};

  // Subpage summary of the download warning
  std::u16string warning_summary_;

  // Subpage buttons
  std::vector<SubpageButton> subpage_buttons_;

  // Observation of the download item this refers to. Only observes while this
  // is associated with a download item.
  base::ScopedObservation<download::DownloadItem,
                          download::DownloadItem::Observer>
      download_item_observation_{this};
};

#endif  // CHROME_BROWSER_UI_DOWNLOAD_DOWNLOAD_BUBBLE_SECURITY_VIEW_INFO_H_
