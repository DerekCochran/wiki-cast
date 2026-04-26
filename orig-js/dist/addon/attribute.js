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
const document_1 = require("../lib/document");
const attribute_1 = require("../src/attribute");
const attributes_1 = require("../src/attributes");
const atom_1 = require("../src/atom");
attribute_1.AttributeToken.prototype.setValue =
    /** @implements */
    function (value) {
        if (value === false) {
            this.remove();
            return;
        }
        else if (value === true) {
            this.setAttribute('equal', '');
            return;
        }
        const { type, lastChild } = this;
        /* c8 ignore next 6 */
        if (type === 'ext-attr' && value.includes('>')) {
            throw new RangeError('Attributes of an extension tag cannot contain ">"!');
        }
        if (value.includes('"') && value.includes(`'`)) {
            throw new RangeError('Attribute values cannot contain single and double quotes simultaneously!');
        }
        const { childNodes } = index_1.default.parseWithRef(value, this, attribute_1.stages[type] + 1);
        lastChild.safeReplaceChildren(childNodes);
        this.setAttribute('equal', this.isInside('parameter') ? '{{=}}' : '=');
        if (value.includes('"')) {
            this.setAttribute('quotes', [`'`, `'`]);
        }
        else if (value.includes(`'`) || !this.getAttribute('quotes')[0]) {
            this.setAttribute('quotes', ['"', '"']);
        }
        else {
            this.close();
        }
    };
attribute_1.AttributeToken.prototype.rename =
    /** @implements */
    function (key) {
        const { type, name, tag, firstChild } = this;
        /* c8 ignore next 3 */
        if (name === 'title' || name === 'alt' && tag === 'img') {
            throw new Error(`${name} attribute cannot be renamed!`);
        }
        const { childNodes } = index_1.default.parseWithRef(key, this, attribute_1.stages[type] + 1);
        firstChild.safeReplaceChildren(childNodes);
    };
attribute_1.AttributeToken.prototype.css =
    /** @implements */
    function (key, value) {
        const { name, lastChild } = this;
        /* c8 ignore next 6 */
        if (name !== 'style') {
            throw new Error('Not a style attribute!');
        }
        if (lastChild.length > 1 || lastChild.length === 1 && lastChild.firstChild.type !== 'text') {
            throw new Error('Complex style attribute!');
        }
        const cssLSP = (0, document_1.loadCssLSP)();
        /* c8 ignore next 3 */
        if (!cssLSP) {
            throw new Error('CSS language service is not available!');
        }
        const doc = new document_1.EmbeddedCSSDocument(this.getRootNode(), lastChild), styleSheet = doc.styleSheet, [{ declarations: { children } }] = styleSheet.children, declaration = children?.filter(({ property }) => property.getText() === key) ?? [];
        if (value === undefined) {
            return declaration.at(-1)?.value.getText();
        }
        else if (typeof value === 'number') {
            value = String(value);
        }
        const style = styleSheet.getText().slice(0, -1);
        if (!value) {
            if (declaration.length === children?.length) {
                this.setValue('');
            }
            else if (declaration.length > 0) {
                let output = '', start = doc.pre.length;
                for (const { offset, length } of declaration) {
                    output += style.slice(start, offset);
                    start = offset + length;
                }
                output += style.slice(start);
                this.setValue(output.replace(/^\s*;\s*|;\s*(?=;)/gu, ''));
            }
            return undefined;
        }
        const hasQuote = value.includes('"'), [quot] = this.getAttribute('quotes');
        /* c8 ignore start */
        if (quot && value.includes(quot) || hasQuote && value.includes(`'`)) {
            const quote = quot || '"';
            throw new RangeError(`Please consider replacing \`${quote}\` with \`${quote === '"' ? `'` : '"'}\`!`);
        }
        /* c8 ignore stop */
        if (declaration.length > 0) {
            const { offset, length } = declaration.at(-1).value;
            this.setValue(style.slice(doc.pre.length, offset) + value + style.slice(offset + length));
        }
        else {
            this.setValue(`${style.slice(doc.pre.length)}${!children?.length || /;\s*$/u.test(style) ? '' : '; '}${key}: ${value}`);
        }
        return undefined;
    };
attributes_1.AttributesToken.prototype.sanitize =
    /** @implements */
    function () {
        const type = (0, attributes_1.toAttributeType)(this.type);
        let dirty = false;
        for (let i = this.length - 1; i >= 0; i--) {
            const child = this.childNodes[i];
            if (child instanceof atom_1.AtomToken && child.text().trim()) {
                dirty = true;
                if (child.previousSibling?.is(type) && child.nextSibling?.is(type)) {
                    child.replaceChildren(' ');
                }
                else {
                    this.removeAt(i);
                }
            }
        }
        if (!debug_1.Shadow.running && dirty) {
            index_1.default.warn('AttributesToken.sanitize will remove invalid attributes!');
        }
    };
attributes_1.AttributesToken.prototype.setAttr =
    /** @implements */
    function (keyOrProp, value) {
        if (typeof keyOrProp === 'object') {
            for (const key in keyOrProp) {
                this.setAttr(key, keyOrProp[key]);
            }
            return;
        }
        const { type, name } = this;
        /* c8 ignore next 3 */
        if (type === 'ext-attrs' && typeof value === 'string' && value.includes('>')) {
            throw new RangeError('Attributes of an extension tag cannot contain ">"!');
        }
        const key = (0, string_1.trimLc)(keyOrProp), attr = this.getAttrToken(key);
        if (attr) {
            attr.setValue(value);
            return;
        }
        else if (value === false) {
            return;
        }
        // @ts-expect-error abstract class
        const token = debug_1.Shadow.run(() => new attribute_1.AttributeToken((0, attributes_1.toAttributeType)(type), name, key, ['"', '"'], this.getAttribute('config'), value === true ? '' : '=', value === true ? '' : value));
        this.insertAt(token);
    };
attributes_1.AttributesToken.prototype.toggleAttr =
    /** @implements */
    function (key, force) {
        key = (0, string_1.trimLc)(key);
        const attr = this.getAttrToken(key);
        /* c8 ignore next 3 */
        if (attr && attr.getValue() !== true) {
            throw new RangeError(`${key} attribute is not Boolean!`);
        }
        if (attr) {
            attr.setValue(force === true);
        }
        else if (force !== false) {
            this.setAttr(key, true);
        }
    };
attributes_1.AttributesToken.prototype.css =
    /** @implements */
    function (key, value) {
        let attr = this.getAttrToken('style');
        if (!attr) {
            // @ts-expect-error abstract class
            const token = debug_1.Shadow.run(() => new attribute_1.AttributeToken((0, attributes_1.toAttributeType)(this.type), this.name, 'style', [], this.getAttribute('config')));
            attr = this.insertAt(token);
        }
        return attr.css(key, value);
    };
constants_1.classes['ExtendedAttributeToken'] = __filename;
