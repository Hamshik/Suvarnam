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
      FOR LPAREN IDENTIFIER IN expr RPAREN expr_stmt
    {
        $$ = new_for($3->var, $5, $7, @1, false);
        ast_free($3);
    }
    | FOR LPAREN MUT IDENTIFIER IN expr RPAREN expr_stmt
    { 
        $$ = new_for($4->var, $6, $8, @1, true); 
        ast_free($4);
    }
    | FOR LPAREN range RPAREN expr_stmt
    {
        $$ = new_for("__SA temp idx__", $3, $5, @1, false);
    }
;

while_stmt:
    WHILE LPAREN expr RPAREN expr_stmt
        { $$ = new_while($3, $5, NULL, @1); }
    | WHILE LPAREN expr RPAREN COLON LPAREN assignment RPAREN expr_stmt
    {
        if($7->assign.op == OP_ASSIGN)
            panic(@7, PARSE_SYNTAX, "expr expects operational assignment not just plain assign");
        $$ = new_while($3, $9, $7, @1);
    }
;
