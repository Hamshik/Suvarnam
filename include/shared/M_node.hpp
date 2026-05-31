#pragma once
#include "enums.h"
#include "structs.h"
#include <vector>
#include <cstring>

class MASTNode {
    public:
    ASTKind kind;
    Type_t *type;      // Every mid-end node is strictly typed
    SV_Location loc;
    bool isglobal;

    // Tracks sequential statements inside a MASTKind::BLOCK node.
    // This must stay OUTSIDE the union to prevent memory corruption.
    
    union{
        std::vector<MASTNode*> *block_stmts;
        // Primitive Literals & Identifiers
        const char *name; // Reused for variable names and function targets & import paths

        // Variable Declaration (e.g., let __end: i64 = 10)
        struct {
            const char *decl_name;
            MASTNode *init_value; // Can be nullptr
        } decl;

        struct {
            SV_Value val;
        } literals;

        // Assignment (e.g., i = i + 1)
        struct {
            MASTNode *target, *value;
            bool is_declaration;
        } assign;

        // Math & Logic Ops (e.g., i < __end)
        struct {
            OP_kind_t op; // "+", "-", "<", ">=", "=="
            MASTNode *left, *right;
        } binary;

        // RANGE
        struct { MASTNode *start, *end, *step;} range;

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
            MASTNode *expr;
        } while_loop;

        // Function Calls (e.g., printlni(i))
        struct {
            const char *target_fn;
            std::vector<MASTNode*> *args;
        } call;

        struct {
            std::vector<MASTNode*> *elements;
        } element;

        struct {
            struct MASTNode* target; // The thing being indexed (e.g., the variable 'list')
            std::vector<MASTNode*>* idx;      // The position (e.g., the number '0' or expr 'i+1')
            bool islhs;
        } index;

        struct {
            std::vector<MASTNode*>* body;
            std::vector<Param_t*>* params;
            size_t param_count;
            const char* name;
        } fn;

        struct { struct MASTNode *value; } ret_stmt;
    }; 

    // Clean Constructor Initialization Tracker
    MASTNode(ASTKind k) : kind(k), type(nullptr), isglobal(false) {
    }
};
