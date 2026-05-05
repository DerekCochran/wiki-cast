#include <stdio.h>
#include "stringzilla/stringzilla.h"
int main(void){
  printf("sz_dynamic_dispatch()=%d\n", sz_dynamic_dispatch());
  printf("sz_capabilities=%s\n", sz_capabilities_to_string(sz_capabilities()));
  return 0;
}
