"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.getCanonicalName = exports.undo = exports.mixin = exports.emptyArray = exports.getMagicWordInfo = exports.setChildNodes = exports.isLink = exports.isRowEnd = exports.isToken = exports.Shadow = void 0;
exports.Shadow = {
    running: false,
    /** @private */
    run(callback, Parser) {
        const { running } = this;
        this.running = true;
        /* PRINT ONLY */
        const internal = Parser?.internal;
        if (Parser) {
            Parser.internal = true;
        }
        /* PRINT ONLY END */
        /** restore state before exit */
        const finish = () => {
            this.running = running;
            /* PRINT ONLY */
            if (Parser) {
                Parser.internal = internal;
            }
        };
        try {
            const { Token } = require('../src/index');
            const result = callback();
            if (result instanceof Token && !result.getAttribute('built')) {
                result.afterBuild();
            }
            finish();
            return result;
        }
        catch (e) /* c8 ignore start */ {
            finish();
            throw e;
        }
        /* c8 ignore stop */
    },
    /** @private */
    internal(callback, Parser) {
        /* PRINT ONLY */
        const { internal } = Parser;
        Parser.internal = true;
        /* PRINT ONLY END */
        const result = callback();
        /* PRINT ONLY */
        Parser.internal = internal;
        /* PRINT ONLY END */
        return result;
    },
    rev: 0,
};
/**
 * 是否是某一特定类型的节点
 * @param type 节点类型
 */
const isToken = (type) => (node) => node.type === type;
exports.isToken = isToken;
/**
 * 是否是行尾
 * @param token 节点
 * @param token.type 节点类型
 */
const isRowEnd = ({ type }) => type === 'tr' || type === 'table-syntax';
exports.isRowEnd = isRowEnd;
/**
 * 是否为普通内链
 * @param type 节点类型
 */
const isLink = (type) => type === 'redirect-target' || type === 'link';
exports.isLink = isLink;
/**
 * 更新chldNodes
 * @param parent 父节点
 * @param position 子节点位置
 * @param deleteCount 移除的子节点数量
 * @param inserted 插入的子节点
 */
const setChildNodes = (parent, position, deleteCount, inserted = []) => {
    let nodes = parent.getChildNodes(), removed;
    if (nodes.length === deleteCount) {
        removed = nodes;
        nodes = inserted;
    }
    else {
        removed = Array.prototype.splice.apply(nodes, [position, deleteCount, ...inserted]);
    }
    for (let i = 0; i < inserted.length; i++) {
        const node = inserted[i];
        node.setAttribute('parentNode', parent);
        node.setAttribute('nextSibling', nodes[position + i + 1]);
        node.setAttribute('previousSibling', nodes[position + i - 1]);
    }
    nodes[position - 1]?.setAttribute('nextSibling', nodes[position]);
    nodes[position + inserted.length]?.setAttribute('previousSibling', nodes[position + inserted.length - 1]);
    parent.setAttribute('childNodes', nodes);
    /* NOT FOR BROWSER */
    for (const node of removed) {
        if (node.parentNode === parent) {
            node.setAttribute('parentNode', undefined);
        }
    }
    /* NOT FOR BROWSER END */
    return removed;
};
exports.setChildNodes = setChildNodes;
/**
 * 获取魔术字的信息
 * @param name 魔术字
 * @param parserFunction 解析设置中的parserFunction属性
 */
const getMagicWordInfo = (name, parserFunction) => {
    const lcName = name.toLowerCase(), [insensitive, sensitive] = parserFunction, isSensitive = Object.prototype.hasOwnProperty.call(sensitive, name);
    return [
        lcName,
        isSensitive,
        isSensitive
            ? sensitive[name]
            : Object.prototype.hasOwnProperty.call(insensitive, lcName) && insensitive[lcName],
    ];
};
exports.getMagicWordInfo = getMagicWordInfo;
/**
 * 生成一个指定长度的空数组
 * @param length 数组长度
 * @param callback 回调函数
 */
const emptyArray = (length, callback) => Array.from({ length }, (_, i) => callback(i));
exports.emptyArray = emptyArray;
/* NOT FOR BROWSER ONLY */
/**
 * 同步混入的类名
 * @param target 混入的目标
 * @param source 混入的源
 */
const mixin = (target, source) => {
    Object.defineProperty(target, 'name', { value: source.name });
};
exports.mixin = mixin;
/* NOT FOR BROWSER ONLY END */
/* NOT FOR BROWSER */
/**
 * 撤销最近一次Mutation
 * @param e 事件
 * @param data 事件数据
 * @throws `RangeError` 无法撤销的事件类型
 */
const undo = (e, data) => {
    const { target, type } = e;
    switch (data.type) {
        case 'remove':
            (0, exports.setChildNodes)(target, data.position, 0, [data.removed]);
            break;
        case 'insert':
            (0, exports.setChildNodes)(target, data.position, 1);
            break;
        case 'replace':
            (0, exports.setChildNodes)(target.parentNode, data.position, 1, [data.oldToken]);
            break;
        case 'text':
            target.setAttribute('data', data.oldText);
            break;
        /* c8 ignore next 2 */
        default:
            throw new RangeError(`Unable to undo events with an unknown type: ${type}`);
    }
};
exports.undo = undo;
/**
 * 获取魔术字的规范名称
 * @param name 魔术字
 * @param parserFunction 解析设置中的parserFunction属性
 */
const getCanonicalName = (name, parserFunction) => {
    const [lcName, , canonicalName] = (0, exports.getMagicWordInfo)(name, parserFunction);
    return [canonicalName || lcName, canonicalName];
};
exports.getCanonicalName = getCanonicalName;
