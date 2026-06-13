"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.ListBaseToken = void 0;
const base_1 = require("./base");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
const debug_1 = require("../../util/debug");
const text_1 = require("../../lib/text");
const index_1 = require("../index");
/* NOT FOR BROWSER END */
/** `;:*#` */
class ListBaseToken extends base_1.NowikiBaseToken {
    /* PRINT ONLY */
    /**
     * number of indentation
     *
     * 缩进数
     * @since v1.16.5
     */
    get indent() {
        LSP: return this.innerText.split(':').length - 1;
    }
    /* PRINT ONLY END */
    /* NOT FOR BROWSER */
    /** @throws `Error` not `<dd>` only */
    set indent(indent) {
        if (/[^:]/u.test(this.innerText)) {
            throw new Error('The token is not <dd>!');
        }
        this.setText(':'.repeat(indent));
    }
    /** whether to contain `:` / 是否包含`:` */
    get dd() {
        return this.innerText.includes(':');
    }
    /** whether to contain `;` / 是否包含`;` */
    get dt() {
        return this.innerText.includes(';');
    }
    /** whether to contain `*` / 是否包含`*` */
    get ul() {
        return this.innerText.includes('*');
    }
    /** whether to contain `#` / 是否包含`#` */
    get ol() {
        return this.innerText.includes('#');
    }
    /* NOT FOR BROWSER END */
    /** @private */
    json(_, depth, start = this.getAbsoluteIndex()) {
        LSP: {
            const json = super.json(undefined, depth, start), { indent } = this;
            if (indent) {
                json['indent'] = indent;
            }
            return json;
        }
    }
    /* NOT FOR BROWSER */
    /**
     * Get the range of the list
     *
     * 获取列表行的范围
     * @throws `Error` 不存在父节点
     */
    getRange() {
        const { parentNode } = this;
        if (!parentNode) {
            throw new Error('There is no parent node!');
        }
        let { nextSibling } = this;
        if (nextSibling?.is('list-range')) {
            return nextSibling;
        }
        const { dt, type } = this;
        let nDt = 0;
        while (nextSibling && (nextSibling.type !== 'text' || !nextSibling.data.includes('\n'))) {
            if (type === 'list') {
                if (nextSibling.is('dd')) {
                    nDt -= nextSibling.indent;
                    if (dt && nDt < 0) {
                        break;
                    }
                }
                else if (nextSibling.is('list') && nextSibling.dt) {
                    nDt++;
                }
            }
            else if (nextSibling.is('dd')) {
                break;
            }
            ({ nextSibling } = nextSibling);
        }
        let start, end, contents;
        if (nextSibling && nextSibling.type !== 'text') {
            const { childNodes } = parentNode;
            start = childNodes.indexOf(this) + 1;
            end = childNodes.indexOf(nextSibling);
            contents = childNodes.slice(start, end);
        }
        else {
            if (type === 'list') {
                while (this.previousSibling?.is('list')) {
                    this.setText(this.previousSibling.innerText + this.innerText);
                    this.previousSibling.remove();
                }
                for (let i = 0; i < nDt; i++) {
                    const token = this.nextSibling;
                    this.setText(this.innerText + token.innerText);
                    token.remove();
                }
                if (parentNode.is('list-range')) {
                    parentNode.previousSibling.setText(parentNode.previousSibling.innerText + this.innerText);
                    this.remove();
                    return parentNode;
                }
            }
            const { childNodes } = parentNode;
            start = childNodes.indexOf(this) + 1;
            if (nextSibling) {
                const { data } = nextSibling, offset = data.indexOf('\n'), text = new text_1.AstText(data.slice(0, offset));
                end = childNodes.indexOf(nextSibling);
                contents = childNodes.slice(start, end);
                const last = contents.at(-1);
                if (last) {
                    last.setAttribute('nextSibling', text);
                    text.setAttribute('previousSibling', last);
                }
                contents.push(text);
                nextSibling.setAttribute('data', data.slice(offset));
            }
            else {
                end = childNodes.length;
                contents = childNodes.slice(start);
            }
        }
        const token = debug_1.Shadow.run(() => {
            const t = new index_1.Token(undefined, this.getAttribute('config'));
            t.type = 'list-range';
            return t;
        });
        token.concat(contents); // eslint-disable-line unicorn/prefer-spread
        (0, debug_1.setChildNodes)(parentNode, start, end - start, [token]);
        return token;
    }
    /** @private */
    toHtmlInternal() {
        return '';
    }
}
exports.ListBaseToken = ListBaseToken;
constants_1.classes['ListBaseToken'] = __filename;
