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
exports.TableBaseToken = exports.escapeTable = void 0;
const debug_1 = require("../../util/debug");
const attributesParent_1 = require("../../mixin/attributesParent");
const index_1 = __importDefault(require("../../index"));
const index_2 = require("../index");
const syntax_1 = require("../syntax");
const attributes_1 = require("../attributes");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
/**
 * 转义表格语法
 * @param syntax 表格语法节点
 */
const escapeTable = (syntax) => {
    const wikitext = syntax.childNodes.map(child => child.type === 'text'
        ? child.data.replace(/\|{1,2}/gu, ({ length }) => `{{${'!'.repeat(length)}}}`)
        : child.toString()).join(''), { childNodes } = index_1.default.parseWithRef(wikitext, syntax, 2);
    debug_1.Shadow.run(() => {
        syntax.safeReplaceChildren(childNodes);
    });
};
exports.escapeTable = escapeTable;
/**
 * table row that contains the newline at the beginning but not at the end
 *
 * 表格行，含开头的换行，不含结尾的换行
 * @classdesc `{childNodes: [SyntaxToken, AttributesToken, ...Token[]]}`
 */
let TableBaseToken = (() => {
    let _classDecorators = [(0, attributesParent_1.attributesParent)(1)];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = index_2.Token;
    var TableBaseToken = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            TableBaseToken = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /* NOT FOR BROWSER END */
        /**
         * @param pattern 表格语法正则
         * @param syntax 表格语法
         * @param type 节点类型
         * @param attr 表格属性
         */
        constructor(pattern, syntax, type, attr, config, accum = [], acceptable) {
            super(undefined, config, accum, acceptable);
            this.append(new syntax_1.SyntaxToken(syntax, pattern, 'table-syntax', config, accum, { 'Stage-1': ':', '!ExtToken': '', TranscludeToken: ':' }), 
            // @ts-expect-error abstract class
            new attributes_1.AttributesToken(attr, 'table-attrs', type, config, accum));
            /* NOT FOR BROWSER */
            this.protectChildren([0, 1]);
        }
        /** @private */
        escape() {
            LSP: for (const child of this.childNodes) {
                if (child instanceof syntax_1.SyntaxToken) {
                    (0, exports.escapeTable)(child);
                }
                else {
                    child.escape();
                }
            }
        }
        /* NOT FOR BROWSER */
        cloneNode() {
            const [syntax, attr, ...cloned] = this.cloneChildNodes();
            return debug_1.Shadow.run(() => {
                const C = this.constructor, token = new C(undefined, undefined, this.getAttribute('config'));
                token.firstChild.safeReplaceWith(syntax);
                token.childNodes[1].safeReplaceWith(attr);
                if (token.is('td')) { // TdToken
                    token.childNodes[2].safeReplaceWith(cloned[0]);
                }
                else {
                    token.safeAppend(cloned);
                }
                return token;
            });
        }
    };
    return TableBaseToken = _classThis;
})();
exports.TableBaseToken = TableBaseToken;
constants_1.classes['TableBaseToken'] = __filename;
