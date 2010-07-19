#include <stdio.h>

#include "lilx.h"

char *testxml = "<people>\n\
 <person>\n\
  <name>Joey Joe Joe Shabidou</name>\n\
  <occupation>Sherpa</occupation>\n\
 </person>\n\
 <person>\n\
  <name>Lionel Hutz</name>\n\
  <occupation>Ambulance chaser</occupation>\n\
 </person>\n\
</people>";

int main (int argc, char *argv[]) {

  element_t root;

  printf("testing liix with this XML snippet:\n");
  printf("%s\n\n", testxml);

  if (lilx_create_tree(testxml, &root)) {
    printf("parse failed :(\n");
    return 1;
  }
  printf("parse succeeded! results:\n\n");

  lilx_print_tree(&root);
  
  lilx_free_tree(&root);

  return 0;
}
