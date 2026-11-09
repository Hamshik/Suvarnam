#pragma once
#include "enums.h"
#include "structs.h"
#include <vector>
#include <cstring>
#include <string>

class HIRNode;

struct Fields {
    enum class NodePtrKind {
        AST,
        HIR
    };

    union NodePointer {
        ASTNode *astNode;
        HIRNode *hirNode;

        NodePointer() : astNode(nullptr) {}
    } nodePtr;

    NodePtrKind nodePtrKind = NodePtrKind::HIR;
    std::string name;
    size_t index = 0;
    bool isMut = false;
    SA::Type *type = nullptr;
};

class HIRNode {
    public:
    ASTKind kind;
    SA::Type *type;      // Every mid-end node is strictly typed
    SA::Location loc;
    bool isglobal;
    bool isDynamic;
    bool isMutable;
    bool hasMutableInfo;

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

        struct { char *name; std::vector<struct Fields*> *fields; } structInit;
        struct { char* name; std::vector<struct Fields*> *fields; } structDec;
        struct {
            HIRNode *target;
            size_t index;
            std::string name;
        } memberAccess;
    }; 

    // Clean Constructor Initialization Tracker
    HIRNode(ASTKind k)
        : kind(k), type(nullptr), isglobal(false), isDynamic(false),
          isMutable(false), hasMutableInfo(false) {
    }
};
