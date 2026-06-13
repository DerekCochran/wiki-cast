"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.EmbeddedCSSDocument = exports.EmbeddedJSONDocument = exports.loadStylelint = exports.loadHtmlData = exports.loadCssLSP = exports.loadJsonLSP = exports.loadTexvcjs = void 0;
const path_1 = __importDefault(require("path"));
const common_1 = require("@bhsd/common");
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER */
const constants_2 = require("../util/constants");
let texcvjs;
const loadTexvcjs = () => {
    NPM: {
        if (texcvjs === undefined) {
            try {
                texcvjs = require('mathoid-texvcjs');
            }
            catch /* c8 ignore start */ {
                texcvjs = null;
            }
            /* c8 ignore stop */
        }
        return texcvjs;
    }
};
exports.loadTexvcjs = loadTexvcjs;
let jsonLSP;
const loadJsonLSP = () => {
    if (jsonLSP === undefined) {
        try {
            jsonLSP = require('vscode-json-languageservice')
                .getLanguageService({
                /* c8 ignore start */
                /** @implements */
                async schemaRequestService(uri) {
                    return (await fetch(uri)).text();
                },
                /* c8 ignore stop */
            });
            const dir = path_1.default.join('..', '..', 'data', 'ext');
            jsonLSP.configure({
                schemas: constants_1.jsonTags.map((tag) => {
                    const uri = path_1.default.join(dir, `${tag}.json`);
                    try {
                        const schema = require(tag === 'maplink' ? path_1.default.join(dir, 'mapframe.json') : uri);
                        return {
                            uri,
                            fileMatch: [tag],
                            schema,
                        };
                    }
                    catch /* c8 ignore start */ {
                        return false;
                    }
                    /* c8 ignore stop */
                }).filter(schema => schema !== false),
            });
        }
        catch /* c8 ignore start */ {
            jsonLSP = null;
        }
        /* c8 ignore stop */
    }
    return jsonLSP;
};
exports.loadJsonLSP = loadJsonLSP;
let cssLSP;
const loadCssLSP = () => {
    if (cssLSP === undefined) {
        try {
            cssLSP = require('vscode-css-languageservice')
                .getCSSLanguageService();
        }
        catch /* c8 ignore start */ {
            cssLSP = null;
        }
        /* c8 ignore stop */
    }
    return cssLSP;
};
exports.loadCssLSP = loadCssLSP;
let htmlData;
const loadHtmlData = () => {
    if (htmlData === undefined) {
        try {
            htmlData = require('vscode-html-languageservice')
                .getDefaultHTMLDataProvider();
        }
        catch /* c8 ignore start */ {
            htmlData = null;
        }
        /* c8 ignore stop */
    }
    return htmlData;
};
exports.loadHtmlData = loadHtmlData;
let stylelint;
const loadStylelint = () => {
    NPM: {
        if (stylelint === undefined) {
            try {
                stylelint = require('stylelint');
            }
            catch /* c8 ignore start */ {
                stylelint = null;
            }
            /* c8 ignore stop */
        }
        return stylelint;
    }
};
exports.loadStylelint = loadStylelint;
/** embedded document */
class EmbeddedDocument {
    uri = '';
    version = 0;
    #root;
    #content;
    #offset;
    #post;
    /**
     * @param id language ID
     * @param root root token
     * @param token current token
     * @param pre padding before the content
     * @param post padding after the content
     */
    constructor(id, root, token, pre = '', post = '') {
        this.languageId = id;
        this.lineCount = root.getLines().length;
        this.#root = root;
        this.#content = token.toString();
        this.#offset = token.getAbsoluteIndex();
        this.pre = pre;
        this.#post = post;
    }
    /** 原始文本 */
    getContent() {
        return this.#content;
    }
    /** @implements */
    getText() {
        return this.pre + this.getContent() + this.#post;
    }
    /** @implements */
    positionAt(offset) {
        const { top, left } = this.#root.posFromIndex(this.#offset + Math.max(Math.min(offset - this.pre.length, this.#content.length), 0));
        return { line: top, character: left };
    }
    /** @implements */
    offsetAt({ line, character }) {
        return Math.min(Math.max(this.#root.indexFromPos(line, character) - this.#offset, 0), this.#content.length)
            + this.pre.length;
    }
}
/** embedded JSON document */
class EmbeddedJSONDocument extends EmbeddedDocument {
    /**
     * @param root root token
     * @param token current token
     */
    constructor(root, token) {
        super('json', root, token);
        this.uri = token.name;
        this.jsonDoc = (0, exports.loadJsonLSP)().parseJSONDocument(this);
    }
}
exports.EmbeddedJSONDocument = EmbeddedJSONDocument;
/** embedded CSS document */
class EmbeddedCSSDocument extends EmbeddedDocument {
    /**
     * @param root root token
     * @param token current token
     */
    constructor(root, token) {
        const { type, tag } = token.parentNode;
        super('css', root, token, `${type === 'ext-attr' ? 'div' : tag}{`, '}');
        this.styleSheet = (0, exports.loadCssLSP)().parseStylesheet(this);
    }
    getContent() {
        return (0, common_1.sanitizeInlineStyle)(super.getContent());
    }
}
exports.EmbeddedCSSDocument = EmbeddedCSSDocument;
constants_2.classes['EmbeddedDocument'] = __filename;
