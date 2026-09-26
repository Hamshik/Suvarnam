range:
      expr DOT_DOT expr 
        { $$ = new_range($1, $3, NULL, false); }
    | expr DOT_DOT expr DOT_DOT expr 
        { $$ = new_range($1, $3, $5, false); }
    | expr DOT_DOT ASSIGN expr 
        { $$ = new_range($1, $4, NULL, 1); }
    | expr DOT_DOT ASSIGN expr DOT_DOT expr 
        { $$ = new_range($1, $4, $6, 1); }
;

for_stmt:
      FOR IDENTIFIER IN with_non_expr block
    {
        $$ = new_for($2->var, $4, $5, @1, false);
        ast_free($2);
    }
    | FOR MUT IDENTIFIER IN with_non_expr block
    { 
        $$ = new_for($3->var, $5, $6, @1, true); 
        ast_free($3);
    }
    | FOR range block
    {
        $$ = new_for("__SA temp idx__", $2, $3, @1, false);
    }
;

while_stmt:
    WHILE expr block
        { $$ = new_while($2, $3, NULL, @1); }
    | WHILE expr COLON assign_expr block
    {
        if($4->assign.op == OP_ASSIGN)
            panic(@4, PARSE_SYNTAX, "with_non_expr expects operational assignment not just plain assign");
        $$ = new_while($2, $4, $5, @1);
    }
;
