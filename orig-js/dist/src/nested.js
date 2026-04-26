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
exports.NestedToken = void 0;
const lint_1 = require("../util/lint");
const rect_1 = require("../lib/rect");
const commentAndExt_1 = require("../parser/commentAndExt");
const braces_1 = require("../parser/braces");
const index_1 = __importDefault(require("../index"));
const index_2 = require("./index");
const ext_1 = require("./tagPair/ext");
const noinclude_1 = require("./nowiki/noinclude");
/* NOT FOR BROWSER */
const debug_1 = require("../util/debug");
const constants_1 = require("../util/constants");
const clone_1 = require("../mixin/clone");
const childTypes = new Set(['comment', 'include', 'arg', 'template', 'magic-word']), lintRegex = /* #__PURE__ */ (() => [false, true].map(article => {
    const noinclude = article ? 'includeonly' : 'noinclude';
    // eslint-disable-next-line @typescript-eslint/no-unused-expressions
    /^(?:<noinclude(?:\s[^>]*)?\/?>|<\/noinclude\s*>)$/iu;
    return new RegExp(String.raw `^(?:<${noinclude}(?:\s[^>]*)?/?>|</${noinclude}\s*>)$`, 'iu');
}))();
/**
 * extension tag that has a nested structure
 *
 * 嵌套式的扩展标签
 * @classdesc `{childNodes: (ExtToken|NoincludeToken|CommentToken)[]}`
 */
let NestedToken = (() => {
    let _classSuper = index_2.Token;
    let _instanceExtraInitializers = [];
    let _cloneNode_decorators;
    return class NestedToken extends _classSuper {
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _cloneNode_decorators = [clone_1.clone];
            __esDecorate(this, null, _cloneNode_decorators, { kind: "method", name: "cloneNode", static: false, private: false, access: { has: obj => "cloneNode" in obj, get: obj => obj.cloneNode }, metadata: _metadata }, null, _instanceExtraInitializers);
            if (_metadata) Object.defineProperty(this, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
        }
        #tags = __runInitializers(this, _instanceExtraInitializers);
        #regex;
        /* NOT FOR BROWSER END */
        get type() {
            return 'ext-inner';
        }
        /**
         * @param regex 内层正则
         * @param tags 内层标签名
         */
        constructor(wikitext, regex, tags, config, accum = []) {
            if (typeof regex === 'boolean') {
                const placeholder = Symbol('NestedToken'), { length } = accum;
                accum.push(placeholder);
                wikitext &&= (0, commentAndExt_1.parseCommentAndExt)(wikitext, config, accum, regex);
                wikitext &&= (0, braces_1.parseBraces)(wikitext, config, accum);
                accum.splice(length, 1);
            }
            else {
                wikitext &&= wikitext.replace(regex, (_, name, attr, inner, closing) => {
                    const str = `\0${accum.length + 1}e\x7F`;
                    // @ts-expect-error abstract class
                    new ext_1.ExtToken(name, attr, inner, closing, config, false, accum);
                    return str;
                });
            }
            wikitext &&= wikitext.replace(/(^|\0\d+.\x7F)([^\0]+)(?=$|\0\d+.\x7F)/gu, (_, lead, substr) => {
                // @ts-expect-error abstract class
                new noinclude_1.NoincludeToken(substr, config, accum);
                return `${lead}\0${accum.length}n\x7F`;
            });
            super(wikitext, config, accum, typeof regex === 'boolean' ? { 'Stage-2': ':', '!HeadingToken': '' } : { NoincludeToken: ':', ExtToken: ':' });
            this.#tags = [...tags];
            this.#regex = regex;
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = super.lint(start, re), rule = 'no-ignored', { lintConfig } = index_1.default, s = lintConfig.getSeverity(rule, this.name);
                if (!s) {
                    return errors;
                }
                const rect = new rect_1.BoundingRect(this, start), regex = typeof this.#regex === 'boolean' ? lintRegex[this.#regex ? 1 : 0] : /^<!--[\s\S]*-->$/u;
                return [
                    ...errors,
                    ...this.childNodes.filter(child => {
                        const { type, name } = child;
                        if (type === 'ext') {
                            return !this.#tags.includes(name);
                        }
                        else if (childTypes.has(type)) {
                            return false;
                        }
                        const str = child.toString().trim();
                        return str && !regex.test(str);
                    }).map(child => {
                        const e = (0, lint_1.generateForChild)(child, rect, rule, index_1.default.msg('invalid-content', this.name), s);
                        if (lintConfig.computeEditInfo) {
                            e.suggestions = [
                                (0, lint_1.fixByRemove)(e),
                                (0, lint_1.fixByComment)(e, child.toString()),
                            ];
                        }
                        return e;
                    }),
                ];
            }
        }
        /* NOT FOR BROWSER */
        /**
         * @override
         * @param token node to be inserted / 待插入的子节点
         * @param i position to be inseted at / 插入位置
         */
        insertAt(token, i) {
            if (!debug_1.Shadow.running && token.is('ext') && !this.#tags.includes(token.name)) {
                this.constructorError(`can only have ${this.#tags.join(' or ')} tags as child nodes`);
            }
            return super.insertAt(token, i);
        }
        cloneNode() {
            // @ts-expect-error abstract class
            return new NestedToken(undefined, this.#regex, this.#tags, this.getAttribute('config'));
        }
    };
})();
exports.NestedToken = NestedToken;
constants_1.classes['NestedToken'] = __filename;
