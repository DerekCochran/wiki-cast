"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Attributes = void 0;
const constants_1 = require("../util/constants");
/** 用于选择器的属性 */
class Attributes {
    token;
    type;
    #link;
    #invalid;
    #siblings;
    #siblingsOfType;
    #siblingsCount;
    #siblingsCountOfType;
    #index;
    #indexOfType;
    #lastIndex;
    #lastIndexOfType;
    constructor(token) {
        this.token = token;
        this.type = token.type;
    }
    get link() {
        this.#link ??= this.token.link;
        return this.#link;
    }
    get invalid() {
        this.#invalid ??= this.token.getAttribute('invalid');
        return this.#invalid;
    }
    get siblings() {
        this.#siblings ??= this.token.parentNode?.children;
        return this.#siblings;
    }
    get siblingsOfType() {
        this.#siblingsOfType ??= this.siblings?.filter(({ type }) => type === this.type);
        return this.#siblingsOfType;
    }
    get siblingsCount() {
        this.#siblingsCount ??= this.siblings?.length ?? 1;
        return this.#siblingsCount;
    }
    get siblingsCountOfType() {
        this.#siblingsCountOfType ??= this.siblingsOfType?.length ?? 1;
        return this.#siblingsCountOfType;
    }
    get index() {
        this.#index ??= (this.siblings?.indexOf(this.token) ?? 0) + 1;
        return this.#index;
    }
    get indexOfType() {
        this.#indexOfType ??= (this.siblingsOfType?.indexOf(this.token) ?? 0) + 1;
        return this.#indexOfType;
    }
    get lastIndex() {
        this.#lastIndex ??= this.siblingsCount - this.index + 1;
        return this.#lastIndex;
    }
    get lastIndexOfType() {
        this.#lastIndexOfType ??= this.siblingsCountOfType - this.indexOfType + 1;
        return this.#lastIndexOfType;
    }
}
exports.Attributes = Attributes;
constants_1.classes['Attributes'] = __filename;
