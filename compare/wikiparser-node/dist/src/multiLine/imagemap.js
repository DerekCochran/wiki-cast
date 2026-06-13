"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.ImagemapToken = void 0;
const lint_1 = require("../../util/lint");
const debug_1 = require("../../util/debug");
const rect_1 = require("../../lib/rect");
const index_1 = __importDefault(require("../../index"));
const index_2 = require("./index");
const commentLine_1 = require("../nowiki/commentLine");
const galleryImage_1 = require("../link/galleryImage");
const imagemapLink_1 = require("../imagemapLink");
/* NOT FOR BROWSER */
const constants_1 = require("../../util/constants");
/* NOT FOR BROWSER END */
/**
 * `<imagemap>`
 * @classdesc `{childNodes: [...CommentLineToken[], GalleryImageToken, ...(CommentLineToken|ImagemapLinkToken|AstText)[]]}`
 */
class ImagemapToken extends index_2.MultiLineToken {
    /* NOT FOR BROWSER END */
    /** 图片 */
    get image() {
        return this.childNodes.find((0, debug_1.isToken)('imagemap-image'));
    }
    /** @param inner 标签内部wikitext */
    constructor(inner, config = index_1.default.getConfig(), accum = []) {
        super(undefined, config, accum, {
            GalleryImageToken: ':', ImagemapLinkToken: ':', CommentLineToken: ':', AstText: ':',
        });
        if (!inner) {
            return;
        }
        const lines = inner.split('\n'), protocols = new Set(config.protocol.split('|'));
        let first = true, error = false;
        for (const line of lines) {
            const trimmed = line.trim();
            if (error || !trimmed || trimmed.startsWith('#')) {
                //
            }
            else if (first) {
                const pipe = line.indexOf('|'), file = pipe === -1 ? line : line.slice(0, pipe), title = this.normalizeTitle(file, 0, { halfParsed: true, temporary: true, page: '' });
                if (title.valid && title.ns === 6
                    && !title.interwiki) {
                    // @ts-expect-error abstract class
                    const token = new galleryImage_1.GalleryImageToken('imagemap', file, pipe === -1 ? undefined : line.slice(pipe + 1), config, accum);
                    super.insertAt(token);
                    first = false;
                    continue;
                }
                else {
                    error = true;
                }
            }
            else if (line.trim().split(/[\t ]/u, 1)[0] === 'desc') {
                super.insertAt(line);
                continue;
            }
            else if (line.includes('[')) {
                const i = line.indexOf('['), substr = line.slice(i), mtIn = /^\[\[([^|]+)(?:\|([^\]]*))?\]\][\w\s]*$/u
                    .exec(substr);
                if (mtIn) {
                    if (this.normalizeTitle(mtIn[1], 0, { halfParsed: true, temporary: true, selfLink: true, page: '' }).valid) {
                        // @ts-expect-error abstract class
                        super.insertAt(new imagemapLink_1.ImagemapLinkToken(line.slice(0, i), mtIn.slice(1), substr.slice(substr.indexOf(']]') + 2), config, accum));
                        continue;
                    }
                }
                else if (substr.startsWith('[//')
                    || protocols.has(substr.slice(1, substr.indexOf(':') + 1))
                    || protocols.has(substr.slice(1, substr.indexOf('//') + 2))) {
                    const mtEx = /^\[([^\]\s]+)(?:(\s+(?!\s))([^\]]*))?\][\w\s]*$/u
                        .exec(substr);
                    if (mtEx) {
                        // @ts-expect-error abstract class
                        super.insertAt(new imagemapLink_1.ImagemapLinkToken(line.slice(0, i), mtEx.slice(1), substr.slice(substr.indexOf(']') + 1), config, accum));
                        continue;
                    }
                }
            }
            // @ts-expect-error abstract class
            super.insertAt(new commentLine_1.CommentLineToken(line, config, accum));
        }
    }
    /** @private */
    lint(start = this.getAbsoluteIndex(), re) {
        LINT: {
            const errors = super.lint(start, re), rect = new rect_1.BoundingRect(this, start), { childNodes, image } = this, rule = 'invalid-imagemap', { lintConfig } = index_1.default, s = lintConfig.getSeverity(rule, image ? 'link' : 'image');
            if (s) {
                if (image) {
                    Array.prototype.push.apply(errors, childNodes.filter(child => {
                        const str = child.toString().trim();
                        return child.is('noinclude')
                            && str && !str.startsWith('#');
                    }).map(child => {
                        const e = (0, lint_1.generateForChild)(child, rect, rule, 'invalid-imagemap-link', s);
                        if (lintConfig.computeEditInfo) {
                            e.suggestions = [
                                (0, lint_1.fixByRemove)(e, -1),
                                (0, lint_1.fixBy)(e, 'comment', '# '),
                            ];
                        }
                        return e;
                    }));
                }
                else {
                    errors.push((0, lint_1.generateForSelf)(this, rect, rule, 'imagemap-without-image', s));
                }
            }
            return errors;
        }
    }
    /* PRINT ONLY */
    /** @private */
    getAttribute(key) {
        return key === 'invalid' ? !this.image : super.getAttribute(key);
    }
    insertAt(token, i) {
        const { image } = this;
        if (!image && (typeof token === 'string' || token.is('imagemap-link') || token.type === 'text')) {
            throw new Error('Missing a valid image!');
        }
        else if (image && typeof token !== 'string' && token.is('imagemap-image')) {
            throw new RangeError('Already have a valid image!');
        }
        return super.insertAt(token, i);
    }
    /** @private */
    removeAt(i) {
        if (!this.parentNode?.selfClosing && this.childNodes[i]?.is('imagemap-image')) {
            throw new Error('Do not remove the image in <imagemap>!');
        }
        return super.removeAt(i);
    }
}
exports.ImagemapToken = ImagemapToken;
constants_1.classes['ImagemapToken'] = __filename;
