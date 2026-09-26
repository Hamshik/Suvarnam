fn_block_t:
    SEMICOLON                   { $$ = static_cast<ASTNode *>(nullptr); }
    | block                     { $$ = $1; }
;

fn_def:
    FN recursive_type IDENTIFIER LPAREN opt_params RPAREN fn_block_t
    {
        $$ = new_fn_def($3->var, $5.params, $5.count, $2, $7, @1);
        ast_free($3);
    }
  | FN IDENTIFIER LPAREN opt_params RPAREN fn_block_t
    {
        $$ = new_fn_def($2->var, $4.params, $4.count, NULL, $6, @1);
        ast_free($2);
    } 
;

opt_params:
    /* empty */                 
    { 
        SA::ParamList res{};
        res.params = static_cast<SA::Param*>(nullptr);
        res.count = 0;
        $$ = res; 
    }
    | params                    { $$ = $1; }
;

params:
    param {
        SA::ParamList res;
        res.count = 1;
        res.params = (SA::Param*)calloc(1, sizeof(SA::Param));
        res.params[0].name = strdup($1->var);
        res.params[0].type = $1->type;
        res.params[0].is_variadic = $1->is_variadic;
        ast_free($1);
        $$ = res;
    }
  | param COMMA params {
        SA::ParamList res;
        res.count = $3.count + 1;
        res.params = (SA::Param*)calloc((size_t)res.count, sizeof(SA::Param));
        res.params[0].name = strdup($1->var);
        res.params[0].type = $1->type;
        res.params[0].is_variadic = $1->is_variadic;
        ast_free($1);
        for (int i = 0; i < $3.count; i++) res.params[i + 1] = $3.params[i];
        if ($3.params) free($3.params);
        $$ = res;
    }
;

param:
    VAR MUT recursive_type param_tail {
        $4->type = $4->is_variadic ? new SA::Type(LIST, $3) : $3;
        $4->type->ismut = true;
        $4->ismut = true;
        $$ = $4;
    }
    | VAR recursive_type param_tail {
        $3->type = $3->is_variadic ? new SA::Type(LIST, $2) : $2;
        $3->type->ismut = false;
        $3->ismut = false;
        $$ = $3;
    }
    | recursive_type param_tail {
        $2->type = $2->is_variadic ? new SA::Type(LIST, $1) : $1;
        $2->type->ismut = false;
        $2->ismut = false;
        $$ = $2;
    }
;

param_tail:
    IDENTIFIER {
        $1->is_variadic = false;
        $$ = $1;
    }
    | ELLIPSIS IDENTIFIER {
        $2->is_variadic = true;
        $$ = $2;
    }
;

return_stmt:
    RETURN with_non_expr                 { $$ = new_return($2, @1); }
    | RETURN                    { $$ = new_return(NULL, @1); }
;

opt_args:
    /* empty */                 { $$ = static_cast<ASTNode *>(nullptr); }
    | args                      { $$ = $1; }
;

args:
    with_non_expr                        { $$ = $1; }
    | with_non_expr COMMA args           { $$ = new_seq($1, $3); }
;