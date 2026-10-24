// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function clearChildren(element: Element) {
  while (element.firstChild) {
    element.firstChild.remove();
  }
}

export function createEl<K extends keyof HTMLElementTagNameMap>(
    tagName: K, parentEl: Element|null = null, classes: string[] = [],
    text: string = ''): HTMLElementTagNameMap[K] {
  const el = document.createElement(tagName);
  el.classList.add(...classes);
  el.textContent = text;
  if (parentEl) {
    parentEl.appendChild(el);
  }
  return el;
}
