%filenames Parser
%baseclass-preinclude "frontend/parser/parser_includes.hpp"
%ltype SA_Location

%polymorphic
    node: ASTNode_t*;
    datatype: DataTypes_t;
    paramlist: ParamList_t;
    idx_epr: idx_expr_t*;
    type: Type_t*;
    size: size_t;
    op: OP_kind_t;

%token LEX_ERROR
%token LBRACE RBRACE SEMICOLON COLON IN COMMA DOT_DOT ELLIPSIS
%token IF FOR WHILE MUT VAR FN RETURN IMPORT
%token CONTINUE BREAK NOT BITNOT SET

%token <datatype> DATATYPES
%token <node> IDENTIFIER NUMBER STRING_LITERAL BOOL_LITERAL CHAR_LITERAL

%type <op> assign_op
%type <node>  top_level_stmts block if_stmt for_stmt while_stmt import_stmt expr_stmts
%type <node>  fn_def param param_tail return_stmt opt_args args list_stmt expr_stmt top_level_stmt index_stmt fn_block_t
%type <node>  lvalue import_list expr assignment program range expr_assign
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
%left LSQUARE RSQUARE
%left LPAREN RPAREN
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

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
        if (!$1) root = $2;
        else if (!$2) root = $1;
        else root = new_seq($1, $2);
    }
;

top_level_stmt:
    fn_def                      { $$ = $1; }
    | expr_stmt                 { $$ = $1; }
;

top_level_stmts:
    /* empty */                 { $$ = static_cast<ASTNode_t *>(nullptr); }
    | top_level_stmt top_level_stmts
    {
        if (!$1) $$ = $2;
        else if (!$2) $$ = $1;
        else $$ = new_seq($1, $2);
    }
;

expr_stmt:
    assignment SEMICOLON        { $$ = $1; }
    | assignment error SEMICOLON { panic(SA_loc_after(@1), PARSE_MISSING_SEMI, NULL); $$ = static_cast<ASTNode_t *>(nullptr); }
    | expr SEMICOLON            { $$ = $1; }
    | expr error SEMICOLON      { panic(SA_loc_after(@1), PARSE_MISSING_SEMI, NULL); $$ = static_cast<ASTNode_t *>(nullptr); }
    | block                     { $$ = $1; }
    | return_stmt SEMICOLON     { $$ = $1; }
    | return_stmt error SEMICOLON { panic(SA_loc_after(@1), PARSE_MISSING_SEMI, NULL); $$ = static_cast<ASTNode_t *>(nullptr); }
    | LEX_ERROR SEMICOLON       { scanner.lexTakeErr(); $$ = static_cast<ASTNode_t *>(nullptr); }
    | LEX_ERROR                 { scanner.lexTakeErr(); $$ = static_cast<ASTNode_t *>(nullptr); }
    | error SEMICOLON           { panic(@1, PARSE_SYNTAX, NULL); $$ = static_cast<ASTNode_t *>(nullptr); }
    | if_stmt                   { $$ = $1; }
    | for_stmt                  { $$ = $1; }
    | while_stmt                { $$ = $1; }
    | CONTINUE SEMICOLON        { $$ = new_continue(@1); }
    | BREAK SEMICOLON           { $$ = new_break(@1); }
    /* declaring assign here, to reduce the ambiguity */
    | lvalue assign_op expr SEMICOLON {
        OP_kind_t op = $2;
        $$ = new_assign($1, $3, nullptr, 1, @1, op);
    }

    | error {
        if (!scanner.lexTakeErr()) panic(@1, PARSE_SYNTAX, g_last_parse_err_msg);
        if(isError) ABORT();
        else ACCEPT();
    }
;

import_list:
    /* empty */                 { $$ = static_cast<ASTNode_t *>(nullptr); }
    | import_stmt SEMICOLON import_list
      {
          if (!$3) $$ = $1;
          else $$ = new_seq($1, $3);
      }    
;

expr_stmts:
    /* empty */                 { $$ = static_cast<ASTNode_t *>(nullptr); }
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
    IF LPAREN expr RPAREN expr_stmt %prec LOWER_THAN_ELSE
        { $$ = new_if($3, $5, NULL, @1); }
    | IF LPAREN expr RPAREN expr_stmt ELSE expr_stmt
        { $$ = new_if($3, $5, $7, @1); }
;

recursive_type:
    DATATYPES {
        $$ = make_type($1, static_cast<Type_t*>(nullptr)); 
    }
    | recursive_type LSQUARE opt_list_size RSQUARE {
        $$ = make_type(LIST, $1);
        $$->size = $3; 
    }
    | recursive_type AMP %prec AMP {
        $$ = make_type(PTR, $1);
    }
    | recursive_type AND %prec AMP {
        Type_t* first_ptr = make_type(PTR, $1);
        $$ = make_type(PTR, first_ptr);
    }
;