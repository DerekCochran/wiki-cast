"use strict";
var __runInitializers = (this && this.__runInitializers) || function (thisArg, initializers, value) {
    var useValue = arguments.length > 2;
    for (var i = 0; i < initializers.length; i++) {
        value = useValue ? initializers[i].call(thisArg, value) : initializers[i].call(thisArg);
    }
    return useValue ? value : void 0;
};
var __esDecorate = (this && this.__esDecorate) || function (ctor, descriptorIn, decorators, contextIn, initializers, extraInitializers) {
    function accept(f) { if (f !== void 0 && typeof f !== "function") throw new TypeError("Function expected"); return f; }
    var kind = contextIn.kind, key = kind === "getter" ? "get" : kind === "setter" ? "set" : "value";
    var target = !descriptorIn && ctor ? contextIn["static"] ? ctor : ctor.prototype : null;
    var descriptor = descriptorIn || (target ? Object.getOwnPropertyDescriptor(target, contextIn.name) : {});
    var _, done = false;
    for (var i = decorators.length - 1; i >= 0; i--) {
        var context = {};
        for (var p in contextIn) context[p] = p === "access" ? {} : contextIn[p];
        for (var p in contextIn.access) context.access[p] = contextIn.access[p];
        context.addInitializer = function (f) { if (done) throw new TypeError("Cannot add initializers after decoration has completed"); extraInitializers.push(accept(f || null)); };
        var result = (0, decorators[i])(kind === "accessor" ? { get: descriptor.get, set: descriptor.set } : descriptor[key], context);
        if (kind === "accessor") {
            if (result === void 0) continue;
            if (result === null || typeof result !== "object") throw new TypeError("Object expected");
            if (_ = accept(result.get)) descriptor.get = _;
            if (_ = accept(result.set)) descriptor.set = _;
            if (_ = accept(result.init)) initializers.unshift(_);
        }
        else if (_ = accept(result)) {
            if (kind === "field") initializers.unshift(_);
            else descriptor[key] = _;
        }
    }
    if (target) Object.defineProperty(target, contextIn.name, descriptor);
    done = true;
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.AttributesToken = exports.toAttributeType = void 0;
const lint_1 = require("../util/lint");
const string_1 = require("../util/string");
const rect_1 = require("../lib/rect");
const index_1 = __importDefault(require("../index"));
const index_2 = require("./index");
const atom_1 = require("./atom");
const attribute_1 = require("./attribute");
/* NOT FOR BROWSER */
const html_1 = require("../util/html");
const debug_1 = require("../util/debug");
const constants_1 = require("../util/constants");
const clone_1 = require("../mixin/clone");
const cached_1 = require("../mixin/cached");
const stages = { 'ext-attrs': 0, 'html-attrs': 2, 'table-attrs': 3 };
/**
 * 将属性类型转换为单属性类型
 * @param type 属性类型
 */
const toAttributeType = (type) => type.slice(0, -1);
exports.toAttributeType = toAttributeType;
/**
 * 将属性类型转换为无效属性类型
 * @param type 属性类型
 */
const toDirty = (type) => `${(0, exports.toAttributeType)(type)}-dirty`;
const wordRegex = /* #__PURE__ */ (() => {
    try {
        // eslint-disable-next-line prefer-regex-literals
        return new RegExp(String.raw `[\p{L}\p{N}]`, 'u');
    }
    catch /* c8 ignore start */ {
        return /[^\W_]/u;
    }
    /* c8 ignore stop */
})();
/**
 * attributes of extension and HTML tags
 *
 * 扩展和HTML标签属性
 * @classdesc `{childNodes: (AtomToken|AttributeToken)[]}`
 */
let AttributesToken = (() => {
    let _classSuper = index_2.Token;
    let _instanceExtraInitializers = [];
    let _cloneNode_decorators;
    let _toHtmlInternal_decorators;
    return class AttributesToken extends _classSuper {
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _cloneNode_decorators = [clone_1.clone];
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _cloneNode_decorators, { kind: "method", name: "cloneNode", static: false, private: false, access: { has: obj => "cloneNode" in obj, get: obj => obj.cloneNode }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            if (_metadata) Object.defineProperty(this, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
        }
        #type = __runInitializers(this, _instanceExtraInitializers);
        /* NOT FOR BROWSER */
        #classList;
        /* NOT FOR BROWSER END */
        get type() {
            return this.#type;
        }
        /* NOT FOR BROWSER */
        /** all attributes / 全部属性 */
        get attributes() {
            return this.getAttrs();
        }
        set attributes(attrs) {
            this.replaceChildren();
            this.setAttr(attrs);
        }
        /** class attribute in string / 以字符串表示的class属性 */
        get className() {
            const attr = this.getAttr('class');
            return typeof attr === 'string' ? attr : '';
        }
        set className(className) {
            this.setAttr('class', className || false);
        }
        /** class attribute in Set / 以Set表示的class属性 */
        get classList() {
            if (!this.#classList) {
                this.#classList = new Set(this.className.split(/\s+/u));
                /**
                 * 更新classList
                 * @param prop 方法名
                 */
                const factory = (prop) => ({
                    value: /** @ignore */ (...args) => {
                        const result = Set.prototype[prop].apply(this.#classList, args);
                        this.setAttr('class', [...this.#classList].join(' '));
                        return result;
                    },
                });
                Object.defineProperties(this.#classList, {
                    add: factory('add'),
                    delete: factory('delete'),
                    clear: factory('clear'),
                });
            }
            return this.#classList;
        }
        /** id attribute / id属性 */
        get id() {
            const attr = this.getAttr('id');
            return typeof attr === 'string' ? attr : '';
        }
        set id(id) {
            this.setAttr('id', id || false);
        }
        /** whether to contain invalid attributes / 是否含有无效属性 */
        get sanitized() {
            return this.childNodes.filter(child => child instanceof atom_1.AtomToken && child.text().trim()).length === 0;
        }
        set sanitized(sanitized) {
            if (sanitized) {
                this.sanitize();
            }
        }
        /* NOT FOR BROWSER END */
        /**
         * @param attr 标签属性
         * @param type 标签类型
         * @param name 标签名
         */
        constructor(attr, type, name, config, accum = []) {
            super(undefined, config, accum, {
                AtomToken: ':', AttributeToken: ':',
            });
            this.#type = type;
            this.setAttribute('name', name);
            if (attr) {
                const regex = /([^\s/](?:(?!\0\d+~\x7F)[^\s/=])*)(?:((?:\s(?:\s|\0\d+[cn]\x7F)*)?(?:=|\0\d+~\x7F)(?:\s|\0\d+[cn]\x7F)*)(?:(["'])([\s\S]*?)(\3|$)|(\S*)))?/gu;
                let out = '', mt = regex.exec(attr), lastIndex = 0;
                const insertDirty = /** 插入无效属性 */ () => {
                    if (out) {
                        super.insertAt(new atom_1.AtomToken(out, toDirty(type), config, accum, {
                            [`Stage-${stages[type]}`]: ':',
                        }));
                        out = '';
                    }
                };
                while (mt) {
                    const { index, 0: full, 1: key, 2: equal, 3: quoteStart, 4: quoted, 5: quoteEnd, 6: unquoted } = mt;
                    out += attr.slice(lastIndex, index);
                    if (/^(?:[\w:]|\0\d+t\x7F)(?:[\w:.-]|\0\d+t\x7F)*$/u.test((0, string_1.removeComment)(key).trim())) {
                        const value = quoted ?? unquoted, quotes = [quoteStart, quoteEnd], 
                        // @ts-expect-error abstract class
                        token = new attribute_1.AttributeToken((0, exports.toAttributeType)(type), name, key, quotes, config, equal, value, accum);
                        insertDirty();
                        super.insertAt(token);
                    }
                    else {
                        out += full;
                    }
                    ({ lastIndex } = regex);
                    mt = regex.exec(attr);
                }
                out += attr.slice(lastIndex);
                insertDirty();
            }
        }
        /** @private */
        afterBuild() {
            const { parentNode } = this;
            if (parentNode?.is('td')) {
                this.setAttribute('name', parentNode.subtype);
            }
            super.afterBuild();
        }
        /**
         * Get all AttributeTokens with the specified attribute name
         *
         * 所有指定属性名的AttributeToken
         * @param key attribute name / 属性名
         */
        getAttrTokens(key) {
            return this.childNodes.filter((child) => child instanceof attribute_1.AttributeToken && (!key || child.name === (0, string_1.trimLc)(key)));
        }
        /**
         * Check if the token has a certain attribute
         *
         * 是否具有某属性
         * @param key attribute name / 属性键
         */
        hasAttr(key) {
            return this.getAttrTokens(key).length > 0;
        }
        /**
         * Get the last AttributeToken with the specified attribute name
         *
         * 指定属性名的最后一个AttributeToken
         * @param key attribute name / 属性名
         */
        getAttrToken(key) {
            LINT: {
                const tokens = this.getAttrTokens(key);
                return tokens[tokens.length - 1];
            }
        }
        /**
         * Get the attribute
         *
         * 获取指定属性
         * @param key attribute name / 属性键
         */
        getAttr(key) {
            LINT: return this.getAttrToken(key)?.getValue();
        }
        /** 是否位于闭合标签内 */
        #lint() {
            const { parentNode } = this;
            return parentNode?.type === 'html' && parentNode.closing && this.text().trim() !== '';
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), { parentNode, childNodes } = this, attrs = new Map(), duplicated = new Set(), rect = new rect_1.BoundingRect(this, start), rules = ['no-ignored', 'no-duplicate'], { lintConfig } = index_1.default, { computeEditInfo, fix } = lintConfig, s = ['closingTag', 'invalidAttributes', 'nonWordAttributes']
                    .map(k => lintConfig.getSeverity(rules[0], k));
                if (s[0] && this.#lint()) {
                    const e = (0, lint_1.generateForSelf)(this, rect, rules[0], 'attributes-of-closing-tag', s[0]);
                    if (computeEditInfo) {
                        const index = parentNode.getAbsoluteIndex();
                        e.suggestions = [
                            (0, lint_1.fixByRemove)(e),
                            (0, lint_1.fixByOpen)(index),
                        ];
                    }
                    errors.push(e);
                }
                for (const attr of childNodes) {
                    if (attr instanceof attribute_1.AttributeToken) {
                        const { name } = attr;
                        if (attrs.has(name)) {
                            duplicated.add(name);
                            attrs.get(name).push(attr);
                        }
                        else {
                            attrs.set(name, [attr]);
                        }
                    }
                    else {
                        const str = attr.text().trim(), severity = s[wordRegex.test(str) ? 1 : 2];
                        if (str && severity) {
                            const e = (0, lint_1.generateForChild)(attr, rect, rules[0], 'invalid-attribute', severity);
                            if (computeEditInfo) {
                                e.suggestions = [(0, lint_1.fixByRemove)(e, 0, ' ')];
                            }
                            errors.push(e);
                        }
                    }
                }
                const severity = lintConfig.getSeverity(rules[1], 'attribute');
                if (severity && duplicated.size > 0) {
                    for (const key of duplicated) {
                        const pairs = attrs.get(key).map(attr => {
                            const value = attr.getValue();
                            return [attr, value === true ? '' : value];
                        });
                        Array.prototype.push.apply(errors, pairs.map(([attr, value], i) => {
                            const e = (0, lint_1.generateForChild)(attr, rect, rules[1], index_1.default.msg('duplicate-attribute', key), severity);
                            if (computeEditInfo || fix) {
                                const remove = (0, lint_1.fixByRemove)(e);
                                if (!value || pairs.slice(0, i).some(([, v]) => v === value)) {
                                    e.fix = remove;
                                }
                                else if (computeEditInfo) {
                                    e.suggestions = [remove];
                                }
                            }
                            return e;
                        }));
                    }
                }
                return errors;
            }
        }
        /** @private */
        escape() {
            LSP: if (this.type !== 'ext-attrs') {
                super.escape();
            }
        }
        /* PRINT ONLY */
        /** @private */
        getAttribute(key) {
            /* NOT FOR BROWSER */
            if (key === 'padding') {
                return this.#leadingSpace(super.toString()).length;
            }
            /* NOT FOR BROWSER END */
            return key === 'invalid' ? this.#lint() : super.getAttribute(key);
        }
        /** @private */
        print() {
            PRINT: return this.toString()
                ? `<span class="wpb-${this.type}${this.#lint() ? ' wpb-invalid' : ''}">${this.childNodes.map(child => child.print(child instanceof atom_1.AtomToken ? { class: child.toString().trim() && 'attr-dirty' } : undefined)).join('')}</span>`
                : '';
        }
        /* PRINT ONLY END */
        /* NOT FOR BROWSER */
        /* c8 ignore start */
        /**
         * Sanitize invalid attributes
         *
         * 清理无效属性
         */
        sanitize() {
            require('../addon/attribute');
            this.sanitize();
        }
        /* c8 ignore stop */
        cloneNode() {
            // @ts-expect-error abstract class
            return new AttributesToken(undefined, this.type, this.name, this.getAttribute('config'));
        }
        /**
         * @override
         * @param token node to be inserted / 待插入的子节点
         * @param i position to be inserted at / 插入位置
         * @throws `RangeError` 标签不匹配
         */
        insertAt(token, i = this.length) {
            if (!(token instanceof attribute_1.AttributeToken)) {
                /* c8 ignore next 3 */
                if (!debug_1.Shadow.running && token.toString().trim()) {
                    this.constructorError('can only insert AttributeToken');
                }
                return super.insertAt(token, i);
            }
            const { type, name, length } = this;
            /* c8 ignore next 3 */
            if (token.type !== type.slice(0, -1) || token.tag !== name) {
                throw new RangeError(`The AttributeToken to be inserted can only be used for <${token.tag}> tag!`);
            }
            if (i === length) {
                const { lastChild } = this;
                if (lastChild instanceof attribute_1.AttributeToken) {
                    lastChild.close();
                }
            }
            else {
                token.close();
            }
            if (this.isInside('parameter')) {
                token.escape();
            }
            super.insertAt(token, i);
            const { previousVisibleSibling, nextVisibleSibling } = token, dirtyType = toDirty(type), config = this.getAttribute('config'), acceptable = { [`Stage-${stages[type]}`]: ':' };
            if (nextVisibleSibling && !/^\s/u.test(nextVisibleSibling.toString())) {
                super.insertAt(debug_1.Shadow.run(() => new atom_1.AtomToken(' ', dirtyType, config, [], acceptable)), i + 1);
            }
            if (previousVisibleSibling && !/\s$/u.test(previousVisibleSibling.toString())) {
                super.insertAt(debug_1.Shadow.run(() => new atom_1.AtomToken(' ', dirtyType, config, [], acceptable)), i);
            }
            return token;
        }
        setAttr(keyOrProp, value) {
            require('../addon/attribute');
            this.setAttr(keyOrProp, value);
        }
        /**
         * Get all attribute names
         *
         * 获取全部的属性名
         */
        getAttrNames() {
            return new Set(this.getAttrTokens().map(({ name }) => name));
        }
        /**
         * Get all attributes
         *
         * 获取全部属性
         */
        getAttrs() {
            return Object.fromEntries(this.getAttrTokens().map(({ name, value }) => [name, value]));
        }
        /**
         * Remove an attribute
         *
         * 移除指定属性
         * @param key attribute name / 属性键
         */
        removeAttr(key) {
            for (const attr of this.getAttrTokens(key)) {
                attr.remove();
            }
        }
        /* c8 ignore start */
        /**
         * Toggle the specified attribute
         *
         * 开关指定属性
         * @param key attribute name / 属性键
         * @param force whether to force enabling or disabling / 强制开启或关闭
         * @throws `RangeError` 不为Boolean类型的属性值
         */
        toggleAttr(key, force) {
            require('../addon/attribute');
            this.toggleAttr(key, force);
        }
        /* c8 ignore stop */
        /**
         * 生成引导空格
         * @param str 属性字符串
         */
        #leadingSpace(str) {
            const { type } = this, leadingRegex = { 'ext-attrs': /^\s/u, 'html-attrs': /^[/\s]/u };
            return str && type !== 'table-attrs' && !leadingRegex[type].test(str) ? ' ' : '';
        }
        /** @private */
        toString(skip) {
            if (this.type === 'table-attrs') {
                (0, string_1.normalizeSpace)(this);
            }
            const str = super.toString(skip);
            return this.#leadingSpace(str) + str;
        }
        /** @private */
        text() {
            if (this.type === 'table-attrs') {
                (0, string_1.normalizeSpace)(this);
            }
            const str = (0, string_1.text)(this.childNodes.filter(child => child instanceof attribute_1.AttributeToken), ' ');
            return this.#leadingSpace(str) + str;
        }
        /** @private */
        toHtmlInternal() {
            const map = new Map(this.childNodes.filter(child => child instanceof attribute_1.AttributeToken).map(child => [child.name, child])), output = map.size === 0 ? '' : (0, html_1.html)([...map.values()], ' ', { removeBlank: true });
            return output && ` ${output}`;
        }
        /* c8 ignore start */
        /**
         * Get the value of a style property
         *
         * 获取某一样式属性的值
         * @param key style property / 样式属性
         * @param value style property value / 样式属性值
         * @since v1.17.1
         */
        css(key, value) {
            require('../addon/attribute');
            return this.css(key, value);
        }
    };
})();
exports.AttributesToken = AttributesToken;
constants_1.classes['AttributesToken'] = __filename;
