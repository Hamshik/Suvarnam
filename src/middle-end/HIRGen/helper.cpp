#include "HIRGen/HIRGen.hpp"
#include "shared/HIRNode.hpp"
#include "shared/nodes.h"
#include "shared/structs.h"
#include <algorithm>
#include <cstring>

extern "C" {
void panic(SA_Location loc, errc_t code, const char *detail);
unsigned __int128 SA_parse_u128(const char *str, int *ok);
__int128 SA_parse_i128(const char *str, int *ok);
Type_t *make_type(DataTypes_t base, Type_t *inner);
}

SA_Value handle_num(ASTNode_t *node);

// Flattens front-end binary sequence trees into a flat vector of Mid-AST
// nodes
void HIRGenerator::flatten_sequence(ASTNode_t *node,
                                    std::vector<HIRNode *> *stmts) {
  if (!node)
    return;

  if (node->kind == AST_SEQ) {
    flatten_sequence(node->seq.a, stmts);
    flatten_sequence(node->seq.b, stmts);
  } else {
    // 1. Clear any leftover side-effects from previous statements
    side_effect_buffer.clear();

    // 2. Generate the current statement (e.g., AST_CALL for printlns)
    HIRNode *m_node = generate(node);

    // 3. 🎯 THE CRITICAL INJECTION: If generating this statement created
    // __deref temporaries, they MUST be pushed into the final statements array
    // BEFORE this statement!
    if (!side_effect_buffer.empty()) {
      for (auto *side_effect : side_effect_buffer) {
        stmts->push_back(side_effect);
      }
      side_effect_buffer.clear(); // Reset the buffer
    }

    // 4. Now safely append the main statement node that relies on those
    // temporaries
    if (m_node) {
      stmts->push_back(m_node);
    } else {
      panic(node->loc, SEM_INTERNAL_ERROR,
            "MASTGen: Failed to lower node kind");
    }
  }
}


HIRNode *HIRGenerator::emit_idx(ASTNode_t *node) {
  HIRNode *target_node = generate(node->index.target);
  
  if (target_node && target_node->type && target_node->type->base == STRINGS) {
    std::vector<HIRNode *> *call_args = new std::vector<HIRNode *>();
    call_args->push_back(target_node);

    idx_expr_t *curr_idx = node->index.idx;
    while (curr_idx) {
      if (HIRNode *lowered_idx = generate(curr_idx->expr_node)) {
        call_args->push_back(lowered_idx);
      }
      curr_idx = curr_idx->next;
    }

    HIRNode *call_node = create_call(
        "_SA_getCharAt", call_args,
        node->type ? node->type : make_type(CHARACTER, nullptr));
      
    call_node->loc = node->loc;
    return call_node;
  }

  HIRNode *index_node = new HIRNode(ASTKind::AST_INDEX);
  index_node->index.target = target_node;
  index_node->type = node->type;
  index_node->loc = node->loc;

  // Instantiate the vector allocation heap pointer
  index_node->index.idx = new std::vector<HIRNode *>();
  index_node->index.islhs = node->index.islhs;

  // 🎯 FIX: Process and translate frontend index sequences to vector layout
  // If node->index.idx is a linked list sequence of frontend AST nodes:
  idx_expr_t *curr_idx = node->index.idx;
  while (curr_idx) {
    if (HIRNode *lowered_idx = generate(curr_idx->expr_node)) {
      index_node->index.idx->push_back(lowered_idx);
    }
    curr_idx = curr_idx->next;
  }

  // 🎯 Add this ONLY if your test shows the indices are inverted:
  std::reverse(index_node->index.idx->begin(), index_node->index.idx->end());

  return index_node;
}

SA_Value handle_num(ASTNode_t *node) {
  if (!node || !node->literal.raw) {
    panic(node ? node->loc : (SA_Location){0}, RT_NUM_LITERAL_UNSUPPORTED,
          "Numeric literal missing raw string value");
    return (SA_Value){0};
  }

  DataTypes_t base =
      (node->type && node->type->base != UNKNOWN) ? node->type->base : I32;
  TypedValue v = {.type = node->type ? node->type : make_type(base, NULL)};
  const char *raw = node->literal.raw;

  switch (base) {
  case I8:
  case I16:
  case I32:
  case I64: {
    long long val = strtoll(raw, NULL, 10);

    if (base == I8)
      v.val.i8 = (int8_t)val;
    else if (base == I16)
      v.val.i16 = (int16_t)val;
    else if (base == I32)
      v.val.i32 = (int32_t)val;
    else
      v.val.i64 = (int64_t)val;

    break;
  }

  case U8:
  case U16:
  case U32:
  case U64: {
    unsigned long long val = strtoull(raw, NULL, 10);

    if (base == U8)
      v.val.u8 = (uint8_t)val;
    else if (base == U16)
      v.val.u16 = (uint16_t)val;
    else if (base == U32)
      v.val.u32 = (uint32_t)val;
    else
      v.val.u64 = (uint64_t)val;

    break;
  }

  case I128:
  case U128: {
    int ok = 0;
    if (base == I128)
      v.val.i128 = SA_parse_i128(raw, &ok);
    else
      v.val.u128 = SA_parse_u128(raw, &ok);

    if (!ok) {
      panic(node->loc, RT_NUM_LITERAL_UNSUPPORTED,
            "Failed to parse 128-bit literal");
      return (SA_Value){0};
    }
    break;
  }

  case F32:
  case UF32:
    v.val.f32 = strtof(raw, NULL);
    break;

  case F64:
  case UF64:
    v.val.f64 = strtod(raw, NULL);
    break;

  case F128:
  case UF128:
    v.val.f128 = strtold(raw, NULL);
    break;

  default:
    panic(node->loc, RT_NUM_LITERAL_UNSUPPORTED,
          "Unsupported numeric base type");
    return (SA_Value){0};
  }

  return v.val;
}

HIRNode *HIRGenerator::create_var(ASTNode_t *node) {
  HIRNode *m_node = new HIRNode(ASTKind::AST_VAR);
  m_node->name = strdup(node->var);
  m_node->type = node->type;
  m_node->isglobal = node->isglobal;
  m_node->loc = node->loc;
  return m_node;
}

bool HIRGenerator::is_param(const std::string& name) const {
  return current_params.find(name) != current_params.end();
}
