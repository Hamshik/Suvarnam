#pragma once
#include "enums.h"
#include "structs.h"
#include <vector>
#include <cstring>

class HIRNode {
    public:
    ASTKind_t kind;
    SA::Type *type;      // Every mid-end node is strictly typed
    SA::Location loc;
    bool isglobal;

    // Tracks sequential statements inside a MASTKind::BLOCK node.
    // This must stay OUTSIDE the union to prevent memory corruption.
    
    union{
        std::vector<HIRNode*> *blockStmts;
        // Primitive Literals & Identifiers
        const char *name; // Reused for variable names and function targets & import paths
        
        SA::Value val;

        // Assignment (e.g., i = i + 1)
        struct {
            HIRNode *target, *value;
            bool isDec;
        } assign;

        // Math & Logic Ops (e.g., i < __end)
        struct {
            OP_kind_t op; // "+", "-", "<", ">=", "=="
            HIRNode *left, *right; //right can be nullptr,so treating it as unop instead
        } binary;

        // RANGE
        struct { HIRNode *start, *end, *step;} range;

        // Structured If/Else Engine
        struct {
            HIRNode *cond, *thenBranch, *elseBranch; // Can be nullptr
        } ifStmt;

        // THE UNIVERSAL LOOP ENGINE
        // This single structure replaces Range loops, Array loops, and For-loops!
        struct {
            HIRNode *cond;   // Simple binary check (e.g., i < __end)
            HIRNode *body;        // Sequential block containing loop instructions
            HIRNode *expr;
        } whileLoop;

        // Function Calls (e.g., printlni(i))
        struct {
            const char *targetFb;
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
            std::vector<SA::Param*>* params;
            size_t paramCount;
            const char* name;
        } fn;

        struct { struct HIRNode *value; } ret;
    }; 

    // Clean Constructor Initialization Tracker
    HIRNode(ASTKind_t k) : kind(k), type(nullptr), isglobal(false) {
    }
};
