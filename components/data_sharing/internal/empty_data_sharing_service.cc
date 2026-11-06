// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/empty_data_sharing_service.h"

#include "base/functional/callback.h"

namespace data_sharing {

EmptyDataSharingService::EmptyDataSharingService() = default;

EmptyDataSharingService::~EmptyDataSharingService() = default;

bool EmptyDataSharingService::IsEmptyService() {
  return true;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
EmptyDataSharingService::GetCollaborationGroupControllerDelegate() {
  return nullptr;
}

void EmptyDataSharingService::AddObserver(Observer* observer) {}

void EmptyDataSharingService::RemoveObserver(Observer* observer) {}

bool EmptyDataSharingService::IsGroupDataModelLoaded() {
  return false;
}

std::optional<GroupData> EmptyDataSharingService::ReadGroup(
    const GroupId& group_id) {
  return std::nullopt;
}

std::set<GroupData> EmptyDataSharingService::ReadAllGroups() {
  return std::set<GroupData>();
}

std::optional<GroupMemberPartialData>
EmptyDataSharingService::GetPossiblyRemovedGroupMember(
    const GroupId& group_id,
    const GaiaId& member_gaia_id) {
  return std::nullopt;
}

std::optional<GroupData> EmptyDataSharingService::GetPossiblyRemovedGroup(
    const GroupId& group_id) {
  return std::nullopt;
}

void EmptyDataSharingService::ReadGroupDeprecated(
    const GroupId& group_id,
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) {}

void EmptyDataSharingService::ReadNewGroup(
    const GroupToken& token,
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) {}

void EmptyDataSharingService::CreateGroup(
    const std::string& group_name,
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) {}

void EmptyDataSharingService::DeleteGroup(
    const GroupId& group_id,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {}

void EmptyDataSharingService::InviteMember(
    const GroupId& group_id,
    const std::string& invitee_email,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {}

void EmptyDataSharingService::AddMember(
    const GroupId& group_id,
    const std::string& access_token,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {}

void EmptyDataSharingService::RemoveMember(
    const GroupId& group_id,
    const std::string& member_email,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {}

void EmptyDataSharingService::LeaveGroup(
    const GroupId& group_id,
    base::OnceCallback<void(PeopleGroupActionOutcome)> callback) {}

bool EmptyDataSharingService::IsLeavingOrDeletingGroup(
    const GroupId& group_id) {
  return false;
}

std::vector<GroupEvent> EmptyDataSharingService::GetGroupEventsSinceStartup() {
  return {};
}

void EmptyDataSharingService::HandleShareURLNavigationIntercepted(
    const GURL& url,
    std::unique_ptr<ShareURLInterceptionContext> context) {}

std::unique_ptr<GURL> EmptyDataSharingService::GetDataSharingUrl(
    const GroupData& group_data) {
  return nullptr;
}

void EmptyDataSharingService::EnsureGroupVisibility(
    const GroupId& group_id,
    base::OnceCallback<void(const GroupDataOrFailureOutcome&)> callback) {}

void EmptyDataSharingService::GetSharedEntitiesPreview(
    const GroupToken& group_token,
    base::OnceCallback<void(const SharedDataPreviewOrFailureOutcome&)>
        callback) {}

void EmptyDataSharingService::GetAvatarImageForURL(
    const GURL& avatar_url,
    int size,
    base::OnceCallback<void(const gfx::Image&)> callback,
    image_fetcher::ImageFetcher* image_fetcher) {}

Logger* EmptyDataSharingService::GetLogger() {
  return nullptr;
}

void EmptyDataSharingService::OnCollaborationGroupRemoved(
    const GroupId& group_id) {}

bool EmptyDataSharingService::IsContextIdShared(const ContextId& context_id) {
  return false;
}

}  // namespace data_sharing
