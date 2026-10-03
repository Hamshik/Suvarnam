list_stmt:
    LSQUARE args RSQUARE       { $$ = newList($2, @1 + @3); }
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
        $$ = newIdx($1, $2, false, @1 + @2);
        $$->isglobal = $1->isglobal;
    }
;

opt_list_size:
    /* empty */                 { $$ = static_cast<size_t>(0); } 
    | SEMICOLON NUMBER          { $$ = (size_t)SA_parse_u128($2->literal.raw, NULL); }
;