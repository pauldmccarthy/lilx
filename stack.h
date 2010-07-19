/**
 * Lightweight stack implementation.
 *
 * Paul McCarthy <paul.mccarthy@gmail.com>
 */
#ifndef __STACK_H__
#define __STACK_H__

#include <stdint.h>

/**
 * Struct representing a stack; the fields should never be accessed 
 * directly; rather, use the stack functions below.
 */
typedef struct __stack {
 
  uint8_t capacity; /**< the maximum capacity of this stack */
  uint8_t size;     /**< the current size of this stack     */ 
  void  **top;      /**< pointer to the top of this stack   */
} stack_t;

/**
 * Mallocs enough memory for a stack of the given size.
 * 
 * \return 0 on success, 1 on malloc failure.
 */
int8_t stack_create(
  stack_t *stack, /**< pointer to a stack     */
  uint8_t  size   /**< the desired stack size */
);

/**
 * Frees the memory that was allocated for the given stack.
 * 
 * \return 0. 
 */
int8_t stack_free(
  stack_t *stack /**< the stack */
);

/**
 * Pushes the given element on to the top of the stack.
 * 
 * \return 0 on success, 1 on failure.
 */
int8_t stack_push(
  stack_t *stack,  /**< the stack                             */
  void    *element /**< the element to push  - cannot be NULL */
);

/**
 * Removes and returns the element that is on top of the stack.
 * 
 * \return a pointer to the popped element, NULL on failure.
 */
void * stack_pop(
  stack_t *stack /**< the stack */
);

/**
 * Returns the element that is on top of the stack, but does not remove it
 * from the stack.
 * 
 * \return the top element of the given stack, NULL on failure.
 */
void * stack_peek(
  stack_t *stack /**< the stack */
);

#endif /* __STACK_H__ */
