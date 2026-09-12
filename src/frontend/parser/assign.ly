lvalue:
      IDENTIFIER                { $$ = $1; }
    | index_stmt                { $$ = $1; $$->index.islhs = 1; $$->isglobal = $1->isglobal; }
    | STAR expr                 { $$ = new_unop($2, @1, OP_DEREF); $$->isglobal = $2->isglobal; }
;

assignment:
    /* Explicitly list IDENTIFIER for declarations to prevent shift/reduce ambiguity */
    VAR recursive_type IDENTIFIER ASSIGN expr {
        ASTNode_t* id = $3;
        $$ = new_assign(id, $5, $2, false, @1, OP_ASSIGN);
        $$->assign.is_declaration = 1;
    }
    | VAR MUT recursive_type IDENTIFIER ASSIGN expr {
        ASTNode_t* id = $4;
        $$ = new_assign(id, $6, $3, 1, @1, OP_ASSIGN);
        $$->assign.is_declaration = 1;
    }
    | VAR IDENTIFIER ASSIGN expr {
        ASTNode_t* id = $2;
        $$ = new_assign(id, $4, NULL, false, @1, OP_ASSIGN);
        $$->assign.is_declaration = 1;
    }
    | VAR MUT IDENTIFIER ASSIGN expr {
        ASTNode_t* id = $3;
        $$ = new_assign(id, $5, NULL, 1, @1, OP_ASSIGN);
        $$->assign.is_declaration = 1;
    }
;

assign_op:
      ASSIGN        { $$ = OP_ASSIGN; }
    | PLUS_ASSIGN   { $$ = OP_PLUS_ASSIGN; }
    | MINUS_ASSIGN  { $$ = OP_MINUS_ASSIGN; }
    | STAR_ASSIGN   { $$ = OP_MUL_ASSIGN; }
    | SLASH_ASSIGN  { $$ = OP_DIV_ASSIGN; }
    | MOD_ASSIGN    { $$ = OP_MOD_ASSIGN; }
    | LSHIFT_ASSIGN { $$ = OP_LSHIFT_ASSIGN; }
    | RSHIFT_ASSIGN { $$ = OP_RSHIFT_ASSIGN; }
    | POWER_ASSIGN  { $$ = OP_POW_ASSIGN; }
;