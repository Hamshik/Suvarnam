%define api.pure full
%define parse.error verbose

%define api.location.type { SV_Location }
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
    #include <stdbool.h>

    char *logf_msg(const char *fmt, ...);
}

%union{
    ASTNode_t *node;
    DataTypes_t datatype;
    struct {
        Param_t *params;
        int count;
    } paramlist;

    idx_expr_t* idx_epr;

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
%token LPAREN RPAREN LBRACE RBRACE SEMICOLON LSQUARE RSQUARE COLON
%token ASSIGN PLUS_ASSIGN MINUS_ASSIGN STAR_ASSIGN SLASH_ASSIGN MOD_ASSIGN POWER_ASSIGN
%token LSHIFT_ASSIGN RSHIFT_ASSIGN IN COMMA DOT_DOT 
%token AND OR NOT EQ NEQ LT LE GT GE AT
%token IF ELSE FOR WHILE MUT VAR FN RETURN IMPORT CONTINUE BREAK

%token <datatype> DATATYPES
%token <node> IDENTIFIER NUMBER STRING_LITERAL BOOL_LITERAL CHAR_LITERAL

%type <node>  top_level_stmts block if_stmt for_stmt while_stmt import_stmt expr_stmts call_stmt
%type <node> fn_def param return_stmt opt_args args list_stmt expr_stmt top_level_stmt index_stmt fn_block_t
%type <node> lvalue import_list expr assignment program range  deref_expression
%type <paramlist> opt_params params
%type <type> recursive_type
%type <size> opt_list_size
%type <idx_epr> indexing

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
    import_list[impl] top_level_stmts[stmts]
    {
        if (! $impl) root = $stmts;
        else if (! $stmts) root = $impl;
        else root = new_seq($impl, $stmts);
    }
;

top_level_stmt:
    fn_def                      { $$ = $1; }
    | expr_stmt                 { $$ = $1; }
;

top_level_stmts: /* empty */    { $$ = NULL; }
    | top_level_stmt[stmt] top_level_stmts[stmts]
    {
        if (! $stmt) $$ = $stmts;
        else if (! $stmts) $$ = $stmt;
        else $$ = new_seq($stmt, $stmts);
    }
;

expr_stmt:
    assignment SEMICOLON        { $$ = $1; }
    | assignment error          { SV_error_LOC(@2, PARSE_MISSING_SEMI, g_last_parse_err_msg); yyerrok; $$ = $1; }
    | expr SEMICOLON            { $$ = $1; }
    | expr error                { SV_error_LOC(@2, PARSE_MISSING_SEMI, g_last_parse_err_msg); yyerrok; $$ = $1; }
    | block                     { $$ = $1; }
    | return_stmt SEMICOLON     { $$ = $1; }
    | return_stmt error         { SV_error_LOC(@2, PARSE_MISSING_SEMI, g_last_parse_err_msg); yyerrok; $$ = $1; }
    | error SEMICOLON           { panic( @1, PARSE_SYNTAX, g_last_parse_err_msg); yyerrok; $$ = NULL; }
    | if_stmt                   { $$ = $1; }
    | for_stmt                  { $$ = $1; }
    | while_stmt                { $$ = $1; }
    | CONTINUE SEMICOLON        { $$ = new_continue(@$); }
    | BREAK SEMICOLON           { $$ = new_break(@$); }
    | error {
        panic( @$, PARSE_SYNTAX, g_last_parse_err_msg);
        yyerrok;
        $$ = NULL; 
    }
;

import_list:
      /* empty */                  { $$ = NULL; }
    | import_stmt[stmt] SEMICOLON import_list[list]
      {
          if (! $list) $$ = $stmt;
          else $$ = new_seq($stmt, $list);
      }    
;

expr_stmts: /* empty */  { $$ = NULL; }
    | expr_stmt[stmt] expr_stmts[stmts]
    {
        if (! $stmt) $$ = $stmts;
        else if (! $stmts) $$ = $stmt;
        else $$ = new_seq($stmt, $stmts);
    }
    ;

import_stmt:
    IMPORT STRING_LITERAL[str]
      {
          $$ = new_import_node($str->literal.raw, @$);
      }
;

block: 
    LBRACE expr_stmts[stmts] RBRACE   { $$ = $stmts; }
;

if_stmt:
    IF LPAREN expr[cond] RPAREN expr_stmt[then_stmt] %prec LOWER_THAN_ELSE
        { $$ = new_if($cond, $then_stmt, NULL, @$); }
    | IF LPAREN expr[cond] RPAREN expr_stmt[then_stmt] ELSE expr_stmt[else_stmt]
        { $$ = new_if($cond, $then_stmt, $else_stmt, @$); }
;

range:
      expr[start] DOT_DOT expr[end] 
        { $$ = new_range($start, $end, NULL, false); }
    | expr[start] DOT_DOT expr[end] DOT_DOT expr[step] 
        { $$ = new_range($start, $end, $step, false); }
    | expr[start] DOT_DOT ASSIGN expr[end] 
        { $$ = new_range($start, $end, NULL, 1); }
    | expr[start] DOT_DOT ASSIGN expr[end] DOT_DOT expr[step] 
        { $$ = new_range($start, $end, $step, 1); }
;

for_stmt:
      FOR LPAREN IDENTIFIER[id] IN expr[iter] RPAREN expr_stmt[body]
    {
        $$ = new_for($id->var, $iter, $body, @$, false);
        free($id);
    }
    | FOR LPAREN MUT IDENTIFIER[id] IN expr[iter] RPAREN expr_stmt[body]
    { 
        $$ = new_for($id->var, $iter, $body, @$, 1); 
        free($id);
    }
    | FOR LPAREN range[iter] RPAREN expr_stmt[body]
    {
        $$ = new_for("__SV temp idx__", $iter, $body, @$, false);
    }
;

while_stmt:
    WHILE LPAREN expr[cond] RPAREN expr_stmt[body]
        { $$ = new_while($cond, $body, NULL, @$); }
    | WHILE LPAREN expr[cond] RPAREN COLON LPAREN assignment[assigns] RPAREN expr_stmt[body]
    {
        if($assigns->assign.op == OP_ASSIGN)
            panic( @1, PARSE_SYNTAX, "expr expects operational assignment not just plain assign");
        $$ = new_while($cond, $body, $assigns, @$);
    }
;

fn_block_t:
    SEMICOLON { $$ =  NULL; }
    | block   { $$ = $1; }
;

fn_def: 
    FN recursive_type[ret_type] IDENTIFIER[id] LPAREN opt_params[params] RPAREN  fn_block_t[body]
    {
        $$ = new_fn_def($id->var, $params.params, $params.count, $ret_type, $body, @$);
        ast_free($id);
    }
  | FN IDENTIFIER[id] LPAREN opt_params[params] RPAREN fn_block_t[body]
    {
        $$ = new_fn_def($id->var, $params.params, $params.count, NULL, $body, @$);
        ast_free($id);
    } 
;

call_stmt:
    IDENTIFIER[id] LPAREN opt_args[args] RPAREN
      {
          $$ = new_fn_call($id->var, $args, @$);
          ast_free($id);
      }
;

opt_params:
    /* empty */ { $$.params = NULL; $$.count = 0; }
    | params      { $$ = $1; }
;

params:
    param[p] {
        $$.count = 1;
        $$.params = calloc(0, sizeof(Param_t));
        $$.params[0].name = strdup($p->var);
        $$.params[0].type = $p->type; 
        ast_free($p);
    }
  | param[p] COMMA params[plist] {
        $$.count = $plist.count + 1;
        $$.params = calloc(0, sizeof(Param_t) * (size_t)$$.count);
        $$.params[0].name = strdup($p->var);
        $$.params[0].type = $p->type;
        ast_free($p);
        for (int i = 0; i < $plist.count; i++) $$.params[i + 1] = $plist.params[i];
        free($plist.params);
    }
;

opt_list_size:
    SEMICOLON          { $$ = 0; } 
    | SEMICOLON NUMBER[num] { $$ = (size_t)SV_parse_u128($num->literal.raw, NULL); }
;

recursive_type:
    DATATYPES[d] {
        $$ = make_type($d, NULL); 
    }
    | LSQUARE recursive_type[inner] opt_list_size[sz] RSQUARE {
        $$ = make_type(LIST, $inner);
        $$->size = $sz; 
    }
    | recursive_type[base] AMP  %prec UADDR {
        $$ = make_type(PTR, $base);
    }
    /* If the lexer accidentally punches out an AND, treat it as two PTR layers! */
    | recursive_type[base] AND %prec UADDR {
        Type_t* first_ptr = make_type(PTR, $base);
        $$ = make_type(PTR, first_ptr);
    }
;

param:
    recursive_type[t] IDENTIFIER[id] {
        $id->type = $t; 
        $$ = $id; 
    }
;

return_stmt:
    RETURN expr[e]  { $$ = new_return($e, @$); }
    | RETURN        { $$ = new_return(NULL, @$); }
;

opt_args:
    /* empty */ { $$ = NULL; }
    | args        { $$ = $1; }
    ;
         
args:
    expr               { $$ = $1; }
    | expr[e] COMMA args[alist]  { $$ = new_seq($e, $alist); }
    ;

list_stmt:
    LSQUARE opt_args[args] RSQUARE { $$ = new_list($args, @$); }
;

indexing:
    LSQUARE expr[e] RSQUARE
    {
        idx_expr_t* idx_node = malloc(sizeof(idx_expr_t));
        idx_node->expr_node = $e;
        idx_node->depth = 1;
        idx_node->next = NULL;
        $$ = idx_node;
    }
    | indexing[idx] LSQUARE expr[e] RSQUARE
    {
        idx_expr_t* idx_node = malloc(sizeof(idx_expr_t));
        idx_node->expr_node = $e;
        idx_node->depth = $idx->depth + 1;
        idx_node->next = $idx; 
        $$ = idx_node;
    }
;

index_stmt:
    IDENTIFIER[id] indexing[idx] 
    { 
        $$ = new_index($id, $idx, false, @1);
        $$->isglobal =  $id->isglobal;
    }
    | call_stmt[call] indexing[idx]  {
        $$ = new_index($call, $idx, false, @1);
        $$->isglobal =  $idx->isglobal;
    }
    | deref_expression[expr] indexing[idx]  {
        $$ = new_index($expr, $idx, false, @1);
        $$->isglobal =  $idx->isglobal;
    }
;

expr:
    NUMBER                      { $$ = $1;}
    | IDENTIFIER                { $$ = $1;}
    | STRING_LITERAL            { $$ = $1;}
    | CHAR_LITERAL              { $$ = $1;}
    | BOOL_LITERAL              { $$ = $1;}

    | expr[lhs] PLUS expr[rhs]        { $$ = new_binop($lhs, $rhs, @$, OP_ADD); }
    | expr[lhs] MINUS expr[rhs]       { $$ = new_binop($lhs, $rhs, @$, OP_SUB); }
    | expr[lhs] STAR expr[rhs]        { $$ = new_binop($lhs, $rhs, @$, OP_MUL); }
    | expr[lhs] SLASH expr[rhs]       { $$ = new_binop($lhs, $rhs, @$, OP_DIV); }
    | expr[lhs] MOD expr[rhs]         { $$ = new_binop($lhs, $rhs, @$, OP_MOD); }
    | expr[lhs] POWER expr[rhs]       { $$ = new_binop($lhs, $rhs, @$, OP_POW); }

    | expr[lhs] LSHIFT expr[rhs]      { $$ = new_binop($lhs, $rhs, @$, OP_LSHIFT); }
    | expr[lhs] RSHIFT expr[rhs]      { $$ = new_binop($lhs, $rhs, @$, OP_RSHIFT); }

    | expr[lhs] AMP expr[rhs]         { $$ = new_binop($lhs, $rhs, @$, OP_BITAND); }
    | expr[lhs] BITXOR expr[rhs]      { $$ = new_binop($lhs, $rhs, @$, OP_BITXOR); }
    | expr[lhs] PIPE expr[rhs]        { $$ = new_binop($lhs, $rhs, @$, OP_BITOR); }

    | expr[lhs] AND expr[rhs]         { $$ = new_binop($lhs, $rhs, @$, OP_AND); }
    | expr[lhs] OR expr[rhs]          { $$ = new_binop($lhs, $rhs, @$, OP_OR); }

    | expr[lhs] EQ expr[rhs]          { $$ = new_binop($lhs, $rhs, @$, OP_EQ); }
    | expr[lhs] NEQ expr[rhs]         { $$ = new_binop($lhs, $rhs, @$, OP_NEQ); }
    | expr[lhs] LT expr[rhs]          { $$ = new_binop($lhs, $rhs, @$, OP_LT); }
    | expr[lhs] LE expr[rhs]          { $$ = new_binop($lhs, $rhs, @$, OP_LE); }
    | expr[lhs] GT expr[rhs]          { $$ = new_binop($lhs, $rhs, @$, OP_GT); }
    | expr[lhs] GE expr[rhs]          { $$ = new_binop($lhs, $rhs, @$, OP_GE); }

    // 🎯 1. Immutable reference (Rust: &i)
    | AMP expr[e] %prec UADDR   
    { 
        $$ = new_unop($e, @$, OP_ADDR); 
        $$->unop.operand->ismut = false; // Custom property flag
    }
    
    // 🎯 2. Mutable reference (Rust: &mut i)
    | AMP MUT expr[e] %prec UADDR   
    { 
        $$ = new_unop($e, @$, OP_ADDR); 
        $$->unop.operand->ismut = true; // Custom property flag
    }
    
    | LPAREN deref_expression[der_expr] RPAREN  { $$ = $der_expr;  }
    | PLUS expr[e] %prec UPLUS        { $$ = new_unop($e, @$, OP_POS); }
    | MINUS expr[e] %prec UMINUS      { $$ = new_unop($e, @$, OP_NEG); }
    | NOT expr[e]                     { $$ = new_unop($e, @$, OP_NOT); }
    | BITNOT expr[e]                  { $$ = new_unop($e, @$, OP_BITNOT); }

    | IDENTIFIER[id] INC %prec POSTFIX  { $$ = new_unop($id, @$, OP_INC); $$->isglobal =  $id->isglobal;}
    | IDENTIFIER[id] DEC %prec POSTFIX  { $$ = new_unop($id, @$, OP_DEC); $$->isglobal =  $id->isglobal; }

    | LPAREN expr[e] RPAREN           { $$ = $e; }
    | call_stmt[call]                 { $$ = $call; }
    | list_stmt[list]                 { $$ = $list;} 
    | index_stmt[idx]                 { $$ = $idx; $$->isglobal =  $idx->isglobal;}
    | LBRACE range[r] RBRACE          { $$ = $r; }
;

deref_expression:
      STAR IDENTIFIER %prec UDEREF       { $$ = new_unop($2, @$, OP_DEREF); }
    | STAR deref_expression %prec UDEREF
    { 
        $$ = new_unop($2, @$, OP_DEREF); 
        /* Carry over global flags if needed */
        $$->isglobal = $2->isglobal; 
    }
;

lvalue:
      IDENTIFIER                            { $$ = $1; }
    | index_stmt[idx]                       { $$ = $idx; $$->index.islhs = 1; $$->isglobal = $idx->isglobal; }
    | deref_expression                      { $$ = $1; }
;

assignment:
    VAR recursive_type[t] lvalue[lval] ASSIGN expr[e] {
        $$ = new_assign($lval, $e, $t, false, @$, OP_ASSIGN);
        $$->assign.is_declaration = 1;
        $$->isglobal = $lval->isglobal;
        if($lval->kind == AST_UNOP && $lval->unop.op == OP_DEREF)
            SV_error_LOC(@3, PARSE_SYNTAX, "unexpected dereferance operator");
    }

    | VAR MUT recursive_type[t] lvalue[lval] ASSIGN expr[e] {
        $$ = new_assign($lval, $e, $t, 1, @$, OP_ASSIGN);
        $$->assign.is_declaration = 1;
        $$->isglobal = $lval->isglobal;
        if($lval->kind == AST_UNOP && $lval->unop.op == OP_DEREF)
            SV_error_LOC(@4, PARSE_SYNTAX, "unexpected dereferance operator");
    }

    | VAR lvalue[lval] ASSIGN expr[e] {
        $$ = new_assign($lval, $e, NULL, false, @$, OP_ASSIGN);
        $$->assign.is_declaration = 1;
        $$->isglobal = $lval->isglobal;
        if($lval->kind == AST_UNOP && $lval->unop.op == OP_DEREF)
            SV_error_LOC(@3, PARSE_SYNTAX, "unexpected dereferance operator");
    }

    | VAR MUT lvalue[lval] ASSIGN expr[e] {
        $$ = new_assign($lval, $e, NULL, 1, @$, OP_ASSIGN);
        $$->assign.is_declaration = 1;
        $$->isglobal = $lval->isglobal;
        if($lval->kind == AST_UNOP && $lval->unop.op == OP_DEREF)
            SV_error_LOC(@4, PARSE_SYNTAX, "unexpected dereferance operator");
    }

    | lvalue[lval] ASSIGN expr[e]
        {
            $$ = new_assign($lval, $e, NULL, 1, @$, OP_ASSIGN);
            $$->isglobal = $lval->isglobal;
        }
    | lvalue[lval] PLUS_ASSIGN expr[e]
        {
            $$ = new_assign($lval, $e, NULL, 1, @$, OP_PLUS_ASSIGN); 
            $$->isglobal = $lval->isglobal;
        }
    | lvalue[lval] MINUS_ASSIGN expr[e]
        { $$ = new_assign($lval, $e, NULL, 1, @$, OP_MINUS_ASSIGN); $$->isglobal = $lval->isglobal; }

    | lvalue[lval] STAR_ASSIGN expr[e]
        { $$ = new_assign($lval, $e, NULL, 1, @$, OP_MUL_ASSIGN); $$->isglobal = $lval->isglobal; }

    | lvalue[lval] SLASH_ASSIGN expr[e]
        { $$ = new_assign($lval, $e, NULL, 1, @$, OP_DIV_ASSIGN); $$->isglobal = $lval->isglobal; }

    | lvalue[lval] MOD_ASSIGN expr[e]
        { $$ = new_assign($lval, $e, NULL, 1, @$, OP_MOD_ASSIGN); $$->isglobal = $lval->isglobal; }

    | lvalue[lval] LSHIFT_ASSIGN expr[e]
        { $$ = new_assign($lval, $e, NULL, 1, @$, OP_LSHIFT_ASSIGN); $$->isglobal = $lval->isglobal; }

    | lvalue[lval] RSHIFT_ASSIGN expr[e]
        { $$ = new_assign($lval, $e, NULL, 1, @$, OP_RSHIFT_ASSIGN); $$->isglobal = $lval->isglobal; }
    
    | lvalue[lval] POWER_ASSIGN expr[e]
        { $$ = new_assign($lval, $e, NULL, 1, @$, OP_POW_ASSIGN); $$->isglobal = $lval->isglobal; }
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
