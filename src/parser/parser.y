%define api.pure full
%define parse.error verbose

%define api.location.type { TQLocation }
%locations

%expect 0

%require "3.8.2"

%code requires {
    #include "parser/parser_helpers.h"
    #include "parser.h"
    #include "shared/structs.h"
    #include "ast/ast.h"

    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
}

%union{
    ASTNode_t *node;
    DataTypes_t datatype;
    struct {
        Param_t *params;
        int count;
    } paramlist;
    Type_t *type;  /* New: for recursive types */
    size_t size;
}

%code {
    int yylex(YYSTYPE *yylval, YYLTYPE *yylloc);
    void yyerror(YYLTYPE *loc, const char *s);
    int yyparse(void);
}

%token PLUS MINUS STAR SLASH MOD POWER
%token INC DEC
%token LSHIFT RSHIFT
%token AMP PIPE BITXOR BITNOT
%token LPAREN RPAREN LBRACE RBRACE SEMICOLON LSQUARE RSQUARE
%token ASSIGN PLUS_ASSIGN MINUS_ASSIGN STAR_ASSIGN SLASH_ASSIGN MOD_ASSIGN POWER_ASSIGN
%token LSHIFT_ASSIGN RSHIFT_ASSIGN COLON COMMA
%token AND OR NOT EQ NEQ LT LE GT GE
%token IF ELSE FOR WHILE MUT VAR FN RETURN IMPORT LISTS

%token <datatype> DATATYPES
%token <node> IDENTIFIER NUMBER STRING_LITERAL BOOL_LITERAL CHAR_LITERAL

%type <node>  top_level_stmts block if_stmt for_stmt while_stmt import_stmt expr_stmts
%type <node> fn_def param return_stmt opt_args args list_stmt expr_stmt top_level_stmt
%type <node> lvalue import_list expr assignment program
%type <paramlist> opt_params params
%type <type> recursive_type
%type <size> opt_list_size

%right ASSIGN PLUS_ASSIGN MINUS_ASSIGN STAR_ASSIGN SLASH_ASSIGN MOD_ASSIGN POWER_ASSIGN LSHIFT_ASSIGN RSHIFT_ASSIGN
%left OR
%left AND
%left PIPE
%left BITXOR
%left AMP
%left EQ NEQ
%left LT LE GT GE
%left LSHIFT RSHIFT
%left PLUS MINUS
%left STAR SLASH MOD
%right POWER
%right UPLUS UMINUS UADDR UDEREF NOT BITNOT
%left INC DEC
%left LPAREN RPAREN
%precedence POSTFIX
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
%nonassoc FOR_PREC 

%start program
%%

program:
    import_list top_level_stmts
    {
        if (!$1) root = $2;
        else if (!$2) root = $1;
        else root = new_seq($1, $2);
    }
;

top_level_stmt:
    fn_def                      { $$ = $1; }
    | expr_stmt                 { $$ = $1; }
;

top_level_stmts: /* empty */    { $$ = NULL; }
    | top_level_stmt top_level_stmts
    {
        if (!$1) $$ = $2;
        else if (!$2) $$ = $1;
        else $$ = new_seq($1, $2);
    }
;

expr_stmt:
    assignment SEMICOLON        { $$ = $1; }
    | assignment error          { TQerror_LOC(@2, PARSE_MISSING_SEMI, g_last_parse_err_msg); yyerrok; $$ = $1; }
    | expr SEMICOLON            { $$ = $1; }
    | expr error                { TQerror_LOC(@2, PARSE_MISSING_SEMI, g_last_parse_err_msg); yyerrok; $$ = $1; }
    | block                     { $$ = $1; }
    | return_stmt SEMICOLON     { $$ = $1; }
    | return_stmt error         { TQerror_LOC(@2, PARSE_MISSING_SEMI, g_last_parse_err_msg); yyerrok; $$ = $1; }
    | error SEMICOLON           { panic(&file, @1, PARSE_SYNTAX, g_last_parse_err_msg); yyerrok; $$ = NULL; }
    | if_stmt                   { $$ = $1; }
    | for_stmt                  { $$ = $1; }
    | while_stmt                { $$ = $1; }
    | error {
        panic(&file, @$, PARSE_SYNTAX, g_last_parse_err_msg);
        yyerrok;
        $$ = NULL; 
    }
;

import_list:
      /* empty */                  { $$ = NULL; }
    | import_stmt SEMICOLON import_list
      {
          if (!$3) $$ = $1;
          else $$ = new_seq($1, $3);
      }    
;

expr_stmts: /* empty */  { $$ = NULL; }
    | expr_stmt expr_stmts
    {
        if (!$1) $$ = $2;
        else if (!$2) $$ = $1;
        else $$ = new_seq($1, $2);
    }
    ;

import_stmt:
    IMPORT STRING_LITERAL
      {
          $$ = new_import_node($2->literal.raw, @$);
      }
;

block: 
    LBRACE expr_stmts RBRACE   { $$ = $2; }
;

if_stmt:
    IF LPAREN expr RPAREN expr_stmt %prec LOWER_THAN_ELSE
        { $$ = new_if($3, $5, NULL, @$); }
    | IF LPAREN expr RPAREN expr_stmt ELSE expr_stmt
        { $$ = new_if($3, $5, $7, @$); }
    | IF LPAREN expr error expr_stmt %prec LOWER_THAN_ELSE
        { 
            TQerror_LOC(@4, PARSE_UNCLOSED_PAREN, NULL); 
            yyerrok; $$ = new_if($3, $5, NULL, @$); 
        }
    | IF LPAREN expr error expr_stmt ELSE expr_stmt
        { 
            TQerror_LOC(@4, PARSE_UNCLOSED_PAREN, NULL); 
            yyerrok; $$ = new_if($3, $5, $7, @$); 
        }
    ;


for_stmt: 
    FOR LPAREN assignment COLON expr RPAREN expr_stmt
        { $$ = new_for($3, $5, NULL, $7, @$); }
    | FOR LPAREN assignment COLON expr COLON expr RPAREN expr_stmt
        { $$ = new_for($3, $5, $7, $9, @$); }
    | FOR LPAREN assignment COLON expr error expr_stmt
        { 
            TQerror_LOC(@6, PARSE_UNCLOSED_PAREN, NULL); 
            yyerrok; $$ = new_for($3, $5, NULL, $7, @$); 
        }
    ;

while_stmt:
    WHILE LPAREN expr RPAREN expr_stmt
        { $$ = new_while($3, $5, @$); }
    | WHILE LPAREN expr error expr_stmt
        { 
            TQerror_LOC(@4, PARSE_UNCLOSED_PAREN, NULL); 
            yyerrok; $$ = new_while($3, $5, @$); 
        }
    ;

fn_def: 
    FN recursive_type IDENTIFIER LPAREN opt_params RPAREN  block
    {
        $$ = new_fn_def($3->var, $5.params, $5.count, $2, $7, @$);
  
        ast_free($3);
    }
  | FN IDENTIFIER LPAREN opt_params RPAREN block
    {
        $$ = new_fn_def($2->var, $4.params, $4.count, NULL, $6, @$);
  
        ast_free($2);
    }
;

opt_params:
    /* empty */ { $$.params = NULL; $$.count = 0; }
    | params      { $$ = $1; }
;

params:
    param {
        $$.count = 1;
        $$.params = calloc(0,sizeof(Param_t));
        $$.params[0].name = strdup($1->var);
        // Access the recursive type we stored in the node
        $$.params[0].type = $1->type; 
        ast_free($1);
    }
  | param COMMA params {
        $$.count = $3.count + 1;
        $$.params = calloc(0,sizeof(Param_t) * (size_t)$$.count);
        $$.params[0].name = strdup($1->var);
        $$.params[0].type = $1->type;
        ast_free($1);
        for (int i = 0; i < $3.count; i++) $$.params[i + 1] = $3.params[i];
        free($3.params);
    }
;

opt_list_size:
    SEMICOLON        { $$ = 0; } // Handle list[int; ]
    | SEMICOLON NUMBER { $$ = (size_t)TQparse_u128($2->literal.raw, NULL); }
;

recursive_type:
    DATATYPES {
        $$ = make_type($1, NULL); 
    }
    /* The recursive part: list[ <any type> ; <size> ] */

    | LISTS LSQUARE recursive_type opt_list_size RSQUARE {
        $$ = make_type(LIST, $3);
        $$->size = $4; 
    }
    | LISTS LSQUARE recursive_type opt_list_size error 
    {
        TQerror_LOC(@5, PARSE_UNCLOSED_BRACKET, NULL);
        yyerrok;
        $$ = make_type(LIST, $3); $$->size = $4;
    }
    
    | recursive_type AMP  %prec UADDR {
        $$ = make_type(PTR, $1);
    }
;


param:
    recursive_type IDENTIFIER {
        $2->type = $1; // Store the whole recursive structure
        $$ = $2; 
    }  /* AST_VAR node typed as param */
;

return_stmt:
    RETURN expr  { $$ = new_return($2, @$); }
    | RETURN       { $$ = new_return(NULL, @$); }
;

opt_args:
    /* empty */ { $$ = NULL; }
    | args        { $$ = $1; }
    ;
         
args:
    expr              { $$ = $1; }
    | expr COMMA args   { $$ = new_seq($1, $3); }        /* list */
    ;

list_stmt:
    LSQUARE opt_args RSQUARE { $$ = new_list($2, @$); }
    | LSQUARE opt_args error 
    { 
        TQerror_LOC(@3, PARSE_UNCLOSED_BRACKET, NULL); 
        yyerrok; $$ = new_list($2, @$); 
    }
;

expr:
    NUMBER                      { $$ = $1;}
    | IDENTIFIER                { $$ = $1;}
    | STRING_LITERAL            {$$ = $1;}
    | CHAR_LITERAL              {$$ = $1;}
    | BOOL_LITERAL              {$$ = $1;}

    | expr PLUS expr            { $$ = new_binop($1, $3, @$, OP_ADD); }
    | expr MINUS expr           { $$ = new_binop($1, $3, @$, OP_SUB); }
    | expr STAR expr            { $$ = new_binop($1, $3, @$, OP_MUL); }
    | expr SLASH expr           { $$ = new_binop($1, $3, @$, OP_DIV); }
    | expr MOD expr             { $$ = new_binop($1, $3, @$, OP_MOD); }
    | expr POWER expr           { $$ = new_binop($1, $3, @$, OP_POW); }

    | expr LSHIFT expr          { $$ = new_binop($1, $3, @$, OP_LSHIFT); }
    | expr RSHIFT expr          { $$ = new_binop($1, $3, @$, OP_RSHIFT); }

    | expr AMP expr             { $$ = new_binop($1, $3, @$, OP_BITAND); }
    | expr BITXOR expr          { $$ = new_binop($1, $3, @$, OP_BITXOR); }
    | expr PIPE expr            { $$ = new_binop($1, $3, @$, OP_BITOR); }

    | expr AND expr             { $$ = new_binop($1, $3, @$, OP_AND); }
    | expr OR expr              { $$ = new_binop($1, $3, @$, OP_OR); }

    | expr EQ expr              { $$ = new_binop($1, $3, @$, OP_EQ); }
    | expr NEQ expr             { $$ = new_binop($1, $3, @$, OP_NEQ); }
    | expr LT expr              { $$ = new_binop($1, $3, @$, OP_LT); }
    | expr LE expr              { $$ = new_binop($1, $3, @$, OP_LE); }
    | expr GT expr              { $$ = new_binop($1, $3, @$, OP_GT); }
    | expr GE expr              { $$ = new_binop($1, $3, @$, OP_GE); }

    | AMP expr %prec UADDR      { $$ = new_unop($2, @$, OP_ADDR); }
    | STAR expr %prec UDEREF    { $$ = new_unop($2, @$, OP_DEREF); }
    | PLUS expr %prec UPLUS     { $$ = new_unop($2, @$, OP_POS); }
    | MINUS expr %prec UMINUS   { $$ = new_unop($2, @$, OP_NEG); }
    | NOT expr                  { $$ = new_unop($2, @$, OP_NOT); }
    | BITNOT expr               { $$ = new_unop($2, @$, OP_BITNOT); }

    | IDENTIFIER INC %prec POSTFIX
        { $$ = new_unop($1, @$, OP_INC); }
    | IDENTIFIER DEC %prec POSTFIX
        { $$ = new_unop($1, @$, OP_DEC); }

    | LPAREN expr RPAREN         { $$ = $2; }
    | LPAREN expr error          {TQerror_LOC(@3, PARSE_UNCLOSED_PAREN, NULL); yyerrok; $$ = $2; }
    | IDENTIFIER LPAREN opt_args RPAREN
      {
          $$ = new_fn_call($1->var, $3, @$);
            
          ast_free($1);
      }
    | list_stmt                   {$$ = $1;}

    | IDENTIFIER LSQUARE expr RSQUARE  { $$ = new_index($1, $3, false, @$); }
    | IDENTIFIER LSQUARE expr error    
    { 
        TQerror_LOC(@4, PARSE_UNCLOSED_BRACKET, NULL); 
        yyerrok; $$ = new_index($1, $3, false, @$); 
    }
;

lvalue:
      IDENTIFIER                        { $$ = $1; }
    | STAR IDENTIFIER %prec UDEREF      { $$ = new_unop($2, @$, OP_DEREF); }
    | IDENTIFIER LSQUARE expr RSQUARE   { $$ = new_index($1, $3, true, @$); }
    | IDENTIFIER LSQUARE expr error     
    { 
        TQerror_LOC(@4, PARSE_UNCLOSED_BRACKET, NULL); 
        yyerrok; $$ = new_index($1, $3, true, @$); 
    }
;


assignment:
    VAR recursive_type lvalue ASSIGN expr {
        $$ = new_assign($3, $5, $2, false, @$, OP_ASSIGN);
        $$->assign.is_declaration = true;
        if($3->kind == AST_UNOP && $3->unop.op == OP_DEREF)
            TQerror_LOC(@3, PARSE_SYNTAX, g_last_parse_err_msg);
    }

    /* var mut x = ... */
    | VAR MUT recursive_type lvalue ASSIGN expr {
        $$ = new_assign($4, $6, $3, true, @$, OP_ASSIGN);
        $$->assign.is_declaration = true;
        if($4->kind == AST_UNOP && $4->unop.op == OP_DEREF)
            TQerror_LOC(@4, PARSE_SYNTAX, g_last_parse_err_msg);
    }

    | VAR lvalue ASSIGN expr {
        $$ = new_assign($2, $4, NULL, false, @$, OP_ASSIGN);
        $$->assign.is_declaration = true;
    }

    | lvalue ASSIGN expr
        {
            $$ = new_assign($1, $3, NULL, true, @$, OP_ASSIGN);
        }
    | lvalue PLUS_ASSIGN expr
        {
            $$ = new_assign($1, $3, NULL, true, @$, OP_PLUS_ASSIGN); 
          
        }
    | lvalue MINUS_ASSIGN expr
        { $$ = new_assign($1, $3, NULL, true, @$, OP_MINUS_ASSIGN); }

    | lvalue STAR_ASSIGN expr
        { $$ = new_assign($1, $3, NULL, true, @$, OP_MUL_ASSIGN); }

    | lvalue SLASH_ASSIGN expr
        { $$ = new_assign($1, $3, NULL, true, @$, OP_DIV_ASSIGN); }

    | lvalue MOD_ASSIGN expr
        { $$ = new_assign($1, $3, NULL, true, @$, OP_MOD_ASSIGN); }

    | lvalue LSHIFT_ASSIGN expr
        { $$ = new_assign($1, $3, NULL, true, @$, OP_LSHIFT_ASSIGN); }

    | lvalue RSHIFT_ASSIGN expr
        { $$ = new_assign($1, $3, NULL, true, @$, OP_RSHIFT_ASSIGN); }
    
    | lvalue POWER_ASSIGN expr
        { $$ = new_assign($1, $3, NULL, true, @$, OP_POW_ASSIGN); }
;

%%

void yyerror(YYLTYPE *loc, const char *s) {
    if (loc) {
        g_last_parse_err_line = loc->first_line;
        g_last_parse_err_col = loc->first_column;
        g_last_parse_err_pos = loc->first_pos;
    }
    g_last_parse_err_msg = s;
}
