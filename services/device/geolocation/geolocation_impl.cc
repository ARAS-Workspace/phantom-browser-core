// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/geolocation/geolocation_impl.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "services/device/geolocation/geolocation_context.h"
#include "services/device/public/cpp/geolocation/geoposition.h"

namespace device {

namespace {
void RecordUmaGeolocationImplClientId(mojom::GeolocationClientId client_id) {
  base::UmaHistogramEnumeration("Geolocation.GeolocationImpl.ClientId",
                                client_id);
}

constexpr double kFixedLatitude = 39.925;
constexpr double kFixedLongitude = 32.836944;
constexpr double kFixedAccuracyMeters = 20.0;

mojom::GeopositionResultPtr MakeFixedPosition() {
  auto position = mojom::Geoposition::New();
  position->latitude = kFixedLatitude;
  position->longitude = kFixedLongitude;
  position->accuracy = kFixedAccuracyMeters;
  position->timestamp = base::Time::Now();
  return mojom::GeopositionResult::NewPosition(std::move(position));
}
}  // namespace

GeolocationImpl::GeolocationImpl(mojo::PendingReceiver<Geolocation> receiver,
                                 const url::Origin& requesting_origin,
                                 mojom::GeolocationClientId client_id,
                                 GeolocationContext* context,
                                 bool has_precise_permission)
    : receiver_(this, std::move(receiver)),
      origin_(requesting_origin),
      client_id_(client_id),
      context_(context),
      high_accuracy_hint_(false),
      has_precise_permission_(has_precise_permission) {
  DCHECK(context_);
  receiver_.set_disconnect_handler(base::BindOnce(
      &GeolocationImpl::OnConnectionError, base::Unretained(this)));
  position_override_ = MakeFixedPosition();
  current_result_ = position_override_.Clone();
}

GeolocationImpl::~GeolocationImpl() {
  // Make sure to respond to any pending callback even without a valid position.
  if (!position_callback_.is_null()) {
    if (!current_result_ || !current_result_->is_error()) {
      current_result_ =
          mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
              mojom::GeopositionErrorCode::kPositionUnavailable,
              /*error_message=*/"", /*error_technical=*/""));
    }
    ReportCurrentPosition();
  }
}

void GeolocationImpl::PauseUpdates() {}

void GeolocationImpl::ResumeUpdates() {
  if (position_override_) {
    OnLocationUpdate(*position_override_);
    return;
  }

  StartListeningForUpdates();
}

void GeolocationImpl::StartListeningForUpdates() {
  OnLocationUpdate(*position_override_);
}

void GeolocationImpl::SetHighAccuracyHint(bool high_accuracy) {
  high_accuracy_hint_ = high_accuracy;

  if (position_override_) {
    OnLocationUpdate(*position_override_);
    return;
  }

  StartListeningForUpdates();
}

void GeolocationImpl::QueryNextPosition(QueryNextPositionCallback callback) {
  if (!position_callback_.is_null()) {
    DVLOG(1) << "Overlapped call to QueryNextPosition!";
    OnConnectionError();  // Simulate a connection error.
    return;
  }

  position_callback_ = std::move(callback);

  if (current_result_) {
    ReportCurrentPosition();
  }
  RecordUmaGeolocationImplClientId(client_id_);
}

void GeolocationImpl::QueryCachedPosition(
    QueryCachedPositionCallback callback) {
  std::move(callback).Run(position_override_.Clone());
}

void GeolocationImpl::SetOverride(const mojom::GeopositionResult& result) {
  if (!position_callback_.is_null()) {
    if (!current_result_) {
      current_result_ =
          mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
              mojom::GeopositionErrorCode::kPositionUnavailable,
              /*error_message=*/"", /*error_technical=*/""));
    }
    ReportCurrentPosition();
  }

  position_override_ = result.Clone();
  if (result.is_error() ||
      (result.is_position() && !ValidateGeoposition(*result.get_position()))) {
    ResumeUpdates();
  }

  OnLocationUpdate(*position_override_);
}

void GeolocationImpl::ClearOverride() {
  position_override_ = MakeFixedPosition();
  StartListeningForUpdates();
}

void GeolocationImpl::OnPermissionUpdated(
    mojom::GeolocationPermissionLevel permission_level) {
  if (permission_level == mojom::GeolocationPermissionLevel::kDenied) {
    if (!position_callback_.is_null()) {
      std::move(position_callback_)
          .Run(mojom::GeopositionResult::NewError(mojom::GeopositionError::New(
              mojom::GeopositionErrorCode::kPermissionDenied,
              /*error_message=*/"User denied Geolocation",
              /*error_technical=*/"")));
      position_callback_.Reset();
    }
  } else {
    has_precise_permission_ =
        (permission_level == mojom::GeolocationPermissionLevel::kPrecise);
    StartListeningForUpdates();
  }
}

void GeolocationImpl::OnConnectionError() {
  context_->OnConnectionError(this);

  // The above call deleted this instance, so the only safe thing to do is
  // return.
}

void GeolocationImpl::OnLocationUpdate(const mojom::GeopositionResult& result) {
  DCHECK(context_);

  current_result_ = result.Clone();

  if (!position_callback_.is_null())
    ReportCurrentPosition();
}

void GeolocationImpl::ReportCurrentPosition() {
  CHECK(current_result_);
  std::move(position_callback_).Run(std::move(current_result_));
}

}  // namespace device
