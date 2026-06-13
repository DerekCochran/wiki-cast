"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.LintConfiguration = void 0;
const base_1 = require("../base");
/* NOT FOR BRWOSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
const dict = new Map([
    [0, false],
    [1, 'warning'],
    [2, 'error'],
    [false, false],
    ['off', false],
    ['warning', 'warning'],
    ['error', 'error'],
]);
const defaultLintRuleConfig = {
    'arg-in-ext': 1,
    'blank-alt': 1,
    'bold-header': [
        1,
        {
        // b: 1,
        // strong: 1,
        },
    ],
    'format-leakage': [
        1,
        {
        // apostrophe: 1,
        },
    ],
    'fostered-content': [
        1,
        {
        // transclusion: 1,
        },
    ],
    h1: [
        1,
        {
        // html: 1,
        },
    ],
    'illegal-attr': [
        2,
        {
        // tabindex: 2,
        // unknown: 2,
        // value: 2,
        },
    ],
    'insecure-style': 2,
    'invalid-gallery': [
        2,
        {
            // extension: 2,
            // image: 2,
            // link: 2,
            parameter: 1,
            // thumb: 2,
        },
    ],
    'invalid-imagemap': [
        2,
        {
        // image: 2,
        // link: 2,
        },
    ],
    'invalid-invoke': [
        2,
        {
        // function: 2,
        // name: 2,
        },
    ],
    'invalid-isbn': 2,
    'invalid-json': [
        2,
        {
            duplicate: 1,
        },
    ],
    'invalid-url': 1,
    'lonely-apos': [
        1,
        {
        // word: 1,
        },
    ],
    'lonely-bracket': [
        1,
        {
            // converter: 1,
            // double: 1,
            extLink: 2,
            // single: 1,
        },
    ],
    'lonely-http': [
        1,
        {
        // ISBN: 1,
        // PMID: 1,
        // RFC: 1,
        },
    ],
    'nested-link': [
        2,
        {
        // file: 2,
        // ref: 2,
        },
    ],
    'no-arg': 1,
    'no-duplicate': [
        2,
        {
            // attribute: 2,
            category: 1,
            id: 1,
            // imageParameter: 2,
            // parameter: 2,
            unknownImageParameter: 1,
        },
    ],
    'no-ignored': [
        2,
        {
            // arg: 2,
            // closingTag: 2,
            // conversionFlag: 2,
            fragment: 1,
            galleryComment: 1,
            // galleryImage: 2,
            galleryNoImage: 1,
            include: 1,
            // invalidAttributes: 2,
            nonWordAttributes: 1,
            redirect: 1,
            // categorytree: 2,
            // choose: 2,
            // combobox: 2,
            // dynamicpagelist: 2,
            // inputbox: 2,
            // references: 2,
        },
    ],
    'obsolete-attr': 1,
    'obsolete-tag': 1,
    'parsing-order': [
        2,
        {
            // ext: 2,
            // heading: 2,
            // html: 2,
            templateInTable: 1,
        },
    ],
    'pipe-like': [
        1,
        {
            double: 2,
            // link: 1,
            // td: 1,
        },
    ],
    'syntax-like': [
        2,
        {
        // heading: 2,
        // redirect: 2,
        },
    ],
    'table-layout': 1,
    'tag-like': [
        2,
        {
            disallowed: 1,
            invalid: 1,
        },
    ],
    'unbalanced-header': 2,
    'unclosed-comment': [
        1,
        {
        // include: 1,
        },
    ],
    'unclosed-quote': 1,
    'unclosed-table': 2,
    unescaped: 2,
    'unknown-page': 1,
    'unmatched-tag': [
        1,
        {
            both: 2,
            // closing: 1,
            // conditional: 1,
            // opening: 1,
            selfClosing: 2,
        },
    ],
    'unterminated-url': [
        1,
        {
        // pipe: 1,
        // punctuation: 1,
        },
    ],
    'url-encoding': [
        1,
        {
        // file: 1,
        },
    ],
    'var-anchor': [
        1,
        {
        // extLink: 1,
        // ref: 1,
        },
    ],
    'void-ext': [
        2,
        {
        // img: 2,
        // languages: 2,
        // section: 2,
        // templatestyles: 2,
        },
    ],
    /* NOT FOR BROWSER ONLY */
    'invalid-css': [
        2,
        {
            warn: 1,
        },
    ],
    'invalid-math': 2,
};
const defaultLintConfig = {
    configurationComment: 'lint',
    ignoreDisables: false,
    fix: true,
    computeEditInfo: true,
};
/**
 * 验证错误级别是否符合规范
 * @param severity 错误级别
 */
const validateSeverity = (severity) => dict.has(severity);
/**
 * 验证设置值是否符合规范
 * @param value 设置值
 */
const validateConfigValue = (value) => validateSeverity(value)
    || Array.isArray(value) && validateSeverity(value[0]) && (value.length === 1 || typeof value[1] === 'object');
/**
 * 设置语法检查规则
 * @param obj 语法检查设置对象
 * @param key 语法检查规则
 * @param value 语法检查规则值
 * @throws `RangeError` 未知的规则或无效的值
 */
const set = (obj, key, value) => {
    /* c8 ignore next 6 */
    if (!base_1.rules.includes(key)) {
        throw new RangeError(`Unknown lint rule: ${key}`);
    }
    if (value === undefined) {
        return false;
    }
    if (validateConfigValue(value)) {
        obj[key] = value;
        return true;
    }
    /* c8 ignore next */
    throw new RangeError(`Invalid lint config for ${key}: ${JSON.stringify(value)}`);
};
const clone = typeof structuredClone === 'function'
    ? structuredClone
    : /* c8 ignore next */ (obj) => JSON.parse(JSON.stringify(obj));
/** 语法规则设置 */
class LintRuleConfiguration {
    /** @param config 语法规则设置 */
    constructor(config) {
        Object.assign(this, clone(defaultLintRuleConfig));
        if (!config) {
            return;
        }
        for (const key in config) {
            set(this, key, config[key]);
        }
    }
    /** @implements */
    getSeverity(rule, key) {
        const value = this[rule];
        if (typeof value !== 'object') {
            return dict.get(value);
        }
        return key ? dict.get(value[1]?.[key]) ?? dict.get(value[0]) : dict.get(value[0]);
    }
}
/** 语法检查设置 */
class LintConfiguration {
    // @ts-expect-error lazy initialization
    #rules;
    /** @implements */
    get rules() {
        return this.#rules;
    }
    set rules(config) {
        this.#rules = new Proxy(new LintRuleConfiguration(config), {
            set,
            /* c8 ignore start */
            /** @ignore */
            deleteProperty() {
                return false;
            },
            /* c8 ignore stop */
        });
    }
    /** @param config 语法检查设置 */
    constructor(config) {
        Object.assign(this, defaultLintConfig);
        if (config
            && !('rules' in config)
            && Object.keys(config).some(key => base_1.rules.includes(key))) {
            this.rules = config;
        }
        else {
            const { rules: ruleConfig, ...other } = (config ?? {});
            this.rules = ruleConfig;
            for (const key in other) {
                const value = other[key];
                if (value !== undefined && Object.prototype.hasOwnProperty.call(defaultLintConfig, key)) {
                    this[key] = value;
                }
            }
        }
    }
    /** @implements */
    getSeverity(rule, key) {
        return this.#rules.getSeverity(rule, key);
    }
}
exports.LintConfiguration = LintConfiguration;
constants_1.classes['LintConfiguration'] = __filename;
