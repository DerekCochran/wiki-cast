"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.expandToken = void 0;
const fs_1 = __importDefault(require("fs"));
const path_1 = __importDefault(require("path"));
const constants_1 = require("../util/constants");
const debug_1 = require("../util/debug");
const string_1 = require("../util/string");
const redirect_1 = require("../parser/redirect");
const magicWords_1 = require("./magicWords");
const index_1 = __importDefault(require("../index"));
const index_2 = require("../src/index");
const solvedMagicWords = new Set([
    'if',
    'ifeq',
    'ifexist',
    'iferror',
    'switch',
]);
/**
 * 隐式换行
 * @param str 字符串
 * @param prev 前一个字符
 */
const implicitNewLine = (str, prev) => prev + (prev !== '\n' && /^(?:\{\||[:;#*])/u.test(str) ? `\n${str}` : str);
/**
 * 加载模板
 * @param title 模板名
 * @param config
 */
const loadTemplate = (title, config) => {
    if (index_1.default.templates.has(title)) {
        return title;
    }
    else if (index_1.default.templateDir === undefined) {
        return false;
    }
    else if (!path_1.default.isAbsolute(index_1.default.templateDir)) {
        index_1.default.templateDir = path_1.default.join(__dirname, '..', '..', index_1.default.templateDir);
    }
    const file = fs_1.default.readdirSync(index_1.default.templateDir, { withFileTypes: true, recursive: true })
        .filter(dirent => dirent.isFile())
        .find(({ name, parentPath }) => {
        const t = path_1.default.relative(index_1.default.templateDir, path_1.default.join(parentPath, name.replace(/\.(?:wiki|txt)$/iu, ''))).replaceAll('꞉', ':');
        try {
            return decodeURIComponent(t) === title;
        }
        catch {
            return t === title;
        }
    });
    if (!file) {
        return false;
    }
    const content = (0, string_1.tidy)(fs_1.default.readFileSync(path_1.default.join(file.parentPath, file.name), 'utf8')), accum = [], parsed = debug_1.Shadow.run(() => (0, redirect_1.parseRedirect)(content, config, accum));
    if (parsed) {
        return loadTemplate(accum[0].lastChild.getTitle().title, config);
    }
    index_1.default.templates.set(title, content);
    return title;
};
/**
 * 清理已展开的节点
 * @param accum
 * @param tokens 要清理的节点
 */
const clean = (accum, tokens) => {
    for (const t of Array.isArray(tokens) ? tokens.map(({ lastChild }) => lastChild) : [tokens]) {
        // @ts-expect-error sparse array
        accum[accum.indexOf(t)] = undefined;
    }
};
/**
 * 展开模板
 * @param wikitext
 * @param page 页面名称
 * @param callPage 调用页面名称
 * @param config
 * @param include
 * @param context 模板调用环境
 * @param now 当前时间
 * @param accum
 * @param stack 模板调用栈
 */
const expand = (wikitext, page, callPage, config, include, context, now = index_1.default.now, accum = [], stack = []) => {
    const n = accum.length, token = new index_2.Token(wikitext, { ...config, inExt: true }, accum);
    token.type = 'root';
    token.pageName = page;
    token.parseOnce(0, include);
    if (context !== false) {
        for (const plain of [...accum.slice(n), token]) {
            if (plain.length !== 1 || plain.firstChild.type !== 'text') {
                continue;
            }
            const { data } = plain.firstChild;
            if (!/\0\d+g\x7F/u.test(data)) {
                continue;
            }
            const expanded = data.replace(/\0(\d+)g\x7F/gu, (_, i) => {
                const target = accum[i];
                if (target.type === 'onlyinclude') {
                    clean(accum, target);
                    return target.firstChild.toString();
                }
                const { lastChild } = target;
                clean(accum, lastChild);
                return lastChild.firstChild.toString().replace(/\0(\d+)c\x7F[\n ]|\0(\d+)n\x7F|^\n|\n$/gu, (m, p1, p2) => {
                    if (p1 !== undefined) {
                        const { innerText } = accum[p1];
                        return /^T:[^_/\n<>~]+$/u.test(innerText) ? '' : m;
                    }
                    else if (p2 !== undefined) {
                        const { type } = accum[p2];
                        return type === 'tvar' ? '' : m;
                    }
                    return '';
                });
            });
            plain.setText(expanded);
        }
        token.setText((0, string_1.removeCommentLine)(token.firstChild.toString(), true));
    }
    token.parseOnce();
    for (const plain of [...accum.slice(n), token]) {
        if (!plain || plain.length !== 1 || plain.firstChild.type !== 'text') {
            continue;
        }
        const { data } = plain.firstChild;
        if (!/\0\d+[tm!{}+~-]\x7F/u.test(data)) {
            continue;
        }
        const expanded = data.replace(/([^\x7F]?)\0(\d+)[tm!{}+~-]\x7F/gu, (m, prev, i) => {
            const target = accum[i], { type, name, length, firstChild: f, childNodes } = target, isTemplate = type === 'template', args = childNodes.slice(1);
            if (type === 'arg') {
                const arg = (0, string_1.removeCommentLine)(f.toString()).trim();
                if (/\0\d+[tm!{}+~-]\x7F/u.test(arg)) {
                    return m;
                }
                else if (!context || !context.hasArg(arg)) {
                    const effective = target.childNodes[1] ?? target;
                    clean(accum, length === 1 ? f : effective);
                    return prev + effective.toString();
                }
                clean(accum, context.getArg(arg).lastChild);
                return prev + context.getValue(arg);
            }
            else if (isTemplate || name === 'int') {
                if (context === false) {
                    return m;
                }
                const nameToken = isTemplate ? f : args[0].lastChild, key = (0, string_1.removeComment)(nameToken.toString()), fallback = isTemplate ? m : `${prev}⧼${key}⧽`, { title, valid } = index_1.default.normalizeTitle((isTemplate ? '' : 'MediaWiki:') + key, 10, include, config, { halfParsed: true, temporary: true, page });
                if (!valid) {
                    clean(accum, nameToken);
                    if (isTemplate) {
                        clean(accum, args);
                        return prev + target.toString();
                    }
                    return fallback;
                }
                const dest = loadTemplate(title, config);
                if (dest === false) {
                    if (!isTemplate) {
                        clean(accum, nameToken);
                    }
                    return fallback;
                }
                else if (stack.includes(dest)) {
                    return `${prev}<span class="error">Template loop detected: [[${dest}]]</span>`;
                }
                let template = index_1.default.templates.get(dest).replace(/\n$/u, '');
                if (!isTemplate) {
                    for (let j = 1; j < args.length; j++) {
                        template = template.replaceAll(`$${j}`, (0, string_1.removeComment)(args[j].toString()));
                    }
                }
                return implicitNewLine(expand(template, dest, callPage, config, true, target, now, accum, [...stack, dest])
                    .toString(), prev);
            }
            else if (context === false && !solvedMagicWords.has(name)) {
                return m;
            }
            else if (constants_1.functionHooks.has(name)) {
                clean(accum, args);
                return implicitNewLine(constants_1.functionHooks.get(name)(target, context || undefined), prev);
            }
            else if (magicWords_1.expandedMagicWords.has(name)) {
                const result = (0, magicWords_1.expandMagicWord)(name, args.map(({ anon, name: key, value }) => anon ? value : `${key}=${value}`), callPage, config, now, accum);
                if (result === false) {
                    return m;
                }
                clean(accum, args);
                return implicitNewLine(result, prev);
            }
            return m;
        });
        plain.setText(expanded);
        if (plain.type === 'parameter-key') {
            plain.parentNode.trimName((0, string_1.removeCommentLine)(expanded));
        }
    }
    return token;
};
/**
 * 展开指定节点的模板
 * @param token 目标节点
 * @param context 模板调用环境
 */
const expandToken = (token, context) => {
    const { pageName } = token;
    return expand(token.toString(), pageName, pageName, token.getAttribute('config'), token.getAttribute('include'), context);
};
exports.expandToken = expandToken;
constants_1.parsers['expandToken'] = __filename;
