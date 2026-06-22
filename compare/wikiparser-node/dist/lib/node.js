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
exports.AstNode = void 0;
/* eslint-disable @typescript-eslint/no-base-to-string */
const common_1 = require("@bhsd/common");
const search_1 = __importDefault(require("../util/search"));
const lint_1 = require("../util/lint");
const debug_1 = require("../util/debug");
const cached_1 = require("../mixin/cached");
const nodeLike_1 = require("../mixin/nodeLike");
/* PRINT ONLY */
const index_1 = __importDefault(require("../index"));
/* PRINT ONLY END */
/* NOT FOR BROWSER */
const strict_1 = __importDefault(require("assert/strict"));
const events_1 = require("events");
const constants_1 = require("../util/constants");
/**
 * Node-like
 *
 * 类似Node
 */
let AstNode = (() => {
    let _classDecorators = [nodeLike_1.nodeLike];
    let _classDescriptor;
    let _classExtraInitializers = [];
    let _classThis;
    let _instanceExtraInitializers = [];
    let _getLines_decorators;
    var AstNode = class {
        static { _classThis = this; }
        static {
            const _metadata = typeof Symbol === "function" && Symbol.metadata ? Object.create(null) : void 0;
            _getLines_decorators = [(0, cached_1.cached)(false)];
            __esDecorate(this, null, _getLines_decorators, { kind: "method", name: "getLines", static: false, private: false, access: { has: obj => "getLines" in obj, get: obj => obj.getLines }, metadata: _metadata }, null, _instanceExtraInitializers);
            __esDecorate(null, _classDescriptor = { value: _classThis }, _classDecorators, { kind: "class", name: _classThis.name, metadata: _metadata }, null, _classExtraInitializers);
            AstNode = _classThis = _classDescriptor.value;
            if (_metadata) Object.defineProperty(_classThis, Symbol.metadata, { enumerable: true, configurable: true, writable: true, value: _metadata });
            __runInitializers(_classThis, _classExtraInitializers);
        }
        childNodes = (__runInitializers(this, _instanceExtraInitializers), []);
        #parentNode;
        #nextSibling;
        #previousSibling;
        #root;
        #aIndex;
        #rIndex = {};
        /* NOT FOR BROWSER */
        #optional = new Set();
        #events = new events_1.EventEmitter();
        /** parent node / 父节点 */
        get parentNode() {
            return this.#parentNode;
        }
        /** next sibling node / 后一个兄弟节点 */
        get nextSibling() {
            return this.#nextSibling;
        }
        /** previous sibling node / 前一个兄弟节点 */
        get previousSibling() {
            return this.#previousSibling;
        }
        /* NOT FOR BROWSER */
        /** next sibling element / 后一个非文本兄弟节点 */
        get nextElementSibling() {
            const childNodes = this.parentNode?.childNodes;
            return childNodes?.slice(childNodes.indexOf(this) + 1)
                .find((child) => child.type !== 'text');
        }
        /** previous sibling element / 前一个非文本兄弟节点 */
        get previousElementSibling() {
            const childNodes = this.parentNode?.childNodes;
            return childNodes?.slice(0, childNodes.indexOf(this))
                .findLast((child) => child.type !== 'text');
        }
        /** next visibling sibling node / 后一个可见的兄弟节点 */
        get nextVisibleSibling() {
            let { nextSibling } = this;
            while (nextSibling?.text() === '') {
                ({ nextSibling } = nextSibling);
            }
            return nextSibling;
        }
        /** previous visible sibling node / 前一个可见的兄弟节点 */
        get previousVisibleSibling() {
            let { previousSibling } = this;
            while (previousSibling?.text() === '') {
                ({ previousSibling } = previousSibling);
            }
            return previousSibling;
        }
        /** whether to be connected to a root token / 是否具有根节点 */
        get isConnected() {
            return this.getRootNode().type === 'root';
        }
        /** whether to be the end of a document / 后方是否还有其他节点（不含后代） */
        get eof() {
            const { parentNode } = this;
            if (!parentNode) {
                return true;
            }
            let { nextSibling } = this;
            while (nextSibling?.type === 'text' && !nextSibling.data.trim()) {
                ({ nextSibling } = nextSibling);
            }
            return nextSibling === undefined && parentNode.eof;
        }
        /** line number relative to its parent / 相对于父容器的行号 */
        get offsetTop() {
            return this.#getPosition().top;
        }
        /** column number of the last line relative to its parent / 相对于父容器的列号 */
        get offsetLeft() {
            return this.#getPosition().left;
        }
        /** position, dimension and paddings / 位置、大小和padding */
        get style() {
            return {
                ...this.#getPosition(),
                ...this.getDimension(),
                padding: this.getAttribute('padding'),
            };
        }
        /** @private */
        get fixed() {
            return false;
        }
        /**
         * font weigth and style
         *
         * 字体样式
         * @since v.1.8.0
         */
        get font() {
            const { bold, italic, b = 0, i = 0 } = this.#getFont();
            return { bold: bold && b >= 0, italic: italic && i >= 0 };
        }
        /**
         * whether to be bold
         *
         * 是否粗体
         * @since v.1.8.0
         */
        get bold() {
            return this.font.bold;
        }
        /**
         * whether to be italic
         *
         * 是否斜体
         * @since v.1.8.0
         */
        get italic() {
            return this.font.italic;
        }
        constructor() {
            if (!index_1.default.viewOnly && !index_1.default.internal) {
                Object.defineProperty(this, 'childNodes', { writable: false });
                Object.freeze(this.childNodes);
            }
        }
        /* NOT FOR BROWSER END */
        /** @private */
        getChildNodes() {
            const { childNodes } = this;
            return Object.isFrozen(childNodes) ? [...childNodes] : childNodes;
        }
        /** @private */
        getAttribute(key) {
            return (key === 'padding' ? 0 : /* c8 ignore next */ this[key]);
        }
        /** @private */
        setAttribute(key, value) {
            switch (key) {
                case 'parentNode':
                    this.#parentNode = value;
                    if (!value) {
                        this.#nextSibling = undefined;
                        this.#previousSibling = undefined;
                    }
                    break;
                case 'nextSibling':
                    this.#nextSibling = value;
                    break;
                case 'previousSibling':
                    this.#previousSibling = value;
                    break;
                case 'aIndex':
                    LINT: {
                        if (index_1.default.viewOnly) {
                            this.#aIndex = [debug_1.Shadow.rev, value];
                        }
                    }
                    break;
                default:
                    /* NOT FOR BROWSER */
                    if (Object.hasOwn(this, key)) {
                        const descriptor = Object.getOwnPropertyDescriptor(this, key), bool = Boolean(value), optional = this.#optional.has(key) && descriptor.enumerable !== bool;
                        if (optional) {
                            descriptor.enumerable = bool;
                        }
                        const oldValue = this[key];
                        if (typeof value === 'object' && typeof oldValue === 'object' && Object.isFrozen(oldValue)) {
                            Object.freeze(value);
                        }
                        if (optional || !descriptor.writable) {
                            Object.defineProperty(this, key, { ...descriptor, value });
                            return;
                        }
                    }
                    /* NOT FOR BROWSER END */
                    this[key] = value; // eslint-disable-line @typescript-eslint/no-explicit-any
            }
        }
        /**
         * Get the root node
         *
         * 获取根节点
         */
        getRootNode() {
            return (0, lint_1.cache)(this.#root, () => this.parentNode?.getRootNode() ?? this, value => {
                const [, root] = value;
                if (root.type === 'root') {
                    this.#root = value;
                }
            });
        }
        /**
         * Convert the position to the character index
         *
         * 将行列号转换为字符位置
         * @param top line number / 行号
         * @param left column number / 列号
         */
        indexFromPos(top, left) {
            LINT: {
                if (top < 0 || left < 0) {
                    return undefined;
                }
                const lines = this.getLines();
                if (top >= lines.length) {
                    return undefined;
                }
                const [, start, end] = lines[top], index = start + left;
                return index > end ? undefined : index;
            }
        }
        /**
         * Convert the character indenx to the position
         *
         * 将字符位置转换为行列号
         * @param index character index / 字符位置
         */
        posFromIndex(index) {
            LINT: {
                const { length } = String(this);
                index += index < 0 ? length : 0;
                if (index >= 0 && index <= length) {
                    const lines = this.getLines(), top = (0, search_1.default)(lines, index, ([, , end], needle) => end - needle);
                    return { top, left: index - lines[top][1] };
                }
                return undefined;
            }
        }
        /** @private */
        getDimension() {
            LINT: {
                const lines = this.getLines(), last = lines[lines.length - 1];
                return { height: lines.length, width: last[2] - last[1] };
            }
        }
        /** @private */
        getGaps(_) {
            return 0;
        }
        /**
         * Get the relative character index of the current node, or its `j`-th child node
         *
         * 获取当前节点的相对字符位置，或其第`j`个子节点的相对字符位置
         * @param j rank of the child node / 子节点序号
         */
        getRelativeIndex(j) {
            if (j === undefined) {
                const { parentNode } = this;
                return parentNode
                    ? parentNode.getRelativeIndex(parentNode.childNodes.indexOf(this))
                    : 0;
            }
            /* NOT FOR BROWSER */
            this.verifyChild(j, 1);
            /* NOT FOR BROWSER END */
            return (0, lint_1.cache)(this.#rIndex[j], () => {
                const { childNodes } = this, parentAIndex = this.#aIndex?.[0] === debug_1.Shadow.rev && this.#aIndex[1], n = j + (j < 0 ? childNodes.length : 0);
                let acc = this.getAttribute('padding');
                for (let i = 0; i < n; i++) {
                    if (index_1.default.viewOnly) {
                        this.#rIndex[i] = [debug_1.Shadow.rev, acc];
                        if (parentAIndex !== false) {
                            childNodes[i].#aIndex = [debug_1.Shadow.rev, parentAIndex + acc];
                        }
                    }
                    acc += childNodes[i].toString().length + this.getGaps(i);
                }
                return acc;
            }, value => {
                this.#rIndex[j] = value;
            });
        }
        /**
         * Get the absolute character index of the current node
         *
         * 获取当前节点的绝对位置
         */
        getAbsoluteIndex() {
            // 也用于Prism-Wiki
            return (0, lint_1.cache)(this.#aIndex, () => {
                const { parentNode } = this;
                return parentNode ? parentNode.getAbsoluteIndex() + this.getRelativeIndex() : 0;
            }, value => {
                this.#aIndex = value;
            });
        }
        /**
         * Get the position and dimension of the current node
         *
         * 获取当前节点的行列位置和大小
         */
        getBoundingClientRect() {
            LSP: return {
                ...this.getDimension(),
                ...this.getRootNode().posFromIndex(this.getAbsoluteIndex()),
            };
        }
        /**
         * Whether to be of a certain type
         *
         * 是否是某种类型的节点
         * @param type token type / 节点类型
         * @since v1.10.0
         */
        is(type) {
            return this.type === type;
        }
        /**
         * Get the text and the start/end positions of all lines
         *
         * 获取所有行的wikitext和起止位置
         * @since v1.16.3
         */
        getLines() {
            LINT: return (0, common_1.splitLines)(String(this));
        }
        /* PRINT ONLY */
        /** @private */
        seal(key, permanent) {
            LSP: {
                if (index_1.default.internal) {
                    return;
                    /* NOT FOR BROWSER */
                }
                else if (!permanent) {
                    this.#optional.add(key);
                    /* NOT FOR BROWSER END */
                }
                const enumerable = !permanent && Boolean(this[key]);
                if (!enumerable
                    || !index_1.default.viewOnly) {
                    Object.defineProperty(this, key, {
                        enumerable,
                        configurable: true,
                        /* NOT FOR BROWSER */
                        writable: index_1.default.viewOnly,
                    });
                }
            }
        }
        /* PRINT ONLY END */
        /* NOT FOR BROWSER */
        /* c8 ignore start */
        /** @private */
        typeError(method, ...types) {
            throw new TypeError(`${this.constructor.name}.${method} method only accepts ${types.join(', ')} as input parameters!`);
        }
        /* c8 ignore stop */
        /** @private */
        constructorError(msg) {
            throw new Error(`${this.constructor.name} ${msg}!`);
        }
        /**
         * Check if the node is identical
         *
         * 是否是全同节点
         * @param node node to be compared to / 待比较的节点
         * @throws `assert.AssertionError`
         */
        isEqualNode(node) {
            try {
                strict_1.default.deepEqual(this, node);
            }
            catch (e) {
                if (e instanceof strict_1.default.AssertionError) {
                    return false;
                }
                /* c8 ignore next 2 */
                throw e;
            }
            return true;
        }
        /** @private */
        insertAdjacent(nodes, offset) {
            const { parentNode } = this;
            /* c8 ignore next 3 */
            if (!parentNode) {
                throw new Error('There is no parent node!');
            }
            const i = parentNode.childNodes.indexOf(this) + offset;
            for (let j = 0; j < nodes.length; j++) {
                parentNode.insertAt(nodes[j], i + j);
            }
        }
        /**
         * Insert a batch of sibling nodes after the current node
         *
         * 在后方批量插入兄弟节点
         * @param nodes nodes to be inserted / 插入节点
         */
        after(...nodes) {
            this.insertAdjacent(nodes, 1);
        }
        /**
         * Insert a batch of sibling nodes before the current node
         *
         * 在前方批量插入兄弟节点
         * @param nodes nodes to be inserted / 插入节点
         */
        before(...nodes) {
            this.insertAdjacent(nodes, 0);
        }
        /**
         * Remove the current node
         *
         * 移除当前节点
         * @param ownLine whether to remove the current line if it is empty / 是否删除所在的空行
         */
        remove(ownLine) {
            const { parentNode, nextSibling, previousSibling } = this, i = parentNode?.childNodes.indexOf(this);
            parentNode?.removeAt(i);
            if (ownLine
                && parentNode?.getGaps(i - 1) === 0
                && nextSibling?.type === 'text' && previousSibling?.type === 'text'
                && nextSibling.data.startsWith('\n') && previousSibling.data.endsWith('\n')) {
                nextSibling.deleteData(0, 1);
            }
        }
        /**
         * Replace the current node with new nodes
         *
         * 将当前节点批量替换为新的节点
         * @param nodes nodes to be inserted / 插入节点
         */
        replaceWith(...nodes) {
            this.insertAdjacent(nodes, 1);
            this.remove();
        }
        /**
         * Check if the node is a descendant
         *
         * 是自身或后代节点
         * @param node node to be compared to / 待检测节点
         */
        contains(node) {
            let parentNode = node;
            while (parentNode && parentNode !== this) {
                ({ parentNode } = parentNode);
            }
            return Boolean(parentNode);
        }
        /** @private */
        verifyChild(i, addition = 0) {
            const { length } = this.childNodes;
            /* c8 ignore next 3 */
            if (i < -length || i >= length + addition) {
                throw new RangeError(`The child node at position ${i} does not exist!`);
            }
        }
        /**
         * Add an event listener
         *
         * 添加事件监听
         * @param types event type / 事件类型
         * @param listener listener function / 监听函数
         * @param options options / 选项
         * @param options.once to be executed only once / 仅执行一次
         */
        addEventListener(types, listener, options) {
            if (Array.isArray(types)) {
                for (const type of types) {
                    this.addEventListener(type, listener, options);
                }
            }
            else {
                this.#events[options?.once ? 'once' : 'on'](types, listener);
            }
        }
        /**
         * Remove an event listener
         *
         * 移除事件监听
         * @param types event type / 事件类型
         * @param listener listener function / 监听函数
         */
        removeEventListener(types, listener) {
            if (Array.isArray(types)) {
                for (const type of types) {
                    this.removeEventListener(type, listener);
                }
            }
            else {
                this.#events.off(types, listener);
            }
        }
        /**
         * Remove all event listeners
         *
         * 移除事件的所有监听
         * @param types event type / 事件类型
         */
        removeAllEventListeners(types) {
            if (Array.isArray(types)) {
                for (const type of types) {
                    this.removeAllEventListeners(type);
                }
            }
            else if (typeof types === 'string') {
                this.#events.removeAllListeners(types);
            }
        }
        /**
         * List all event listeners
         *
         * 列举事件监听
         * @param type event type / 事件类型
         */
        listEventListeners(type) {
            return this.#events.listeners(type);
        }
        /**
         * Dispatch an event
         *
         * 触发事件
         * @param e event object / 事件对象
         * @param data event data / 事件数据
         */
        dispatchEvent(e, data) {
            if (!e.target) { // 初始化
                Object.defineProperty(e, 'target', { value: this, enumerable: true });
            }
            Object.defineProperties(e, {
                prevTarget: { value: e.currentTarget, enumerable: true, configurable: true },
                currentTarget: { value: this, enumerable: true, configurable: true },
            });
            this.#events.emit(e.type, e, data);
            if (e.bubbles && !e.cancelBubble && this.parentNode) {
                this.parentNode.dispatchEvent(e, data);
            }
        }
        /**
         * Get all the ancestor nodes
         *
         * 获取所有祖先节点
         */
        getAncestors() {
            const ancestors = [];
            let { parentNode } = this;
            while (parentNode) {
                ancestors.push(parentNode);
                ({ parentNode } = parentNode);
            }
            return ancestors;
        }
        /**
         * Compare the relative position with another node
         *
         * 比较和另一个节点的相对位置
         * @param other node to be compared with / 待比较的节点
         * @throws `RangeError` 不在同一个语法树
         */
        compareDocumentPosition(other) {
            if (this === other) {
                return 0;
            }
            else if (this.contains(other)) {
                return -1;
            }
            else if (other.contains(this)) {
                return 1;
            }
            /* c8 ignore next 3 */
            if (this.getRootNode() !== other.getRootNode()) {
                throw new RangeError('Nodes to be compared are not in the same document!');
            }
            const aAncestors = [...this.getAncestors().reverse(), this], bAncestors = [...other.getAncestors().reverse(), other], depth = aAncestors.findIndex((ancestor, i) => bAncestors[i] !== ancestor), { childNodes } = aAncestors[depth - 1];
            return childNodes.indexOf(aAncestors[depth]) - childNodes.indexOf(bAncestors[depth]);
        }
        /** 获取当前节点的相对位置 */
        #getPosition() {
            return this.parentNode?.posFromIndex(this.getRelativeIndex()) ?? { top: 0, left: 0 };
        }
        /**
         * Destroy the instance
         *
         * 销毁
         */
        destroy() {
            this.parentNode?.destroy();
            for (const child of this.childNodes) {
                child.setAttribute('parentNode', undefined);
            }
            Object.setPrototypeOf(this, null);
        }
        /**
         * Get the wikitext of a line
         *
         * 获取某一行的wikitext
         * @param n line number / 行号
         */
        getLine(n) {
            return this.getLines()[n]?.[0];
        }
        /** 字体样式 */
        #getFont() {
            const { parentNode } = this, acceptable = parentNode?.getAcceptable();
            if (!parentNode || acceptable && !('QuoteToken' in acceptable)) {
                return { bold: false, italic: false };
            }
            const { childNodes, type } = parentNode;
            let bold, italic, b = 0, i = 0;
            /**
             * 更新字体样式
             * @param node 父节点或兄弟节点
             */
            const update = (node) => {
                const font = node.#getFont();
                if (bold === undefined) {
                    ({ bold } = font);
                    b += font.b ?? 0;
                }
                if (italic === undefined) {
                    ({ italic } = font);
                    i += font.i ?? 0;
                }
            };
            for (let j = childNodes.indexOf(this) - 1; j >= 0; j--) {
                const child = childNodes[j];
                if (child.is('quote')) {
                    const { closing } = child;
                    bold ??= closing.bold === undefined ? undefined : !closing.bold;
                    italic ??= closing.italic === undefined ? undefined : !closing.italic;
                    if (bold !== undefined && italic !== undefined) {
                        break;
                    }
                }
                else if (child.is('html')) {
                    const { name, closing } = child;
                    if (bold === undefined && name === 'b' && b <= 0) {
                        b += closing ? -1 : 1;
                    }
                    else if (italic === undefined && name === 'i' && i <= 0) {
                        i += closing ? -1 : 1;
                    }
                }
                else if (child.is('ext-link') && child.length === 2 && child.lastChild.length > 0) {
                    update(child.lastChild.lastChild);
                    break;
                }
                else if (child.type === 'text' && child.data.includes('\n')) {
                    bold = Boolean(bold);
                    italic = Boolean(italic);
                    break;
                }
            }
            if ((bold === undefined || italic === undefined) && type === 'ext-link-text' && parentNode.parentNode) {
                update(parentNode.parentNode);
            }
            return { bold: Boolean(bold), italic: Boolean(italic), b, i };
        }
    };
    return AstNode = _classThis;
})();
exports.AstNode = AstNode;
constants_1.classes['AstNode'] = __filename;
