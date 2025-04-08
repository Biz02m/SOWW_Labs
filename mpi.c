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

    requests = (MPI_Request *) malloc (3 * (nproc - 1) * sizeof (MPI_Request));
    resulttemp = (int *) malloc((nproc - 1) * sizeof(int));

    if (!requests || !resulttemp)
	  {
      printf ("\nNot enough memory");
	    MPI_Finalize ();
	    return -1;
	  }

    // the first nproc requests will be for receiving, the latter ones for sending
    // MEANING - each slave gets a pair of communication handles (in view of master): receiving results, sending data
    // in addition we also have space for sending finish signals. 
    // for example for 3 slaves:
    // 0..2 - receiving
    // 3..5 - sending
    // 6..8 - finish signal sending  
	  for (i = 0; i < 2 * (nproc - 1); i++){
      requests[i] = MPI_REQUEST_NULL;	// none active at this point
    }
      

    // start sending some stuff to be processed any means necessary
    for (int i = 0; i < nproc; i++){
      #ifdef DEBUG
	    printf ("\nMaster sending number: %lu to process %d", numbers[indexToSend], i);
	    fflush (stdout);
      #endif
      MPI_Send(&numbers[indexToSend], 1, MPI_UNSIGNED_LONG, i, DATA, MPI_COMM_WORLD);
      indexToSend++;
    }

    // start receiving for results from the slaves
	  for (i = 1; i < nproc; i++){
      MPI_Irecv (&(resulttemp[i - 1]), 1, MPI_UNSIGNED_LONG, i, RESULT, MPI_COMM_WORLD, &(requests[i - 1]));
    }
    
	  // start sending new data parts to the slaves by nonblocking functions
	  for (i = 1; i < nproc; i++)
	  {
      #ifdef DEBUG
	    printf ("\nMaster sending number: %lu to process %d", numbers[indexToSend], i);
	    fflush (stdout);
      #endif
	    // send it to process i
	    MPI_Isend (&(numbers[indexToSend]), 1, MPI_UNSIGNED_LONG, i, DATA, MPI_COMM_WORLD, &(requests[nproc - 2 + i]));
      indexToSend++;
	  }

    //main loop of communication
    while(indexToSend < inputArgument){
      // wait if any communications (sending or receiving) handler has finished work
      // requestscompleted will contain the value of index of requests table of the request that has been finished
      // something like that
      MPI_Waitany (2 * nproc - 2, requests, &requestCompleted, MPI_STATUS_IGNORE);

      if (requestCompleted < (nproc - 1)){
        result += resulttemp[requestCompleted];

        #ifdef DEBUG
		    printf ("\nMaster received result %lu from process %d", resulttemp[requestcompleted], requestcompleted + 1);
		    fflush (stdout);
        #endif
        
        // wait till the sending to that process has finished
        MPI_Wait (&(requests[nproc - 1 + requestCompleted]), MPI_STATUS_IGNORE);

        #ifdef DEBUG
		    printf ("\nMaster sending number %lu to process %d", numbers[indexToSend], requestcompleted + 1);
		    fflush (stdout);
        #endif
        //send new data to slave
        MPI_Isend (&(numbers[indexToSend]), 1, MPI_UNSIGNED_LONG, requestCompleted + 1, DATA, MPI_COMM_WORLD, &(requests[nproc - 1 + requestCompleted]));
        indexToSend++;

        //issue a new handle for receiving that result
        MPI_Irecv (&(resulttemp[requestcompleted]), 1, MPI_UNSIGNED_LONG, requestcompleted + 1, RESULT, MPI_COMM_WORLD, &(requests[requestcompleted]));
      }

      // stop slaves
      unsigned long int meaninglesValue = -1;
      for (int i = 1; i < nproc; i++){
        MPI_Isend(&meaninglesValue, 1, MPI_UNSIGNED_LONG, i, FINISH, MPI_COMM_WORLD, &(requests[2 * nproc - 3 + i]));
      }
      
      // wait for all processes to finish
      MPI_Waitall(3 * nproc - 3, requests, MPI_STATUSES_IGNORE);

      //sum up what is saved in receiving buffor
      for(int i = 0; i < (nproc - 1); i++) //sum last irecv results
		  {
			  result += resulttemp[i];
		  }

      // sum up last incoming messages
      for(int i = 0; i < (nproc - 1); i++) //sum last irecv results
		  {
        MPI_Recv(&(resulttemp[i]), 1, MPI_UNSIGNED_LONG, i + 1, RESULT, MPI_COMM_WORLD, &status);
			  result += resulttemp[i];
		  }

      printf ("\nHi, I am process 0, the result is %lu\n", result);
      fflush(stdout)
    }

  }
  //SLAVE 
  else{
    requests = (MPI_Request *) malloc (2 * sizeof (MPI_Request));
    resulttemp = (int *) malloc (sizeof (int));

    if(!requests || !resulttemp ){
      printf ("\nNot enough memory");
	    MPI_Finalize ();
	    return -1;
    }

    requests[0] = requests[1] = MPI_REQUEST_NULL;

    unsigned long int numberToProcess;
    // first receive the initial data
    MPI_Recv (&numberToProcess, 1, MPI_UNSIGNED_LONG, 0, DATA, MPI_COMM_WORLD, &status);
    #ifdef DEBUG
    printf ("\nSlave received number %lu", numberToProcess);
    fflush (stdout);
    #endif

    // keep receiving untill you get a finishing signal from master
    while(status.MPI_TAG != FINISH){
      //before computing your data, start receiving next
      MPI_Irecv (&numberToProcess, 1, MPI_UNSIGNED_LONG, 0, DATA, MPI_COMM_WORLD, &(requests[0]));
      resulttemp[0] = isPrime(numberToProcess);

      // couldnt do waitall because we care about the status 
      MPI_Wait(&(requests[1]), MPI_STATUS_IGNORE);
			MPI_Wait(&(requests[0]), &status);


      MPI_Isend(&resulttemp[0], 1, MPI_UNSIGNED_LONG, 0, RESULT, MPI_COMM_WORLD, &(requests[1]));
    }

    //wait for last send
    MPI_Wait(&(requests[1]), MPI_STATUS_IGNORE); 
  }

  // synchronize/finalize your computations

  if (!myrank) {
    gettimeofday(&ins__tstop, NULL);
    ins__printtime(&ins__tstart, &ins__tstop, ins__args.marker);
  }
  
  MPI_Finalize();

}
