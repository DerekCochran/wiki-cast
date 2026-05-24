"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.nodeLike = void 0;
const debug_1 = require("../util/debug");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/** @ignore */
const nodeLike = (constructor) => {
    class NodeLike extends constructor {
        get firstChild() {
            return this.childNodes[0];
        }
        get lastChild() {
            return this.childNodes[this.childNodes.length - 1];
        }
        get offsetHeight() {
            LINT: return this.getDimension().height;
        }
        get offsetWidth() {
            LINT: return this.getDimension().width;
        }
    }
    (0, debug_1.mixin)(NodeLike, constructor);
    return NodeLike;
};
exports.nodeLike = nodeLike;
constants_1.mixins['nodeLike'] = __filename;
