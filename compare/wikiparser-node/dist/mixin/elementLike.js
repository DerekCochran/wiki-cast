"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.elementLike = void 0;
const debug_1 = require("../util/debug");
const selector_1 = require("../util/selector");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/** @ignore */
const elementLike = (constructor) => {
    LINT: {
        class ElementLike extends constructor {
            /* NOT FOR BROWSER */
            get children() {
                return this.childNodes.filter((child) => child.type !== 'text');
            }
            get firstElementChild() {
                return this.childNodes.find((child) => child.type !== 'text');
            }
            get lastElementChild() {
                return this.childNodes.findLast((child) => child.type !== 'text');
            }
            get childElementCount() {
                return this.children.length;
            }
            /* NOT FOR BROWSER END */
            #getCondition(selector) {
                return (0, selector_1.getCondition)(selector, 
                // eslint-disable-next-line unicorn/no-negated-condition, @stylistic/operator-linebreak
                !('type' in this) ?
                    undefined : // eslint-disable-line @stylistic/operator-linebreak
                    this);
            }
            getElementBy(condition) {
                const stack = [...this.childNodes].reverse();
                while (stack.length > 0) {
                    const child = stack.pop(), { type, childNodes } = child;
                    if (type === 'text') {
                        continue;
                    }
                    else if (condition(child)) {
                        return child;
                    }
                    for (let i = childNodes.length - 1; i >= 0; i--) {
                        stack.push(childNodes[i]);
                    }
                }
                return undefined;
            }
            querySelector(selector) {
                return this.getElementBy(this.#getCondition(selector));
            }
            getElementsBy(condition) {
                const stack = [...this.childNodes].reverse(), descendants = [];
                while (stack.length > 0) {
                    const child = stack.pop(), { type, childNodes } = child;
                    if (type === 'text') {
                        continue;
                    }
                    else if (condition(child)) {
                        descendants.push(child);
                    }
                    for (let i = childNodes.length - 1; i >= 0; i--) {
                        stack.push(childNodes[i]);
                    }
                }
                return descendants;
            }
            querySelectorAll(selector) {
                return this.getElementsBy(this.#getCondition(selector));
            }
            escape() {
                LSP: {
                    for (const child of this.childNodes) {
                        child.escape();
                    }
                    /* NOT FOR BROWSER */
                    this.detach?.();
                }
            }
            /* NOT FOR BROWSER */
            getElementByTypes(types) {
                const typeSet = new Set(types.split(',').map(str => str.trim()));
                return this.getElementBy((({ type }) => typeSet.has(type)));
            }
            getElementById(id) {
                return this.getElementBy((token => 'id' in token && token.id === id));
            }
            getElementsByClassName(className) {
                return this.getElementsBy((token => 'classList' in token && token.classList.has(className)));
            }
            getElementsByTagName(tag) {
                return this.getElementsBy((({ type, name }) => name === tag && (type === 'html' || type === 'ext')));
            }
        }
        (0, debug_1.mixin)(ElementLike, constructor);
        return ElementLike;
    }
};
exports.elementLike = elementLike;
constants_1.mixins['elementLike'] = __filename;
