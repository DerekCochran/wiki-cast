"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.attributesParent = void 0;
const debug_1 = require("../util/debug");
/* NOT FOR BROWSER */
const constants_1 = require("../util/constants");
/**
 * 子节点含有AttributesToken的类
 * @param i AttributesToken子节点的位置
 */
const attributesParent = (i = 0) => (constructor) => {
    LINT: {
        class AttributesParent extends constructor {
            /* NOT FOR BROWSER */
            get attributes() {
                return this.#getAttributesChild().attributes;
            }
            set attributes(attributes) {
                this.#getAttributesChild().attributes = attributes;
            }
            get className() {
                return this.#getAttributesChild().className;
            }
            set className(className) {
                this.#getAttributesChild().className = className;
            }
            get classList() {
                return this.#getAttributesChild().classList;
            }
            get id() {
                return this.#getAttributesChild().id;
            }
            set id(id) {
                this.#getAttributesChild().id = id;
            }
            /* NOT FOR BROWSER END */
            /** AttributesToken子节点 */
            #getAttributesChild() {
                return this.childNodes[i];
            }
            hasAttr(key) {
                LSP: return this.#getAttributesChild().hasAttr(key);
            }
            getAttr(key) {
                return this.#getAttributesChild().getAttr(key);
            }
            /* NOT FOR BROWSER */
            getAttrNames() {
                return this.#getAttributesChild().getAttrNames();
            }
            getAttrs() {
                return this.#getAttributesChild().getAttrs();
            }
            setAttr(keyOrProp, value) {
                this.#getAttributesChild().setAttr(keyOrProp, value);
            }
            removeAttr(key) {
                this.#getAttributesChild().removeAttr(key);
            }
            toggleAttr(key, force) {
                this.#getAttributesChild().toggleAttr(key, force);
            }
            css(key, value) {
                return this.#getAttributesChild().css(key, value);
            }
        }
        (0, debug_1.mixin)(AttributesParent, constructor);
        return AttributesParent;
    }
};
exports.attributesParent = attributesParent;
constants_1.mixins['attributesParent'] = __filename;
