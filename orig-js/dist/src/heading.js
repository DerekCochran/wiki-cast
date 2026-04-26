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
exports.HeadingToken = void 0;
const lint_1 = require("../util/lint");
const debug_1 = require("../util/debug");
const rect_1 = require("../lib/rect");
const noEscape_1 = require("../mixin/noEscape");
const index_1 = __importDefault(require("../index"));
const index_2 = require("./index");
const syntax_1 = require("./syntax");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
const string_1 = require("../util/string");
const html_1 = require("../util/html");
const fixed_1 = require("../mixin/fixed");
const sol_1 = require("../mixin/sol");
const cached_1 = require("../mixin/cached");
/* NOT FOR BROWSER END */
/**
 * section heading
 *
 * 章节标题
 * @classdesc `{childNodes: [Token, SyntaxToken]}`
 */
let HeadingToken = (() => {
    let _classDecorators = [fixed_1.fixedToken, (0, sol_1.sol)(), noEscape_1.noEscape];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.Token;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    var HeadingToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            HeadingToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        #level = __runInitializers(this, _instanceExtraInitializers);
        /* NOT FOR BROWSER END */
        get type() {
            return 'heading';
        }
        /** level of the heading / 标题层级 */
        get level() {
            return this.#level;
        }
        /* NOT FOR BROWSER */
        set level(n) {
            this.setLevel(n);
        }
        /** inner wikitext / 内部wikitext */
        get innerText() {
            return this.firstChild.text().trim();
        }
        /** @throws `Error` 首尾包含`=` */
        set innerText(text) {
            if (text.length > 1 && text.startsWith('=') && text.endsWith('=')) {
                throw new Error('Please use HeadingToken.setLevel method to change the level of the heading!');
            }
            const { childNodes } = index_1.default.parseWithRef(text, this);
            this.firstChild.safeReplaceChildren(childNodes);
        }
        /**
         * id attribute
         *
         * id属性
         * @since v1.12.4
         */
        get id() {
            return this.#getId(true);
        }
        /* NOT FOR BROWSER END */
        /**
         * @param level 标题层级
         * @param input 标题文字
         */
        constructor(level, input, config, accum = []) {
            super(undefined, config, accum);
            this.#level = level;
            const token = new index_2.Token(input[0], config, accum);
            token.type = 'heading-title';
            token.setAttribute('stage', 2);
            const trail = new syntax_1.SyntaxToken(input[1], /^\s*$/u, 'heading-trail', config, accum, { 'Stage-1': ':', '!ExtToken': '' });
            this.append(token, trail);
        }
        /** 标题格式的等号 */
        #getEquals() {
            return '='.repeat(this.level);
        }
        /** @private */
        toString(skip) {
            const equals = this.#getEquals();
            return equals + this.firstChild.toString(skip) + equals + this.lastChild.toString(skip);
        }
        /** @private */
        text() {
            const equals = this.#getEquals();
            return equals + this.firstChild.text() + equals;
        }
        /** @private */
        getAttribute(key) {
            /* PRINT ONLY */
            if (key === 'invalid') {
                return (this.inHtmlAttrs() === 2);
            }
            /* PRINT ONLY END */
            return key === 'padding' ? this.level : super.getAttribute(key);
        }
        /** @private */
        getGaps() {
            return this.level;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), { firstChild, level } = this, innerStr = firstChild.toString(), unbalancedStart = innerStr.startsWith('='), unbalanced = unbalancedStart || innerStr.endsWith('='), rect = new rect_1.BoundingRect(this, start), s = this.inHtmlAttrs(), rules = ['h1', 'unbalanced-header', 'format-leakage'], { lintConfig } = index_1.default, { computeEditInfo, fix } = lintConfig, severities = rules.map(rule => lintConfig.getSeverity(rule, 'apostrophe'));
                if (severities[0] && this.level === 1) {
                    const e = (0, lint_1.generateForChild)(firstChild, rect, rules[0], '<h1>', severities[0]);
                    if (computeEditInfo && !unbalanced) {
                        e.suggestions = [(0, lint_1.fixBy)(e, 'h2', `=${innerStr}=`)];
                    }
                    errors.push(e);
                }
                if (severities[1] && unbalanced) {
                    const msg = index_1.default.msg('unbalanced-in-section-header', '"="'), e = (0, lint_1.generateForChild)(firstChild, rect, rules[1], msg, severities[1]);
                    if (!computeEditInfo || innerStr === '=') {
                        //
                    }
                    else if (unbalancedStart) {
                        const [extra] = /^=+/u.exec(innerStr), newLevel = level + extra.length;
                        e.suggestions = [{ desc: `h${level}`, range: [e.startIndex, e.startIndex + extra.length], text: '' }];
                        if (newLevel < 7) {
                            e.suggestions.push({ desc: `h${newLevel}`, range: [e.endIndex, e.endIndex], text: extra });
                        }
                    }
                    else {
                        const extra = /[^=](=+)$/u.exec(innerStr)[1], newLevel = level + extra.length;
                        e.suggestions = [{ desc: `h${level}`, range: [e.endIndex - extra.length, e.endIndex], text: '' }];
                        if (newLevel < 7) {
                            e.suggestions.push({ desc: `h${newLevel}`, range: [e.startIndex, e.startIndex], text: extra });
                        }
                    }
                    errors.push(e);
                }
                if (s) {
                    const rule = 'parsing-order', severity = lintConfig.getSeverity(rule, s === 2 ? 'heading' : 'templateInTable');
                    if (severity) {
                        errors.push((0, lint_1.generateForSelf)(this, rect, rule, 'header-in-html', severity));
                    }
                }
                if (severities[2]) {
                    const rootStr = this.getRootNode().toString(), quotes = firstChild.childNodes.filter((0, debug_1.isToken)('quote')), boldQuotes = quotes.filter(({ bold }) => bold), italicQuotes = quotes.filter(({ italic }) => italic);
                    if (boldQuotes.length % 2) {
                        const e = (0, lint_1.generateForChild)(boldQuotes[boldQuotes.length - 1], {
                            ...rect, // eslint-disable-line @typescript-eslint/no-misused-spread
                            start: start + level,
                            left: rect.left + level,
                        }, rules[2], index_1.default.msg('unbalanced-in-section-header', 'bold-apostrophes'), severities[2]);
                        if (computeEditInfo || fix) {
                            const end = start + level + innerStr.length, remove = (0, lint_1.fixByRemove)(e);
                            if (rootStr.slice(e.endIndex, end).trim()) {
                                if (computeEditInfo) {
                                    e.suggestions = [
                                        remove,
                                        (0, lint_1.fixByClose)(end, `'''`),
                                    ];
                                }
                            }
                            else if (boldQuotes.length === 1 && italicQuotes.length === 0) {
                                e.fix = remove;
                            }
                            else if (computeEditInfo) {
                                e.suggestions = [remove];
                            }
                        }
                        errors.push(e);
                    }
                    if (italicQuotes.length % 2) {
                        const e = (0, lint_1.generateForChild)(italicQuotes[italicQuotes.length - 1], { start: start + level }, rules[2], index_1.default.msg('unbalanced-in-section-header', 'italic-apostrophes'), severities[2]);
                        if (computeEditInfo || fix) {
                            const end = start + level + innerStr.length;
                            if (rootStr.slice(e.endIndex, end).trim()) {
                                if (computeEditInfo) {
                                    e.suggestions = [(0, lint_1.fixByClose)(end, `''`)];
                                }
                            }
                            else if (italicQuotes.length === 1 && boldQuotes.length === 0) {
                                e.fix = (0, lint_1.fixByRemove)(e);
                            }
                            else if (computeEditInfo) {
                                e.suggestions = [(0, lint_1.fixByRemove)(e)];
                            }
                        }
                        errors.push(e);
                    }
                }
                return errors;
            }
        }
        /** @private */
        print() {
            PRINT: {
                const equals = this.#getEquals();
                return super.print({ pre: equals, sep: equals });
            }
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start);
                json['level'] = this.level;
                return json;
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const [title, trail] = this.cloneChildNodes();
            return debug_1.Shadow.run(() => {
                // @ts-expect-error abstract class
                const token = new HeadingToken(this.level, [], this.getAttribute('config'));
                token.firstChild.safeReplaceWith(title);
                token.lastChild.safeReplaceWith(trail);
                return token;
            });
        }
        /**
         * Set the level of heading
         *
         * 设置标题层级
         * @param n level of heading / 标题层级
         */
        setLevel(n) {
            this.#level = Math.min(Math.max(n, 1), 6);
        }
        /**
         * Remove the invisible content following the heading
         *
         * 移除标题后的不可见内容
         */
        removeTrail() {
            this.lastChild.replaceChildren();
        }
        /**
         * id属性
         * @param expand 是否展开模板
         */
        #getId(expand) {
            return (0, html_1.getId)(this.firstChild[expand ? 'expand' : 'cloneNode']());
        }
        /** @private */
        toHtmlInternal() {
            let id = this.#getId();
            const { level, firstChild } = this, lcId = id.toLowerCase(), headings = constants_1.states.get(this.getRootNode())?.headings;
            if (headings?.has(lcId)) {
                let i = 2;
                for (; headings.has(`${lcId}_${i}`); i++) {
                    //
                }
                id = `${id}_${i}`;
                headings.add(`${lcId}_${i}`);
            }
            else {
                headings?.add(lcId);
            }
            return `<div class="mw-heading mw-heading${level}"><h${level} id="${(0, string_1.sanitizeId)(id)}">${firstChild.toHtmlInternal().trim()}</h${level}></div>`;
        }
        /**
         * Get the section led by this heading
         *
         * 获取由此标题引导的章节
         * @since v1.30.0
         */
        section() {
            const { parentNode, level } = this;
            if (!parentNode) {
                return undefined;
            }
            const range = this.createRange(), { childNodes, length } = parentNode;
            let i = childNodes.indexOf(this);
            range.setStart(parentNode, i);
            for (i++; i < length; i++) {
                const sibling = childNodes[i];
                if (sibling.is('heading') && sibling.level <= level) {
                    break;
                }
            }
            range.setEnd(parentNode, i);
            return range;
        }
    };
    return HeadingToken = _classThis;
})();
exports.HeadingToken = HeadingToken;
constants_1.classes['HeadingToken'] = __filename;
