// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview <cr-auto-img> is a specialized <img> that facilitates embedding
 * images into WebUIs via its auto-src attribute. <cr-auto-img> automatically
 * determines if the image is local (e.g. data: or chrome://) or external (e.g.
 * https://), and embeds the image directly or via the chrome://image data
 * source accordingly. Usage:
 *
 *   1. In C++ register |SanitizedImageSource| for your WebUI.
 *
 *   2. In HTML instantiate
 *
 *      <img is="cr-auto-img" auto-src="https://foo.com/bar.png">
 *
 *      If you want the image to reset to an empty state when auto-src changes
 *      and the new image is still loading, set the clear-src attribute:
 *
 *      <img is="cr-auto-img" auto-src="[[calculateSrc()]]" clear-src>
 *
 *      If you want your image to be always encoded as a static image (even if
 *      the source image is animated), set the static-encode attribute:
 *
 *      <img is="cr-auto-img" auto-src="https://foo.com/bar.png"
 *          static-encode>
 *
 *      Static images are encoded as PNG by default. If you want your image to
 *      be encoded as a Webp image, set the encode-type attribute to "webp".
 *
 *      <img is="cr-auto-img" auto-src="https://foo.com/bar.png"
 *          static-encode encode-type="webp">
 *
 * NOTE: Since <cr-auto-img> may use the chrome://image data source some images
 * may be transcoded to PNG.
 */

const AUTO_SRC: string = 'auto-src';

const CLEAR_SRC: string = 'clear-src';

const STATIC_ENCODE: string = 'static-encode';

const ENCODE_TYPE: string = 'encode-type';

export class CrAutoImgElement extends HTMLImageElement {
  static get observedAttributes() {
    return [AUTO_SRC, STATIC_ENCODE, ENCODE_TYPE];
  }

  attributeChangedCallback(
      name: string, _oldValue: string|null, _newValue: string|null) {
    if (name !== AUTO_SRC && name !== STATIC_ENCODE && name !== ENCODE_TYPE) {
      return;
    }

    if (this.hasAttribute(CLEAR_SRC)) {
      // Remove the src attribute so that the old image is not shown while the
      // new one is loading.
      this.removeAttribute('src');
    }

    let url = null;
    try {
      url = new URL(this.getAttribute(AUTO_SRC) || '');
    } catch (_) {
    }

    if (!url || url.protocol === 'chrome-untrusted:') {
      // Loading chrome-untrusted:// directly kills the renderer process.
      // Loading chrome-untrusted:// via the chrome://image data source
      // results in a broken image.
      this.removeAttribute('src');
      return;
    }
    if (url.protocol === 'data:' || url.protocol === 'chrome:') {
      this.src = url.href;
      return;
    }
    if (!this.hasAttribute(STATIC_ENCODE) && !this.hasAttribute(ENCODE_TYPE)) {
      this.src = 'chrome://image?' + url.href;
      return;
    }

    this.src = `chrome://image?url=${encodeURIComponent(url.href)}`;
    if (this.hasAttribute(STATIC_ENCODE)) {
      this.src += `&staticEncode=true`;
    }
    if (this.hasAttribute(ENCODE_TYPE)) {
      this.src += `&encodeType=${this.getAttribute(ENCODE_TYPE)}`;
    }
  }

  set autoSrc(src: string) {
    this.setAttribute(AUTO_SRC, src);
  }

  get autoSrc(): string {
    return this.getAttribute(AUTO_SRC) || '';
  }

  set clearSrc(_: string) {
    this.setAttribute(CLEAR_SRC, '');
  }

  get clearSrc(): string {
    return this.getAttribute(CLEAR_SRC) || '';
  }

  set staticEncode(enabled: boolean) {
    if (enabled) {
      this.setAttribute(STATIC_ENCODE, '');
    } else {
      this.removeAttribute(STATIC_ENCODE);
    }
  }

  get staticEncode(): boolean {
    return this.hasAttribute(STATIC_ENCODE);
  }

  set encodeType(type: string) {
    if (type) {
      this.setAttribute(ENCODE_TYPE, type);
    } else {
      this.removeAttribute(ENCODE_TYPE);
    }
  }

  get encodeType(): string {
    return this.getAttribute(ENCODE_TYPE) || '';
  }
}

customElements.define('cr-auto-img', CrAutoImgElement, {extends: 'img'});
