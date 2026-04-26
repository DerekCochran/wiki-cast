"use strict";
/* eslint @stylistic/operator-linebreak: [2, "before", {overrides: {"=": "after"}}] */
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const constants_1 = require("../util/constants");
const debug_1 = require("../util/debug");
const string_1 = require("../util/string");
const index_1 = __importDefault(require("../index"));
const index_2 = require("../src/index");
const base_1 = require("../src/link/base");
const index_3 = require("../src/link/index");
const atom_1 = require("../src/atom");
base_1.LinkBaseToken.prototype.setTarget =
    /** @implements */
    function (link) {
        const { childNodes } = index_1.default.parseWithRef(link, this, 2), token = debug_1.Shadow.run(() => new atom_1.AtomToken(undefined, 'link-target', this.getAttribute('config'), [], { 'Stage-2': ':', '!ExtToken': '', '!HeadingToken': '' }));
        token.concat(childNodes); // eslint-disable-line unicorn/prefer-spread
        this.firstChild.safeReplaceWith(token);
    };
base_1.LinkBaseToken.prototype.setFragment =
    /** @implements */
    function (fragment) {
        const { type, name } = this;
        if (fragment === undefined || (0, debug_1.isLink)(type)) {
            fragment &&= (0, string_1.encode)(fragment);
            this.setTarget(name + (fragment === undefined ? '' : `#${fragment}`));
        }
    };
base_1.LinkBaseToken.prototype.setLinkText =
    /** @implements */
    function (linkStr) {
        if (linkStr === undefined) {
            this.childNodes[1]?.remove();
            return;
        }
        else if (this.length === 1) {
            this.insertAt(debug_1.Shadow.run(() => {
                const inner = new index_2.Token(undefined, this.getAttribute('config'), [], {
                    'Stage-5': ':', QuoteToken: ':', ConverterToken: ':',
                });
                inner.type = 'link-text';
                return inner;
            }));
        }
        this.lastChild.safeReplaceChildren(index_1.default.parseWithRef(linkStr, this).childNodes);
    };
index_3.LinkToken.prototype.setLangLink =
    /** @implements */
    function (lang, link) {
        link = link.trim();
        /* c8 ignore next 3 */
        if (link.startsWith('#')) {
            throw new SyntaxError('An interlanguage link cannot be fragment only!');
        }
        this.setTarget(lang + (link.startsWith(':') ? '' : ':') + link);
    };
index_3.LinkToken.prototype.asSelfLink =
    /** @implements */
    function (fragment) {
        fragment ??= this.fragment;
        /* c8 ignore next 3 */
        if (!fragment?.trim()) {
            throw new RangeError('LinkToken.asSelfLink method must specify a non-empty fragment!');
        }
        this.setTarget(`#${(0, string_1.encode)(fragment)}`);
    };
index_3.LinkToken.prototype.pipeTrick =
    /** @implements */
    function () {
        const linkText = this.firstChild.text();
        /* c8 ignore next 3 */
        if (linkText.includes('#') || linkText.includes('%')) {
            throw new Error('Pipe trick cannot be used with "#" or "%"!');
        }
        const m1 = /^:?(?:[ \w\x80-\xFF-]+:)?([^(]+?) ?\(.+\)$/u.exec(linkText);
        if (m1) {
            this.setLinkText(m1[1]);
            return;
        }
        const m2 = /^:?(?:[ \w\x80-\xFF-]+:)?([^（]+?) ?（.+）$/u.exec(linkText);
        if (m2) {
            this.setLinkText(m2[1]);
            return;
        }
        const m3 = /^:?(?:[ \w\x80-\xFF-]+:)?(.*?)(?: ?(?<!\()\(.+\))?(?:(?:, |，|، ).|$)/u
            .exec(linkText);
        this.setLinkText(m3[1]);
    };
constants_1.classes['ExtendedLinkToken'] = __filename;
