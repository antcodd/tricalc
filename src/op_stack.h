#pragma once
#include <pebble.h>
#include "tricalc.h"

#define TRICALC_OP_STACK_LEN 10
  
typedef struct {
  double val;
  struct {
    tricalc_op op;
    double a;
    double b;
  } last;
} tricalc_op_stack_entry;

typedef struct {
  tricalc_op_stack_entry head[TRICALC_OP_STACK_LEN];
  int items;
} tricalc_op_stack;

void tricalc_op_stack_init(tricalc_op_stack* stack);
bool tricalc_op_stack_push(tricalc_op_stack* stack, double result, tricalc_op op, double a, double b);
bool tricalc_op_stack_push_value(tricalc_op_stack* stack, double result);
bool tricalc_op_stack_pop_value(tricalc_op_stack* stack, double *result);
tricalc_op_stack_entry* tricalc_op_stack_get(tricalc_op_stack* stack, int i);
int tricalc_op_stack_peek2(tricalc_op_stack* stack, double results[2]);
tricalc_op_stack_entry* tricalc_op_stack_peek(tricalc_op_stack* stack);