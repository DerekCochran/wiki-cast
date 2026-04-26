"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
/**
 * Execute the data script.
 * @param obj MediaWiki module implementation
 */
const execute = (obj) => {
    Object.entries(obj.files).find(([k]) => k.endsWith('.data.js'))[1]();
};
Object.assign(globalThis, {
    mw: {
        loader: {
            done: false,
            /** @ignore */
            impl(callback) {
                execute(callback()[1]);
            },
            /** @ignore */
            implement(name, callback) {
                if (typeof callback === 'object') {
                    execute(callback);
                }
                else if (!this.done) {
                    callback();
                }
                if (name.startsWith('ext.CodeMirror.data')) {
                    this.done = true;
                }
            },
            /** @ignore */
            state() {
                //
            },
        },
        config: {
            /** @ignore */
            set({ extCodeMirrorConfig }) {
                console.log(JSON.stringify(extCodeMirrorConfig));
            },
        },
    },
});
