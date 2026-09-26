%filenames Parser
%baseclass-preinclude "frontend/parser/parser_includes.hpp"
%ltype SA::Location

%polymorphic
    node: ASTNode*;
    datatype: DataTypes_t;
    paramlist: SA::ParamList;
    idx_epr: SA::idxExpr*;
    type: SA::Type*;
    size: size_t;
    op: OP_kind_t;

%token LEX_ERROR
%token LBRACE RBRACE SEMICOLON COLON IN COMMA ELLIPSIS
%token IF FOR WHILE MUT VAR FN RETURN IMPORT
%token CONTINUE BREAK NOT BITNOT

%token <datatype> DATATYPES
%token <node> IDENTIFIER NUMBER STRING_LITERAL BOOL_LITERAL CHAR_LITERAL

%type <op> assign_op
%type <node>  top_level_stmts block if_stmt for_stmt while_stmt import_stmt expr_stmts
%type <node>  fn_def param param_tail return_stmt opt_args args list_stmt with_non_expr expr_stmt top_level_stmt index_stmt fn_block_t
%type <node>  assign_expr import_list expr assignment program range
%type <paramlist> opt_params params
%type <type>  recursive_type
%type <size>  opt_list_size
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
%left INC DEC
%left LPAREN RPAREN
%left LSQUARE RSQUARE
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE
%nonassoc DOT_DOT

%start program


%%

%include expr.ly
%include fn.ly
%include list.ly
%include loops.ly
%include assign.ly

program:
    import_list top_level_stmts
    {
        if (!$1) $$ = $2;
        else if (!$2) $$ = $1;
        else $$ = new_seq($1, $2);
        root = $$;
    }
;

top_level_stmt:
    fn_def                      { $$ = $1; }
    | expr_stmt                 { $$ = $1; }
;

top_level_stmts:
    /* empty */                 { $$ = null(ASTNode*); }
    | top_level_stmt top_level_stmts
    {
        if (!$1) $$ = $2;
        else if (!$2) $$ = $1;
        else $$ = new_seq($1, $2);
    }
;

expr_stmt:
    assignment SEMICOLON        { $$ = $1; }
    | assignment error SEMICOLON { panic(SA_loc_after(@1), PARSE_MISSING_SEMI, NULL); $$ = null(ASTNode*); }
    | with_non_expr SEMICOLON            { $$ = $1; }
    | with_non_expr error SEMICOLON      { panic(SA_loc_after(@1), PARSE_MISSING_SEMI, NULL); $$ = null(ASTNode*); }
    | block                     { $$ = $1; }
    | return_stmt SEMICOLON     { $$ = $1; }
    | return_stmt error SEMICOLON { panic(SA_loc_after(@1), PARSE_MISSING_SEMI, NULL); $$ = null(ASTNode*); }
    | LEX_ERROR SEMICOLON       { scanner.lexTakeErr(); $$ = null(ASTNode*); }
    | LEX_ERROR                 { scanner.lexTakeErr(); $$ = null(ASTNode*); }
    | error SEMICOLON           { panic(@1, PARSE_SYNTAX, NULL); $$ = null(ASTNode*); }
    | if_stmt                   { $$ = $1; }
    | for_stmt                  { $$ = $1; }
    | while_stmt                { $$ = $1; }
    | CONTINUE SEMICOLON        { $$ = new_continue(@1); }
    | BREAK SEMICOLON           { $$ = new_break(@1); }
    /* Keep assignment operators out of expr to avoid assignment ambiguity. */
    | assign_expr SEMICOLON       { $$ = $1; }

    | error {
        if (!scanner.lexTakeErr()) panic(@1, PARSE_SYNTAX, ErrMsg);
        ABORT();
        $$ = null(ASTNode*);
    }
;

import_list:
    /* empty */                 { $$ = null(ASTNode*); }
    | import_stmt SEMICOLON import_list
      {
          if (!$3) $$ = $1;
          else $$ = new_seq($1, $3);
      }    
;

expr_stmts:
    /* empty */                 { $$ = null(ASTNode*); }
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
          $$ = new_import_node($2->literal.raw, @1);
      }
;

block:
    LBRACE expr_stmts RBRACE    { $$ = $2; }
;

if_stmt:
    IF LPAREN with_non_expr RPAREN expr_stmt %prec LOWER_THAN_ELSE
        { $$ = new_if($3, $5, NULL, @1); }
    | IF LPAREN with_non_expr RPAREN expr_stmt ELSE expr_stmt
        { $$ = new_if($3, $5, $7, @1); }
;

recursive_type:
    DATATYPES {
        $$ = new SA::Type($1, null(SA::Type*)); 
    }
    | recursive_type LSQUARE opt_list_size RSQUARE {
        $$ = new SA::Type(LIST, $1);
        $$->size = $3; 
    }
    | recursive_type AMP %prec AMP {
        $$ = new SA::Type(PTR, $1);
    }
    | recursive_type AND %prec AMP {
        SA::Type* first_ptr = new SA::Type(PTR, $1);
        $$ = new SA::Type(PTR, first_ptr);
    }
;