#include "MAST-Gen/Mast_gen.hpp"
#include "shared/M_node.hpp"
#include "shared/structs.h"
#include <algorithm>
#include <cstring>

extern "C"{
void panic(SV_Location loc, errc_t code, const char *detail);
unsigned __int128 SV_parse_u128(const char *str, int *ok);
__int128 SV_parse_i128(const char *str, int *ok);
Type_t* make_type(DataTypes_t base, Type_t* inner);
}

SV_Value handle_num(ASTNode_t *node);
// Flattens front-end binary sequence trees into a flat vector of Mid-AST
// nodes
void MASTGenerator::flatten_sequence(ASTNode_t *node,
                                     std::vector<MASTNode *> *stmts) {
  if (!node)
    return;
  if (node->kind == AST_SEQ) {
    flatten_sequence(node->seq.a, stmts);
    flatten_sequence(node->seq.b, stmts);
  } else {
    MASTNode *m_node = generate(node);
    if (m_node)
      stmts->push_back(m_node);
  }
}

MASTNode *MASTGenerator::emit_call(ASTNode_t *node) {
  MASTNode *call_node = new MASTNode(ASTKind::AST_CALL);
  call_node->call.target_fn = strdup(node->call.name);

  // 🎯 FIX: Capture by reference (&) to directly modify the actual object
  call_node->call.args = new std::vector<MASTNode *>();
  auto args = call_node->call.args;
  flatten_sequence(node->call.args, args);

  call_node->type = node->type;
  return call_node;
}

MASTNode *MASTGenerator::emit_idx(ASTNode_t *node) {
  MASTNode *index_node = new MASTNode(ASTKind::AST_INDEX);
  index_node->index.target = generate(node->index.target);
  index_node->type = node->type;

  // Instantiate the vector allocation heap pointer
  index_node->index.idx = new std::vector<MASTNode *>();
  index_node->index.islhs = node->index.islhs;

  // 🎯 FIX: Process and translate frontend index sequences to vector layout
  // If node->index.idx is a linked list sequence of frontend AST nodes:
  idx_expr_t *curr_idx = node->index.idx;
  while (curr_idx) {
    if (MASTNode *lowered_idx = generate(curr_idx->expr_node)) {
      index_node->index.idx->push_back(lowered_idx);
    }
    curr_idx = curr_idx->next;
  }

  // 🎯 Add this ONLY if your test shows the indices are inverted:
  std::reverse(index_node->index.idx->begin(), index_node->index.idx->end());

  return index_node;
}

SV_Value handle_num(ASTNode_t *node) {
  if (!node || !node->literal.raw) {
    panic( node ? node->loc : (SV_Location){0}, RT_NUM_LITERAL_UNSUPPORTED,
          "Numeric literal missing raw string value");
    return (SV_Value){0};
  }
  
  DataTypes_t base = node->type ? node->type->base : UNKNOWN;
  TypedValue v = {0};
  
  // If semantic analysis didn't assign a type, default to I32 for literals.
  // This prevents the interpreter from failing on simple indices like [0].
  if (base == UNKNOWN) base = I32;

  switch (base) {
  case I8:
    v.val.i8 = (int8_t)strtol(node->literal.raw, NULL, 10);
    break;
  case I16:
    v.val.i16 = (short)strtol(node->literal.raw, NULL, 10);
    break;
  case I32:
    v.val.i32 = (int)strtol(node->literal.raw, NULL, 10);
    break;
  case I64:
    v.val.i64 = (int64_t)strtoll(node->literal.raw, NULL, 10);
    break;
  case I128: {
    int ok = 0;
    v.val.i128 = SV_parse_i128(node->literal.raw, &ok);
    if (!ok) {
      panic( node->loc, RT_NUM_LITERAL_UNSUPPORTED,
            NULL);
      return (SV_Value){0};
    }
    break;
  }
  case U8:
    v.val.u8 = (uint8_t)strtoul(node->literal.raw, NULL, 10);
    break;
  case U16:
    v.val.u16 = (uint16_t)strtoul(node->literal.raw, NULL, 10);
    break;
  case U32:
    v.val.u32 = (uint32_t)strtoul(node->literal.raw, NULL, 10);
    break;
  case U64:
    v.val.u64 = (uint64_t)strtoull(node->literal.raw, NULL, 10);
    break;
  case U128: {
    int ok = 0;
    v.val.u128 = SV_parse_u128(node->literal.raw, &ok);
    if (!ok) {
      panic( node->loc, RT_NUM_LITERAL_UNSUPPORTED,
            NULL);
      return (SV_Value){0};
    }
    break;
  }
  case F32:
    v.val.f32 = strtof(node->literal.raw, NULL);
    break;
  case F64:
    v.val.f64 = strtod(node->literal.raw, NULL);
    break;
  case F128:
    v.val.f128 = strtold(node->literal.raw, NULL);
    break;
  case UF32:
    v.val.f32 = strtof(node->literal.raw, NULL);
    break;
  case UF64:
    v.val.f64 = strtod(node->literal.raw, NULL);
    break;
  case UF128:
    v.val.f128 = strtold(node->literal.raw, NULL);
    break;
  default:
    panic( node->loc, RT_NUM_LITERAL_UNSUPPORTED,
          NULL);
    return (SV_Value){0};
  }

  // Ensure we return a valid type object. If the node had no type,
  // we create a temporary I32 type so the rest of the eval logic works.
  if (node->type && node->type->base != UNKNOWN) {
      v.type = node->type;
  } else {
      v.type = make_type(base, NULL);
  }

  return v.val;
}