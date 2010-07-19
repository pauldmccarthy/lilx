/**
 * lilx - a small DOM style parser for XML snippets.
 * 
 * Paul McCarthy <paul.mccarthy@gmail.com>
 */
#ifndef __LILX_H__
#define __LILX_H__

#include <stdint.h>

/**
 * The maximum possible size of element names and bodies, and attribute 
 * names and values.
 */
#define LILX_MAX_TOKEN_LENGTH 1000

/**
 * The maximum XML tree depth that can be parsed. Attributes are stored on 
 * the same stack, so if you expect individual elements to have lots of 
 * attributes, increase the stack size.
 */
#define LILX_STACK_SIZE 100

/**
 * Certain shitty devices (I'm not going to divulge the specific device) wrap
 * attributes in single quotes instead of double quotes. Set this to non-0 to
 * look for single quotes instead of double quotes.
 */
#define LILX_USE_SINGLE_QUOTES 0

/*******
 * Types
 ******/

struct __lilx_attribute;
struct __lilx_element;
typedef struct __lilx_attribute attribute_t;
typedef struct __lilx_element element_t;

/**
 * XML element attribute.
 */
struct __lilx_attribute {
 
  char *name;  /**< attribute name */
  char *value; /**< attribute value */
};

/**
 * XML element.
 */
struct __lilx_element {
 
  char          *name;           /**< element name                  */
  char          *body;           /**< element body, if present      */
  uint8_t        num_attributes; /**< number of attributes          */
  attribute_t ** attributes;     /**< the attributes themselves     */
  uint8_t        num_children;   /**< number of child elements      */
  element_t   ** children;       /**< the child elements themselves */
};

/*******************************
 * Tree creation and destruction
 ******************************/

/**
 * Create a DOM tree from the given text, using the given element as the root.
 * This is the public interface to the parser. It turns a string of XML into a
 * DOM tree.
 * 
 * This function is the guts of the parser, iterating through the XML
 * character by character, implementing state changes and executing state
 * change handlers.
 * 
 * \return 0 on success, non-0 on failure.
 * 
 * \note If the function succeeds, when you are done with the tree, you must
 * free it via lilx_free_tree. If the function fails, you don't need to free
 * anything.
 */
uint8_t lilx_create_tree(
  char      *raw_text,/**< the raw XML string                        */
  element_t *root     /**< pointer to an element to use as the root  */
);

/**
 * Frees the memory that has been allocated for the given tree. Does not free
 * the root element - that is your responsibility.
 * 
 * \return 0 on success, non-0 otherwise.
 */
uint8_t lilx_free_tree(
  element_t *root /**< the root of the tree to be freed */
);

/******************************
 * Tree traversal and utilities
 *****************************/

/**
 * Returns the number of elements with the given name that exist below the 
 * given (sub)tree root. If you just want to check for the existence of a
 * particular element, use this function, as it doesn't allocate, or 
 * require the allocation of any memory.
 * 
 * \return the number of elements with the given name that exist below the
 * given (sub)tree root.
 */

uint8_t lilx_count_elements_by_name(
  element_t *root, /**< root of the (sub)tree to be searched */
  char      *name  /**< element name to search for           */
);

/**
 * Recursively searches the given (sub)tree starting at \p root for elements
 * of the given \p name. Pointers to elements which are found are stored in
 * the given \p elements array.
 * 
 * \return 0 if no elements were found, otherwise the number of elements that
 * were stored in the \p elements array.
 */
uint8_t lilx_get_elements_by_name(
  element_t  *root,           /**< root of the (sub)tree to search         */
  char       *name,           /**< name of the element to search for       */
  element_t **elements,       /**< array to store pointers to the elements */
  uint8_t     elements_length /**< length of the elements array            */
);

/**
 * Searches in the given element for an attribute with the given name.
 * 
 * \return a pointer to the attribute with the given name, or NULL if there
 * was no such attribute.
 */
attribute_t * lilx_get_attribute_by_name(
  element_t *element, /**< the element to search               */
  char      *name     /**< name of the attribute to search for */
);

/**
 * Prints a representation of the given tree via printf.
 */
void lilx_print_tree(
  element_t *root /**< the root of the tree to print                */
);

#endif /* __LILX_H__ */
