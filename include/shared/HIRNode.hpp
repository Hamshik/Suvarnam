#pragma once
#include "enums.h"
#include "structs.h"
#include <vector>
#include <cstring>

class HIRNode {
    public:
    ASTKind kind;
    Type_t *type;      // Every mid-end node is strictly typed
    SV_Location loc;
    bool isglobal;

    // Tracks sequential statements inside a MASTKind::BLOCK node.
    // This must stay OUTSIDE the union to prevent memory corruption.
    
    union{
        std::vector<HIRNode*> *block_stmts;
        // Primitive Literals & Identifiers
        const char *name; // Reused for variable names and function targets & import paths

        // Variable Declaration (e.g., let __end: i64 = 10)
        struct {
            const char *decl_name;
            HIRNode *init_value; // Can be nullptr
        } decl;

        struct {
            SV_Value val;
        } literals;

        // Assignment (e.g., i = i + 1)
        struct {
            HIRNode *target, *value;
            bool is_declaration;
        } assign;

        // Math & Logic Ops (e.g., i < __end)
        struct {
            OP_kind_t op; // "+", "-", "<", ">=", "=="
            HIRNode *left, *right;
        } binary;

        // RANGE
        struct { HIRNode *start, *end, *step;} range;

        // Structured If/Else Engine
        struct {
            HIRNode *condition;
            HIRNode *then_branch;
            HIRNode *else_branch; // Can be nullptr
        } if_stmt;

        // THE UNIVERSAL LOOP ENGINE
        // This single structure replaces Range loops, Array loops, and For-loops!
        struct {
            HIRNode *condition;   // Simple binary check (e.g., i < __end)
            HIRNode *body;        // Sequential block containing loop instructions
            HIRNode *expr;
        } while_loop;

        // Function Calls (e.g., printlni(i))
        struct {
            const char *target_fn;
            std::vector<HIRNode*> *args;
        } call;

        struct {
            std::vector<HIRNode*> *elements;
        } element;

        struct {
            struct HIRNode* target; // The thing being indexed (e.g., the variable 'list')
            std::vector<HIRNode*>* idx;      // The position (e.g., the number '0' or expr 'i+1')
            bool islhs;
        } index;

        struct {
            std::vector<HIRNode*>* body;
            std::vector<Param_t*>* params;
            size_t param_count;
            const char* name;
        } fn;

        struct { struct HIRNode *value; } ret_stmt;
    }; 

    // Clean Constructor Initialization Tracker
    HIRNode(ASTKind k) : kind(k), type(nullptr), isglobal(false) {
    }
};
