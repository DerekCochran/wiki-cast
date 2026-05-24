"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.rules = exports.stages = void 0;
exports.stages = (() => {
    const obj = {
        redirect: 1,
        onlyinclude: 1,
        noinclude: 1,
        include: 1,
        comment: 1,
        ext: 1,
        arg: 2,
        'magic-word': 2,
        template: 2,
        heading: 2,
        html: 3,
        table: 4,
        hr: 5,
        'double-underscore': 5,
        link: 6,
        category: 6,
        file: 6,
        quote: 7,
        'ext-link': 8,
        'free-ext-link': 9,
        'magic-link': 9,
        list: 10,
        dd: 10,
        converter: 11,
        /* NOT FOR BROWSER */
        'list-range': 11,
    };
    Object.setPrototypeOf(obj, null);
    return obj;
})();
exports.rules = (() => {
    const arr = [
        'arg-in-ext',
        'blank-alt',
        'bold-header',
        'format-leakage',
        'fostered-content',
        'h1',
        'illegal-attr',
        'insecure-style',
        'invalid-gallery',
        'invalid-imagemap',
        'invalid-invoke',
        'invalid-isbn',
        'invalid-json',
        'invalid-url',
        'lonely-apos',
        'lonely-bracket',
        'lonely-http',
        'nested-link',
        'no-arg',
        'no-duplicate',
        'no-ignored',
        'obsolete-attr',
        'obsolete-tag',
        'parsing-order',
        'pipe-like',
        'syntax-like',
        'table-layout',
        'tag-like',
        'unbalanced-header',
        'unclosed-comment',
        'unclosed-quote',
        'unclosed-table',
        'unescaped',
        'unknown-page',
        'unmatched-tag',
        'unterminated-url',
        'url-encoding',
        'var-anchor',
        'void-ext',
        /* NOT FOR BROWSER ONLY */
        'invalid-css',
        'invalid-math',
    ];
    Object.freeze(arr);
    return arr;
})();
