#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>
#include "numgen.c"

#define DATA 0
#define RESULT 1
#define FINISH 2
#define DEBUG

int isPrime(int n) {
  if (n < 2) return 0;
  if (n == 2) return 1;
  if (n % 2 == 0) return 0;
  for (int i = 3; i * i <= n; i += 2) {
      if (n % i == 0) return 0;
  }
  return 1;
}

int main(int argc,char **argv) {

  Args ins__args;
  parseArgs(&ins__args, &argc, argv);

  //program input argument
  long inputArgument = ins__args.arg; 

  struct timeval ins__tstart, ins__tstop;

  int myrank,nproc;
  unsigned long int *numbers;

  MPI_Init(&argc,&argv);

  // obtain my rank
  MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
  // and the number of processes
  MPI_Comm_size(MPI_COMM_WORLD,&nproc);

  if(!myrank){
      	gettimeofday(&ins__tstart, NULL);
	numbers = (unsigned long int*)malloc(inputArgument * sizeof(unsigned long int));
  	numgen(inputArgument, numbers);
  }

	if(nproc < 2)
	{
		printf("Run with al least 2 processes");
		MPI_Finalize();
		return -1;
	}

  MPI_Status status;
	MPI_Request *requests;
	int requestCount = 0;
	int requestCompleted;
	int *resulttemp;

  // run your computations here (including MPI communication)
  //MASTER
  if(myrank == 0){
    int indexToSend = 0;
    unsigned long int result = 0;

    requests = (MPI_Request *) malloc (3 * (proccount - 1) * sizeof (MPI_Request));
    resulttemp = (int *) malloc((nproc - 1) * sizeof(int));

    if (!requests || !resulttemp)
	  {
      printf ("\nNot enough memory");
	    MPI_Finalize ();
	    return -1;
	  }

    // the first proccount requests will be for receiving, the latter ones for sending
    // MEANING - each slave gets a pair of communication handles (in view of master): receiving results, sending data
    // in addition we also have space for sending finish signals. 
    // for example for 3 slaves:
    // 0..2 - receiving
    // 3..5 - sending
    // 6..8 - finish signal sending  
	  for (i = 0; i < 2 * (proccount - 1); i++){
      requests[i] = MPI_REQUEST_NULL;	// none active at this point
    }
      

    // start sending some stuff to be processed any means necessary
    for (int i = 0; i < nproc; i++){
      #ifdef DEBUG
	    printf ("\nMaster sending number: %d to process %d", numbers[indexToSend], i);
	    fflush (stdout);
      #endif
      MPI_Send(&numbers[indexToSend], 1, MPI_UNSIGNED_LONG, i, DATA, MPI_COMM_WORLD);
      indexToSend++;
    }



  }
  //SLAVE 
  else{

  }

  // synchronize/finalize your computations

  if (!myrank) {
    gettimeofday(&ins__tstop, NULL);
    ins__printtime(&ins__tstart, &ins__tstop, ins__args.marker);
  }
  
  MPI_Finalize();

}
