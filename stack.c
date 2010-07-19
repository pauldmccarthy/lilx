/**
 * Lightweight stack implementation.
 *
 * Paul McCarthy <paul.mccarthy@gmail.com>
 */
#include <stdlib.h>
#include <stdint.h>

#include "stack.h"

int8_t stack_create(stack_t *stack, uint8_t size) {
 
  stack->top = malloc(size * sizeof(void *));
  stack->capacity = size;
  stack->size = 0;
 
  if (stack->top == NULL) return 1;
  
  return 0;
}

int8_t stack_free(stack_t *stack) {
 
  stack->top -= (stack->size-1);
  free(stack->top);
 
  return 0;
}

int8_t stack_push(stack_t *stack, void *element) {
 
  if (element == NULL) return 1;
  if (stack->size >= stack->capacity) return 1;
 
  /*don't increment top if the stack is empty*/
  if (stack->size > 0) stack->top++;
  *(stack->top) = element;
  stack->size ++;
 
  return 0;
}

void * stack_pop(stack_t *stack) {
 
  void * element;
 
  if (stack->size == 0 || stack->size > stack->capacity) return NULL;
 
  element = *(stack->top--);
  stack->size--;
 
  return element;
}

void * stack_peek(stack_t *stack) {
 
  if (stack->size == 0 || stack->size > stack->capacity) return NULL;
 
  return *(stack->top);
}
