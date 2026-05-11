"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
/* eslint n/exports-style: 0 */
//const base_1 = require("./base");
const debug_1 = require("./util/debug");
const constants_1 = require("./util/constants");
const string_1 = require("./util/string");
const lintConfig_1 = require("./lib/lintConfig");
const title_1 = require("./lib/title");
/* NOT FOR BROWSER */
//const nodejs_1 = require("@bhsd/nodejs");
const redirectMap_1 = require("./lib/redirectMap");
/* NOT FOR BROWSER END */
/* NOT FOR BROWSER ONLY */
const fs_1 = __importDefault(require("fs"));
const path_1 = __importDefault(require("path"));
const common_1 = require("@bhsd/common");
const diff_1 = require("./util/diff");
/* NOT FOR BROWSER ONLY */
const re = new RegExp(String.raw `^https?:\/\/([^./]+)\.(${common_1.wmf})\.org`, 'iu');
/**
 * require一个JSON文件
 * @param file 文件名
 * @throws {RangeError} 仅支持JSON文件
 */
const jsonRequire = (file) => {
    const fullPath = require.resolve(file);
    if (fullPath.endsWith('.json')) {
        return require(fullPath);
    }
    throw new RangeError('Only JSON files are supported!');
};
/**
 * 从根路径require
 * @param file 文件名
 * @param dir 子路径
 */
const rootRequire = (file, dir) => jsonRequire(path_1.default.isAbsolute(file)
    ? /* c8 ignore next */ file
    : path_1.default.join('..', file.includes('/') ? '' : dir, file));
/* NOT FOR BROWSER ONLY END */
let viewOnly = false;
/* NOT FOR BROWSER */
const promises = [Promise.resolve()];
let redirectMap = new redirectMap_1.RedirectMap(), now;
/* NOT FOR BROWSER END */
let lintConfig = (() => {
    LINT: return new lintConfig_1.LintConfiguration();
})(), i18n;
/**
 * 判断参数顺序
 * @param includeOrPage include or page
 * @param configOrInclude config or include
 * @param pageOrConfig page or config
 */
const getParams = (includeOrPage, configOrInclude, pageOrConfig) => typeof includeOrPage === 'string'
    ? [Boolean(configOrInclude), pageOrConfig, includeOrPage]
    : [Boolean(includeOrPage), configOrInclude, pageOrConfig];
const Parser = {
    config: 'default',
    /** @implements */
    get rules() {
        LINT: return base_1.rules;
    },
    /** @implements */
    get i18n() {
        LINT: return { ...constants_1.enMsg, ...i18n };
    },
    set i18n(data) {
        /* NOT FOR BROWSER ONLY */
        if (typeof data === 'string') {
            i18n = rootRequire(data, 'i18n');
        }
        else {
            /* NOT FOR BROWSER ONLY END */
            LINT: i18n = data;
        }
    },
    /** @implements */
    get lintConfig() {
        LINT: return lintConfig;
    },
    set lintConfig(config) {
        LINT: lintConfig = new lintConfig_1.LintConfiguration(config);
    },
    /** @implements */
    get viewOnly() {
        return viewOnly;
    },
    set viewOnly(value) {
        if (viewOnly && !value) {
            debug_1.Shadow.rev++;
        }
        viewOnly = value;
    },
    /* NOT FOR BROWSER */
    conversionTable: new Map(),
    templates: new redirectMap_1.RedirectMap(undefined, false),
    warning: true,
    debugging: false,
    /** @implements */
    get now() {
        return now ?? new Date();
    },
    set now(value) {
        now = value;
    },
    /** @implements */
    get redirects() {
        return redirectMap;
    },
    set redirects(redirects) {
        redirectMap = redirects instanceof redirectMap_1.RedirectMap ? redirects : new redirectMap_1.RedirectMap(redirects);
    },
    /* NOT FOR BROWSER END */
    /* NOT FOR BROWSER ONLY */
    configPaths: [],
    /* NOT FOR BROWSER ONLY END */
    /* PRINT ONLY */
    internal: false,
    /* PRINT ONLY END */
    /** @implements */
    getConfig(config) {
        /* NOT FOR BROWSER ONLY */
        if (!config && typeof this.config === 'string') {
            // console.info(`Loading parser configuration from ${this.config}...`);
            if (!path_1.default.isAbsolute(this.config)) {
                for (const p of this.configPaths) {
                    try {
                        this.config = jsonRequire(path_1.default.resolve(process.cwd(), p, this.config));
                        break;
                    }
                    catch { }
                }
            }
            if (typeof this.config === 'string') {
                this.config = rootRequire(this.config, 'config');
            }
            /* c8 ignore next 3 */
            if (this.config.doubleUnderscore.length < 3 || !('functionHook' in this.config)) {
                (0, diff_1.error)(`The schema (${path_1.default.join(__dirname, '..', 'config', '.schema.json')}) of parser configuration is updated.`);
            }
            return this.getConfig();
        }
        //console.info(`Loading parser configuration from ${JSON.stringify(this.config)}...`);
        /* NOT FOR BROWSER ONLY END */
        const parserConfig = config ?? this.config, { doubleUnderscore, ext, parserFunction, variable, 
        /* NOT FOR BROWSER */
        conversionTable, redirects, } = parserConfig;
        for (let i = 0; i < 2; i++) {
            if (doubleUnderscore.length > i + 2 && doubleUnderscore[i].length === 0) {
                doubleUnderscore[i] = Object.keys(doubleUnderscore[i + 2]);
            }
        }
        if (ext.includes('translate') && !variable.includes('translationlanguage')) {
            variable.push('translationlanguage');
            parserFunction[1]['TRANSLATIONLANGUAGE'] = 'translationlanguage';
        }
        /* NOT FOR BROWSER */
        if (conversionTable) {
            this.conversionTable = new Map(conversionTable);
        }
        if (redirects) {
            this.redirects = new redirectMap_1.RedirectMap(redirects);
        }
        /* NOT FOR BROWSER END */
        return {
            ...parserConfig,
            excludes: [],
        };
    },
    /** @implements */
    msg(msg, arg = '') {
        LINT: return msg
            && (this.i18n[msg] ?? msg).replace('$1', this.msg(arg));
    },
    /** @implements */
    normalizeTitle(title, defaultNs = 0, include, config = Parser.getConfig(), opt) {
        let titleObj;
        if (opt?.halfParsed) {
            titleObj = new title_1.Title(title, defaultNs, config, opt);
        }
        else {
            const { Token } = require('./src/index');
            titleObj = debug_1.Shadow.run(() => {
                const root = new Token(title, config);
                root.type = 'root';
                root.pageName = opt?.page;
                root.parseOnce(0, include).parseOnce();
                const t = new title_1.Title(root.firstChild.toString(), defaultNs, config, opt);
                root.build();
                for (const key of ['main', 'fragment']) {
                    const str = t[key];
                    if (str?.includes('\0')) {
                        const s = root.buildFromStr(str, constants_1.BuildMethod.Text);
                        if (key === 'main') {
                            t.main = s;
                        }
                        else {
                            t.setFragment(s);
                        }
                    }
                }
                return t;
            }, this);
        }
        /* NOT FOR BROWSER */
        titleObj.conversionTable = this.conversionTable;
        titleObj.redirects = this.redirects;
        /* NOT FOR BROWSER END */
        return titleObj;
    },
    /** @implements */
    parse(wikitext, includeOrPage, maxStageOrInclude, configOrStage, pageOrConfig) {
        wikitext = (0, string_1.tidy)(wikitext);
        let include, maxStage, config, page;
        if (typeof includeOrPage === 'string') {
            include = Boolean(maxStageOrInclude);
            maxStage = configOrStage;
            config = pageOrConfig;
            page = includeOrPage;
        }
        else {
            include = Boolean(includeOrPage);
            maxStage = maxStageOrInclude;
            config = configOrStage;
            page = pageOrConfig;
        }
        maxStage ??= constants_1.MAX_STAGE;
        config ??= this.getConfig();
        let types;
        LINT: if (typeof maxStage !== 'number') {
            types = Array.isArray(maxStage) ? maxStage : [maxStage];
            maxStage = Math.max(...types.map(t => base_1.stages[t] || constants_1.MAX_STAGE));
        }
        const { Token } = require('./src/index');
        const root = debug_1.Shadow.run(() => {
            const token = new Token(wikitext, config);
            token.type = 'root';
            token.pageName = page;
            try {
                return token.parse(maxStage, include);
                /* NOT FOR BROWSER ONLY */
            }
            catch (e) /* c8 ignore start */ {
                if (e instanceof Error) {
                    const file = path_1.default.join(__dirname, '..', 'errors', new Date().toISOString()), stage = token.getAttribute('stage');
                    for (const k in config) {
                        if (k.startsWith('regex') || config[k] instanceof Set) {
                            delete config[k];
                        }
                    }
                    fs_1.default.writeFileSync(file, stage === constants_1.MAX_STAGE ? wikitext : token.toString());
                    fs_1.default.writeFileSync(`${file}.err`, e.stack);
                    fs_1.default.writeFileSync(`${file}.json`, JSON.stringify({ stage, include, config, page }, null, '\t'));
                }
                throw e;
            }
            /* c8 ignore stop */
            /* NOT FOR BROWSER ONLY END */
        });
        /* NOT FOR BROWSER */
        if (types?.includes('list-range')) {
            root.buildLists();
        }
        /* c8 ignore start */
        if (this.debugging) {
            let restored = root.toString(), proc = 'parsing';
            if (restored === wikitext) {
                const entities = { lt: '<', gt: '>', amp: '&' };
                restored = root.print().replace(/<[^<]+?>|&([lg]t|amp);/gu, (_, s) => s ? entities[s] : '');
                proc = 'printing';
            }
            if (restored !== wikitext) {
                const { length } = promises, cur = promises[length - 1];
                promises.push((async () => {
                    await cur;
                    this.error(`Original wikitext is altered when ${proc}!`);
                    return (0, diff_1.diff)(wikitext, restored, length);
                })());
            }
        }
        /* c8 ignore stop */
        /* NOT FOR BROWSER END */
        return root;
    },
    /** @implements */
    parseWithRef(wikitext, ref, maxStage, include = ref.getAttribute('include')) {
        return this.parse(wikitext, include, maxStage, ref.getAttribute('config'), ref.pageName);
    },
    /** @implements */
    createLanguageService(uri = {}) {
        LSP: {
            const { LanguageService, tasks } = require('./lib/lsp');
            this.viewOnly = true;
            return tasks.get(uri) ?? new LanguageService(uri);
        }
    },
    /** @implements */
    lint(wikitext, includeOrPage, configOrInclude, pageOrConfig) {
        LINT: {
            const [include, config, page] = getParams(includeOrPage, configOrInclude, pageOrConfig);
            return debug_1.Shadow.internal(() => this.parse(wikitext, include, undefined, config, page).lint(), this);
        }
    },
    /** @implements */
    print(wikitext, includeOrPage, configOrInclude, pageOrConfig) {
        PRINT: {
            const [include, config, page] = getParams(includeOrPage, configOrInclude, pageOrConfig);
            return debug_1.Shadow.internal(() => this.parse(wikitext, include, undefined, config, page).print(), this);
        }
    },
    /* NOT FOR BROWSER ONLY */
    /** @implements */
    getWMFSite(url) {
        const mt = re.exec(url);
        /* c8 ignore next 3 */
        if (!mt) {
            throw new RangeError('Not a recognizable WMF site!');
        }
        const type = mt[2].toLowerCase();
        return [mt[1].toLowerCase() + (type === 'wikipedia' ? 'wiki' : type), mt[0]];
    },
    /* c8 ignore start */
    /** @implements */
    async fetchConfig(site, url, user) {
        const { default: fetchConfig } = require('./bin/config');
        return this.getConfig(await fetchConfig(site, url, user, false, true));
    },
    /* c8 ignore stop */
    /* NOT FOR BROWSER ONLY END */
    /* NOT FOR BROWSER */
    /** @implements */
    toHtml(wikitext, includeOrPage, configOrInclude, pageOrConfig) {
        const [include, config, page] = getParams(includeOrPage, configOrInclude, pageOrConfig);
        return debug_1.Shadow.internal(() => this.parse(wikitext, include, undefined, config, page).toHtml(), this);
    },
    /** @implements */
    setFunctionHook(name, hook) {
        constants_1.functionHooks.set((0, debug_1.getCanonicalName)(name, this.getConfig().parserFunction)[0], hook);
    },
    /** @implements */
    setHook(name, hook) {
        constants_1.tagHooks.set(name.toLowerCase(), hook);
    },
    /** @implements */
    callParserFunction(name, arg, ...args) {
        if (typeof arg === 'string') {
            args.unshift(arg);
        }
        else if (Array.isArray(arg)) {
            args = arg;
        }
        else if (arg) {
            for (let i = 1; i in arg; i++) {
                args.push(arg[i]);
                delete arg[i];
            }
            for (const key in arg) {
                args.push(`${key}=${arg[key]}`);
            }
        }
        const { parserFunction } = this.getConfig(), [lcName, canonicalName] = (0, debug_1.getCanonicalName)(name, parserFunction), custom = constants_1.functionHooks.has(lcName);
        let result;
        if (custom) {
            if (!canonicalName) {
                const [insensitive, sensitive] = parserFunction, entry = Object.entries(sensitive).find(([, v]) => v === lcName)
                    || Object.entries(insensitive).find(([, v]) => v === lcName);
                /* c8 ignore next 3 */
                if (!entry) {
                    throw new RangeError(`Unable to resolve parser function: ${name}`);
                }
                [name] = entry;
            }
            const { firstChild, length } = debug_1.Shadow.internal(() => this.parse(`{{${name}:${args.join('|')}}}`, false, 2), this);
            result = length === 1 && firstChild.is('magic-word')
                && constants_1.functionHooks.get(lcName)(firstChild);
        }
        else {
            const { expandMagicWord } = require('./render/magicWords');
            result = expandMagicWord(lcName, args);
        }
        /* c8 ignore next 3 */
        if (result === false) {
            throw new RangeError(`Unable to resolve ${custom ? 'custom' : 'built-in'} parser function: ${name}`);
        }
        return result;
    },
    /** @implements */
    warn(msg, ...args) {
        /* c8 ignore start */
        if (this.warning) {
            console.warn((0, nodejs_1.yellow)(msg), ...args);
        }
        /* c8 ignore stop */
    },
    /** @implements */
    debug(msg, ...args) {
        /* c8 ignore start */
        if (this.debugging) {
            console.debug((0, nodejs_1.blue)(msg), ...args);
        }
        /* c8 ignore stop */
    },
    error: diff_1.error,
    info: diff_1.info,
    /* c8 ignore start */
    /** @implements */
    log(f) {
        if (typeof f === 'function') {
            console.log(String(f));
        }
    },
    /* c8 ignore stop */
    /** @implements */
    require(name) {
        // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
        return Object.hasOwn(constants_1.classes, name) ? require(constants_1.classes[name])[name] : require(path_1.default.join(__dirname, name));
    },
    /* c8 ignore start */
    /** @implements */
    async clearCache() {
        await (0, diff_1.cmd)('npm', ['--prefix', path_1.default.join(__dirname, '..'), 'run', 'build:core']);
        const entries = Object.entries(constants_1.classes);
        for (const [, filePath] of entries) {
            try {
                delete require.cache[require.resolve(filePath)];
            }
            catch { }
        }
        for (const [name, filePath] of entries) {
            if (name in globalThis) {
                // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
                Object.assign(globalThis, { [name]: require(filePath)[name] });
            }
        }
        this.info('已重新加载Parser');
    },
    /* c8 ignore stop */
    /** @implements */
    isInterwiki(title, config = Parser.getConfig()) {
        return (0, string_1.isInterwiki)(title, config);
    },
    /* c8 ignore start */
    /** @implements */
    reparse(date = '') {
        const dir = path_1.default.join(__dirname, '..', 'errors'), main = fs_1.default.readdirSync(dir).find(name => name.startsWith(date) && name.endsWith('Z'));
        if (!main) {
            throw new RangeError(`找不到对应时间戳的错误记录：${date}`);
        }
        const { Token } = require('./src/index');
        const file = path_1.default.join(dir, main), wikitext = fs_1.default.readFileSync(file, 'utf8'), { stage = constants_1.MAX_STAGE, include, config, page } = (() => {
            try {
                return require(`${file}.json`);
            }
            catch {
                return {};
            }
        })();
        debug_1.Shadow.run(() => {
            const halfParsed = stage < constants_1.MAX_STAGE, token = new Token(halfParsed ? wikitext : (0, string_1.tidy)(wikitext), config);
            token.type = 'root';
            token.pageName = page;
            if (halfParsed) {
                token.setAttribute('stage', stage);
                token.parseOnce(stage, include);
            }
            else {
                token.parse(undefined, include);
            }
            fs_1.default.unlinkSync(file);
            fs_1.default.rmSync(`${file}.err`, { force: true });
            fs_1.default.rmSync(`${file}.json`, { force: true });
        }, this);
    },
    /* c8 ignore stop */
};
const def = {
    default: { value: Parser },
}, enumerable = new Set([
    'lintConfig',
    'normalizeTitle',
    'parse',
    'createLanguageService',
    /* NOT FOR BROWSER ONLY */
    'fetchConfig',
    /* NOT FOR BROWSER ONLY END */
    /* NOT FOR BROWSER */
    'warning',
    'debugging',
    'isInterwiki',
]);
for (const key in Parser) {
    if (!enumerable.has(key)) {
        def[key] = { enumerable: false };
    }
}
Object.defineProperties(Parser, def);
exports.default = Parser;
module.exports = Parser;
