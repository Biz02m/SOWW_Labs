#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <omp.h>
#include "numgen.c"
#include <math.h>

// funkcja sprawdzająca, czy liczba jest pierwsza
int isPrime(unsigned long x) {
  if (x <= 1) return 0;
  if (x == 2) return 1;
  unsigned long limit = (unsigned long)sqrt((double)x);
  for (unsigned long i = 2; i <= limit; i++) {
      if (x % i == 0) return 0;
  }
  return 1;
}

int main(int argc,char **argv) {


  Args ins__args;
  parseArgs(&ins__args, &argc, argv);

  //set number of threads
  omp_set_num_threads(ins__args.n_thr);
  
  //program input argument
  long inputArgument = ins__args.arg; 
  unsigned long int *numbers = (unsigned long int*)malloc(inputArgument * sizeof(unsigned long int));
  numgen(inputArgument, numbers);

  struct timeval ins__tstart, ins__tstop;
  gettimeofday(&ins__tstart, NULL);
  
  // run your computations here (including OpenMP stuff)

  long totalPrimes = 0;

  #pragma omp parallel for reduction(+:totalPrimes)
  for (long i = 0; i < inputArgument; i++) {
      if (isPrime(numbers[i])) {
          totalPrimes++;
      }
  }

  printf("Liczba liczb pierwszych: %ld\n", totalPrimes);
  
  // synchronize/finalize your computations
  gettimeofday(&ins__tstop, NULL);
  ins__printtime(&ins__tstart, &ins__tstop, ins__args.marker);

  free(numbers);
  return 0;
}
