range:
      expr DOT_DOT expr 
        { $$ = newRange($1, $3, NULL, false); }
    | expr DOT_DOT expr DOT_DOT expr 
        { $$ = newRange($1, $3, $5, false); }
    | expr DOT_DOT ASSIGN expr 
        { $$ = newRange($1, $4, NULL, 1); }
    | expr DOT_DOT ASSIGN expr DOT_DOT expr 
        { $$ = newRange($1, $4, $6, 1); }
;

for_stmt:
    FOR IDENTIFIER IN with_non_expr block
    {
        $$ = newFor($2->var, $4, $5, @1 + @5, false);
        ast_free($2);
    }
    | FOR MUT IDENTIFIER IN with_non_expr block
    { 
        $$ = newFor($3->var, $5, $6, @1 + @6, true); 
        ast_free($3);
    }
    | FOR range block
    {
        $$ = newFor("__SA temp idx__", $2, $3, @1 + @3, false);
    }

    | FOR IDENTIFIER IN with_non_expr COLON expr_stmt
    {
        $$ = newFor($2->var, $4, $6, @1 + @6, false);
        ast_free($2);
    }
    | FOR MUT IDENTIFIER IN with_non_expr COLON expr_stmt
    { 
        $$ = newFor($3->var, $5, $7, @1 + @7, true); 
        ast_free($3);
    }
    | FOR range COLON expr_stmt
    {
        $$ = newFor("__SA temp idx__", $2, $4, @1 + @4, false);
    }

;

while_stmt:
    WHILE expr block
        { $$ = newWhilw($2, $3, NULL, @1 + @3); }
    | WHILE expr COLON assign_expr block
    {
        if($4->assign.op == OP_ASSIGN)
            panic(@4, PARSE_SYNTAX, "with_non_expr expects operational assignment not just plain assign");
        $$ = newWhilw($2, $5, $4, @1 + @5);
    }
    | WHILE expr COLON expr_stmt
        { $$ = newWhilw($2, $4, NULL, @1 + @4); } 
    | WHILE expr COLON assign_expr COLON expr_stmt
    {
        if($4->assign.op == OP_ASSIGN)
            panic(@4, PARSE_SYNTAX, "with_non_expr expects operational assignment not just plain assign");
        $$ = newWhilw($2, $6, $4, @1 + @6);    
    }
;
