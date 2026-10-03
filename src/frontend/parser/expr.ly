with_non_expr:
    range                      { $$ = $1; }
    | expr                     { $$ = $1; } %prec ASSIGN
;

expr:
    NUMBER                      { $$ = $1; }
    | IDENTIFIER                { $$ = $1; }
    | STRING_LITERAL            { $$ = $1; }
    | CHAR_LITERAL              { $$ = $1; }
    | BOOL_LITERAL              { $$ = $1; }

    | expr PLUS expr            { $$ = newBinop($1, $3, @1 + @3, OP_ADD); }
    | expr MINUS expr           { $$ = newBinop($1, $3, @1 + @3, OP_SUB); }
    | expr STAR expr            { $$ = newBinop($1, $3, @1 + @3, OP_MUL); }
    | expr SLASH expr           { $$ = newBinop($1, $3, @1 + @3, OP_DIV); }
    | expr MOD expr             { $$ = newBinop($1, $3, @1 + @3, OP_MOD); }
    | expr POWER expr           { $$ = newBinop($1, $3, @1 + @3, OP_POW); }

    | expr LSHIFT expr          { $$ = newBinop($1, $3, @1 + @3, OP_LSHIFT); }
    | expr RSHIFT expr          { $$ = newBinop($1, $3, @1 + @3, OP_RSHIFT); }

    | expr AMP expr             { $$ = newBinop($1, $3, @1 + @3, OP_BITAND); }
    | expr BITXOR expr          { $$ = newBinop($1, $3, @1 + @3, OP_BITXOR); }
    | expr PIPE expr            { $$ = newBinop($1, $3, @1 + @3, OP_BITOR); }

    | expr AND expr             { $$ = newBinop($1, $3, @1 + @3, OP_AND); }
    | expr OR expr              { $$ = newBinop($1, $3, @1 + @3, OP_OR); }

    | expr EQ expr              { $$ = newBinop($1, $3, @1 + @3, OP_EQ); }
    | expr NEQ expr             { $$ = newBinop($1, $3, @1 + @3, OP_NEQ); }
    | expr LT expr              { $$ = newBinop($1, $3, @1 + @3, OP_LT); }
    | expr LE expr              { $$ = newBinop($1, $3, @1 + @3, OP_LE); }
    | expr GT expr              { $$ = newBinop($1, $3, @1 + @3, OP_GT); }
    | expr GE expr              { $$ = newBinop($1, $3, @1 + @3, OP_GE); }

    | AMP expr %prec AMP        { $$ = newUnop($2, @1 + @2, OP_ADDR); }

    | STAR expr %prec PLUS      { $$ = newUnop($2, @1 + @2, OP_DEREF); }
    
    | PLUS expr %prec PLUS      { $$ = newUnop($2, @1 + @2, OP_POS); }
    | MINUS expr %prec MINUS    { $$ = newUnop($2, @1 + @2, OP_NEG); }
    | NOT expr                  { $$ = newUnop($2, @1 + @2, OP_NOT); }
    | BITNOT expr               { $$ = newUnop($2, @1 + @2, OP_BITNOT); }

    | IDENTIFIER INC %prec INC  { $$ = newUnop($1, @1, OP_INC); $$->isglobal = $1->isglobal;}
    | IDENTIFIER DEC %prec INC  { $$ = newUnop($1, @1, OP_DEC); $$->isglobal = $1->isglobal; }

    | LPAREN expr RPAREN         { $$ = $2; }
    | IDENTIFIER LPAREN opt_args RPAREN
      {
          $$ = newFnCall($1->var, $3, @1 + @4);
          ast_free($1);
      }

    | list_stmt                  { $$ = $1; } 
    | index_stmt                 { $$ = $1; $$->isglobal = $1->isglobal;}
;