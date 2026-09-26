list_stmt:
    LBRACE args RBRACE       { $$ = new_list($2, @1); }
;

indexing:
    LSQUARE expr RSQUARE
    {
        SA::idxExpr* idx_node = new SA::idxExpr;
        idx_node->exprNode = $2;
        idx_node->depth = 1;
        idx_node->next = NULL;
        $$ = idx_node;
    }
;

index_stmt:
    expr indexing 
    { 
        $$ = new_index($1, $2, false, @1);
        $$->isglobal = $1->isglobal;
    }
;

opt_list_size:
    /* empty */                 { $$ = static_cast<size_t>(0); } 
    | SEMICOLON NUMBER          { $$ = (size_t)SA_parse_u128($2->literal.raw, NULL); }
;