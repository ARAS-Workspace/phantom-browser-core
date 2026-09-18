// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {Action, AutocompleteMatch, AutocompleteResult, OmniboxPopupSelection} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {KeywordType, SelectionDirection, SelectionLineState, SelectionStep} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {kDefaultSelection} from './searchbox_match.js';

export type {
  Action,
  AutocompleteMatch,
  AutocompleteResult,
  OmniboxPopupSelection,
};
export {SelectionDirection, SelectionLineState, SelectionStep};

export function selectionsEqual(
    a: OmniboxPopupSelection, b: OmniboxPopupSelection): boolean {
  return a.line === b.line && a.state === b.state &&
      a.actionIndex === b.actionIndex;
}

export function selectionToString(s: OmniboxPopupSelection): string {
  return `{${s.line},${s.state},${s.actionIndex}}`;
}

function getSelectionsForMatch(
    match: AutocompleteMatch, matchIndex: number): OmniboxPopupSelection[] {
  if (match.isHidden && !match.allowedToBeDefaultMatch) {
    return [];
  }
  const selections: OmniboxPopupSelection[] = [{
    line: matchIndex,
    state: SelectionLineState.kNormal,
    actionIndex: 0,
  }];
  if (match.keywordModel?.type === KeywordType.kChip) {
    selections.push({
      line: matchIndex,
      state: SelectionLineState.kKeywordMode,
      actionIndex: 0,
    });
  }
  if (match.actions) {
    match.actions.forEach((_: Action, actionIndex: number) => {
      selections.push({
        line: matchIndex,
        state: SelectionLineState.kFocusedButtonAction,
        actionIndex: actionIndex,
      });
    });
  }
  if (match.supportsDeletion) {
    selections.push({
      line: matchIndex,
      state: SelectionLineState.kFocusedButtonRemoveSuggestion,
      actionIndex: 0,
    });
  }
  return selections;
}

export function getMatchSelections(result: AutocompleteResult|null):
    OmniboxPopupSelection[] {
  if (!result) {
    return [];
  }
  return result.matches.flatMap(
      (match: AutocompleteMatch, matchIndex: number) =>
          getSelectionsForMatch(match, matchIndex));
}

type Constructor<T> = new (...args: any[]) => T;
type AbstractConstructor<T> = abstract new (...args: any[]) => T;

export interface SearchboxSelectionMixinInterface {
  selection: OmniboxPopupSelection;
  setSelection(selection: OmniboxPopupSelection): void;

  getAvailableSelections(result: AutocompleteResult|null):
      OmniboxPopupSelection[];

  getNextSelection(
      result: AutocompleteResult|null, from: OmniboxPopupSelection,
      direction: SelectionDirection,
      step: SelectionStep): OmniboxPopupSelection;

  onSelectionChanged(e: CustomEvent<{value: OmniboxPopupSelection}>): void;
}

export type SearchboxSelectionMixinBase = CrLitElement;

export const SearchboxSelectionMixin = <
    T extends Constructor<SearchboxSelectionMixinBase>>(
    superClass: T): T&AbstractConstructor<SearchboxSelectionMixinInterface> => {
  abstract class SearchboxSelectionMixin extends superClass implements
      SearchboxSelectionMixinInterface {
    private selection_: OmniboxPopupSelection = kDefaultSelection;

    get selection(): OmniboxPopupSelection {
      return this.selection_;
    }

    setSelection(selection: OmniboxPopupSelection) {
      if (selectionsEqual(this.selection_, selection)) {
        return;
      }

      const oldSelection = this.selection_;
      this.selection_ = selection;

      this.requestUpdate('selection', oldSelection);
    }

    onSelectionChanged(e: CustomEvent<{value: OmniboxPopupSelection}>) {
      this.setSelection(e.detail.value);
    }

    getAvailableSelections(result: AutocompleteResult|null):
        OmniboxPopupSelection[] {
      if (!result) {
        return [];
      }
      return getMatchSelections(result);
    }

    getNextSelection(
        result: AutocompleteResult|null, from: OmniboxPopupSelection,
        direction: SelectionDirection,
        step: SelectionStep): OmniboxPopupSelection {
      const available = this.getAvailableSelections(result);
      if (available.length === 0) {
        return from;
      }
      const isNormal = (selection: OmniboxPopupSelection) =>
          selection.state === SelectionLineState.kNormal;
      let fromIndex = available.findIndex(s => selectionsEqual(from, s));
      if (fromIndex < 0 && from.state === SelectionLineState.kKeywordMode) {
        fromIndex = available.findIndex(
            s => selectionsEqual(
                {...from, state: SelectionLineState.kNormal}, s));
      }

      const selectionsList = [...available];
      if (fromIndex < 0) {
        selectionsList.splice(0, 0, from);
        fromIndex = 0;
      }
      if (step === SelectionStep.kAllLines) {
        const normalIndex = direction === SelectionDirection.kBackward ?
            selectionsList.findIndex(isNormal) :
            selectionsList.findLastIndex(isNormal);
        return normalIndex < 0 ? from : selectionsList[normalIndex]!;
      }

      const remainder = (lhs: number, rhs: number) => ((lhs % rhs) + rhs) % rhs;
      for (let offset = 1; offset < selectionsList.length; offset++) {
        const offsetDirection =
            direction === SelectionDirection.kForward ? offset : -offset;
        const index =
            remainder(fromIndex + offsetDirection, selectionsList.length);
        const selection = selectionsList[index]!;
        if (step === SelectionStep.kStateOrLine || isNormal(selection)) {
          return selection;
        }
      }
      return from;
    }
  }

  return SearchboxSelectionMixin;
};
