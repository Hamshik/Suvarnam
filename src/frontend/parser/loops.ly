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

for_iterable:
    IDENTIFIER                           { $$ = $1; } %prec FOR_ITERABLE_END
    | list_stmt
    | range
    | IDENTIFIER LPAREN opt_args RPAREN
    {
        $$ = newFnCall($1->var, $3, @1 + @4);
        ast_free($1);
    }
    | LPAREN expr RPAREN                { $$ = $2; }
;

for_stmt:
    FOR IDENTIFIER IN for_iterable body
    {
        $$ = newFor($2->var, $4, $5, @1 + @5, false);
        ast_free($2);
    }
    | FOR MUT IDENTIFIER IN for_iterable body
    {
        $$ = newFor($3->var, $5, $6, @1 + @6, true);
        ast_free($3);
    }
;

whileAssign:
    standAloneExpr
    | assign_expr
;

while_stmt:
    WHILE expr body
        { $$ = newWhilw($2, $3, NULL, @1 + @3); }
    | WHILE expr COLON whileAssign body
    {
        $$ = newWhilw($2, $5, $4, @1 + @5);
    }
;
