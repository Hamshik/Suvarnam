structs:
    STRUCT IDENTIFIER LBRACE fields RBRACE
    {
        $$ = newStruct($2->var, $4, @1 + @5);
        ast_free($2);
    }
;

fields:
    /* empty */                         { $$ = static_cast<ASTNode *>(nullptr); }
    | field                             { $$ = $1; }
    | fields field                      { $$ = newSeq($1, $2); }
;

field:
    VAR recursive_type IDENTIFIER opt_field_default SEMICOLON
    {
        $$ = newField($2, $4, $3->var, @1 + @5);
        ast_free($3);
    }
    | VAR MUT recursive_type IDENTIFIER opt_field_default SEMICOLON
    {
        $$ = newField($3, $5, $4->var, @1 + @6);
        $$->ismut = true;
        ast_free($4);
    }
;

opt_field_default:
    /* empty */                         { $$ = static_cast<ASTNode *>(nullptr); }
    | ASSIGN expr                       { $$ = $2; }

;
