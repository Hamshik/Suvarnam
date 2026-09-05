%filenames Scanner
%baseclass-preinclude "frontend/lexer/lexer.h"

%x COMMENT3

%%

"\357\273\277"               { /* skip BOM */ }
[ \t\r\n]+                   { /* skip whitespace */ }

"'''"                        {
    SA_lexer_get_cursor(&lex_loc);
    SA_comment_start_line = lex_loc.first_line;
    SA_comment_start_col  = lex_loc.first_column;
    SA_comment_start_pos  = lex_loc.first_pos;
    begin(StartCondition_::COMMENT3);
}
<COMMENT3>"'''"               { begin(StartCondition_::INITIAL); }
<COMMENT3>\n                  { /* skip */ }
<COMMENT3>.                   { /* skip */ }
<COMMENT3><<EOF>>             {
    panic(
        (SA_Location){
            (size_t)SA_comment_start_line,
            (size_t)SA_comment_start_col,
            (size_t)SA_comment_start_pos,
            0,0,0
        },
        LEX_UNCLOSED_COMMENT,
        NULL);
    SA_lexer_mark_error();
    return 0;
}

"''".*                       { /* skip single-line comments */ }

\"([^"\\\n]|\\.)*\"          {
    const char *inner = matched().c_str() + 1;
    size_t inner_len = length() - 2;
    size_t out_len = 0;
    int err_i = -1;
    const char *err_msg = NULL;

    char *s = SA_unescape_string(inner, inner_len, &out_len, &err_i, &err_msg);
    if (!s) {
        int off = (err_i >= 0) ? err_i : 0;
        panic(
            (SA_Location){
                lex_loc.first_line,
                lex_loc.first_column + 1 + off,
                lex_loc.first_pos + 1 + off,
                0,0,0
            },
            LEX_INVALID_ESCAPE,
            err_msg);
        SA_lexer_mark_error();
        return Parser::LEX_ERROR;
    }

    VAL(node, new_str(s, lex_loc));
    free(s);
    return Parser::STRING_LITERAL;
}

\'([^\'\\\n]|\\.)+\'         {
    const char *inner = matched().c_str() + 1;
    size_t inner_len = length() - 2;
    size_t out_len = 0;
    int err_i = -1;
    const char *err_msg = NULL;

    char *s = SA_unescape_string(inner, inner_len, &out_len, &err_i, &err_msg);
    if (!s || out_len == 0) {
        int off = (err_i >= 0) ? err_i : 0;
        panic(
            (SA_Location){
                lex_loc.first_line,
                lex_loc.first_column + 1 + off,
                lex_loc.first_pos + 1 + off,
                0,0,0
            },
            LEX_INVALID_ESCAPE,
            err_msg ? err_msg : "invalid character literal");
        free(s);
        SA_lexer_mark_error();
        return Parser::LEX_ERROR;
    }

    VAL(node, new_char_bytes(s, out_len, lex_loc));
    free(s);
    return Parser::CHAR_LITERAL;
}

#[a-zA-Z_][a-zA-Z0-9_]*     {
    const SA_Keyword *keyword = SA_find_keyword(matched().c_str(), length());
    if (keyword && keyword->kind == SA_KEYWORD_TOKEN) return keyword->token;
    panic(lex_loc, LEX_UNKNOWN_CHAR, "unknown directive");
    SA_lexer_mark_error();
    return Parser::LEX_ERROR;
}

[0-9]+(\.[0-9]+)? {
    VAL(node, new_num(matched().c_str(), UNKNOWN, lex_loc));
    return Parser::NUMBER;
}

\.[0-9]+ {
    char* buffer = (char*)calloc(1, length() + 2);
    snprintf(buffer, length() + 2, "0%s", matched().c_str());
    VAL(node, new_num(buffer, UNKNOWN, lex_loc));
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
"{"                 { SA_brace_depth++; return Parser::LBRACE; }
"}"                 { if (SA_brace_depth > 0) SA_brace_depth--; return Parser::RBRACE; }
".."                { return Parser::DOT_DOT; }
"..."               { return Parser::ELLIPSIS; }

[@a-zA-Z_][a-zA-Z0-9_]*|[a-zA-Z_] {
    const SA_Keyword *keyword = SA_find_keyword(matched().c_str(), length());
    if (keyword) {
        if (keyword->kind == SA_KEYWORD_TOKEN) return keyword->token;
        if (keyword->kind == SA_KEYWORD_DATATYPE) {
            VAL(datatype, keyword->datatype);
            return keyword->token;
        }
        VAL(node, new_bool(keyword->bool_value, lex_loc));
        return keyword->token;
    }
    VAL(node, new_var(matched().c_str(), UNKNOWN, lex_loc));
    return Parser::IDENTIFIER;
}

.                   {
    if (matched()[0] == '\'' || matched()[0] == '"') {
        panic(lex_loc, PARSE_SYNTAX, "unclosed quotation mark");
        SA_lexer_mark_error();
        return Parser::LEX_ERROR;
    }
    unsigned char b0 = static_cast<unsigned char>(matched()[0]);
    char detail[32];
    snprintf(detail, sizeof(detail), "0x%02X", b0);
    panic(lex_loc, LEX_UNKNOWN_CHAR, detail);
    SA_lexer_mark_error();
    return Parser::LEX_ERROR;
}

<<EOF>>             {
    if (SA_brace_depth > 0) {
        SA_Location lex_loc;
        SA_lexer_get_cursor(&lex_loc);
        panic(lex_loc, PARSE_UNCLOSED_BRACE, NULL);
        SA_lexer_mark_error();
    }
    return 0;
}