#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>
#include "numgen.c"

#define RANGESIZE 10
#define DATA 0
#define RESULT 1
#define FINISH 2

// This is the version where the input argument is the number to which we check how many prime numbers are within range
// of (2, input)

int isPrime(int n) {
    if (n < 2) return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;
    for (int i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return 0;
    }
    return 1;
}

int NumberOfPrimes(int start, int end) {
    int count = 0;
    for (int i = start; i < end; i++) {
        if (isPrime(i)) {
            count++;
        }
    }
    return count;
}

int main(int argc,char **argv) {
  Args ins__args;
  parseArgs(&ins__args, &argc, argv);
  //program input argument
  long inputArgument = ins__args.arg;
  struct timeval ins__tstart, ins__tstop;

  int myrank,nproc;
  unsigned long int *numbers; //not used in this version
  int a = 2, b = inputArgument;
  int result = 0, resulttemp;
  int range[2];
  int sentcount=0;
  int i;
  MPI_Status status;

  MPI_Init(&argc,&argv);

  // obtain my rank
  MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
  // and the number of processes
  MPI_Comm_size(MPI_COMM_WORLD,&nproc);

  if (nproc < 2)
  {
    printf ("Run with at least 2 processes");
    MPI_Finalize ();
    return -1;
  }

  if (((b - a) / RANGESIZE) < 2 * (nproc - 1)) {
    printf("More subranges needed\n");
    MPI_Finalize();
    return -1;
  }

  if(!myrank){ //not used in this version
      	gettimeofday(&ins__tstart, NULL);
	numbers = (unsigned long int*)malloc(inputArgument * sizeof(unsigned long int));
  	numgen(inputArgument, numbers);
  }

  // run your computations here (including MPI communication)

  // now the master will distribute the data and slave processes will perform computations
  if (myrank == 0)
  {
    range[0] = a;
    // first distribute some ranges to all slaves
    for (i = 1; i < nproc; i++)
    {
      range[1] = range[0] + RANGESIZE;
      #ifdef DEBUG
      printf ("\nMaster sending range %d,%d to process %d", range[0], range[1], i);
      fflush (stdout);
      #endif
      // send it to process i
      MPI_Send (range, 2, MPI_DOUBLE, i, DATA, MPI_COMM_WORLD);
      sentcount++;
      range[0] = range[1];
    }

    do
    {
      // distribute remaining subranges to the processes which have completed their parts
      MPI_Recv (&resulttemp, 1, MPI_DOUBLE, MPI_ANY_SOURCE, RESULT, MPI_COMM_WORLD, &status);
      result += resulttemp;

      #ifdef DEBUG
      printf ("\nMaster received result %d from process %d", resulttemp, status.MPI_SOURCE);
      fflush (stdout);
      #endif

      // check the sender and send some more data
      range[1] = range[0] + RANGESIZE;
      if (range[1] > b)
        range[1] = b;

      #ifdef DEBUG
      printf ("\nMaster sending range %d,%d to process %d", range[0], range[1], status.MPI_SOURCE);
      fflush (stdout);
      #endif

      MPI_Send (range, 2, MPI_DOUBLE, status.MPI_SOURCE, DATA, MPI_COMM_WORLD);
      range[0] = range[1];
    }
    while (range[1] < b);


    // now receive results from the processes
    for (i = 0; i < (nproc - 1); i++)
    {
      MPI_Recv (&resulttemp, 1, MPI_DOUBLE, MPI_ANY_SOURCE, RESULT, MPI_COMM_WORLD, &status);
      #ifdef DEBUG
      printf ("\nMaster received result %d from process %d", resulttemp, status.MPI_SOURCE);
      fflush (stdout);
      #endif
      result += resulttemp;
    }
        // shut down the slaves
    for (i = 1; i < nproc; i++)
    {
      MPI_Send (NULL, 0, MPI_DOUBLE, i, FINISH, MPI_COMM_WORLD);
    }
    // now display the result
    printf ("\nHi, I am process 0, the result is %d\n", result);
  }
  //slave doing all the hard work
  else
  {
      do
      {
          MPI_Probe (0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

          if (status.MPI_TAG == DATA)
          {
              MPI_Recv (range, 2, MPI_DOUBLE, 0, DATA, MPI_COMM_WORLD, &status);
              // compute my part
              resulttemp = NumberOfPrimes (range[0], range[1]);
              // send the result back
              MPI_Send (&resulttemp, 1, MPI_DOUBLE, 0, RESULT, MPI_COMM_WORLD);
          }
      }
      while (status.MPI_TAG != FINISH);
  }




  // synchronize/finalize your computations

  if (!myrank) {
    gettimeofday(&ins__tstop, NULL);
    ins__printtime(&ins__tstart, &ins__tstop, ins__args.marker);
  }
  
  MPI_Finalize();

}
