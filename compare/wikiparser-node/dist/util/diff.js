"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.info = exports.error = exports.diff = exports.cmd = void 0;
const promises_1 = __importDefault(require("fs/promises"));
const util_1 = __importDefault(require("util"));
const child_process_1 = require("child_process");
/* c8 ignore start */
process.on('unhandledRejection', e => {
    console.error(e);
});
/* c8 ignore stop */
/**
 * 将shell命令转化为Promise对象
 * @param command shell指令
 * @param args shell输入参数
 */
const cmd = (command, args) => new Promise(resolve => {
    NPM: {
        let timer, shell;
        /**
         * 清除进程并返回
         * @param val 返回值
         */
        const r = (val) => {
            clearTimeout(timer);
            shell?.kill('SIGINT');
            resolve(val);
        };
        try {
            shell = (0, child_process_1.spawn)(command, args);
            timer = setTimeout(() => {
                /* c8 ignore next */
                shell.kill('SIGINT');
            }, 60 * 1e3);
            let buf = '';
            shell.stdout.on('data', data => {
                buf += String(data);
            });
            shell.stdout.on('end', () => {
                r(buf);
            });
            shell.on('exit', () => {
                r(shell.killed ? undefined : '');
            });
            shell.on('error', () => {
                r(undefined);
            });
        }
        catch /* c8 ignore start */ {
            r(undefined);
        }
        /* c8 ignore stop */
    }
});
exports.cmd = cmd;
/* c8 ignore start */
/**
 * 比较两个文件
 * @param oldStr 旧文本
 * @param newStr 新文本
 * @param uid 唯一标识
 */
const diff = async (oldStr, newStr, uid) => {
    NPM: {
        if (oldStr === newStr) {
            return;
        }
        const oldFile = `diffOld${uid}`, newFile = `diffNew${uid}`;
        await Promise.all([promises_1.default.writeFile(oldFile, oldStr), promises_1.default.writeFile(newFile, newStr)]);
        const stdout = await (0, exports.cmd)('git', [
            'diff',
            '--color-words=[\xC0-\xFF][\x80-\xBF]+|<?/?\\w+/?>?|[^[:space:]]',
            '-U0',
            '--no-index',
            oldFile,
            newFile,
        ]);
        console.log(stdout?.split('\n').slice(4).join('\n'));
        await Promise.allSettled([promises_1.default.unlink(oldFile), promises_1.default.unlink(newFile)]);
    }
};
exports.diff = diff;
/* c8 ignore stop */
/* c8 ignore start */
/** @implements */
const error = (msg, ...args) => {
    console.error(util_1.default.styleText('red', msg), ...args);
};
exports.error = error;
/* c8 ignore stop */
/* c8 ignore start */
/** @implements */
const info = (msg, ...args) => {
    NPM: console.info(util_1.default.styleText('green', msg), ...args);
};
exports.info = info;
/* c8 ignore stop */
