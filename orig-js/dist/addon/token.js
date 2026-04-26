"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const constants_1 = require("../util/constants");
const debug_1 = require("../util/debug");
const index_1 = require("../src/index");
const comment_1 = require("../src/nowiki/comment");
const include_1 = require("../src/tagPair/include");
const ext_1 = require("../src/tagPair/ext");
const html_1 = require("../src/tag/html");
const attributes_1 = require("../src/attributes");
index_1.Token.prototype.createComment = /** @implements */ function (data = '') {
    // @ts-expect-error abstract class
    return debug_1.Shadow.run(() => new comment_1.CommentToken(data.replaceAll('-->', '--&gt;'), true, this.getAttribute('config')));
};
index_1.Token.prototype.createElement = /** @implements */ function (tagName, { selfClosing, closing } = {}) {
    tagName = tagName.toLowerCase();
    const config = this.getAttribute('config'), include = this.getAttribute('include');
    if (tagName === (include ? 'noinclude' : 'includeonly')) {
        return debug_1.Shadow.run(
        // @ts-expect-error abstract class
        () => new include_1.IncludeToken(tagName, '', undefined, selfClosing ? undefined : tagName, config));
    }
    else if (config.ext.includes(tagName)) {
        return debug_1.Shadow.run(
        // @ts-expect-error abstract class
        () => new ext_1.ExtToken(tagName, '', undefined, selfClosing ? undefined : tagName, config, include));
    }
    else if (config.html.some(tags => tags.includes(tagName))) {
        return debug_1.Shadow.run(() => {
            // @ts-expect-error abstract class
            const attr = new attributes_1.AttributesToken(undefined, 'html-attrs', tagName, config);
            attr.afterBuild();
            // @ts-expect-error abstract class
            return new html_1.HtmlToken(tagName, attr, Boolean(closing), Boolean(selfClosing), config);
        });
    }
    /* c8 ignore next */
    throw new RangeError(`Invalid tag name: ${tagName}`);
};
index_1.Token.prototype.sections = /** @implements */ function () {
    /* c8 ignore next 3 */
    if (this.type !== 'root') {
        return undefined;
    }
    const { childNodes, length } = this, headings = [...childNodes.entries()]
        .filter((entry) => entry[1].is('heading'))
        .map(([i, { level }]) => [i, level]), lastHeading = [-1, -1, -1, -1, -1, -1], sections = headings.map(([i]) => {
        const range = this.createRange();
        range.setStart(this, i);
        return range;
    });
    for (let i = 0; i < headings.length; i++) {
        const [index, level] = headings[i];
        for (let j = level; j < 6; j++) {
            const last = lastHeading[j];
            if (last >= 0) {
                sections[last].setEnd(this, index);
            }
            lastHeading[j] = j === level ? i : -1;
        }
    }
    for (const last of lastHeading) {
        if (last >= 0) {
            sections[last].setEnd(this, length);
        }
    }
    const range = this.createRange();
    range.setStart(this, 0);
    range.setEnd(this, headings[0]?.[0] ?? length);
    sections.unshift(range);
    return sections;
};
index_1.Token.prototype.findEnclosingHtml = /** @implements */ function (tag) {
    tag &&= tag.toLowerCase();
    const { html } = this.getAttribute('config'), normalTags = new Set(html[0]), voidTags = new Set(html[2]);
    /* c8 ignore next 6 */
    if (voidTags.has(tag)) {
        throw new RangeError(`Void tag: ${tag}`);
    }
    if (tag && !normalTags.has(tag) && !html[1].includes(tag)) {
        throw new RangeError(`Invalid tag name: ${tag}`);
    }
    const { parentNode } = this;
    if (!parentNode) {
        return undefined;
    }
    const { childNodes } = parentNode, index = childNodes.indexOf(this);
    let i = index - 1, j;
    for (; i >= 0; i--) {
        const open = childNodes[i], { name, closing, selfClosing } = open;
        if (open.is('html') && !closing
            && (tag ? name === tag : !voidTags.has(name))
            && (normalTags.has(name) || !selfClosing)) {
            const close = open.findMatchingTag();
            if (close) {
                j = childNodes.indexOf(close);
                if (j > index) {
                    break;
                }
            }
        }
    }
    if (i === -1) {
        return parentNode.findEnclosingHtml(tag);
    }
    const range = this.createRange();
    range.setStart(parentNode, i);
    range.setEnd(parentNode, j + 1); // eslint-disable-line @typescript-eslint/no-unnecessary-type-assertion
    return range;
};
constants_1.classes['ExtendedToken'] = __filename;
