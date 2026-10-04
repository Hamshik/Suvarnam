structs:
    STRUCT IDENTIFIER LBRACE fields RBRACE
    {
        $$ = newStruct($2->var, $4, @1 + @5);
        ast_free($2);
    }
;

struct_init:
    IDENTIFIER LBRACE struct_init_fields RBRACE
    {
        $$ = newStructInit($1->var, $3, @1 + @4);
        ast_free($1);
    }
;

struct_init_fields:
    /* empty */                         { $$ = null(ASTNode*); }
    | struct_init_field                { $$ = $1; }
    | struct_init_fields COMMA struct_init_field
                                        { $$ = newSeq($1, $3); }
    | struct_init_fields COMMA         { $$ = $1; }
;

struct_init_field:
    IDENTIFIER COLON expr
    {
        $$ = newField(nullptr, $3, $1->var, @1 + @3);
        ast_free($1);
    }
    | expr                              { $$ = $1; }
;

fields:
    field                               { $$ = $1; }
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
    /* empty */                         { $$ = null(ASTNode*); }
    | ASSIGN expr                       { $$ = $2; }

;
