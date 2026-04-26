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
const transclude_1 = require("../src/transclude");
const parameter_1 = require("../src/parameter");
const atom_1 = require("../src/atom");
/**
 * 调整最后一个子节点的换行符
 * @param token 魔术字或模板节点
 */
const format = (token) => {
    const { lastChild, type } = token, isParameter = lastChild.is('parameter');
    if (!(type === 'template' ? isParameter && lastChild.anon : lastChild.is('magic-word-name'))
        && !lastChild.toString().endsWith('\n')) {
        (isParameter ? lastChild.lastChild : lastChild).insertAt('\n');
    }
};
transclude_1.TranscludeToken.prototype.newAnonArg =
    /** @implements */
    function (val, newline) {
        const { childNodes } = index_1.default.parseWithRef(val, this), token = debug_1.Shadow.run(
        // @ts-expect-error abstract class
        () => new parameter_1.ParameterToken(undefined, undefined, this.getAttribute('config')));
        token.lastChild.concat(childNodes); // eslint-disable-line unicorn/prefer-spread
        if (newline) {
            format(this);
        }
        return this.insertAt(token);
    };
transclude_1.TranscludeToken.prototype.setValue =
    /** @implements */
    function (key, value, newline) {
        /* c8 ignore next 3 */
        if (!this.isTemplate()) {
            throw new Error('TranscludeToken.setValue method is only for templates!');
        }
        const arg = this.getArg(key);
        if (arg) {
            arg.setValue(value);
            return;
        }
        const k = index_1.default.parseWithRef(key, this), v = index_1.default.parseWithRef(value, this), token = debug_1.Shadow.run(
        // @ts-expect-error abstract class
        () => new parameter_1.ParameterToken(undefined, undefined, this.getAttribute('config')));
        token.firstChild.safeAppend([...k.childNodes]);
        token.lastChild.concat(v.childNodes); // eslint-disable-line unicorn/prefer-spread
        if (newline) {
            format(this);
        }
        this.insertAt(token);
    };
transclude_1.TranscludeToken.prototype.replaceTemplate =
    /** @implements */
    function (title) {
        const { type, firstChild } = this;
        /* c8 ignore next 3 */
        if (type === 'magic-word') {
            throw new Error('TranscludeToken.replaceTemplate method is only for templates!');
        }
        const { childNodes } = index_1.default.parseWithRef(title, this, 2);
        firstChild.safeReplaceChildren(childNodes);
    };
transclude_1.TranscludeToken.prototype.replaceModule =
    /** @implements */
    function (title) {
        const { type, name, length, childNodes: [, mod] } = this;
        /* c8 ignore next 3 */
        if (type !== 'magic-word' || name !== 'invoke') {
            throw new Error('TranscludeToken.replaceModule method is only for modules!');
        }
        if (length === 1) {
            index_2.Token.prototype.insertAt.call(this, debug_1.Shadow.run(() => new atom_1.AtomToken(undefined, 'invoke-module', this.getAttribute('config'), [], { 'Stage-1': ':', '!ExtToken': '' })));
            return;
        }
        const { childNodes } = index_1.default.parseWithRef(title, this, 2);
        mod.safeReplaceChildren(childNodes);
    };
transclude_1.TranscludeToken.prototype.replaceFunction =
    /** @implements */
    function (func) {
        const { type, name, length, childNodes: [, , fun] } = this;
        /* c8 ignore next 6 */
        if (type !== 'magic-word' || name !== 'invoke') {
            throw new Error('TranscludeToken.replaceModule method is only for modules!');
        }
        if (length < 2) {
            throw new Error('No module name specified!');
        }
        if (length === 2) {
            index_2.Token.prototype.insertAt.call(this, debug_1.Shadow.run(() => new atom_1.AtomToken(undefined, 'invoke-function', this.getAttribute('config'), [], { 'Stage-1': ':', '!ExtToken': '' })));
            return;
        }
        const { childNodes } = index_1.default.parseWithRef(func, this, 2);
        fun.safeReplaceChildren(childNodes);
    };
transclude_1.TranscludeToken.prototype.fixDuplication =
    /** @implements */
    function (aggressive) {
        if (!this.hasDuplicatedArgs()) {
            return [];
        }
        const duplicatedKeys = [];
        let { anonCount } = this;
        for (const [key, args] of this.getDuplicatedArgs()) {
            if (args.length <= 1) {
                continue;
            }
            const values = new Map();
            for (const arg of args) {
                const val = arg.getValue().trim();
                if (values.has(val)) {
                    values.get(val).push(arg);
                }
                else {
                    values.set(val, [arg]);
                }
            }
            let noMoreAnon = anonCount === 0 || !key.trim() || !Number.isInteger(Number(key));
            const emptyArgs = values.get('') ?? [], duplicatedArgs = [...values].filter(([val, { length }]) => val && length > 1).flatMap(([, curArgs]) => {
                const anonIndex = noMoreAnon ? -1 : curArgs.findIndex(({ anon }) => anon);
                if (anonIndex !== -1) {
                    noMoreAnon = true;
                }
                curArgs.splice(anonIndex, 1);
                return curArgs;
            }), badArgs = [...emptyArgs, ...duplicatedArgs], index = noMoreAnon ? -1 : emptyArgs.findIndex(({ anon }) => anon);
            if (badArgs.length === args.length) {
                badArgs.splice(index, 1);
            }
            else if (index !== -1) {
                this.anonToNamed();
                anonCount = 0;
            }
            for (const arg of badArgs) {
                arg.remove();
            }
            let remaining = args.length - badArgs.length;
            if (remaining === 1) {
                continue;
            }
            else if (aggressive && (anonCount ? /\D\d+$/u : /(?:^|\D)\d+$/u).test(key)) {
                let last;
                const str = key.slice(0, -/(?<!\d)\d+$/u.exec(key)[0].length);
                /^a\d+$/u; // eslint-disable-line @typescript-eslint/no-unused-expressions
                const regex = new RegExp(String.raw `^${(0, string_1.escapeRegExp)(str)}\d+$`, 'u'), series = this.getAllArgs().filter(({ name }) => regex.test(name)), ordered = series.every(({ name }, i) => {
                    const j = Number(name.slice(str.length)), cmp = j <= i + 1 && (i === 0 || j >= last || name === key);
                    last = j;
                    return cmp;
                });
                if (ordered) {
                    for (let i = 0; i < series.length; i++) {
                        const arg = series[i], name = `${str}${i + 1}`;
                        if (arg.name !== name) {
                            if (arg.name === key) {
                                remaining--;
                            }
                            arg.rename(name, true);
                        }
                    }
                }
            }
            if (remaining > 1) {
                index_1.default.error(`${this.type === 'template'
                    ? this.name
                    : this.normalizeTitle(`Module:${this.childNodes[1].text()}`, 828, { temporary: true, page: '' }).title} still has ${remaining} duplicated ${key} parameters:\n${[...this.getArgs(key)].map(arg => {
                    const { top, left } = arg.getBoundingClientRect();
                    return `Line ${String(top)} Column ${String(left)}`;
                }).join('\n')}`);
                duplicatedKeys.push(key);
            }
        }
        return duplicatedKeys;
    };
transclude_1.TranscludeToken.prototype.escapeTables =
    /** @implements */
    function () {
        if (!/\n[^\S\n]*(?::+[^\S\n]*)?\{\|/u.test(this.text())) {
            return this;
        }
        const stripped = this.toString().slice(2, -2), parsed = debug_1.Shadow.internal(() => {
            const token = index_1.default.parseWithRef(stripped, this, 4);
            for (const table of token.childNodes) {
                if (table.is('table')) {
                    table.escape();
                }
            }
            return token;
        }, index_1.default);
        const { firstChild, length } = index_1.default.parseWithRef(`{{${parsed.toString()}}}`, this);
        /* c8 ignore next 3 */
        if (length !== 1 || !(firstChild instanceof transclude_1.TranscludeToken)) {
            throw new Error('Failed to escape tables!');
        }
        this.safeReplaceWith(firstChild);
        return firstChild;
    };
constants_1.classes['ExtendedTranscludeToken'] = __filename;
