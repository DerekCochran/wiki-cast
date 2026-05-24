"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.NowikiToken = void 0;
const common_1 = require("@bhsd/common");
const constants_1 = require("../../util/constants");
const lint_1 = require("../../util/lint");
const rect_1 = require("../../lib/rect");
const index_1 = __importDefault(require("../../index"));
const base_1 = require("./base");
/* NOT FOR BROWSER ONLY */
const constants_2 = require("../../util/constants");
const document_1 = require("../../lib/document");
/** @ignore */
const updateLocation = ({ startIndex, startLine, startCol, endIndex, endLine, endCol }, { offset, line, column }, n) => {
    const index = startIndex + offset - n;
    if (index < startIndex) {
        return [startIndex, startLine, startCol];
    }
    else if (index > endIndex) {
        return [endIndex, endLine, endCol];
    }
    return [index, startLine + line - 1, (line === 1 ? startCol - n : 0) + column - 1];
};
/* NOT FOR BROWSER ONLY END */
/<\s*(?:\/\s*)?(nowiki)\b/giu; // eslint-disable-line @typescript-eslint/no-unused-expressions
const getLintRegex = /* #__PURE__ */ (0, common_1.getRegex)(name => new RegExp(String.raw `<\s*(?:/\s*)${name === 'nowiki' ? '' : '?'}(${name})\b`, 'giu'));
const voidExt = new Set(['languages', 'section', 'templatestyles']);
/**
 * text-only token inside an extension tag
 *
 * 扩展标签内的纯文字Token
 */
class NowikiToken extends base_1.NowikiBaseToken {
    /* NOT FOR BROWSER END */
    get type() {
        return 'ext-inner';
    }
    /** 扩展标签内的无效内容 */
    #lint() {
        const { name, firstChild: { data } } = this;
        return voidExt.has(name) && Boolean(data);
    }
    /** @private */
    lint(start = this.getAbsoluteIndex()) {
        LINT: {
            const { name, innerText, 
            /* NOT FOR BROWSER ONLY */
            previousSibling, } = this, { lintConfig } = index_1.default;
            let rule = 'void-ext', s = lintConfig.getSeverity(rule, name);
            if (s && this.#lint()) {
                const e = (0, lint_1.generateForSelf)(this, { start }, rule, index_1.default.msg('nothing-in', name), s);
                if (lintConfig.computeEditInfo) {
                    e.suggestions = [(0, lint_1.fixByRemove)(e)];
                }
                return [e];
            }
            NPM: {
                rule = 'invalid-json';
                const sSyntax = lintConfig.getSeverity(rule), sDuplicate = lintConfig.getSeverity(rule, 'duplicate');
                if (constants_1.jsonTags.includes(name) && (sSyntax || sDuplicate)) {
                    return (name === 'templatedata' ? common_1.lintJSON : common_1.lintJSONC)(innerText).map(({ message, from, to = from, line, endLine = line, column, endColumn = column, severity, }) => {
                        s = severity === 'warning' ? sDuplicate : sSyntax;
                        if (!s) {
                            return false;
                        }
                        const rect = new rect_1.BoundingRect(this, start);
                        return {
                            rule,
                            message,
                            severity: s,
                            startIndex: start + from,
                            endIndex: start + to,
                            startLine: rect.top + line - 1,
                            endLine: rect.top + endLine - 1,
                            startCol: (line > 1 ? 0 : rect.left) + column - 1,
                            endCol: (endLine > 1 ? 0 : rect.left) + endColumn - 1,
                        };
                    }).filter((e) => e !== false);
                }
                /* NOT FOR BROWSER ONLY */
                rule = 'invalid-math';
                s = lintConfig.getSeverity(rule);
                if (s && constants_2.mathTags.has(name)) {
                    const texvcjs = (0, document_1.loadTexvcjs)();
                    if (texvcjs) {
                        const isChem = name !== 'math', display = previousSibling?.getAttr('display') ?? 'block';
                        let tex = innerText, n = 0;
                        if (isChem) {
                            tex = String.raw `\ce{${tex}}`;
                            n = 4;
                        }
                        switch (display) {
                            case 'block':
                                tex = String.raw `{\displaystyle ${tex}}`;
                                n += 15;
                                break;
                            case 'inline':
                                tex = String.raw `{\textstyle ${tex}}`;
                                n += 12;
                                break;
                            case 'linebreak':
                                tex = String.raw `\[ ${tex} \]`;
                                n += 3;
                            // no default
                        }
                        const result = texvcjs.check(tex, {
                            usemhchem: isChem || Boolean(previousSibling?.hasAttr('chem')),
                        });
                        if (result.status === '+') {
                            return [];
                        }
                        const e = (0, lint_1.generateForSelf)(this, { start }, rule, 'chem-required', s);
                        if (result.status !== 'C') {
                            /** @todo native MathML supports more macros than texvcjs */
                            const { message, location } = result.error, [endIndex, endLine, endCol] = updateLocation(e, location.end, n);
                            [e.startIndex, e.startLine, e.startCol] = updateLocation(e, location.start, n);
                            Object.assign(e, { endIndex, endLine, endCol, message });
                        }
                        return [e];
                    }
                }
            }
            /* NOT FOR BROWSER ONLY END */
            return super.lint(start, getLintRegex(name));
        }
    }
    /* PRINT ONLY */
    /** @private */
    getAttribute(key) {
        return key === 'invalid' ? this.#lint() : super.getAttribute(key);
    }
    /* PRINT ONLY END */
    /* NOT FOR BROWSER */
    /** @private */
    safeReplaceChildren(elements) {
        if (elements.length === 0) {
            this.setText('');
        }
        else {
            super.safeReplaceChildren(elements);
        }
    }
}
exports.NowikiToken = NowikiToken;
constants_2.classes['NowikiToken'] = __filename;
