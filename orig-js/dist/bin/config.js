"use strict";
/* eslint-disable n/no-process-exit */
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const path_1 = __importDefault(require("path"));
const fs_1 = __importDefault(require("fs"));
const child_process_1 = require("child_process");
const strict_1 = __importDefault(require("assert/strict"));
const cm_util_1 = require("@bhsd/cm-util");
const diff_1 = require("../util/diff");
/**
 * Converts an array to an object.
 * @param config parser configuration
 * @param config.articlePath article path
 */
const arrToObj = ({ articlePath, ...obj }) => {
    for (const k in obj) {
        const v = obj[k];
        if (Array.isArray(v) && v.every(x => typeof x === 'string')) {
            Object.assign(obj, { [k]: Object.fromEntries(v.map(x => [x, true])) });
        }
    }
    return obj;
};
/**
 * Gets the aliases of magic words.
 * @param magicwords magic words
 * @param targets magic word names
 */
const getAliases = (magicwords, targets) => magicwords
    .filter(({ name }) => targets.has(name))
    .flatMap(({ aliases }) => aliases.map(s => s.replace(/:$/u, '').toLowerCase()));
/**
 * Filters out gadget-related namespaces.
 * @param id namespace ID
 */
const filterGadget = (id) => {
    const n = Number(id);
    return n < 2300 || n > 2303; // Gadget, Gadget talk, Gadget definition, Gadget definition talk
};
const pkg = "wikiparser-node", version = "1.38.1";
/**
 * Get the parser configuration for a Wikimedia Foundation project.
 * @param site site nickname
 * @param url script path
 * @param user URI for wiki userpage or email address of the user
 * @param force whether to overwrite the existing configuration
 * @param internal for internal use
 */
exports.default = async (site, url, user, force, internal) => {
    // wrong calls
    if (!site || !url) {
        if (internal) {
            throw new RangeError('Site nickname and script path are required!');
        }
        else {
            (0, diff_1.error)('Usage: npx getParserConfig <site> <script path> [user] [force]');
            process.exit(1);
        }
    }
    // internal calls with stored configuration
    const dir = path_1.default.join('..', '..', 'config'), file = path_1.default.join(__dirname, dir, `${site}.json`);
    if (internal && !force && fs_1.default.existsSync(file)) {
        return require(file);
    }
    // fetching configuration
    if (/(?:\.php|\/)$/u.test(url)) {
        url = url.slice(0, url.lastIndexOf('/'));
    }
    if (user === 'git') {
        user = (0, child_process_1.execSync)('git config user.email', { encoding: 'utf8' }).trim();
    }
    const headers = user
        ? {
            headers: {
                'User-Agent': `${pkg}/${version} (https://www.npmjs.com/package/${pkg}; ${user}) Node.js/${process.version}`,
            },
        }
        : undefined, m = await (await fetch(`${url}/load.php?modules=ext.CodeMirror.data|ext.CodeMirror`, headers)).text(), params = {
        action: 'query',
        meta: 'siteinfo',
        siprop: 'general|magicwords|functionhooks|namespaces|namespacealiases',
        format: 'json',
        formatversion: '2',
    }, { general: { articlepath, variants, langconversion }, magicwords, namespaces, namespacealiases, functionhooks, } = (await (await fetch(`${url}/api.php?${new URLSearchParams(params).toString()}`, headers)).json()).query, tempFile = path_1.default.join(__dirname, 'mw.js');
    fs_1.default.writeFileSync(tempFile, m);
    const { stdout, stderr } = (0, child_process_1.spawnSync)(process.execPath, [
        '-r',
        './env.js',
        process.allowedNodeEnvironmentFlags.has('--permission')
            ? '--permission'
            : '--experimental-permission',
        `--allow-fs-read=${__dirname}`,
        '--disable-warning=ExperimentalWarning',
        tempFile,
    ], { cwd: __dirname, encoding: 'utf8' });
    fs_1.default.unlinkSync(tempFile);
    if (stderr) {
        console.error(stderr);
        throw new Error('Failed to execute the fetched MediaWiki module!', { cause: m });
    }
    let mwConfig;
    try {
        mwConfig = JSON.parse(stdout);
    }
    catch {
        throw new RangeError('Extension:CodeMirror is not installed!');
    }
    const ns = Object.entries(namespaces).filter(([id]) => filterGadget(id))
        .flatMap(([id, { name, canonical = '' }]) => [
        [id, name],
        ...name === canonical ? [] : [[id, canonical]],
    ]), config = {
        ...(0, cm_util_1.getParserConfig)(require(path_1.default.join(dir, 'minimum.json')), mwConfig),
        ...(0, cm_util_1.getKeywords)(magicwords),
        variants: langconversion ? (0, cm_util_1.getVariants)(variants) : [],
        namespaces: Object.fromEntries(ns),
        nsid: Object.fromEntries([
            ...ns.map(([id, canonical]) => [canonical.toLowerCase(), Number(id)]),
            ...namespacealiases.filter(({ id }) => filterGadget(id)).map(({ id, alias }) => [alias.toLowerCase(), id]),
        ]),
        articlePath: articlepath,
    }, { doubleUnderscore, parserFunction, variable } = config;
    doubleUnderscore[0] = [];
    doubleUnderscore[1] = [];
    Object.assign(parserFunction[0], (0, cm_util_1.getConfig)(magicwords, ({ name }) => name === 'msgnw'));
    parserFunction[2] = getAliases(magicwords, new Set(['msg', 'raw']));
    parserFunction[3] = getAliases(magicwords, new Set(['subst', 'safesubst']));
    if (!mwConfig.functionHooks) {
        Object.assign(config, { functionHook: [...functionhooks.map(s => s.toLowerCase()), 'msgnw'] });
    }
    if (!mwConfig.variableIDs) {
        const { variables } = (await (await fetch(`${url}/api.php?${new URLSearchParams({ ...params, siprop: 'variables' }).toString()}`, headers)).json()).query;
        Object.assign(config, { variable: [...new Set([...variables, '='])] });
    }
    if ('#choose' in parserFunction[0]) {
        delete parserFunction[0]['choose'];
        const i = variable.indexOf('choose');
        if (i !== -1) {
            variable.splice(i, 1);
        }
    }
    // saving configuration
    if (force || !fs_1.default.existsSync(file)) {
        fs_1.default.writeFileSync(file, `${JSON.stringify(config, null, '\t')}\n`);
    }
    else {
        const oldConfig = arrToObj(require(file)), newConfig = arrToObj(config);
        for (const k in newConfig) {
            try {
                strict_1.default.deepStrictEqual(oldConfig[k], newConfig[k]);
            }
            catch (e) {
                if (e instanceof strict_1.default.AssertionError) {
                    (0, diff_1.error)(`Configuration mismatch for "${k}"`);
                    delete e.actual;
                    delete e.expected;
                }
                throw e;
            }
        }
    }
    return config;
};
