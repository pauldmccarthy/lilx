/**
 * lilx - a small DOM style parser for XML snippets.
 *
 * Paul McCarthy <paul.mccarthy@gmail.com>
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

#include "lilx.h"
#include "stack.h"

/*uncomment for debug output*/
/*#define __LILX_DEBUG*/

/*****************************
 * Private function prototypes
 ****************************/

/**
 * Quick way to initialise an element_t struct to default values.
 */
static void __lilx_init_element(
  element_t *element /**< the element to initialise */
);

/**
 * Compares the given raw xml with the given transition. If the xml conforms
 * to the transition, 0 is returned, and the token size is stored in the given
 * offset pointer. This offset equates to the amount of characters that should
 * be skipped over in the raw xml when processing resumes.
 * 
 * \return 0 if there is a match, non-0 otherwise.
 */
static uint8_t __lilx_compare(
  char    *xml,        /**< the raw xml string                  */
  char    *transition, /**< the string against which to compare */
  uint8_t *offset      /**< place to store size on a match      */
);

/**
 * Given the current state, and the XML input, figures out the next state.
 * 
 * The current_state parameter should contain the current state; if a state
 * change is detected, the new state will be stored in this field.
 * 
 * If a state change is detected, the number of characters that should be
 * skipped forward over the XML input is stored in the offset parameter.
 * 
 * If a state change is detected, the transition string that caused the change
 * is stored in the transition parameter.
 * 
 * \return 0 if the a state change was detected, 1 for no state change.
 */
static uint8_t __lilx_get_next_state(
  uint8_t *current_state, /**< current state, place to store new state      */
  char    *xml,           /**< raw XML input                                */
  uint8_t *offset,        /**< place to store chars to skip on state change */
  char   **transition     /**< place to store transition on state change    */
);

/**
 * When we find a new element in the XML, we create an element_t struct, and
 * push it on to the stack. If there is an element already on the stack, the
 * new element is added to it as a child before being pushed on to the
 * stack. If the transition indicates that the element is self-closing, it is
 * not pushed on to the stack.
 * 
 * \return 0 on success, non-0 on failure (malloc or stack_push can fail).
 */
static uint8_t __lilx_elem_name_start_action(
  stack_t *stack,     /**< the processing stack                        */
  char    *tkn,       /**< the element name                            */
  char    *transition /**< the transition which ended the element name */
);

/**
 * When we find an end tag in the XML, make sure the element on top of the
 * stack has the same name as the end tag element, and then pop that element
 * from the stack.
 * 
 * \return 0 on success, 1 on failure.
 */
static uint8_t __lilx_elem_name_end_action(
  stack_t *stack,     /**< the processing stack                   */
  char    *tkn,       /**< the element name                       */
  char    *transition /**< the transition which ended the end tag */
);

/**
 * When we find an attribute in the XML, create an attribute_t struct, and add
 * it to the element on top of the stack, then push the attribute on to the
 * stack.
 * 
 * \return 0 on success, non-0 on failure.
 */
static uint8_t __lilx_attr_name_action(
  stack_t *stack,     /**< the processing stack                          */
  char    *tkn,       /**< the attribute name                            */
  char    *transition /**< the transition which ended the attribute name */
);

/**
 * When we find an attribute value, add it to the attribute on top of the
 * stack, and then pop that attribute from the stack.
 * 
 * \return 0 on success, non-0 on failure.
 */
static uint8_t __lilx_attr_val_action(
  stack_t *stack,     /**< the processing stack                           */
  char    *tkn,       /**< the attribute value                            */
  char    *transition /**< the transition which ended the attribute value */
);

/**
 * When we find an element body, add that body to the element on top of the
 * stack.
 * 
 * \return 0 on success, non-0 on failure.
 */
static uint8_t __lilx_elem_action(
  stack_t *stack,     /**< the processing stack                        */
  char    *tkn,       /**< the element body                            */
  char    *transition /**< the transition which ended the element body */
);

/**
 * Called when a comment token is found. Does nothing.
 * 
 * \return 0.
 */
static uint8_t __lilx_comment_action(
  stack_t *stack,     /**, the processing stack                         */
  char    *tkn,       /**< the comment body                             */
  char    *transition /**< the transition which ended the comment block */
);

/**
 * No-op. This function should never actually be executed.
 * 
 * \return 0.
 */
static uint8_t __lilx_end_action(
  stack_t *stack,     /**< the processing stack                */
  char    *tkn,       /**< not relevant                        */
  char    *transition /**< the transition that ended the input */
);

/**
 * Recursively frees the memory that has been allocated for the given 
 * subtree.
 * 
 * \return 0 on success, non-0 otherwise.
 */
static uint8_t __lilx_free_tree(
  element_t *element, /**< the root element of the subtree to free     */
  uint8_t    is_root  /**< is this element the root of the entire tree */
);

/**
 * Adds the given child element as a child of the given parent element.
 * 
 * \return 0 on success, non-0 on failure.
 */
static uint8_t __lilx_add_child(
  element_t *parent, /**< the parent element */
  element_t *child   /**< the child element  */
);

/**
 * Adds the given attribute to the given element.
 * 
 * \return 0 on success, non-0 on failure.
 */
static uint8_t __lilx_add_attr(
  element_t   *element, /**< the element   */
  attribute_t *attr     /**< the attribute */
);

/**
 * Recursively prints the given DOM tree via printf.
 */
static void __lilx_print_tree(
  element_t *root, /**< Root of the tree                   */
  uint8_t    depth /**< Current depth - pass in 0 to start */
);

/****************
 * Private fields
 ***************/

/**
 * Total number of states
 */
#define NUM_STATES 7

/**
 * The states that can exist during parsing.
 */
enum {
 
  ELEM_NAME_START = 0, /**< Inside a starting (or self closing) tag */
  ELEM_NAME_END   = 1, /**< Inside an ending tag                    */
  ATTR_NAME       = 2, /**< Inside an attribute name                */
  ATTR_VAL        = 3, /**< Inside an attribute value               */
  ELEM            = 4, /**< Inside an element body                  */
  COMMENT         = 5, /**< Inside a comment body                   */
  END             = 6  /**< At the end of the document              */
};

/**
 * Action handlers. When a state transition is encountered, the handler for
 * the current state is executed. The handler is found by indexing into 
 * this array.
 */
static uint8_t (*__lilx_actions[NUM_STATES])
               (stack_t *stack, char *tkn, char *transition) = {
 
  &__lilx_elem_name_start_action,
  &__lilx_elem_name_end_action,
  &__lilx_attr_name_action,
  &__lilx_attr_val_action,
  &__lilx_elem_action,
  &__lilx_comment_action,
  &__lilx_end_action
};

/**
 * State transition table. This table contains a bunch of strings which,
 * when encountered in the xml input, will trigger a state change. Note 
 * that a state can change to itself (e.g. "<one><two></two></one>" 
 * contains a state change from ELEM_NAME_START to itself, and another 
 * state change from ELEM_NAME_END to itself). 
 * 
 * Also note that more than one transition string is allowed for one state
 * transition.
 * 
 * All the characters in the transition strings represent themselves, 
 * except for the following:
 * 
 *   * a == One alphanumeric character (i.e. [a-zA-Z0-9])
 *   * A == One character which is alphanumeric, or contained in 
 *          XML_BODY_CHARS (see below)
 *   * S == Exactly one whitespace character (i.e. [ \r\n\t])
 *   * s == Any number of whitespace characters (i.e. [ \r\n\t]*)
 *   * 0 == end of file (i.e. $)
 */

/**
 * Characters that element bodies, attribute values, and comment bodies are
 * allowed to start with.
 */
#define XML_BODY_CHARS "!@#$%^&*()-_=+[{]}\\/|;:,.?"

/**
 * Number of different transition strings per state transition. If you want to
 * change this to, for example 3, you have to make sure that every element in
 * the table is a 3 field array. ugly, i know. go complain to someone who
 * gives a damn
 */
#define STATE_CHOICES 2

static char *__lilx_transitions[NUM_STATES][NUM_STATES][STATE_CHOICES] = {
 
  /*ELEM_NAME_START*/
  { 
    {"s>s<a", "s/>s<a"}, {"s>s</a","s/>s</a"}, {"Ssa",NULL}, 
    {NULL,NULL}, {"s>sA",NULL}, {"s>s<!--sA", "s/>s<!--sA"},
    {"s/>s0",NULL}
  }, 
  /*ELEM_NAME_END*/
  { 
    {"s>s<a",NULL}, {"s>s</a",NULL}, {NULL,NULL}, {NULL,NULL}, 
    {"s>sA",NULL}, {"s>s<!--", NULL}, {"s>s0",NULL}
  },
  #if XML_USE_SINGLE_QUOTES == 0
  /*ATTR_NAME*/
  { 
    {NULL,NULL}, {NULL,NULL}, {NULL,NULL}, {"=\"sA",NULL}, {NULL,NULL},
    {NULL,NULL}, {NULL,NULL}
  },
  /*ATTR_VAL*/
  { 
    {"\"s>s<a","\"s/>s<a"}, {"\"s>s</a","\"s/>s</a"}, {"\"Ssa",NULL},
    {NULL,NULL}, {"\"s>A","\"s/>sA"}, {"\"s>s<!--sA", "\"s/>s<!--sA"},
    {"\"s/>s0",NULL}
  },
  #else
  /*ATTR_NAME*/
  { 
    {NULL,NULL}, {NULL,NULL}, {NULL,NULL}, {"='sA",NULL}, {NULL,NULL},
    {NULL,NULL}, {NULL,NULL}
  },
  /*ATTR_VAL*/
  {
    {"'s>s<a","'s/>s<a"}, {"'s>s</a","'s/>s</a"}, {"'Ssa",NULL},
    {NULL,NULL}, {"'s>A","'s/>sA"}, {"'s>s<!--sA", "'s/>s<!--sA"}, 
    {"'s/>s0",NULL}
  },
  #endif
  /*ELEM*/
  { 
    {"s<a",NULL}, {"s</a",NULL}, {NULL,NULL}, {NULL,NULL}, {NULL,NULL},
    {"<!--sA",NULL}, {NULL,NULL}
  },

  /*COMMENT*/
  { 
    {"-->s<a",NULL}, {"-->s</a",NULL}, {NULL,NULL}, {NULL,NULL}, 
    {"-->sA",NULL}, {"-->s<!--sA",NULL}, {NULL,NULL}
  },
  /*END*/
  { 
    {NULL,NULL}, {NULL,NULL}, {NULL,NULL}, {NULL,NULL}, {NULL,NULL},
    {NULL,NULL}, {NULL,NULL}
  }
};

/****************************
 * Public interface functions
 ***************************/

uint8_t lilx_create_tree(char *xml, element_t *root) {
 
  uint8_t state, next_state;
  uint8_t offset;
  uint16_t len;
 
  stack_t stack;
 
  uint16_t tknidx = 0;
  char *transition;
  char *token;
 
  __lilx_init_element(root);
 
  /*save xml length; make sure xml starts with '<'*/
  len = strlen(xml);
  if (xml[0] != '<') return 1;
  xml++;
 
  /*create stack*/
  if (stack_create(&stack, LILX_STACK_SIZE) != 0) return 1;
 
  /*malloc space for saving xml tokens*/
  token = (char *)malloc(LILX_MAX_TOKEN_LENGTH);
  if (token == NULL) {
    stack_free(&stack);
    return 1;
  }
 
  /*initialise root element*/
  __lilx_init_element(root);
  root->name = (char *)malloc(strlen("root") + 1);
  if (root->name == NULL) {
    stack_free(&stack);
    free(token);
    return 1;
  }
  strcpy(root->name, "root");
 
  /*push root element on to stack*/
  if (stack_push(&stack, root) != 0) {
    stack_free(&stack);
    free(token);
    free(root->name);
    return 1;
  }
 
  /*initialise state, and begin parsing*/
  state = ELEM_NAME_START;
  while (state != END && (*xml) != '\0') {
  
    #ifdef __LILX_DEBUG
    printf("in state %u: (%s)\n", state, xml);
    #endif
  
    next_state = state;
  
    /*if there is no state change, save the current character 
      in the current token and move on to the next character*/
    if (__lilx_get_next_state(&next_state, xml, &offset, &transition) != 0) {
   
      if (tknidx == LILX_MAX_TOKEN_LENGTH) {
    
        #ifdef __LILX_DEBUG
        printf("token is too big - aborting\n");
        #endif
        break;
      }
   
      #ifdef __LILX_DEBUG
      printf("adding %c to token\n", *xml);
      #endif
   
      token[tknidx++] = *xml;
      xml++;
    }
  
    /*if there is a state change, execute the appropriate action
      handler, skip over the xml input by the offset specified by
      the handler, and change the current state*/
    else {
   
      token[tknidx] = '\0';
      tknidx = 0;
   
      #ifdef __LILX_DEBUG
      printf("%u: %s -> %u (%s)\n", 
        state, token, next_state, transition);
      #endif
   
      /*bail immediately if the action returns an error code*/
      if (__lilx_actions[state](&stack, token, transition) != 0) break;
   
      xml += offset;
      state = next_state;
    }
  }
 
  stack_free(&stack);
  free(token);
 
  /*if state != END or stack size is not 
    1, it means that parsing failed.*/
  if (state != END || stack.size != 1) {
  
    lilx_free_tree(root);
    return 1;
  }
 
  return 0;
}

uint8_t lilx_free_tree(element_t *root) {
  return __lilx_free_tree(root, 1);
}

uint8_t lilx_count_elements_by_name(element_t *root, char *name) {
 
  uint8_t i;
  uint8_t count = 0;
 
  /*if the given element has the name, add 1*/
  if (strcmp(root->name, name) == 0) count++;
 
  /*recursively count the rest of the tree - this is the 
    terminating case, as leaf nodes will have no children*/
  for (i = 0; i < root->num_children; i++) 
    count += lilx_count_elements_by_name(root->children[i], name);
 
  return count;
}

uint8_t lilx_get_elements_by_name(
element_t *root, char *name, element_t **elements, uint8_t elements_length) {
 
  uint8_t i;
  uint8_t temp;
  uint8_t found = 0;
   
  if (elements_length == 0) return 0;
 
  /*does this element match the name?*/
  if (strcmp(root->name, name) == 0) {
  
    *elements = root;
    elements++;
    found++;
    elements_length--;
  }
 
  /*recursively search each of this element's children*/
  for (i = 0; i < root->num_children; i++) {
  
    temp = lilx_get_elements_by_name(
      root->children[i], name, elements, elements_length);
  
    found += temp;
    elements += temp;
    elements_length -= temp;
  }
 
  return found;
}

attribute_t * lilx_get_attribute_by_name(element_t *element, char *name) {
 
  uint8_t i;
  uint8_t len = strlen(name);
 
  for (i = 0; i < element->num_attributes; i++)
  
  if (strncmp(element->attributes[i]->name, name, len) == 0)
    return element->attributes[i];
 
  return NULL;
}

void lilx_print_tree(element_t *root) {
  __lilx_print_tree(root, 0);
}

/*******************
 * Private functions
 ******************/

void __lilx_init_element(element_t *element) {
  element->name = NULL;
  element->body = NULL;
  element->children = NULL;
  element->attributes = NULL;
  element->num_children = 0;
  element->num_attributes = 0;
}

uint8_t __lilx_compare(char *xml, char *transition, uint8_t *offset) {
 
  #ifdef __LILX_DEBUG
  printf("cmp (%s), (%s)\n", transition, xml);
  #endif
 
  (*offset) = 0;
  while (*transition != '\0') {
  
    switch (*transition) {

      case 'A':
        /*if not printing char, fall through 
          to 'a' for alphanumeric test*/
        if ( strchr(XML_BODY_CHARS, *xml) != NULL
     
          /*ugly hack - strchr passes when given '\0'*/
          && *xml != '\0') 
          break;
    
      case 'a':
        /*if not alphanumeric, fail*/
        if (isalnum(*xml) == 0) return 1;
        break;
   
      case 'S':
        /*if not whitespace, fail*/
        if (isspace(*xml) == 0) return 1;
        break;

      case 's':
        /*if char is whitespace, stay on the 's' in 
          the transition, but move the xml forward*/
        if (isspace(*xml) != 0) 
          transition--;
    
        /*otherwise keep xml at the current char, 
          but move the transition forward*/
        else {
          (*offset)--;
          xml--;
        }
        break;
    
      case '0':
        /*if char is not the end of the input, fail*/
        if (*xml != '\0') return 1;
        break;
    
      default:
        /*if transition doesn't match xml, fail*/
        if (*transition != *xml) return 1;
        break;
    }
  
    (*offset)++;
    xml++;
    transition++;
  }
 
  (*offset)--;
  return 0;
}

uint8_t __lilx_get_next_state(
uint8_t *current_state, char *xml, uint8_t *offset, char **transition) {
 
  uint8_t i, j, tranlen, temp_offset;
  int8_t last = -1;
  uint8_t state = *current_state;
  char *tran;
 
  /*Iterate through all of the possible 
    state changes for the current state*/
  for (i = 0; i < NUM_STATES; i++) {
    for (j = 0; j < STATE_CHOICES; j++) {
   
      tran = __lilx_transitions[state][i][j];
      if (tran == NULL) continue;
   
      tranlen = strlen(tran);
   
      /*have we found a transition?*/
      if (__lilx_compare(xml, tran, &temp_offset) == 0) {
    
        #ifdef __LILX_DEBUG
        printf("matched (%i,%i)\n", i, j);
        #endif
    
        /*in case of more than one transition matching, we want
          the transition that matches the most characters (i.e.
          the transition string with the longest length)*/
        if (tranlen > last) {
     
          *transition = tran;
          (*offset) = temp_offset;
          last = tranlen;
          *current_state = i;
        }
      }
    }
  }
 
  if (last == -1) return 1;
  return 0;
}

/*****************
 * Action handlers
 ****************/

uint8_t __lilx_elem_name_start_action(
stack_t *stack, char *tkn, char *transition) {
 
  element_t *element, *parent;
 
  #ifdef __LILX_DEBUG
  printf("elem_name_start_action %s (%s)\n", tkn, transition);
  #endif
 
  /*malloc space for a new element*/
  element = (element_t *)malloc(sizeof(element_t));
  if (element == NULL) return 1;
 
  /*initialise the element fields*/
  __lilx_init_element(element);
 
  /*malloc space for the element name*/
  element->name = (char *)malloc(strlen(tkn) + 1);
  if (element->name == NULL) {
  
    free(element);
    return 1;
  }
  strcpy(element->name, tkn);
 
  /*is there an element already on the stack? if so, add 
    the new element as a child of the existing element*/
  parent = (element_t *)stack_peek(stack);
 
  if (parent != NULL && __lilx_add_child(parent, element) != 0) {
  
    free(element->name);
    free(element);
    return 1;
  }
 
  /*is the element self closing? if so, don't push it on to the stack*/
  if (strstr(transition, "/>") != NULL) return 0;
 
  /*push the element on the stack*/
  if (stack_push(stack, (void *)element) != 0) {
  
    free(element->name);
    free(element);
    return 1;
  }
  return 0;
}

uint8_t __lilx_elem_name_end_action(
stack_t *stack, char *tkn, char *transition) {
 
  element_t *element;
 
  #ifdef __LILX_DEBUG
  printf("elem_name_end_action %s (%s)\n", tkn, transition);
  #endif
 
  element = stack_peek(stack);
 
  /*no element on stack, or stack corrupt?*/
  if (element == NULL) return 1;
 
  /*if the element on top of the stack doesn't match the current closing
    tag, either the XML is invalid, or the stack is corrupt*/
  if (strncmp(element->name, tkn, strlen(tkn)) != 0) return 1;
 
  /*the element is closed - pop it from the stack*/
  if (stack_pop(stack) != element) return 1;
 
  return 0;
}

uint8_t __lilx_attr_name_action(
stack_t *stack, char *tkn, char *transition) {
 
  attribute_t *attr;
  element_t *element;
 
  #ifdef __LILX_DEBUG
  printf("attr_name_action %s (%s)\n", tkn, transition);
  #endif
 
  /*malloc space for the new attribute and initialise its fields*/
  attr = (attribute_t *)malloc(sizeof(attribute_t));
  if (attr == NULL) return 1;
  attr->name = NULL;
  attr->value = NULL;
 
  /*malloc space for the attribute name*/
  attr->name = (char *)malloc(strlen(tkn) + 1);
  if (attr->name == NULL) {
  
    free(attr);
    return 1;
  }
  strcpy(attr->name, tkn);
 
  /*get the attribute's parent from the stack*/
  element = (element_t *)stack_peek(stack);
  if (element == NULL || __lilx_add_attr(element, attr) != 0) {
  
    free(attr->name);
    free(attr);
    return 1;
  }
 
  /*push the attribute on to the stack*/
  if (stack_push(stack, (void *)attr) != 0) {
    free(attr->name);
    free(attr);
    return 1;
  }
 
  return 0;
}

uint8_t __lilx_attr_val_action(
stack_t *stack, char *tkn, char *transition) {
 
  attribute_t *attr;
 
  #ifdef __LILX_DEBUG
  printf("attr_val_action %s (%s)\n", tkn, transition);
  #endif
 
  /*get the attribute on top of the stack*/
  attr = (attribute_t *)stack_peek(stack);
  if (attr == NULL) return 1;
 
  /*weak check that the top of the stack is an attribute (the
    fields are initialised to null when the attribute is created)*/
  if (attr->value != NULL) return 1;
 
  /*malloc space for the attribute value */
  attr->value = (char *)malloc(strlen(tkn) + 1);
  if (attr->value == NULL) return 1;
  strcpy(attr->value, tkn);
 
  /*pop the attribute from the stack*/
  if (stack_pop(stack) != attr) {
  
    free(attr->value);
    return 1;
  }
 
  /*if the transition indicates that the  element is self closing, 
    we need to pop the element from the stack*/
  if (strstr(transition, "/>") != NULL && stack_pop(stack) == NULL) {
  
    free(attr->value);
    return 1;
  }
 
  return 0;
}

uint8_t __lilx_elem_action(
stack_t *stack, char *tkn, char *transition) {
 
  element_t *element;
 
  #ifdef __LILX_DEBUG
  printf("elem_action %s (%s)\n", tkn, transition);
  #endif
 
  /*get the element on top of the stack*/
  element = (element_t *)stack_peek(stack);
  if (element == NULL) return 1;
 
  /*malloc space for the element body*/
  if (element->body != NULL) free(element->body);
  element->body = (char *)malloc(strlen(tkn) + 1);
  if (element->body == NULL) return 1;
  strcpy(element->body, tkn);
 
  return 0;
}

uint8_t __lilx_comment_action(
stack_t *stack, char *tkn, char *transition) {
 
  #ifdef __LILX_DEBUG
   printf("comment_action %s:(%s)\n", tkn, transition);
  #endif
 
  return 0;
}

uint8_t __lilx_end_action(
stack_t *stack, char *tkn, char *transition) {
 
  #ifdef __LILX_DEBUG
  printf("end_action %s (%s)\n", tkn, transition);
  #endif
 
  return 0;
}

/*******************
 * Utility functions
 ******************/

uint8_t __lilx_free_tree(element_t *element, uint8_t is_root) {
 
  /*just in case*/
  if (element == NULL) return 0;
 
  /*free the element name and body*/
  if (element->name != NULL) free(element->name);
  if (element->body != NULL) free(element->body);
 
  /*free the element's attributes and the attribute array*/
  for (; element->num_attributes > 0; element->num_attributes--) {
    free(element->attributes[element->num_attributes-1]->name);
    free(element->attributes[element->num_attributes-1]->value);
    free(element->attributes[element->num_attributes-1]);
  }
  if (element->attributes != NULL) free(element->attributes);
 
  /*recursively free the children - this is the terminating 
    case, as leaf elements will have no children*/
  for (; element->num_children > 0; element->num_children--)
    __lilx_free_tree(element->children[element->num_children-1], 0);
 
  /*free children array*/
  if (element->children != NULL) free(element->children);
 
  /*if this is the actual root of the entire tree, don't free it - 
    that's the caller's responsibility*/
  if (is_root == 0) free(element);
 
  return 0;
}

uint8_t __lilx_add_child(element_t *parent, element_t *child) {
 
  /*we are creating a new child list, copying the parent's old list
    over to the new one, appending the new child to the list, and then
    replacing the parent's old list with the new one*/
 
  int i = 0;
 
  /*allocate space for the parent's new child list*/
  element_t **new_child_list = (element_t **)
  malloc( sizeof(element_t *) * (parent->num_children + 1) );
 
  if (new_child_list == NULL) return 1;
 
  /*copy the old list across*/
  if (parent->children != NULL) {
    for (i = 0; i < parent->num_children; i++)
      new_child_list[i] = parent->children[i];
  }
 
  /*add the new child*/
  new_child_list[i] = child;
 
  /*replace the parent's old list with the new list*/
  if (parent->children != NULL) free(parent->children);
  parent->children = new_child_list;
  parent->num_children ++;
 
  return 0;
}

uint8_t __lilx_add_attr(element_t *element, attribute_t *attr) {
 
  /*same concept as described in __lilx_add_child*/
 
  int i = 0;
 
  /*allocate space for element's new attr list*/
  attribute_t **new_attr_list = (attribute_t **)
  malloc( sizeof(attribute_t *) * (element->num_attributes + 1) );
 
  if (new_attr_list == NULL) return 1;
 
  /*copy the old list across*/
  if (element->attributes != NULL) {
    for (i = 0; i < element->num_attributes; i++)
      new_attr_list[i] = element->attributes[i];
  }
 
  /*add the new child*/
  new_attr_list[i] = attr;
 
  /*replace the element's old list with the new list*/
  if (element->attributes != NULL) free(element->attributes);
  element->attributes = new_attr_list;
  element->num_attributes ++;
 
  return 0;
}

static void __lilx_print_tree(element_t *root, uint8_t depth) {

  int i;
  char *prefix = (char *)malloc( depth + 2 );
  if (prefix == NULL) {
    printf("couldn't print tree\n");
    return;
  }
 
  /*based on depth in tree, print some spaces at start of every line*/
  for (i = 0; i < depth + 1; i++) {
    prefix[i] = ' ';
  }
  prefix[i] = '\0';
 
  /*print element name, followed by attributes and body*/
  printf("%s%s ", prefix, root->name);
 
  for (i = 0; i < root->num_attributes; i++) 
    printf("(%s=%s) ", root->attributes[i]->name, 
                       root->attributes[i]->value);
  if (root->body != NULL) printf("%s", root->body);
  printf("\n");
 
  /*recursively descend into tree - this is the 
    terminating case, as leaf nodes have no children*/
  for (i = 0; i < root->num_children; i++)
    __lilx_print_tree(root->children[i], depth+1);
 
  free(prefix);
}
