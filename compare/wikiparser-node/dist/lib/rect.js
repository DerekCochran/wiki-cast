"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.BoundingRect = void 0;
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/* NOT FOR BROWSER END */
/** 节点位置 */
class BoundingRect {
    #token;
    #start;
    #pos;
    /** 起点 */
    get start() {
        return this.#start;
    }
    /** 起点行 */
    get top() {
        return this.#getPosition().top;
    }
    /** 起点列 */
    get left() {
        return this.#getPosition().left;
    }
    /**
     * @param token 节点
     * @param start 起点
     */
    constructor(token, start) {
        this.#token = token;
        this.#start = start;
    }
    /** 计算位置 */
    #getPosition() {
        this.#pos ??= this.#token.getRootNode().posFromIndex(this.#start);
        return this.#pos;
    }
}
exports.BoundingRect = BoundingRect;
constants_1.classes['BoundingRect'] = __filename;
