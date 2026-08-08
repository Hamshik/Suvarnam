%option noyywrap
%option bison-bridge
%option bison-locations
%option noinput
%option nounput

%{
    #include <stddef.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>

    #include "lexer/lexer.h"
    #include "frontend/lexer/keywords.h"

    static int SA_brace_depth = 0;
    static int SA_comment_start_line = 1;
    static int SA_comment_start_col = 1;
    static int SA_comment_start_pos = 0;

%}

%x COMMENT3

%%
"\357\273\277"               { /* skip */ }
[ \t\r\n]+                   { /* skip */ }

"'''"                        {
    SA_comment_start_line = yylloc->first_line;
    SA_comment_start_col = yylloc->first_column;
    SA_comment_start_pos = yylloc->first_pos;
    BEGIN(COMMENT3);
}
<COMMENT3>"'''"               { BEGIN(INITIAL); }
<COMMENT3>\n                  { /* skip */ }
<COMMENT3>.                   { /* skip */ }
<COMMENT3><<EOF>>             {
    panic(
          (SA_Location){
            SA_comment_start_line,
            SA_comment_start_col,
            SA_comment_start_pos,
            0,0,0
          },
          LEX_UNCLOSED_COMMENT,
          NULL);
    SA_lexer_mark_error();
    return 0;
}

"''".*                       { /* skip single-line comments */ }

\"([^"\\\n]|\\.)*\"          {
    const char *inner = yytext + 1;
    size_t inner_len = (size_t)yyleng - 2;
    size_t out_len = 0;
    int err_i = -1;
    const char *err_msg = NULL;

    char *s = SA_unescape_string(inner, inner_len, &out_len, &err_i, &err_msg);
    if (!s) {
        int off = (err_i >= 0) ? err_i : 0;
        panic(
            (SA_Location){
              yylloc->first_line,
              yylloc->first_column + 1 + off,
              yylloc->first_pos + 1 + off,
              0,0,0
            },
              LEX_INVALID_ESCAPE,
              err_msg);
        SA_lexer_mark_error();
        return LEX_ERROR;
    }

    yylval->node = new_str(s, *yylloc);
    free(s);
    return STRING_LITERAL;
}

\'([^\'\\\n]|\\.)+\'            {
    const char *inner = yytext + 1;          /* skip leading quote */
    size_t inner_len = (size_t)yyleng - 2;   /* exclude quotes */
    size_t out_len = 0;
    int err_i = -1;
    const char *err_msg = NULL;

    char *s = SA_unescape_string(inner, inner_len, &out_len, &err_i, &err_msg);
    if (!s || out_len == 0) {
        int off = (err_i >= 0) ? err_i : 0;
        panic(
            (SA_Location){
              yylloc->first_line,
              yylloc->first_column + 1 + off,
              yylloc->first_pos + 1 + off,
              0,0,0
            },
              LEX_INVALID_ESCAPE,
              err_msg ? err_msg : "invalid character literal");
        free(s);
        SA_lexer_mark_error();
        return LEX_ERROR;
    }

    /* enforce exactly one UTF-8 codepoint */
    if (!SA_utf8_single(s, out_len)) {
        panic(
            (SA_Location){
              yylloc->first_line,
              yylloc->first_column + 1,
              yylloc->first_pos + 1,
              yylloc->first_line,
              yylloc->first_column + (size_t)yyleng - 2,
              yylloc->first_pos + (size_t)yyleng - 2
            },
              INVAILD_UTF8_CHAR, NULL);
        free(s);
        SA_lexer_mark_error();
        return LEX_ERROR;
    }

    yylval->node = new_char_bytes(s, out_len, *yylloc);
    free(s);
    return CHAR_LITERAL;
}

#[a-zA-Z_][a-zA-Z0-9_]* {
    const SA_Keyword *keyword = SA_find_keyword(yytext, (size_t)yyleng);
    if (keyword && keyword->kind == SA_KEYWORD_TOKEN) return keyword->token;
    panic(*yylloc, LEX_UNKNOWN_CHAR, "unknown directive");
    SA_lexer_mark_error();
    return LEX_ERROR;
}

[0-9]+(\.[0-9]+)? {
    yylval->node = new_num(yytext, UNKNOWN, *yylloc);
    
    return NUMBER;
}

\.[0-9]+ {
    char* buffer = (char*)calloc(1,strlen(yytext) + 2);
    snprintf(buffer, strlen(yytext) + 2, "0%s", yytext);
    yylval->node = new_num(buffer, UNKNOWN, *yylloc);
    free(buffer);
    return NUMBER;
}

","                 { return COMMA; }
"++"                { return INC; }
"--"                { return DEC; }
":"                 { return COLON;}
"+"                 { return PLUS; }
"-"                 { return MINUS; }
"*"                 { return STAR; }
"/"                 { return SLASH; }
"%"                 { return MOD; }
"^^"                { return POWER; }
"["                 { return LSQUARE;}
"]"                 { return RSQUARE;}
"("                 { return LPAREN; }
")"                 { return RPAREN; }
"="                 { return ASSIGN; }
"+="                { return PLUS_ASSIGN; }
"-="                { return MINUS_ASSIGN; }
"*="                { return STAR_ASSIGN; }
"/="                { return SLASH_ASSIGN; }
"%="                { return MOD_ASSIGN; }
"^^="               { return POWER_ASSIGN; }
";"                 { return SEMICOLON; }
">>"                { return RSHIFT; }
"<<"                { return LSHIFT; }
"<<="               { return LSHIFT_ASSIGN; }
">>="               { return RSHIFT_ASSIGN; }
"&&"                { return AND; }
"||"                { return OR; }
"!"                 { return NOT; }
"&"                 { return AMP; }
"|"                 { return PIPE; }
"^"                 { return BITXOR; }
"~"                 { return BITNOT; }
"=="                { return EQ; }
"!="                { return NEQ; }
"<="                { return LE; }
">="                { return GE; }
"<"                 { return LT; }
">"                 { return GT; }
"{"                 { SA_brace_depth++; return LBRACE; }
"}"                 { if (  SA_brace_depth > 0) SA_brace_depth--; return RBRACE; }
".."                { return DOT_DOT; }
"..."               { return ELLIPSIS; }
[@a-zA-Z_][a-zA-Z0-9_]*|[a-zA-Z_] {
    const SA_Keyword *keyword = SA_find_keyword(yytext, (size_t)yyleng);
    if (keyword) {
        if (keyword->kind == SA_KEYWORD_TOKEN) return keyword->token;
        if (keyword->kind == SA_KEYWORD_DATATYPE) {
            yylval->datatype = keyword->datatype;
            return keyword->token;
        }
        yylval->node = new_bool(keyword->bool_value, *yylloc);
        return keyword->token;
    }
    yylval->node = new_var(yytext, UNKNOWN, *yylloc);
    return IDENTIFIER;
}

.                  {
    if(*yytext == '\'' || *yytext == '"'){
        SA_Location loc;
        if (yylloc) {
            loc = *yylloc;
        } else {
            SA_lexer_get_cursor(&loc);
        }
        panic( loc, PARSE_SYNTAX, " unclosed quatation mark");
        SA_lexer_mark_error();
        return LEX_ERROR;
    }
    unsigned char b0 = (unsigned char)yytext[0];
    char detail[32];
    snprintf(detail, sizeof(detail), "0x%02X", b0);
    SA_Location loc;
    if (yylloc) {
        loc = *yylloc;
    } else {
        SA_lexer_get_cursor(&loc);
    }
    panic( loc, LEX_UNKNOWN_CHAR, detail);
    SA_lexer_mark_error();
    return LEX_ERROR;
}

<<EOF>>             {
    if (SA_brace_depth > 0) {
        SA_Location loc;
        SA_lexer_get_cursor(&loc);
        panic( loc, PARSE_UNCLOSED_BRACE, NULL);
        SA_lexer_mark_error();
    }
    return 0;
}
%%
