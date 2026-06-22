"use strict";
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
var __runInitializers = (this && this.__runInitializers) || function (thisArg, initializers, value) {
    var useValue = arguments.length > 2;
    for (var i = 0; i < initializers.length; i++) {
        value = useValue ? initializers[i].call(thisArg, value) : initializers[i].call(thisArg);
    }
    return useValue ? value : void 0;
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.IncludeToken = void 0;
const lint_1 = require("../../util/lint");
const rect_1 = require("../../lib/rect");
const hidden_1 = require("../../mixin/hidden");
const index_1 = __importDefault(require("../../index"));
const index_2 = require("./index");
/* NOT FOR BROWSER */
const debug_1 = require("../../util/debug");
const constants_1 = require("../../util/constants");
/* NOT FOR BROWSER END */
/**
 * `<includeonly>`, `<noinclude>` or `<onlyinclude>`
 *
 * `<includeonly>`或`<noinclude>`或`<onlyinclude>`
 * @classdesc `{childNodes: [AstText, AstText]}`
 */
let IncludeToken = (() => {
    let _classDecorators = [(0, hidden_1.hiddenToken)(false)];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.TagPairToken;
    var IncludeToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            IncludeToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /* NOT FOR BROWSER END */
        get type() {
            return 'include';
        }
        /* NOT FOR BROWSER */
        get innerText() {
            return super.innerText;
        }
        set innerText(text) {
            if (text === undefined) {
                this.selfClosing = true;
            }
            else {
                this.selfClosing = false;
                this.setText(text);
            }
        }
        /* NOT FOR BROWSER END */
        /**
         * @param name 标签名
         * @param attr 标签属性
         * @param inner 内部wikitext
         * @param closed 是否封闭
         */
        constructor(name, attr = '', inner, closed, config, accum) {
            super(name, attr, inner ?? '', inner === undefined ? closed : closed ?? '', config, accum);
        }
        /** @private */
        toString(skip) {
            return skip ? '' : super.toString();
        }
        /** @private */
        lint(start = this.getAbsoluteIndex()) {
            LINT: {
                const errors = [], { firstChild, closed, name } = this, rect = new rect_1.BoundingRect(this, start), rules = ['no-ignored', 'unclosed-comment'], { lintConfig } = index_1.default, { computeEditInfo } = lintConfig, s = rules.map(rule => lintConfig.getSeverity(rule, 'include'));
                if (s[0] && firstChild.data.trim()) {
                    const e = (0, lint_1.generateForChild)(firstChild, rect, rules[0], 'useless-attribute', s[0]);
                    if (computeEditInfo) {
                        e.suggestions = [(0, lint_1.fixByRemove)(e)];
                    }
                    errors.push(e);
                }
                if (s[1] && !closed) {
                    const e = (0, lint_1.generateForSelf)(this, rect, rules[1], index_1.default.msg('unclosed', `<${name}>`), s[1]);
                    if (computeEditInfo) {
                        e.suggestions = [(0, lint_1.fixByClose)(e.endIndex, `</${name}>`)];
                    }
                    errors.push(e);
                }
                return errors;
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const tags = this.getAttribute('tags'), { innerText, firstChild: { data }, selfClosing, closed } = this;
            // @ts-expect-error abstract class
            return debug_1.Shadow.run(() => new IncludeToken(tags[0], data, innerText, selfClosing || !closed ? undefined : tags[1], this.getAttribute('config')));
        }
        /**
         * @override
         * @param str new text / 新文本
         */
        setText(str) {
            return super.setText(str, 1);
        }
        /**
         * Remove tag attributes
         *
         * 清除标签属性
         */
        removeAttr() {
            super.setText('');
        }
    };
    return IncludeToken = _classThis;
})();
exports.IncludeToken = IncludeToken;
constants_1.classes['IncludeToken'] = __filename;
