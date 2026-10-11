// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import './icons.html.js';
import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {assert} from 'chrome://resources/js/assert.js';

import {getHtml} from './power_bookmarks_labels.html.js';
import type {Label} from './power_bookmarks_service.js';

export class PowerBookmarksLabelsElement extends CrLitElement {
  static get is() {
    return 'power-bookmarks-labels';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      disabled: {type: Boolean},
      labels: {
        type: Array,
        notify: true,
      },
    };
  }

  accessor disabled: boolean = false;
  accessor labels: Label[] = [];

  protected getLabelIcon(label: Label): string {
    return label.active ? (loadTimeData.getBoolean('webuiRoundedIconsEnabled') ?
                               'bookmarks:check' :
                               'bookmarks:check-old') :
                          label.icon;
  }

  protected onLabelClick(event: Event) {
    event.preventDefault();
    event.stopPropagation();
    const index = Number((event.currentTarget as HTMLElement).dataset['index']);
    const labels = [...this.labels];
    assert(index >= 0 && index < labels.length);
    labels[index] = {...labels[index]!, active: !labels[index]!.active};
    this.labels = labels;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'power-bookmarks-labels': PowerBookmarksLabelsElement;
  }
}

customElements.define(
    PowerBookmarksLabelsElement.is, PowerBookmarksLabelsElement);
