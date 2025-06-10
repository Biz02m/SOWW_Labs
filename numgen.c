#include "numgen.h"
#include <stdlib.h>

#define SEED 12345678

unsigned int numgen(unsigned int count, unsigned long int dest[])
{

  unsigned int i = 0;

  srandom(SEED);

  while(count--) {
    dest[i++] = random();
  }

  return i;
}

