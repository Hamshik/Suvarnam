%filenames Scanner
%baseclass-preinclude "frontend/parser/Parser.h"

%x COMMENT3

%%

"\357\273\277"               { /* skip BOM */ }
[ \t\r\n]+                   { /* skip whitespace */ }

"'''"                        {
    setOpenDelimPos(*loc);
    begin(StartCondition_::COMMENT3);
}
<COMMENT3>"'''"               { begin(StartCondition_::INITIAL); }
<COMMENT3>\n                  { /* skip */ }
<COMMENT3>.                   { /* skip */ }
<COMMENT3><<EOF>>             {
    panic(
        getCursor(),
        LEX_UNCLOSED_COMMENT,
        NULL);
    lexMarkErr();
    return 0;
}

"''".*                       { /* skip single-line comments */ }

\"([^"\\\n]|\\.)*\"          {
    const char *inner = matched().c_str() + 1;
    size_t inner_len = length() - 2;
    size_t out_len = 0;
    int err_i = -1;
    const char *err_msg = NULL;

    char *s = Scanner::unescapeStr(inner, inner_len, &out_len, &err_i, &err_msg);
    if (!s) {
        int off = (err_i >= 0) ? err_i : 0;
        panic(
            (SA::Location){
                loc->firstLn,
                loc->firstCol + 1 + off,
                loc->firstPos + 1 + off,
                0,0,0
            },
            LEX_INVALID_ESCAPE,
            err_msg);
        lexMarkErr();
        return Parser::LEX_ERROR;
    }

    VAL(node, new_str(s, tokenLoc()));
    free(s);
    return Parser::STRING_LITERAL;
}

\'([^\'\\\n]|\\.)+\'         {
    const char *inner = matched().c_str() + 1;
    size_t inner_len = length() - 2;
    size_t out_len = 0;
    int err_i = -1;
    const char *err_msg = NULL;

    char *s = Scanner::unescapeStr(inner, inner_len, &out_len, &err_i, &err_msg);
    if (!s || out_len == 0) {
        int off = (err_i >= 0) ? err_i : 0;
        panic(
            (SA::Location){
                loc->firstLn,
                loc->firstCol + 1 + off,
                loc->firstPos + 1 + off,
                0,0,0
            },
            LEX_INVALID_ESCAPE,
            err_msg ? err_msg : "invalid character literal");
        free(s);
        lexMarkErr();
        return Parser::LEX_ERROR;
    }

    VAL(node, new_char_bytes(s, out_len, tokenLoc()));
    free(s);
    return Parser::CHAR_LITERAL;
}

#[a-zA-Z_][a-zA-Z0-9_]*     {
    const SA_Keyword *keyword = Scanner::findKeyword(matched().c_str(), length());
    if (keyword && keyword->kind == SA_KEYWORD_TOKEN) return keyword->token;
    panic(*loc, LEX_UNKNOWN_CHAR, "unknown directive");
    lexMarkErr();
    return Parser::LEX_ERROR;
}

[0-9]+(\.[0-9]+)? {
    VAL(node, new_num(matched().c_str(), UNKNOWN, tokenLoc()));
    return Parser::NUMBER;
}

\.[0-9]+ {
    char* buffer = (char*)calloc(1, length() + 2);
    snprintf(buffer, length() + 2, "0%s", matched().c_str());
    VAL(node, new_num(buffer, UNKNOWN, tokenLoc()));
    free(buffer);
    return Parser::NUMBER;
}

","                 { return Parser::COMMA; }
"++"                { return Parser::INC; }
"--"                { return Parser::DEC; }
":"                 { return Parser::COLON; }
"+"                 { return Parser::PLUS; }
"-"                 { return Parser::MINUS; }
"*"                 { return Parser::STAR; }
"/"                 { return Parser::SLASH; }
"%"                 { return Parser::MOD; }
"^^"                { return Parser::POWER; }
"["                 { return Parser::LSQUARE; }
"]"                 { return Parser::RSQUARE; }
"("                 { return Parser::LPAREN; }
")"                 { return Parser::RPAREN; }
"="                 { return Parser::ASSIGN; }
"+="                { return Parser::PLUS_ASSIGN; }
"-="                { return Parser::MINUS_ASSIGN; }
"*="                { return Parser::STAR_ASSIGN; }
"/="                { return Parser::SLASH_ASSIGN; }
"%="                { return Parser::MOD_ASSIGN; }
"^^="               { return Parser::POWER_ASSIGN; }
";"                 { return Parser::SEMICOLON; }
">>"                { return Parser::RSHIFT; }
"<<"                { return Parser::LSHIFT; }
"<<="               { return Parser::LSHIFT_ASSIGN; }
">>="               { return Parser::RSHIFT_ASSIGN; }
"&&"                { return Parser::AND; }
"||"                { return Parser::OR; }
"!"                 { return Parser::NOT; }
"&"                 { return Parser::AMP; }
"|"                 { return Parser::PIPE; }
"^"                 { return Parser::BITXOR; }
"~"                 { return Parser::BITNOT; }
"=="                { return Parser::EQ; }
"!="                { return Parser::NEQ; }
"<="                { return Parser::LE; }
">="                { return Parser::GE; }
"<"                 { return Parser::LT; }
">"                 { return Parser::GT; }
"{"                 { return Parser::LBRACE; }
"}"                 { return Parser::RBRACE; }
".."                { return Parser::DOT_DOT; }
"..."               { return Parser::ELLIPSIS; }

[@a-zA-Z_][a-zA-Z0-9_]*|[a-zA-Z_] {
    const SA_Keyword *keyword = Scanner::findKeyword(matched().c_str(), length());
    if (keyword) {
        if (keyword->kind == SA_KEYWORD_TOKEN) return keyword->token;
        if (keyword->kind == SA_KEYWORD_DATATYPE) {
            VAL(datatype, keyword->datatype);
            return keyword->token;
        }
        VAL(node, new_bool(keyword->bool_value, tokenLoc()));
        return keyword->token;
    }
    VAL(node, new_var(matched().c_str(), UNKNOWN, tokenLoc()));
    return Parser::IDENTIFIER;
}

.                   {
    if (matched()[0] == '\'' || matched()[0] == '"') {
        panic(*loc, PARSE_SYNTAX, "unclosed quotation mark");
        lexMarkErr();
        return Parser::LEX_ERROR;
    }
    unsigned char b0 = static_cast<unsigned char>(matched()[0]);
    char detail[32];
    snprintf(detail, sizeof(detail), "0x%02X", b0);
    panic(*loc, LEX_UNKNOWN_CHAR, detail);
    lexMarkErr();
    return Parser::LEX_ERROR;
}

<<EOF>>             {
    if (braceDepth > 0) {
        panic(*loc, PARSE_UNCLOSED_BRACE, NULL);
        lexMarkErr();
    }
    return 0;
}
