#pragma once
#include "shared/enums.h"
#include "structs.h"
#include <vector>

class MASTNode {
    public:
    ASTKind kind;
    Type_t *type;      // Every mid-end node is strictly typed

    union {
        // Primitive Literals & Identifiers
        const char *name; // Reused for variable names and function targets

        // Variable Declaration (e.g., let __end: i64 = 10)
        struct {
            const char *name;
            MASTNode *init_value; // Can be nullptr
        } decl;

        struct {
            SV_Value val;
        } literals;

        // Assignment (e.g., i = i + 1)
        struct {
            const char *target;
            MASTNode *value;
        } assign;

        // Math & Logic Ops (e.g., i < __end)
        struct {
            OP_kind_t op; // "+", "-", "<", ">=", "=="
            MASTNode *left;
            MASTNode *right;
        } binary;

        // Structured If/Else Engine
        struct {
            MASTNode *condition;
            MASTNode *then_branch;
            MASTNode *else_branch; // Can be nullptr
        } if_stmt;

        // THE UNIVERSAL LOOP ENGINE
        // This single structure replaces Range loops, Array loops, and For-loops!
        struct {
            MASTNode *condition;   // Simple binary check (e.g., i < __end)
            MASTNode *body;        // Sequential block containing loop instructions
        } while_loop;

        // Function Calls (e.g., printlni(i))
        struct {
            const char *target_fn;
            std::vector<MASTNode*> *args;
        } call;

        struct {
            std::vector<MASTNode*> elements;
        } element;

        struct {
            struct MASTNode* target; // The thing being indexed (e.g., the variable 'list')
            std::vector<MASTNode*> *idx;      // The position (e.g., the number '0' or expr 'i+1')
            bool islhs;
        } index;

        std::vector<MASTNode*> block_stmts;
    };

    // Tracks sequential statements inside a MASTKind::BLOCK node

    // Clean Constructor Initialization Tracker
    MASTNode(ASTKind k) : kind(k) {
        // Initialize union pointers to prevent UB
        if (k == ASTKind::AST_CALL) {
            call.args = nullptr;
        }
        kind = k;
    }
};