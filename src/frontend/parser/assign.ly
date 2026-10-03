assignment:
    /* Explicitly list IDENTIFIER for declarations to prevent shift/reduce ambiguity */
    VAR recursive_type IDENTIFIER ASSIGN expr {
        ASTNode* id = $3;
        $$ = newAssign(id, $5, $2, false, @1 + @5, OP_ASSIGN);
        $$->assign.isDec = true;
    }
    | VAR MUT recursive_type IDENTIFIER ASSIGN expr {
        ASTNode* id = $4;
        $$ = newAssign(id, $6, $3, true, @1 + @6, OP_ASSIGN);
        $$->assign.isDec = true;
    }
    | VAR IDENTIFIER ASSIGN expr {
        ASTNode* id = $2;
        $$ = newAssign(id, $4, NULL, false, @1 + @4, OP_ASSIGN);
        $$->assign.isDec = true;
    }
    | VAR MUT IDENTIFIER ASSIGN expr {
        ASTNode* id = $3;
        $$ = newAssign(id, $5, NULL, true, @1 + @5, OP_ASSIGN);
        $$->assign.isDec = true;
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

assign_expr:
    expr assign_op with_non_expr {
        OP_kind_t op = $2;
        if ($1->kind == AST_INDEX)
            $1->index.islhs = true;
        $$ = newAssign($1, $3, nullptr, false, @1 + @3, op);
    }
;