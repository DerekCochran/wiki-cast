"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.LinkToken = void 0;
const common_1 = require("@bhsd/common");
const lint_1 = require("../../util/lint");
const index_1 = __importDefault(require("../../index"));
const base_1 = require("./base");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
/* NOT FOR BROWSER END */
/**
 * internal link
 *
 * 内链
 * @classdesc `{childNodes: [AtomToken, ?Token]}`
 */
class LinkToken extends base_1.LinkBaseToken {
    /* NOT FOR BROWSER END */
    get type() {
        return 'link';
    }
    /** link text / 链接显示文字 */
    get innerText() {
        LINT: return this.length > 1
            ? this.lastChild.text()
            : (0, common_1.rawurldecode)(this.firstChild.text().replace(/^\s*:?/u, ''));
    }
    /* NOT FOR BROWSER */
    set innerText(text) {
        this.setLinkText(text);
    }
    /** whether to be a self link / 是否链接到自身 */
    get selfLink() {
        const { title, fragment } = this.link;
        return !title && Boolean(fragment);
    }
    set selfLink(selfLink) {
        if (selfLink) {
            this.asSelfLink();
        }
    }
    /* NOT FOR BROWSER END */
    /** @private */
    lint(start = this.getAbsoluteIndex(), re) {
        LINT: {
            const errors = super.lint(start, re), rule = 'nested-link', { lintConfig } = index_1.default, s = lintConfig.getSeverity(rule);
            if (s && this.isInside('ext-link-text')) {
                const e = (0, lint_1.generateForSelf)(this, { start }, rule, 'link-in-extlink', s);
                if (lintConfig.computeEditInfo || lintConfig.fix) {
                    e.fix = (0, lint_1.fixBy)(e, 'delink', this.innerText);
                }
                errors.push(e);
            }
            return errors;
        }
    }
    /* NOT FOR BROWSER */
    /* c8 ignore start */
    /**
     * Set the interlanguage link
     *
     * 设置跨语言链接
     * @param lang language prefix / 语言前缀
     * @param link page title / 页面标题
     * @throws `SyntaxError` 仅有片段标识符
     */
    setLangLink(lang, link) {
        require('../../addon/link');
        this.setLangLink(lang, link);
    }
    /* c8 ignore stop */
    /* c8 ignore start */
    /**
     * Convert to a self link
     *
     * 修改为到自身的链接
     * @param fragment URI fragment / 片段标识符
     * @throws `RangeError` 空的片段标识符
     */
    asSelfLink(fragment) {
        require('../../addon/link');
        this.asSelfLink(fragment);
    }
    /* c8 ignore stop */
    /* c8 ignore start */
    /**
     * Automatically generate the link text after the pipe
     *
     * 自动生成管道符后的链接文字
     * @throws `Error` 带有"#"或"%"时不可用
     */
    pipeTrick() {
        require('../../addon/link');
        this.pipeTrick();
    }
}
exports.LinkToken = LinkToken;
constants_1.classes['LinkToken'] = __filename;
