// #include "eval/eval.h"
// #include "shared/structs.h"
// #include "utils/uhash.h"

// SV_Value default_step(DataTypes_t type) {
//   SV_Value step = {0};
//   switch (type) {
//   case I8:
//     step.i8 = 1;
//     break;
//   case I16:
//     step.i16 = 1;
//     break;
//   case I32:
//     step.i32 = 1;
//     break;
//   case I128:
//     step.i128 = 1;
//     break;
//   case U8:
//     step.u8 = 1;
//     break;
//   case U16:
//     step.u16 = 1;
//     break;
//   case U32:
//     step.u32 = 1;
//     break;
//   case U64:
//     step.u64 = 1;
//     break;
//   case U128:
//     step.u128 = 1;
//     break;
//   case F32:
//     step.f32 = 1.0f;
//     break;
//   case F64:
//     step.f64 = 1.0;
//     break;
//   case F128:
//     step.f128 = 1.0L;
//     break;
//   case UF32:
//     step.f32 = 1.0f;
//     break;
//   case UF64:
//     step.f64 = 1.0;
//     break;
//   case UF128:
//     step.f128 = 1.0L;
//     break;
//   default:
//     fprintf(stderr, "for loop supports numeric init types only\n");
//     exit(EXIT_FAILURE);
//   }
//   return step;
// }

// bool step_is_positive(DataTypes_t type, SV_Value step) {
//   switch (type) {
//   case I8:
//     return step.i8 > 0;
//   case I16:
//     return step.i16 > 0;
//   case I32:
//     return step.i32 > 0;
//   case I128:
//     return step.i128 > 0;
//   case U8:
//     return step.u8 > 0;
//   case U16:
//     return step.u16 > 0;
//   case U32:
//     return step.u32 > 0;
//   case U64:
//     return step.u64 > 0;
//   case U128:
//     return step.u128 > 0;
//   case F32:
//     return step.f32 > 0;
//   case F64:
//     return step.f64 > 0;
//   case F128:
//     return step.f128 > 0;
//   case UF32:
//     return step.f32 > 0;
//   case UF64:
//     return step.f64 > 0;
//   case UF128:
//     return step.f128 > 0;
//   default:
//     return 0;
//   }
// }

// bool step_is_zero(DataTypes_t type, SV_Value step) {
//   switch (type) {
//   case I8:
//     return step.i8 == 0;
//   case I16:
//     return step.i16 == 0;
//   case I32:
//     return step.i32 == 0;
//   case I128:
//     return step.i128 == 0;
//   case U8:
//     return step.u8 == 0;
//   case U16:
//     return step.u16 == 0;
//   case U32:
//     return step.u32 == 0;
//   case U64:
//     return step.u64 == 0;
//   case U128:
//     return step.u128 == 0;
//   case F32:
//     return fabsf(step.f32) < 1e-12f;
//   case F64:
//     return fabs(step.f64) < 1e-12;
//   case F128:
//     return fabsl(step.f128) < 1e-18L;
//   case UF32:
//     return fabsf(step.f32) < 1e-12f;
//   case UF64:
//     return fabs(step.f64) < 1e-12;
//   case UF128:
//     return fabsl(step.f128) < 1e-18L;
//   default:
//     return 1;
//   }
// }

// bool should_continue_for(DataTypes_t type, SV_Value cur, SV_Value end,
//                          SV_Value step) {
//   if (step_is_positive(type, step)) {
//     switch (type) {
//     case I8:
//       return cur.i8 < end.i8;
//     case I16:
//       return cur.i16 < end.i16;
//     case I32:
//       return cur.i32 < end.i32;
//     case I128:
//       return cur.i128 < end.i128;
//     case U8:
//       return cur.u8 < end.u8;
//     case U16:
//       return cur.u16 < end.u16;
//     case U32:
//       return cur.u32 < end.u32;
//     case U64:
//       return cur.u64 < end.u64;
//     case U128:
//       return cur.u128 < end.u128;
//     case F32:
//       return cur.f32 < end.f32;
//     case F64:
//       return cur.f64 < end.f64;
//     case F128:
//       return cur.f128 < end.f128;
//     case UF32:
//       return cur.f32 < end.f32;
//     case UF64:
//       return cur.f64 < end.f64;
//     case UF128:
//       return cur.f128 < end.f128;
//     default:
//       return 0;
//     }
//   }

//   switch (type) {
//   case I8:
//     return cur.i8 > end.i8;
//   case I16:
//     return cur.i16 > end.i16;
//   case I32:
//     return cur.i32 > end.i32;
//   case I128:
//     return cur.i128 > end.i128;
//   case U8:
//     return cur.u8 > end.u8;
//   case U16:
//     return cur.u16 > end.u16;
//   case U32:
//     return cur.u32 > end.u32;
//   case U64:
//     return cur.u64 > end.u64;
//   case U128:
//     return cur.u128 > end.u128;
//   case F32:
//     return cur.f32 > end.f32;
//   case F64:
//     return cur.f64 > end.f64;
//   case F128:
//     return cur.f128 > end.f128;
//   case UF32:
//     return cur.f32 > end.f32;
//   case UF64:
//     return cur.f64 > end.f64;
//   case UF128:
//     return cur.f128 > end.f128;
//   default:
//     return 0;
//   }
// }

// SV_Value add_step_for(DataTypes_t type, SV_Value cur, SV_Value step) {
//   SV_Value next = cur;
//   switch (type) {
//   case I8:
//     next.i8 = (int8_t)(next.i8 + step.i8);
//     break;
//   case I16:
//     next.i16 += step.i16;
//     break;
//   case I32:
//     next.i32 += step.i32;
//     break;
//   case I128:
//     next.i128 += step.i128;
//     break;
//   case U8:
//     next.u8 = (uint8_t)(next.u8 + step.u8);
//     break;
//   case U16:
//     next.u16 = (uint16_t)(next.u16 + step.u16);
//     break;
//   case U32:
//     next.u32 += step.u32;
//     break;
//   case U64:
//     next.u64 += step.u64;
//     break;
//   case U128:
//     next.u128 += step.u128;
//     break;
//   case F32:
//     next.f32 += step.f32;
//     break;
//   case F64:
//     next.f64 += step.f64;
//     break;
//   case F128:
//     next.f128 += step.f128;
//     break;
//   case UF32:
//     next.f32 += step.f32;
//     break;
//   case UF64:
//     next.f64 += step.f64;
//     break;
//   case UF128:
//     next.f128 += step.f128;
//     break;
//   default:
//     fprintf(stderr, "for loop supports numeric init types only\n");
//     exit(EXIT_FAILURE);
//   }
//   return next;
// }

// TypedValue eval_for(ASTNode_t *node, bool g_returning, TypedValue g_return_value) {
//   if (!node->fornode.init || node->fornode.init->kind != AST_ASSIGN ||
//       node->fornode.init->assign.lhs->kind != AST_VAR ||
//       node->fornode.init->assign.op != OP_ASSIGN) {
//     panic( node->loc, RT_FOR_INIT_INVALID, NULL);
//     return (TypedValue){0};
//   }

//   ast_eval(node->fornode.init);
//   DataTypes_t loop_type = node->fornode.init->type->base;
//   const char *loop_name = node->fornode.init->assign.lhs->var;

//   TypedValue endt = SV_cast_typed(ast_eval(node->fornode.end), node->fornode.init->type);
//   SV_Value endv_cast = endt.val;
//   TypedValue stept =
//       node->fornode.step
//           ? SV_cast_typed(ast_eval(node->fornode.step), node->fornode.init->type)
//           : (TypedValue){.type = node->fornode.init->type, .val = default_step(loop_type)};
//   SV_Value stepv = stept.val;

//   if (step_is_zero(loop_type, stepv)) {
//     panic( node->loc, RT_FOR_STEP_ZERO, NULL);
//     return (TypedValue){0};
//   }

//   TypedValue last = {0};

//   while (should_continue_for(
//       loop_type,
//       SV_runtime_env_get(loop_name, node->fornode.init->type, node->loc),
//       endv_cast, stepv)) {
//     last = ast_eval(node->fornode.body);
//     if (g_returning)
//       return g_return_value;
//     SV_Value cur = SV_runtime_env_get(loop_name, node->fornode.init->type, node->loc);
//     SV_Value next = add_step_for(loop_type, cur, stepv);
//     SV_runtime_env_set(loop_name, &next, node->fornode.init->type);
//   }

//   return last;
// }