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
exports.ArgToken = void 0;
const string_1 = require("../util/string");
const lint_1 = require("../util/lint");
const rect_1 = require("../lib/rect");
const padded_1 = require("../mixin/padded");
const gapped_1 = require("../mixin/gapped");
const noEscape_1 = require("../mixin/noEscape");
const index_1 = __importDefault(require("../index"));
const index_2 = require("./index");
const atom_1 = require("./atom");
const hidden_1 = require("./hidden");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
const debug_1 = require("../util/debug");
const cached_1 = require("../mixin/cached");
/* NOT FOR BROWSER END */
/**
 * argument wrapped in `{{{}}}`
 *
 * `{{{}}}`包裹的参数
 * @classdesc `{childNodes: [AtomToken, ?Token, ...HiddenToken[]]}`
 */
let ArgToken = (() => {
    let _classDecorators = [noEscape_1.noEscape, (0, padded_1.padded)('{{{'), (0, gapped_1.gapped)()];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.Token;
    let _instanceExtraInitializers = [];
    let _toHtmlInternal_decorators;
    var ArgToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _toHtmlInternal_decorators = [(0, cached_1.cached)()];
            __esDecorate(this, null, _toHtmlInternal_decorators, { kind: "method", name: "toHtmlInternal", static: false, private: false, access: { has: obj => "toHtmlInternal" in obj, get: obj => obj.toHtmlInternal }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            ArgToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'arg';
        }
        /* PRINT ONLY */
        /** default value / 预设值 */
        get default() {
            LSP: return this.childNodes[1]?.text() ?? false;
        }
        /* PRINT ONLY END */
        /* NOT FOR BROWSER */
        set default(value) {
            this.setDefault(value);
        }
        /* NOT FOR BROWSER END */
        /** @param parts 以'|'分隔的各部分 */
        constructor(parts, config, accum = []) {
            super(undefined, config, accum, {
                AtomToken: 0, Token: 1, HiddenToken: '2:',
            });
            __runInitializers(this, _instanceExtraInitializers);
            for (let i = 0; i < parts.length; i++) {
                const part = parts[i];
                if (i === 0) {
                    const token = new atom_1.AtomToken(part, 'arg-name', config, accum, {
                        'Stage-2': ':', '!HeadingToken': '',
                    });
                    super.insertAt(token);
                }
                else if (i > 1) {
                    const token = new hidden_1.HiddenToken(part, config, accum);
                    super.insertAt(token);
                }
                else {
                    const token = new index_2.Token(part, config, accum);
                    token.type = 'arg-default';
                    token.setAttribute('stage', 2);
                    super.insertAt(token);
                }
            }
            /* NOT FOR BROWSER */
            this.protectChildren(0);
        }
        /** @private */
        toString(skip) {
            return `{{{${super.toString(skip, '|')}}}}`;
        }
        /** @private */
        text() {
            return `{{{${(0, string_1.text)(this.childNodes.slice(0, 2), '|')}}}}`;
        }
        /** 更新name */
        #setName() {
            LSP: this.setAttribute('name', this.firstChild.text().trim());
        }
        /** @private */
        afterBuild() {
            LSP: this.#setName();
            super.afterBuild();
            /* NOT FOR BROWSER */
            const /** @implements */ argListener = ({ prevTarget }) => {
                if (prevTarget === this.firstChild) {
                    this.#setName();
                }
            };
            this.addEventListener(['remove', 'insert', 'replace', 'text'], argListener);
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const [argName, argDefault, ...rest] = this.childNodes;
                argName.setAttribute('aIndex', start + 3);
                const errors = argName.lint(start + 3, re);
                if (argDefault) {
                    const index = start + 4 + argName.toString().length;
                    argDefault.setAttribute('aIndex', index);
                    const childErrors = argDefault.lint(index, re);
                    if (childErrors.length > 0) {
                        Array.prototype.push.apply(errors, childErrors);
                    }
                }
                const rules = ['no-ignored', 'no-arg'], { lintConfig } = index_1.default, { computeEditInfo } = lintConfig, rect = new rect_1.BoundingRect(this, start), s = rules.map(rule => lintConfig.getSeverity(rule, 'arg'));
                if (s[0] && rest.length > 0) {
                    Array.prototype.push.apply(errors, rest.map(child => {
                        const e = (0, lint_1.generateForChild)(child, rect, rules[0], 'invisible-triple-braces', s[0]);
                        e.startIndex--;
                        e.startCol--;
                        if (computeEditInfo) {
                            e.suggestions = [
                                (0, lint_1.fixByRemove)(e),
                                (0, lint_1.fixByEscape)(e.startIndex, '{{!}}'),
                            ];
                        }
                        return e;
                    }));
                }
                if (s[1] && !this.getAttribute('include')) {
                    const e = (0, lint_1.generateForSelf)(this, rect, rules[1], 'unexpected-argument', s[1]);
                    if (computeEditInfo && argDefault) {
                        e.suggestions = [(0, lint_1.fixBy)(e, 'expand', argDefault.text())];
                    }
                    errors.push(e);
                }
                const ext = this.closest('ext');
                if (ext) {
                    const rule = 'arg-in-ext', severity = lintConfig.getSeverity(rule, ext.name);
                    if (severity) {
                        errors.push((0, lint_1.generateForSelf)(this, rect, rule, 'argument-in-ext', severity));
                    }
                }
                return errors;
            }
        }
        /** @private */
        print() {
            PRINT: return super.print({ pre: '{{{', post: '}}}', sep: '|' });
        }
        /** @private */
        json(_, depth, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = super.json(undefined, depth, start);
                json['default'] = this.default;
                return json;
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const [name, ...cloned] = this.cloneChildNodes();
            return debug_1.Shadow.run(() => {
                // @ts-expect-error abstract class
                const token = new ArgToken([''], this.getAttribute('config'));
                token.firstChild.safeReplaceWith(name);
                token.safeAppend(cloned);
                return token;
            });
        }
        /**
         * Remove redundant parts
         *
         * 移除无效部分
         */
        removeRedundant() {
            debug_1.Shadow.run(() => {
                for (let i = this.length - 1; i > 1; i--) {
                    super.removeAt(i);
                }
            });
        }
        /**
         * @override
         * @param i position of the child node / 移除位置
         */
        removeAt(i) {
            if (i === 1) {
                this.removeRedundant();
            }
            return super.removeAt(i);
        }
        /**
         * @override
         * @param token node to be inserted / 待插入的子节点
         * @param i position to be inserted at / 插入位置
         */
        insertAt(token, i = this.length) {
            i += i < 0 ? this.length : 0;
            if (debug_1.Shadow.running) {
                //
            }
            else if (i > 1) {
                this.constructorError('cannot insert redundant child nodes');
            }
            else if (typeof token === 'string') {
                this.constructorError('cannot insert text nodes');
            }
            super.insertAt(token, i);
            if (i === 1) {
                token.type = 'arg-default';
            }
            return token;
        }
        /**
         * Set the argument name
         *
         * 设置参数名
         * @param name new argument name / 新参数名
         */
        setName(name) {
            const { childNodes } = index_1.default.parseWithRef(name, this, 2);
            this.firstChild.safeReplaceChildren(childNodes);
        }
        /**
         * Set the default value
         *
         * 设置预设值
         * @param value default value / 预设值
         */
        setDefault(value) {
            if (value === false) {
                this.removeAt(1);
                return;
            }
            const [, oldDefault] = this.childNodes, root = index_1.default.parseWithRef(value, this);
            if (oldDefault) {
                oldDefault.safeReplaceChildren(root.childNodes);
            }
            else {
                root.type = 'arg-default';
                this.insertAt(root);
            }
        }
        /** @private */
        toHtmlInternal(opt) {
            if (this.length === 1) {
                const html = this.toString();
                return opt?.nowrap ? html.replaceAll('\n', ' ') : html;
            }
            return this.childNodes[1].toHtmlInternal(opt);
        }
    };
    return ArgToken = _classThis;
})();
exports.ArgToken = ArgToken;
constants_1.classes['ArgToken'] = __filename;
