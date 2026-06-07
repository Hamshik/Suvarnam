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

    static int SV_brace_depth = 0;
    static int SV_comment_start_line = 1;
    static int SV_comment_start_col = 1;
    static int SV_comment_start_pos = 0;

%}

%x COMMENT3

%%
"\357\273\277"               { /* skip */ }
[ \t\r\n]+                   { /* skip */ }

"'''"                        {
    SV_comment_start_line = yylloc->first_line;
    SV_comment_start_col = yylloc->first_column;
    SV_comment_start_pos = yylloc->first_pos;
    BEGIN(COMMENT3);
}
<COMMENT3>"'''"               { BEGIN(INITIAL); }
<COMMENT3>\n                  { /* skip */ }
<COMMENT3>.                   { /* skip */ }
<COMMENT3><<EOF>>             {
    panic(
          (SV_Location){
            SV_comment_start_line,
            SV_comment_start_col,
            SV_comment_start_pos,
            0,0,0
          },
          LEX_UNCLOSED_COMMENT,
          NULL);
    return 0;
}

"''".*                       { /* skip single-line comments */ }

\"([^"\\\n]|\\.)*\"          {
    const char *inner = yytext + 1;
    size_t inner_len = (size_t)yyleng - 2;
    size_t out_len = 0;
    int err_i = -1;
    const char *err_msg = NULL;

    char *s = SV_unescape_string(inner, inner_len, &out_len, &err_i, &err_msg);
    if (!s) {
        int off = (err_i >= 0) ? err_i : 0;
        panic(
            (SV_Location){
              yylloc->first_line,
              yylloc->first_column + 1 + off,
              yylloc->first_pos + 1 + off,
              0,0,0
            },
              LEX_INVALID_ESCAPE,
              err_msg);
        return 0;
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

    char *s = SV_unescape_string(inner, inner_len, &out_len, &err_i, &err_msg);
    if (!s || out_len == 0) {
        int off = (err_i >= 0) ? err_i : 0;
        panic(
            (SV_Location){
              yylloc->first_line,
              yylloc->first_column + 1 + off,
              yylloc->first_pos + 1 + off,
              0,0,0
            },
              LEX_INVALID_ESCAPE,
              err_msg ? err_msg : "invalid character literal");
        free(s);
        return 0;
    }

    /* enforce exactly one UTF-8 codepoint */
    if (!SV_utf8_single(s, out_len)) {
        panic(
            (SV_Location){
              yylloc->first_line,
              yylloc->first_column + 1,
              yylloc->first_pos + 1,
              0,0,0
            },
              INVAILD_UTF8_CHAR, "character literal must be exactly one UTF-8 codepoint");
        free(s);
        return 0;
    }

    yylval->node = new_char_bytes(s, out_len, *yylloc);
    free(s);
    return CHAR_LITERAL;
}

#import                      { return IMPORT; }
else                         { return ELSE; }
if                           { return IF; }
for                          { return FOR; }
var                          { return VAR; }
i8                           { yylval->datatype = I8; return DATATYPES; }
i16                          { yylval->datatype = I16; return DATATYPES; }
i32                          { yylval->datatype = I32; return DATATYPES; }
i64                          { yylval->datatype = I64; return DATATYPES; }
i128                         { yylval->datatype = I128; return DATATYPES; }

u8                           { yylval->datatype = U8; return DATATYPES; }
u16                          { yylval->datatype = U16; return DATATYPES; }
u32                          { yylval->datatype = U32; return DATATYPES; }
u64                          { yylval->datatype = U64; return DATATYPES; }
u128                         { yylval->datatype = U128; return DATATYPES; }

f32                          { yylval->datatype = F32; return DATATYPES; }
f64                          { yylval->datatype = F64; return DATATYPES; }
f128                         { yylval->datatype = F128; return DATATYPES; }

uf32                         { yylval->datatype = UF32; return DATATYPES; }
uf64                         { yylval->datatype = UF64; return DATATYPES; }
uf128                        { yylval->datatype = UF128; return DATATYPES; }
str                          { yylval->datatype = STRINGS; return DATATYPES; }
char                         { yylval->datatype = CHARACTER; return DATATYPES;}
bool                         { yylval->datatype = BOOL; return DATATYPES;}
void                         { yylval->datatype = VOID; return DATATYPES;}
while                        { return WHILE; }
in                           { return IN; }
break                        { return BREAK; }
continue                     { return CONTINUE; }

true                         {
    yylval->node = new_bool(1, *yylloc);
    return BOOL_LITERAL;
}

false               {
    yylval->node = new_bool(0, *yylloc);
    
    return BOOL_LITERAL;
}

mut                 { return MUT; }
fn                  { return FN; }
(ret|return)        { return RETURN; }

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
"{"                 { SV_brace_depth++; return LBRACE; }
"}"                 { if (  SV_brace_depth > 0) SV_brace_depth--; return RBRACE; }
".."                { return DOT_DOT; }
[@a-zA-Z_][a-zA-Z0-9_]*|[a-zA-Z_] {
    yylval->node = new_var(yytext, UNKNOWN, *yylloc);
    return IDENTIFIER;
}

.                  {
    unsigned char b0 = (unsigned char)yytext[0];
    char detail[32];
    snprintf(detail, sizeof(detail), "0x%02X", b0);
    SV_Location loc;
    if (yylloc) {
        loc = *yylloc;
    } else {
        SV_lexer_get_cursor(&loc);
    }
    panic( loc, LEX_UNKNOWN_CHAR, detail);
}

<<EOF>>             {
    if (SV_brace_depth > 0) {
        SV_Location loc;
        SV_lexer_get_cursor(&loc);
        panic( loc, PARSE_UNCLOSED_BRACE, NULL);
    }
    return 0;
}
%%
