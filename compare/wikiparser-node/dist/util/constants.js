"use strict";
/* NOT FOR BROWSER */
Object.defineProperty(exports, "__esModule", { value: true });
exports.tagHooks = exports.functionHooks = exports.states = exports.aliases = exports.parsers = exports.mixins = exports.classes = exports.mathTags = exports.jsonTags = exports.extensions = exports.galleryParams = exports.enMsg = exports.BuildMethod = exports.MAX_STAGE = void 0;
/* NOT FOR BROWSER END */
exports.MAX_STAGE = 11;
var BuildMethod;
(function (BuildMethod) {
    BuildMethod[BuildMethod["String"] = 0] = "String";
    BuildMethod[BuildMethod["Text"] = 1] = "Text";
})(BuildMethod || (exports.BuildMethod = BuildMethod = {}));
exports.enMsg = (() => {
    // eslint-disable-next-line n/no-missing-require
    LSP: return require('../../i18n/en.json');
})();
exports.galleryParams = new Set(['alt', 'link', 'lang', 'page', 'caption']);
exports.extensions = new Set(['tiff', 'tif', 'png', 'gif', 'jpg', 'jpeg', 'webp', 'xcf', 'pdf', 'svg', 'djvu']);
exports.jsonTags = ['templatedata', 'mapframe', 'maplink'];
/* NOT FOR BROWSER ONLY */
exports.mathTags = new Set(['math', 'chem', 'ce']);
/* NOT FOR BROWSER ONLY END */
/* NOT FOR BROWSER */
exports.classes = {}, exports.mixins = exports.classes, exports.parsers = exports.classes;
exports.aliases = [
    ['AstText'],
    ['CommentToken', 'ExtToken', 'IncludeToken', 'NoincludeToken', 'TranslateToken'],
    ['ArgToken', 'TranscludeToken', 'HeadingToken'],
    ['HtmlToken'],
    ['TableToken'],
    ['HrToken', 'DoubleUnderscoreToken'],
    ['LinkToken', 'FileToken', 'CategoryToken'],
    ['QuoteToken'],
    ['ExtLinkToken'],
    ['MagicLinkToken'],
    ['ListToken', 'DdToken'],
    ['ConverterToken'],
];
exports.states = new WeakMap();
exports.functionHooks = new Map();
exports.tagHooks = new Map();
