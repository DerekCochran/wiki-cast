"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.RedirectTargetToken = void 0;
const lint_1 = require("../../util/lint");
const index_1 = __importDefault(require("../../index"));
const base_1 = require("./base");
const noinclude_1 = require("../nowiki/noinclude");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
/* NOT FOR BROWSER END */
/**
 * target of a redirect
 *
 * 重定向目标
 * @classdesc `{childNodes: [AtomToken, ?NoincludeToken]}`
 */
class RedirectTargetToken extends base_1.LinkBaseToken {
    /* NOT FOR BROWSER END */
    get type() {
        return 'redirect-target';
    }
    /* NOT FOR BROWSER */
    /**
     * link text
     *
     * 链接显示文字
     * @since v1.10.0
     */
    get innerText() {
        return this.link.toString(true);
    }
    /* NOT FOR BROWSER END */
    /**
     * @param link 链接标题
     * @param linkText 链接显示文字
     */
    constructor(link, linkText, config, accum) {
        super(link, undefined, config, accum);
        if (linkText !== undefined) {
            // @ts-expect-error abstract class
            this.insertAt(new noinclude_1.NoincludeToken(linkText, config, accum));
        }
        /* NOT FOR BROWSER */
        this.setAttribute('acceptable', { AtomToken: 0, NoincludeToken: 1 });
        // @ts-expect-error abstract getter
        this.firstChild.setAttribute('acceptable', { AstText: ':' });
    }
    /** @private */
    getTitle() {
        return this.normalizeTitle(this.firstChild.toString(), 0, { halfParsed: true, decode: true, page: '' });
    }
    /** @private */
    lint(start = this.getAbsoluteIndex()) {
        LINT: {
            const errors = super.lint(start, false), rule = 'no-ignored', { lintConfig } = index_1.default, s = lintConfig.getSeverity(rule, 'redirect');
            if (s && this.length === 2) {
                const e = (0, lint_1.generateForChild)(this.lastChild, { start }, rule, 'useless-link-text', s);
                e.startIndex--;
                e.startCol--;
                if (lintConfig.computeEditInfo || lintConfig.fix) {
                    e.fix = (0, lint_1.fixByRemove)(e);
                }
                errors.push(e);
            }
            return errors;
        }
    }
    /* NOT FOR BROWSER */
    /** @private */
    setTarget(link) {
        this.firstChild.setText(link);
    }
    /** @private */
    setLinkText(linkStr) {
        if (!linkStr) {
            this.childNodes[1]?.remove();
        }
    }
}
exports.RedirectTargetToken = RedirectTargetToken;
constants_1.classes['RedirectTargetToken'] = __filename;
