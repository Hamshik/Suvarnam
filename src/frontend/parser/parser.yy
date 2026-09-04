%filenames Parser
%scanner Scanner.h
%baseclass-preinclude "frontend/parser/parser_includes.hpp"
%lsp-needed

%polymorphic
    node: ASTNode_t*;
    datatype: DataTypes_t;
    paramlist: ParamList_t;
    idx_epr: idx_expr_t*;
    type: Type_t*;
    size: size_t;

%token LEX_ERROR Eof
%token LBRACE RBRACE SEMICOLON COLON IN COMMA DOT_DOT ELLIPSIS
%token IF FOR WHILE MUT VAR FN RETURN IMPORT CONTINUE BREAK NOT BITNOT

%token <datatype> DATATYPES
%token <node> IDENTIFIER NUMBER STRING_LITERAL BOOL_LITERAL CHAR_LITERAL

%type <node>  top_level_stmts block if_stmt for_stmt while_stmt import_stmt expr_stmts call_stmt
%type <node>  fn_def param param_tail return_stmt opt_args args list_stmt expr_stmt top_level_stmt index_stmt fn_block_t
%type <node>  lvalue import_list expr assignment program range expr_class
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

program:
    import_list top_level_stmts
    {
        if (!NODE($1)) root = NODE($2);
        else if (!NODE($2)) root = NODE($1);
        else root = new_seq(NODE($1), NODE($2));
    }
    | Eof {
        $$ = static_cast<ASTNode_t *>(nullptr);
        ACCEPT();
    }
;

top_level_stmt:
    fn_def                      { $$ = NODE($1); }
    | expr_stmt                 { $$ = NODE($1); }
;

top_level_stmts:
    /* empty */                 { $$ = static_cast<ASTNode_t *>(nullptr); }
    | top_level_stmt top_level_stmts
    {
        if (!NODE($1)) $$ = NODE($2);
        else if (!NODE($2)) $$ = NODE($1);
        else $$ = new_seq(NODE($1), NODE($2));
    }
;

expr_stmt:
    assignment SEMICOLON        { $$ = NODE($1); }
    | assignment error SEMICOLON { SA_error_LOC(SA_loc_after(SA_parser_loc(@1)), PARSE_MISSING_SEMI, NULL); $$ = static_cast<ASTNode_t *>(nullptr); }
    | expr SEMICOLON            { $$ = NODE($1); }
    | expr error SEMICOLON      { SA_error_LOC(SA_loc_after(SA_parser_loc(@1)), PARSE_MISSING_SEMI, NULL); $$ = static_cast<ASTNode_t *>(nullptr); }
    | block                     { $$ = NODE($1); }
    | return_stmt SEMICOLON     { $$ = NODE($1); }
    | return_stmt error SEMICOLON { SA_error_LOC(SA_loc_after(SA_parser_loc(@1)), PARSE_MISSING_SEMI, NULL); $$ = static_cast<ASTNode_t *>(nullptr); }
    | LEX_ERROR SEMICOLON       { SA_lexer_take_error(); $$ = static_cast<ASTNode_t *>(nullptr); }
    | LEX_ERROR                 { SA_lexer_take_error(); $$ = static_cast<ASTNode_t *>(nullptr); }
    | error SEMICOLON           { panic(SA_parser_loc(@1), PARSE_SYNTAX, NULL); $$ = static_cast<ASTNode_t *>(nullptr); }
    | if_stmt                   { $$ = NODE($1); }
    | for_stmt                  { $$ = NODE($1); }
    | while_stmt                { $$ = NODE($1); }
    | CONTINUE SEMICOLON        { $$ = new_continue(lex_loc); }
    | BREAK SEMICOLON           { $$ = new_break(lex_loc); }
    | error {
        if (!SA_lexer_take_error()) panic(SA_parser_loc(lex_loc), PARSE_SYNTAX, g_last_parse_err_msg);
        ABORT();
    }
;

import_list:
    /* empty */                 { $$ = static_cast<ASTNode_t *>(nullptr); }
    | import_stmt SEMICOLON import_list
      {
          if (!NODE($3)) $$ = NODE($1);
          else $$ = new_seq(NODE($1), NODE($3));
      }    
;

expr_stmts:
    /* empty */                 { $$ = static_cast<ASTNode_t *>(nullptr); }
    | expr_stmt expr_stmts
    {
        if (!NODE($1)) $$ = NODE($2);
        else if (!NODE($2)) $$ = NODE($1);
        else $$ = new_seq(NODE($1), NODE($2));
    }
;

import_stmt:
    IMPORT STRING_LITERAL
      {
          $$ = new_import_node(NODE($2)->literal.raw, lex_loc);
      }
;

block:
    LBRACE expr_stmts RBRACE    { $$ = NODE($2); }
;

if_stmt:
    IF LPAREN expr RPAREN expr_stmt %prec LOWER_THAN_ELSE
        { $$ = new_if(NODE($3), NODE($5), NULL, lex_loc); }
    | IF LPAREN expr RPAREN expr_stmt ELSE expr_stmt
        { $$ = new_if(NODE($3), NODE($5), NODE($7), lex_loc); }
;

range:
      expr DOT_DOT expr 
        { $$ = new_range(NODE($1), NODE($3), NULL, false); }
    | expr DOT_DOT expr DOT_DOT expr 
        { $$ = new_range(NODE($1), NODE($3), NODE($5), false); }
    | expr DOT_DOT ASSIGN expr 
        { $$ = new_range(NODE($1), NODE($4), NULL, 1); }
    | expr DOT_DOT ASSIGN expr DOT_DOT expr 
        { $$ = new_range(NODE($1), NODE($4), NODE($6), 1); }
;

for_stmt:
      FOR LPAREN IDENTIFIER IN expr RPAREN expr_stmt
    {
        $$ = new_for(NODE($3)->var, NODE($5), NODE($7), lex_loc, false);
        ast_free(NODE($3));
    }
    | FOR LPAREN MUT IDENTIFIER IN expr RPAREN expr_stmt
    { 
        $$ = new_for(NODE($4)->var, NODE($6), NODE($8), lex_loc, 1); 
        ast_free(NODE($4));
    }
    | FOR LPAREN range RPAREN expr_stmt
    {
        $$ = new_for("__SA temp idx__", NODE($3), NODE($5), lex_loc, false);
    }
;

while_stmt:
    WHILE LPAREN expr RPAREN expr_stmt
        { $$ = new_while(NODE($3), NODE($5), NULL, lex_loc); }
    | WHILE LPAREN expr RPAREN COLON LPAREN assignment RPAREN expr_stmt
    {
        if(NODE($7)->assign.op == OP_ASSIGN)
            panic(SA_parser_loc(@7), PARSE_SYNTAX, "expr expects operational assignment not just plain assign");
        $$ = new_while(NODE($3), NODE($9), NODE($7), lex_loc);
    }
;

fn_block_t:
    SEMICOLON                   { $$ = static_cast<ASTNode_t *>(nullptr); }
    | block                     { $$ = NODE($1); }
;

fn_def:
    FN recursive_type IDENTIFIER LPAREN opt_params RPAREN fn_block_t
    {
        $$ = new_fn_def(NODE($3)->var, PARAMS($5).params, PARAMS($5).count, TYPE($2), NODE($7), lex_loc);
        ast_free(NODE($3));
    }
  | FN IDENTIFIER LPAREN opt_params RPAREN fn_block_t
    {
        $$ = new_fn_def(NODE($2)->var, PARAMS($4).params, PARAMS($4).count, NULL, NODE($6), lex_loc);
        ast_free(NODE($2));
    } 
;

call_stmt:
    IDENTIFIER LPAREN opt_args RPAREN
      {
          $$ = new_fn_call(NODE($1)->var, NODE($3), lex_loc);
          ast_free(NODE($1));
      }
;

opt_params:
    /* empty */                 { $$.params = NULL; $$.count = 0; }
    | params                    { $$ = PARAMS($1); }
;

params:
    param {
        ParamList_t res;
        res.count = 1;
        res.params = (Param_t*)calloc(1, sizeof(Param_t));
        res.params[0].name = strdup(NODE($1)->var);
        res.params[0].type = NODE($1)->type;
        res.params[0].is_variadic = NODE($1)->is_variadic;
        ast_free(NODE($1));
        $$ = res;
    }
  | param COMMA params {
        ParamList_t res;
        res.count = PARAMS($3).count + 1;
        res.params = (Param_t*)calloc((size_t)res.count, sizeof(Param_t));
        res.params[0].name = strdup(NODE($1)->var);
        res.params[0].type = NODE($1)->type;
        res.params[0].is_variadic = NODE($1)->is_variadic;
        ast_free(NODE($1));
        for (int i = 0; i < PARAMS($3).count; i++) res.params[i + 1] = PARAMS($3).params[i];
        free(PARAMS($3).params);
        $$ = res;
    }
;

opt_list_size:
    /* empty */                 { $$ = static_cast<size_t>(0); } 
    | SEMICOLON NUMBER          { $$ = (size_t)SA_parse_u128(NODE($2)->literal.raw, NULL); }
;

recursive_type:
    DATATYPES {
        $$ = make_type(DATA($1), static_cast<Type_t*>(nullptr)); 
    }
    | recursive_type LSQUARE opt_list_size RSQUARE {
        $$ = make_type(LIST, TYPE($1));
        $$->size = SIZE($3); 
    }
    | recursive_type AMP %prec AMP {
        $$ = make_type(PTR, TYPE($1));
    }
    | recursive_type AND %prec AMP {
        Type_t* first_ptr = make_type(PTR, TYPE($1));
        $$ = make_type(PTR, first_ptr);
    }
;

param:
    VAR MUT recursive_type param_tail {
        NODE($4)->type = NODE($4)->is_variadic ? make_type(LIST, TYPE($3)) : TYPE($3);
        NODE($4)->type->ismut = true;
        NODE($4)->ismut = true;
        $$ = NODE($4);
    }
    | VAR recursive_type param_tail {
        NODE($3)->type = NODE($3)->is_variadic ? make_type(LIST, TYPE($2)) : TYPE($2);
        NODE($3)->type->ismut = false;
        NODE($3)->ismut = false;
        $$ = NODE($3);
    }
    | recursive_type param_tail {
        NODE($2)->type = NODE($2)->is_variadic ? make_type(LIST, TYPE($1)) : TYPE($1);
        NODE($2)->type->ismut = false;
        NODE($2)->ismut = false;
        $$ = NODE($2);
    }
;

param_tail:
    IDENTIFIER {
        NODE($1)->is_variadic = false;
        $$ = NODE($1);
    }
    | ELLIPSIS IDENTIFIER {
        NODE($2)->is_variadic = true;
        $$ = NODE($2);
    }
;

return_stmt:
    RETURN expr                 { $$ = new_return(NODE($2), lex_loc); }
    | RETURN                    { $$ = new_return(NULL, lex_loc); }
;

opt_args:
    /* empty */                 { $$ = static_cast<ASTNode_t *>(nullptr); }
    | args                      { $$ = NODE($1); }
;

args:
    expr                        { $$ = NODE($1); }
    | expr COMMA args           { $$ = new_seq(NODE($1), NODE($3)); }
;

list_stmt:
    LSQUARE opt_args RSQUARE    { $$ = new_list(NODE($2), lex_loc); }
;

indexing:
    LSQUARE expr RSQUARE
    {
        idx_expr_t* idx_node = (idx_expr_t*)malloc(sizeof(idx_expr_t));
        idx_node->expr_node = NODE($2);
        idx_node->depth = 1;
        idx_node->next = NULL;
        $$ = idx_node;
    }
    | indexing LSQUARE expr RSQUARE
    {
        idx_expr_t* idx_node = (idx_expr_t*)malloc(sizeof(idx_expr_t));
        idx_node->expr_node = NODE($3);
        idx_node->depth = IDX($1)->depth + 1;
        idx_node->next = IDX($1); 
        $$ = idx_node;
    }
;

index_stmt:
    expr indexing 
    { 
        $$ = new_index(NODE($1), IDX($2), false, SA_parser_loc(@1));
        $$->isglobal = NODE($1)->isglobal;
    }
;

expr:
    NUMBER                      { $$ = NODE($1); }
    | IDENTIFIER                { $$ = NODE($1); }
    | STRING_LITERAL            { $$ = NODE($1); }
    | CHAR_LITERAL              { $$ = NODE($1); }
    | BOOL_LITERAL              { $$ = NODE($1); }

    | expr PLUS expr            { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_ADD); }
    | expr MINUS expr           { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_SUB); }
    | expr STAR expr            { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_MUL); }
    | expr SLASH expr           { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_DIV); }
    | expr MOD expr             { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_MOD); }
    | expr POWER expr           { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_POW); }

    | expr LSHIFT expr          { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_LSHIFT); }
    | expr RSHIFT expr          { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_RSHIFT); }

    | expr AMP expr             { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_BITAND); }
    | expr BITXOR expr          { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_BITXOR); }
    | expr PIPE expr            { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_BITOR); }

    | expr AND expr             { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_AND); }
    | expr OR expr              { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_OR); }

    | expr EQ expr              { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_EQ); }
    | expr NEQ expr             { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_NEQ); }
    | expr LT expr              { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_LT); }
    | expr LE expr              { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_LE); }
    | expr GT expr              { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_GT); }
    | expr GE expr              { $$ = new_binop(NODE($1), NODE($3), lex_loc, OP_GE); }

    | AMP expr %prec AMP   
    { 
        $$ = new_unop(NODE($2), lex_loc, OP_ADDR); 
        $$->unop.operand->ismut = false;
    }
    
    | AMP MUT expr %prec AMP   
    { 
        $$ = new_unop(NODE($3), lex_loc, OP_ADDR); 
        $$->unop.operand->ismut = true;
    }

    | STAR expr %prec PLUS      { $$ = new_unop(NODE($2), lex_loc, OP_DEREF); }
    
    | PLUS expr %prec PLUS      { $$ = new_unop(NODE($2), lex_loc, OP_POS); }
    | MINUS expr %prec MINUS    { $$ = new_unop(NODE($2), lex_loc, OP_NEG); }
    | NOT expr                   { $$ = new_unop(NODE($2), lex_loc, OP_NOT); }
    | BITNOT expr                { $$ = new_unop(NODE($2), lex_loc, OP_BITNOT); }

    | IDENTIFIER INC %prec INC  { $$ = new_unop(NODE($1), lex_loc, OP_INC); $$->isglobal = NODE($1)->isglobal;}
    | IDENTIFIER DEC %prec INC  { $$ = new_unop(NODE($1), lex_loc, OP_DEC); $$->isglobal = NODE($1)->isglobal; }

    | LPAREN expr RPAREN         { $$ = NODE($2); }
    | call_stmt                  { $$ = NODE($1); }
    | list_stmt                  { $$ = NODE($1); } 
    | index_stmt                 { $$ = NODE($1); $$->isglobal = NODE($1)->isglobal;}
    | LBRACE range RBRACE        { $$ = NODE($2); }
;

lvalue:
      IDENTIFIER                { $$ = NODE($1); }
    | index_stmt                { $$ = NODE($1); $$->index.islhs = 1; $$->isglobal = NODE($1)->isglobal; }
    | STAR expr                 { $$ = new_unop(NODE($2), lex_loc, OP_DEREF); $$->isglobal = NODE($2)->isglobal; }
;

assignment:
    VAR recursive_type lvalue ASSIGN expr {
        $$ = new_assign(NODE($3), NODE($5), TYPE($2), false, lex_loc, OP_ASSIGN);
        $$->assign.is_declaration = 1;
        $$->isglobal = NODE($3)->isglobal;
        if(NODE($3)->kind == AST_UNOP && NODE($3)->unop.op == OP_DEREF)
            panic(SA_parser_loc(@3), PARSE_SYNTAX, "unexpected dereference operator");
    }

    | VAR MUT recursive_type lvalue ASSIGN expr {
        $$ = new_assign(NODE($4), NODE($6), TYPE($3), 1, lex_loc, OP_ASSIGN);
        $$->assign.is_declaration = 1;
        $$->isglobal = NODE($4)->isglobal;
        if(NODE($4)->kind == AST_UNOP && NODE($4)->unop.op == OP_DEREF)
            panic(SA_parser_loc(@4), PARSE_SYNTAX, "unexpected dereference operator");
    }

    | VAR lvalue ASSIGN expr {
        $$ = new_assign(NODE($2), NODE($4), NULL, false, lex_loc, OP_ASSIGN);
        $$->assign.is_declaration = 1;
        $$->isglobal = NODE($2)->isglobal;
        if(NODE($2)->kind == AST_UNOP && NODE($2)->unop.op == OP_DEREF)
            panic(SA_parser_loc(@3), PARSE_SYNTAX, "unexpected dereference operator");
    }

    | VAR MUT lvalue ASSIGN expr {
        $$ = new_assign(NODE($3), NODE($5), NULL, 1, lex_loc, OP_ASSIGN);
        $$->assign.is_declaration = 1;
        $$->isglobal = NODE($3)->isglobal;
        if(NODE($3)->kind == AST_UNOP && NODE($3)->unop.op == OP_DEREF)
            panic(SA_parser_loc(@4), PARSE_SYNTAX, "unexpected dereference operator");
    }

    | lvalue ASSIGN expr
        {
            $$ = new_assign(NODE($1), NODE($3), NULL, 1, lex_loc, OP_ASSIGN);
            $$->isglobal = NODE($1)->isglobal;
        }
    | lvalue PLUS_ASSIGN expr
        {
            $$ = new_assign(NODE($1), NODE($3), NULL, 1, lex_loc, OP_PLUS_ASSIGN); 
            $$->isglobal = NODE($1)->isglobal;
        }
    | lvalue MINUS_ASSIGN expr
        { $$ = new_assign(NODE($1), NODE($3), NULL, 1, lex_loc, OP_MINUS_ASSIGN); $$->isglobal = NODE($1)->isglobal; }

    | lvalue STAR_ASSIGN expr
        { $$ = new_assign(NODE($1), NODE($3), NULL, 1, lex_loc, OP_MUL_ASSIGN); $$->isglobal = NODE($1)->isglobal; }

    | lvalue SLASH_ASSIGN expr
        { $$ = new_assign(NODE($1), NODE($3), NULL, 1, lex_loc, OP_DIV_ASSIGN); $$->isglobal = NODE($1)->isglobal; }

    | lvalue MOD_ASSIGN expr
        { $$ = new_assign(NODE($1), NODE($3), NULL, 1, lex_loc, OP_MOD_ASSIGN); $$->isglobal = NODE($1)->isglobal; }

    | lvalue LSHIFT_ASSIGN expr
        { $$ = new_assign(NODE($1), NODE($3), NULL, 1, lex_loc, OP_LSHIFT_ASSIGN); $$->isglobal = NODE($1)->isglobal; }

    | lvalue RSHIFT_ASSIGN expr
        { $$ = new_assign(NODE($1), NODE($3), NULL, 1, lex_loc, OP_RSHIFT_ASSIGN); $$->isglobal = NODE($1)->isglobal; }
    
    | lvalue POWER_ASSIGN expr
        { $$ = new_assign(NODE($1), NODE($3), NULL, 1, lex_loc, OP_POW_ASSIGN); $$->isglobal = NODE($1)->isglobal; }
;