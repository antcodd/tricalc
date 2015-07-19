#include <pebble.h>
#include "op_stack.h"
  
bool tricalc_op_stack_push(tricalc_op_stack* stack, double result, tricalc_op op, double a, double b) {
  tricalc_op_stack_entry* item = NULL;
  
  if (stack && stack->items < TRICALC_OP_STACK_LEN)
  { 
    item = &stack->head[stack->items];
    item->val = result;
    item->last.op = op;
    item->last.a = a;
    item->last.b = b;

    stack->items++;
    
    return true;
  }
  
  return false;
}

bool tricalc_op_stack_push_value(tricalc_op_stack* stack, double result) {
  return tricalc_op_stack_push(stack, result, OP_NUM_OPS, result, result);
}

bool tricalc_op_stack_pop_value(tricalc_op_stack* stack, double *result) {
  tricalc_op_stack_entry* item = NULL;
  
  if (stack && stack->items > 0 && stack->items <= TRICALC_OP_STACK_LEN) {
    item = &stack->head[stack->items-1];
    if (result)
      *result = item->val;
    
    stack->items--;
    
    return true;
  }
  
  return false;
}

tricalc_op_stack_entry* tricalc_op_stack_get(tricalc_op_stack* stack, int i) {
  if (stack && stack->items > i+1 && stack->items <= TRICALC_OP_STACK_LEN)
    return &stack->head[i];
  
  return NULL;
}

tricalc_op_stack_entry* tricalc_op_stack_peek(tricalc_op_stack* stack) {
  if (stack && stack->items > 0 && stack->items <= TRICALC_OP_STACK_LEN)
    return &stack->head[stack->items-1];
  
  return NULL;
}

int tricalc_op_stack_peek2(tricalc_op_stack* stack, double results[2]) {
  int count = 0;
  if (stack) {
    if (stack->items >= 2) {
      results[0] = stack->head[stack->items-1].val;
      results[1] = stack->head[stack->items-2].val;
      count = 2;
    } else if (stack->items == 1) {
      results[0] = stack->head[stack->items-1].val;
      count = 1;
    }
  }
  
  return count;
}

void tricalc_op_stack_init(tricalc_op_stack* stack) {
  if (stack) {
    //memset(stack->head, 0, sizeof(stack->head));
    stack->items = 0;
  }
}