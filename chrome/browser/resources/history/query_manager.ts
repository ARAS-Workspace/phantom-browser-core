// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {QueryResult, QueryState} from 'chrome://resources/cr_components/history/history.mojom-webui.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {RESULTS_PER_PAGE} from './constants.js';

export type ChangeQueryEvent = CustomEvent<{
  search: string | null,
  includeUserVisits?: boolean,
  includeActorVisits?: boolean,
}>;

declare global {
  interface HTMLElementEventMap {
    'change-query': ChangeQueryEvent;
  }
}

export class HistoryQueryManagerElement extends CrLitElement {
  static get is() {
    return 'history-query-manager';
  }

  static get template() {
    return null;
  }

  static override get properties() {
    return {
      queryState: {
        type: Object,
        notify: true,
      },

      queryResult: {
        type: Object,
      },
    };
  }

  accessor queryState: QueryState = {
    incremental: false,
    querying: false,
    searchTerm: '',
    after: null,
    includeUserVisits: true,
    includeActorVisits: true,
  };
  accessor queryResult: QueryResult = {
    info: null,
    value: [],
  };
  private eventTracker_: EventTracker = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document, 'change-query', this.onChangeQuery_.bind(this));
    this.eventTracker_.add(
        document, 'query-history', this.onQueryHistory_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  initialize() {
    this.queryHistory_(false /* incremental */);
  }

  private queryHistory_(incremental: boolean) {
    this.queryState = {
      ...this.queryState,
      querying: true,
      incremental: incremental,
    };

    const browserProxy = BrowserProxyImpl.getInstance();
    const promise = incremental ?
        browserProxy.handler.queryHistoryContinuation() :
        browserProxy.handler.queryHistory(
            this.queryState.searchTerm, RESULTS_PER_PAGE, null,
            this.queryState.includeUserVisits,
            this.queryState.includeActorVisits);
    // Ignore rejected (cancelled) queries.
    promise.then((result) => this.onQueryResult_(result.results), () => {});
  }

  private onChangeQuery_(e: ChangeQueryEvent) {
    const changes = e.detail;
    let needsUpdate = false;

    if (changes.search !== null &&
        changes.search !== this.queryState.searchTerm) {
      this.queryState = {...this.queryState, searchTerm: changes.search};
      this.searchTermChanged_();
      needsUpdate = true;
    }

    if (changes.includeUserVisits !== undefined &&
        changes.includeUserVisits !== this.queryState.includeUserVisits) {
      this.queryState = {
        ...this.queryState,
        includeUserVisits: changes.includeUserVisits,
      };
      needsUpdate = true;
    }

    if (changes.includeActorVisits !== undefined &&
        changes.includeActorVisits !== this.queryState.includeActorVisits) {
      this.queryState = {
        ...this.queryState,
        includeActorVisits: changes.includeActorVisits,
      };
      needsUpdate = true;
    }

    if (needsUpdate) {
      this.queryHistory_(false);
    }
  }

  private onQueryHistory_(e: CustomEvent<boolean>): boolean {
    this.queryHistory_(e.detail);
    return false;
  }

  /**
   * @param results List of results with information about the query.
   */
  private onQueryResult_(results: QueryResult) {
    this.queryState = {...this.queryState, querying: false};
    this.queryResult = {
      ...this.queryResult,
      info: results.info,
      value: results.value,
    };
    this.fire('query-finished', {result: this.queryResult});
  }

  private searchTermChanged_() {
    // TODO(tsergeant): Ignore incremental searches in this metric.
    if (this.queryState.searchTerm) {
      BrowserProxyImpl.getInstance().recordAction('Search');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'history-query-manager': HistoryQueryManagerElement;
  }
}

customElements.define(
    HistoryQueryManagerElement.is, HistoryQueryManagerElement);
