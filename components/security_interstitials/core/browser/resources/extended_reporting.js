// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SecurityInterstitialCommandId, sendCommand} from 'chrome://interstitials/common/resources/interstitial_common.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

// Other constants defined in security_interstitial_page.h.
const SB_BOX_CHECKED = 'boxchecked';
const SB_DISPLAY_CHECK_BOX = 'displaycheckbox';

// This sets up the Extended Safe Browsing Reporting opt-in, either for
// reporting malware or invalid certificate chains. Does nothing if the
// interstitial type is not SAFEBROWSING or SSL.
export function setupExtendedReportingCheckbox() {
  const interstitialType = loadTimeData.getString('type');
  if (interstitialType !== 'SAFEBROWSING' && interstitialType !== 'SSL') {
    return;
  }

  if (!loadTimeData.getBoolean(SB_DISPLAY_CHECK_BOX)) {
    return;
  }

  document.querySelector('#opt-in-checkbox').checked =
      loadTimeData.getBoolean(SB_BOX_CHECKED);
  document.querySelector('#extended-reporting-opt-in')
      .classList.remove('hidden');

  const billing =
      interstitialType === 'SAFEBROWSING' && loadTimeData.getBoolean('billing');

  let className = 'ssl-opt-in';
  if (interstitialType === 'SAFEBROWSING' && !billing) {
    className = 'safe-browsing-opt-in';
  }

  document.querySelector('#extended-reporting-opt-in').classList.add(className);

  document.querySelector('#body').classList.add(
      'extended-reporting-has-checkbox');

  const optInCheckbox = document.querySelector('#opt-in-checkbox');
  optInCheckbox.addEventListener('click', function() {
    sendCommand(
        optInCheckbox.checked ? SecurityInterstitialCommandId.CMD_DO_REPORT :
                                SecurityInterstitialCommandId.CMD_DONT_REPORT);
  });
}
