function buildJsAst(jsToken, parentToken) {
    const curToken = {};
    curToken.type = jsToken.type;
    if( jsToken.type === 'heading' ) {
        // curToken.level = jsToken.level;
        // curToken.id = jsToken.id;
    } else if( jsToken.type === 'text' ) {
        curToken.data = jsToken.data;
    } else if( jsToken.type === 'redirect' ) {
        curToken.pre = jsToken.pre;
        curToken.post = jsToken.post;
        curToken.link = jsToken.link;
        curToken.display = jsToken.display;
    } else if( jsToken.type === 'ext' ) {
        curToken.name = jsToken.name;
        curToken.attr = jsToken.attr;
        curToken.inner = jsToken.inner;
        curToken.closing = jsToken.closing;
        curToken.self_closing = jsToken.self_closing;
    } else if( jsToken.type === 'include' ) {
        curToken.tag = jsToken.tag;
        curToken.attr = jsToken.attr;
        curToken.inner = jsToken.inner;
        curToken.closing = jsToken.closing;
    } else if( jsToken.type === 'html' ) {
        curToken.self_closing = jsToken.self_closing;
        //curToken.closing = jsToken.closing;
        curToken.orig_tag = jsToken.orig_tag;
    } else if( jsToken.type === 'td' ) {
        curToken.inner_syntax = jsToken.inner_syntax;
    } else if( jsToken.type === 'dunder' ) {
        curToken.case_sensitive = jsToken.case_sensitive;
        curToken.fullwidth = jsToken.fullwidth;
    } else if( jsToken.type === 'quote' ) {
        // curToken.bold = jsToken.bold;
        // curToken.italic = jsToken.italic;
    } else if( jsToken.type === 'image-parameter' ) {
        // curToken.value = jsToken.value;
        // curToken.width = jsToken.width;
        // curToken.height = jsToken.height;
    } else if( jsToken.type === 'ext-link' ) {
        // curToken.link = jsToken.link;
        // curToken.protocol = jsToken.protocol;
        // curToken.innerText = jsToken.innerText;
    } else if( jsToken.type === 'imagemap-link' ) {
        // curToken.link = jsToken.link;
    } else if( jsToken.type === 'template' || jsToken.type === 'magic-word' ) {
        // curToken.modifier = jsToken.modifier;
        // curToken.name = jsToken.name;
//        curToken.module = jsToken.module;
//        curToken.function = jsToken.function;
        // curToken.duplication = jsToken.duplication;
        // curToken.anonCount = jsToken.anonCount; --- IGNORE ---
    } else if( jsToken.type === 'parameter' ) {
        curToken.name = jsToken.name;
        // curToken.value = jsToken.value;
        // curToken.anon = jsToken.anon; --- IGNORE ---
        // curToken.duplicated = jsToken.duplicated; --- IGNORE ---
    } else if( jsToken.type === 'arg' ) {
        curToken.name = jsToken.name;
        // curToken.default = jsToken.default;
    } else if( jsToken.type === 'converter' ) {
        curToken.variant = jsToken.variant;
        curToken.unidirectional = jsToken.unidirectional;
        curToken.bidirectional = jsToken.bidirectional;
    } else if( jsToken.type === 'converter-flags' ) {
        curToken.flags = jsToken.flags;
    } else if( jsToken.type === 'attributes' ) {
        curToken.name = jsToken.name;
        curToken.className = jsToken.className;
        curToken.id = jsToken.id;
        curToken.sanitized = jsToken.sanitized;
    } else if( jsToken.type === 'atom' ) {
        curToken.type = jsToken.type;
    } else if( jsToken.type === 'syntax' ) {
        curToken.type = jsToken.type;
    } else if( jsToken.type === 'comment' ) {
        //curToken.closed = jsToken.closed;
    }else if (jsToken.type === 'onlyinclude') {
        curToken.innerText = jsToken.innerText;        
    } else if (jsToken.type === 'param-line') {
        curToken.name = jsToken.name;
    } else if (jsToken.type === 'list-range') {
        curToken.start = jsToken.start;
        curToken.end = jsToken.end;
    } else if (jsToken.type === 'list' || jsToken.type === 'dd') {
        // curToken.indent = jsToken.indent;
        // curToken.dd = jsToken.dd;
        // curToken.dt = jsToken.dt;
        // curToken.ul = jsToken.ul;
        // curToken.ol = jsToken.ol;
    }
    if( jsToken.childNodes ) {
        for( let i = 0; i < jsToken.childNodes.length; i++ ) {
            buildJsAst(jsToken.childNodes[i], curToken) 
        }
    }
    
    if( parentToken ) {
        if( !parentToken.childNodes ) {
            parentToken.childNodes = [];
        }
        parentToken.childNodes.push(curToken);
    }

    return curToken;
}

exports.buildJsAst = buildJsAst;