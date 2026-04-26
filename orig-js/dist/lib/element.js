"use strict";
var __runInitializers = (this && this.__runInitializers) || function (thisArg, initializers, value) {
    var useValue = arguments.length > 2;
    for (var i = 0; i < initializers.length; i++) {
        value = useValue ? initializers[i].call(thisArg, value) : initializers[i].call(thisArg);
    }
    return useValue ? value : void 0;
};
var __esDecorate = (this && this.__esDecorate) || function (ctor, descriptorIn, decorators, contextIn, initializers, extraInitializers) {
    function accept(f) { if (f !== void 0 && typeof f !== "function") throw new TypeError("Function expected"); return f; }
    var kind = contextIn.kind, key = kind === "getter" ? "get" : kind === "setter" ? "set" : "value";
    var target = !descriptorIn && ctor ? contextIn["static"] ? ctor : ctor.prototype : null;
    var descriptor = descriptorIn || (target ? Object.getOwnPropertyDescriptor(target, contextIn.name) : {});
    var _, done = false;
    for (var i = decorators.length - 1; i >= 0; i--) {
        var context = {};
        for (var p in contextIn) context[p] = p === "access" ? {} : contextIn[p];
        for (var p in contextIn.access) context.access[p] = contextIn.access[p];
        context.addInitializer = function (f) { if (done) throw new TypeError("Cannot add initializers after decoration has completed"); extraInitializers.push(accept(f || null)); };
        var result = (0, decorators[i])(kind === "accessor" ? { get: descriptor.get, set: descriptor.set } : descriptor[key], context);
        if (kind === "accessor") {
            if (result === void 0) continue;
            if (result === null || typeof result !== "object") throw new TypeError("Object expected");
            if (_ = accept(result.get)) descriptor.get = _;
            if (_ = accept(result.set)) descriptor.set = _;
            if (_ = accept(result.init)) initializers.unshift(_);
        }
        else if (_ = accept(result)) {
            if (kind === "field") initializers.unshift(_);
            else descriptor[key] = _;
        }
    }
    if (target) Object.defineProperty(target, contextIn.name, descriptor);
    done = true;
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.AstElement = void 0;
const string_1 = require("../util/string");
const debug_1 = require("../util/debug");
const selector_1 = require("../util/selector");
const node_1 = require("./node");
const elementLike_1 = require("../mixin/elementLike");
/* NOT FOR BROWSER */
const fs_1 = __importDefault(require("fs"));
const path_1 = __importDefault(require("path"));
const constants_1 = require("../util/constants");
const readOnly_1 = require("../mixin/readOnly");
/**
 * HTMLElement-like
 *
 * 类似HTMLElement
 */
let AstElement = (() => {
    let _classDecorators = [elementLike_1.elementLike];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _classSuper = node_1.AstNode;
    let _instanceExtraInitializers = [];
    let _removeAt_decorators;
    let _insertAt_decorators;
    var AstElement = class extends _classSuper {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(_classSuper[Symbol.metadata] ?? null) : void 0;
            _removeAt_decorators = [(0, readOnly_1.readOnly)()];
            _insertAt_decorators = [(0, readOnly_1.readOnly)()];
            __esDecorate(this, null, _removeAt_decorators, { kind: "method", name: "removeAt", static: false, private: false, access: { has: obj => "removeAt" in obj, get: obj => obj.removeAt }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(this, null, _insertAt_decorators, { kind: "method", name: "insertAt", static: false, private: false, access: { has: obj => "insertAt" in obj, get: obj => obj.insertAt }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            AstElement = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        /** number of child nodes / 子节点总数 */
        get length() {
            return this.childNodes.length;
        }
        /* NOT FOR BROWSER */
        set length(n) {
            if (n >= 0 && n < this.length) {
                for (let i = this.length - 1; i >= n; i--) {
                    this.removeAt(i);
                }
            }
        }
        /** parent node / 父节点 */
        get parentElement() {
            return this.parentNode;
        }
        /** visible text / 可见部分 */
        get outerText() {
            return this.text();
        }
        /** invisible / 不可见 */
        get hidden() {
            return !this.text();
        }
        /** height of the inner / 内部高度 */
        get clientHeight() {
            return this.innerText?.split('\n').length;
        }
        /** width of the inner / 内部宽度 */
        get clientWidth() {
            return this.innerText?.split('\n').pop().length;
        }
        /** all images, including gallery images / 所有图片，包括图库 */
        get images() {
            return this.querySelectorAll('file,gallery-image,imagemap-image');
        }
        /** all internal, external and free external links / 所有内链、外链和自由外链 */
        get links() {
            return this.querySelectorAll('link,redirect-target,ext-link,free-ext-link,magic-link,image-parameter#link').filter(({ parentNode }) => !parentNode?.is('image-parameter')
                || parentNode.name !== 'link');
        }
        /** all templates and modules / 所有模板和模块 */
        get embeds() {
            return this.querySelectorAll('template,magic-word#invoke');
        }
        constructor() {
            super();
            __runInitializers(this, _instanceExtraInitializers);
            this.seal('name');
        }
        /* NOT FOR BROWSER END */
        /** @private */
        text(separator) {
            return (0, string_1.text)(this.childNodes, separator);
        }
        /**
         * Remove a child node
         *
         * 移除子节点
         * @param i position of the child node / 移除位置
         */
        removeAt(i) {
            /* NOT FOR BROWSER */
            this.verifyChild(i);
            /* NOT FOR BROWSER END */
            LSP: return (0, debug_1.setChildNodes)(this, i, 1)[0];
        }
        /**
         * Insert a child node
         *
         * 插入子节点
         * @param node node to be inserted / 待插入的子节点
         * @param i position to be inserted at / 插入位置
         * @throws `RangeError` 不能插入祖先或子节点
         */
        insertAt(node, i = this.length) {
            /* NOT FOR BROWSER */
            /* c8 ignore next 6 */
            if (node.contains(this)) {
                throw new RangeError('Cannot insert an ancestor node!');
            }
            if (node.parentNode === this) {
                throw new RangeError('Cannot insert its own child node!');
            }
            this.verifyChild(i, 1);
            node.parentNode?.removeChild(node);
            /* NOT FOR BROWSER END */
            (0, debug_1.setChildNodes)(this, i, 0, [node]);
            return node;
        }
        /**
         * Get the closest ancestor node that matches the selector
         *
         * 最近的符合选择器的祖先节点
         * @param selector selector / 选择器
         */
        closest(selector) {
            const condition = (0, selector_1.getCondition)(selector, this);
            let { parentNode } = this;
            while (parentNode) {
                if (condition(parentNode)) {
                    return parentNode;
                }
                ({ parentNode } = parentNode);
            }
            return undefined;
        }
        /** @private */
        isInside(type) {
            return this.closest(`${type},ext`)?.type === type;
        }
        /**
         * Insert a batch of child nodes at the end
         *
         * 在末尾批量插入子节点
         * @param elements nodes to be inserted / 插入节点
         */
        append(...elements) {
            this.safeAppend(elements);
        }
        /** @private */
        safeAppend(elements) {
            for (const element of elements) {
                this.insertAt(element);
            }
        }
        /** @private */
        safeReplaceChildren(elements) {
            LSP: {
                for (let i = this.length - 1; i >= 0; i--) {
                    this.removeAt(i);
                }
                this.safeAppend([...elements]);
            }
        }
        /**
         * Modify the text child node
         *
         * 修改文本子节点
         * @param str new text / 新文本
         * @param i position of the text child node / 子节点位置
         * @throws `RangeError` 对应位置的子节点不是文本节点
         */
        setText(str, i = 0) {
            i += i < 0 ? this.length : 0;
            /* NOT FOR BROWSER */
            this.verifyChild(i);
            /* NOT FOR BROWSER END */
            const oldText = this.childNodes[i];
            if (oldText.type === 'text') {
                const { data } = oldText;
                oldText.replaceData(str);
                return data;
            }
            /* NOT FOR BROWSER */
            /* c8 ignore next */
            throw new RangeError(`The child node at position ${i} is ${oldText.constructor.name}!`);
        }
        /** @private */
        toString(skip, separator = '') {
            return this.childNodes.map(child => child.toString(skip)).join(separator);
        }
        /**
         * Get the caret position from the character index
         *
         * 找到给定位置
         * @param index character index / 位置
         */
        caretPositionFromIndex(index) {
            LSP: {
                if (index === undefined) {
                    return undefined;
                }
                const { length } = this.toString();
                if (index > length || index < -length) {
                    return undefined;
                }
                index += index < 0 ? length : 0;
                let self = this, acc = 0, start = 0;
                while (self.type !== 'text') {
                    const { childNodes } = self;
                    acc += self.getAttribute('padding');
                    for (let i = 0; acc <= index && i < childNodes.length; i++) {
                        const cur = childNodes[i], { nextSibling } = cur, str = cur.toString(), l = str.length;
                        cur.setAttribute('aIndex', acc);
                        acc += l;
                        // 优先选择靠前的非文本兄弟节点，但永不进入假节点
                        if (acc > index
                            || acc === index && l > 0 && (!nextSibling
                                || nextSibling.type === 'text'
                                || cur.type !== 'text' && (str.trim() || !nextSibling.toString().trim()))) {
                            self = cur;
                            acc -= l;
                            start = acc;
                            break;
                        }
                        acc += self.getGaps(i);
                    }
                    if (self.childNodes === childNodes) {
                        return { offsetNode: self, offset: index - start };
                    }
                }
                return { offsetNode: self, offset: index - start };
            }
        }
        /**
         * Get the closest ancestor element from the character index
         *
         * 找到给定位置所在的最内层非文本节点
         * @param index character index / 位置
         */
        elementFromIndex(index) {
            LSP: {
                const node = this.caretPositionFromIndex(index)?.offsetNode;
                return node?.type === 'text' ? node.parentNode : node;
            }
        }
        /**
         * Get the closest ancestor element from the position
         *
         * 找到给定位置所在的最内层非文本节点
         * @param x column number / 列数
         * @param y line number / 行数
         */
        elementFromPoint(x, y) {
            LSP: return this.elementFromIndex(this.indexFromPos(y, x));
        }
        /** @private */
        lint(start = this.getAbsoluteIndex(), re) {
            LINT: {
                const errors = [];
                for (let i = 0, cur = start + this.getAttribute('padding'); i < this.length; i++) {
                    const child = this.childNodes[i];
                    child.setAttribute('aIndex', cur);
                    const childErrors = child.lint(cur, re);
                    if (childErrors.length > 0) {
                        Array.prototype.push.apply(errors, childErrors);
                    }
                    cur += child.toString().length + this.getGaps(i);
                }
                return errors;
            }
        }
        /** @private */
        print(opt = {}) {
            PRINT: {
                const cl = opt.class;
                if (this.toString()) {
                    return (cl === ''
                        ? ''
                        : `<span class="wpb-${cl ?? this.type}${this.getAttribute('invalid') ? ' wpb-invalid' : ''}">`)
                        + (0, string_1.print)(this.childNodes, opt)
                        + (cl === '' ? '' : '</span>');
                }
                return '';
            }
        }
        /**
         * Save in JSON format
         *
         * 保存为JSON
         * @param file file name / 文件名
         * @param depth depth of the node / 节点深度
         * @param start
         */
        json(file, depth = Infinity, start = this.getAbsoluteIndex()) {
            LSP: {
                const json = {
                    ...this, // eslint-disable-line @typescript-eslint/no-misused-spread
                    type: this.type,
                    range: [start, start + this.toString().length],
                    childNodes: [],
                };
                if (depth >= 1) {
                    for (let i = 0, cur = start + this.getAttribute('padding'); i < this.length; i++) {
                        const child = this.childNodes[i], { length } = child.toString();
                        child.setAttribute('aIndex', cur);
                        json.childNodes.push(child.type === 'text'
                            ? { data: child.data, range: [cur, cur + length] }
                            : child.json(undefined, depth - 1, cur));
                        cur += length + this.getGaps(i);
                    }
                }
                /* NOT FOR BROWSER */
                /* c8 ignore start */
                if (typeof file === 'string') {
                    fs_1.default.writeFileSync(path_1.default.join(__dirname, '..', '..', 'printed', file + (file.endsWith('.json') ? '' : '.json')), JSON.stringify(json, null, 2));
                }
                /* c8 ignore stop */
                /* NOT FOR BROWSER END */
                return json;
            }
        }
        /* NOT FOR BROWSER */
        /**
         * Merge adjacent text child nodes
         *
         * 合并相邻的文本子节点
         */
        normalize() {
            const childNodes = this.getChildNodes();
            /**
             * 移除子节点
             * @param i 移除位置
             */
            const remove = (i) => {
                childNodes[i].setAttribute('parentNode', undefined);
                childNodes.splice(i, 1);
                childNodes[i - 1]?.setAttribute('nextSibling', childNodes[i]);
                childNodes[i]?.setAttribute('previousSibling', childNodes[i - 1]);
            };
            for (let i = childNodes.length - 1; i >= 0; i--) {
                const { type, data } = childNodes[i];
                if (type !== 'text' || childNodes.length === 1 || this.getGaps(i - (i && 1))) {
                    //
                }
                else if (data === '') {
                    remove(i);
                }
                else {
                    const prev = childNodes[i - 1];
                    if (prev?.type === 'text') {
                        prev.setAttribute('data', prev.data + data);
                        remove(i);
                    }
                }
            }
            this.setAttribute('childNodes', childNodes);
        }
        /**
         * Check if the current element matches the selector
         *
         * 检查是否符合选择器
         * @param selector selector / 选择器
         */
        matches(selector) {
            return (0, selector_1.getCondition)(selector, this)(this);
        }
        /**
         * Insert a batch of child nodes at the start
         *
         * 在开头批量插入子节点
         * @param elements nodes to be inserted / 插入节点
         */
        prepend(...elements) {
            for (let i = 0; i < elements.length; i++) {
                this.insertAt(elements[i], i);
            }
        }
        /**
         * 获取子节点的位置
         * @param node 子节点
         * @throws `RangeError` 找不到子节点
         */
        #getChildIndex(node) {
            const i = this.childNodes.indexOf(node);
            /* c8 ignore next 3 */
            if (i === -1) {
                throw new RangeError('Not a child node!');
            }
            return i;
        }
        /**
         * Remove a child node
         *
         * 移除子节点
         * @param node child node to be removed / 子节点
         */
        removeChild(node) {
            return this.removeAt(this.#getChildIndex(node));
        }
        /**
         * Replace all child nodes
         *
         * 批量替换子节点
         * @param elements nodes to be inserted / 新的子节点
         */
        replaceChildren(...elements) {
            this.safeReplaceChildren(elements);
        }
        insertBefore(child, reference) {
            return reference === undefined
                ? this.insertAt(child)
                : this.insertAt(child, this.#getChildIndex(reference));
        }
        /**
         * Get the caret position from the point
         *
         * 找到给定位置
         * @param x column number / 列数
         * @param y line number / 行数
         */
        caretPositionFromPoint(x, y) {
            return this.caretPositionFromIndex(this.indexFromPos(y, x));
        }
        /**
         * Get all ancestor elements from the character index
         *
         * 找到给定位置所在的所有节点
         * @param index character index / 位置
         */
        elementsFromIndex(index) {
            const offsetNode = this.elementFromIndex(index);
            return offsetNode ? [...offsetNode.getAncestors().reverse(), offsetNode] : [];
        }
        /**
         * Get all ancestor elements from the position
         *
         * 找到给定位置所在的所有节点
         * @param x column number / 列数
         * @param y line number / 行数
         */
        elementsFromPoint(x, y) {
            return this.elementsFromIndex(this.indexFromPos(y, x));
        }
    };
    return AstElement = _classThis;
})();
exports.AstElement = AstElement;
constants_1.classes['AstElement'] = __filename;
