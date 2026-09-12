expr:
    NUMBER                      { $$ = $1; }
    | IDENTIFIER                { $$ = $1; }
    | STRING_LITERAL            { $$ = $1; }
    | CHAR_LITERAL              { $$ = $1; }
    | BOOL_LITERAL              { $$ = $1; }

    | expr PLUS expr            { $$ = new_binop($1, $3, @1, OP_ADD); }
    | expr MINUS expr           { $$ = new_binop($1, $3, @1, OP_SUB); }
    | expr STAR expr            { $$ = new_binop($1, $3, @1, OP_MUL); }
    | expr SLASH expr           { $$ = new_binop($1, $3, @1, OP_DIV); }
    | expr MOD expr             { $$ = new_binop($1, $3, @1, OP_MOD); }
    | expr POWER expr           { $$ = new_binop($1, $3, @1, OP_POW); }

    | expr LSHIFT expr          { $$ = new_binop($1, $3, @1, OP_LSHIFT); }
    | expr RSHIFT expr          { $$ = new_binop($1, $3, @1, OP_RSHIFT); }

    | expr AMP expr             { $$ = new_binop($1, $3, @1, OP_BITAND); }
    | expr BITXOR expr          { $$ = new_binop($1, $3, @1, OP_BITXOR); }
    | expr PIPE expr            { $$ = new_binop($1, $3, @1, OP_BITOR); }

    | expr AND expr             { $$ = new_binop($1, $3, @1, OP_AND); }
    | expr OR expr              { $$ = new_binop($1, $3, @1, OP_OR); }

    | expr EQ expr              { $$ = new_binop($1, $3, @1, OP_EQ); }
    | expr NEQ expr             { $$ = new_binop($1, $3, @1, OP_NEQ); }
    | expr LT expr              { $$ = new_binop($1, $3, @1, OP_LT); }
    | expr LE expr              { $$ = new_binop($1, $3, @1, OP_LE); }
    | expr GT expr              { $$ = new_binop($1, $3, @1, OP_GT); }
    | expr GE expr              { $$ = new_binop($1, $3, @1, OP_GE); }

    | AMP expr %prec AMP   
    { 
        $$ = new_unop($2, @1, OP_ADDR); 
        $$->unop.operand->ismut = false;
    }
    
    | AMP MUT expr %prec AMP   
    { 
        $$ = new_unop($3, @1, OP_ADDR); 
        $$->unop.operand->ismut = true;
    }

    | STAR expr %prec PLUS      { $$ = new_unop($2, @1, OP_DEREF); }
    
    | PLUS expr %prec PLUS      { $$ = new_unop($2, @1, OP_POS); }
    | MINUS expr %prec MINUS    { $$ = new_unop($2, @1, OP_NEG); }
    | NOT expr                   { $$ = new_unop($2, @1, OP_NOT); }
    | BITNOT expr                { $$ = new_unop($2, @1, OP_BITNOT); }

    | IDENTIFIER INC %prec INC  { $$ = new_unop($1, @1, OP_INC); $$->isglobal = $1->isglobal;}
    | IDENTIFIER DEC %prec INC  { $$ = new_unop($1, @1, OP_DEC); $$->isglobal = $1->isglobal; }

    | LPAREN expr RPAREN         { $$ = $2; }
    | IDENTIFIER LPAREN opt_args RPAREN
      {
          $$ = new_fn_call($1->var, $3, @1);
          ast_free($1);
      }

    | list_stmt                  { $$ = $1; } 
    | index_stmt                 { $$ = $1; $$->isglobal = $1->isglobal;}
    | LBRACE range RBRACE        { $$ = $2; }
;