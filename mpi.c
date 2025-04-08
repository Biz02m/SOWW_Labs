#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>
#include "numgen.c"

#define DATA 0
#define RESULT 1
#define FINISH 2
#define BATCHSIZE 10
#define DEBUG
#define FEED 4

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
  	//numgen(inputArgument, numbers);
    for(int i = 0; i < BATCHSIZE; i++){
      numbers[i] = i;
    }
  }

  // run your computations here (including MPI communication)
  // Kroki wyjasnione przez mata
  // 1. Nakarmienie/przekarmienie slaveow - czyli wysylamy np do kazdego slave poczatkowo 5 paczek danych
  //  kazdy z ktorych bedzie juz przez mpiaja zakolejkowany. Kazal na to patrzec w taki sposob ze mamy np 100 kartek
  //  do korekty i sale ludzi (slaveow XD). kazdemu na sali rozdajemy poczatkowo po 3 lub 4 czy ilekolwiek kartek na start zeby zaczeli pracowac
  // 2. odbieramy wyniki od slavea i odrazu do tego samego slavea wysylamy nowa paczke danych. 
  //  liczbe ile paczek wysylalismy mamy trzymac w jakims liczniku ktory wzrasta kiedy wysylamy paczke i obniza sie po odebraniu paczki
  //  w ten sposob zapewniamy ze slave zawsze bedzie mial co przetwarzac. 
  //  robimy to tak dlugo jak mamy jakies dane do przetworzenia
  // 3. konczymy wysylanie wiecej paczek i odbieramy pozostale wyniki od slaveow
  //  wiemy ile paczek musimy zebrac za pomoca tego licznika

  //MASTER
  if(myrank == 0 ){
    MPI_Request *requests;
    MPI_Status status;
    int counter = 0;
	  int requestCompleted;
	  int *resulttemp;
    int indexToSend = 0; 

    requests = (MPI_Request *) malloc (3 * (nproc - 1) * sizeof (MPI_Request));
    resulttemp = (int *) malloc((nproc - 1) * sizeof(int));
    
    if (!requests || !resulttemp){
      printf ("\nNot enough memory");
	    MPI_Finalize ();
	    return -1;
	  }

	  for (i = 0; i < 2 * (nproc - 1); i++){
      requests[i] = MPI_REQUEST_NULL;	// none active at this point
    }

    //przekarmienie slaveow
    for(int i = 0; i < FEED; i++){
      for(int j = 0 ; j < (nproc - 1); j++){
        #ifdef DEBUG
        printf("Master sending batch [%d, %d] to process %d\n", indexToSend, indexToSend + BATCHSIZE, j);
        fflush(stdout);
        #endif

        MPI_Send(&(numbers[indexToSend]), BATCHSIZE, MPI_UNSIGNED_LONG, j, DATA, MPI_COMM_WORLD);
        indexToSend += BATCHSIZE;
        counter++;
      }
    }

    // zaczynamy odbierac juz 
    for(int i = 1; i < nproc; i++){
      MPI_Irecv (&(resulttemp[i - 1]), 1, MPI_UNSIGNED_LONG, i, RESULT, MPI_COMM_WORLD, &(requests[i - 1]));
    }

    // odpalamy wysylke 
    for(int i = 1; i < nproc; i++){
      #ifdef DEBUG
      printf("Master sending batch [%d, %d] to process %d\n", indexToSend, indexToSend + BATCHSIZE, i);
      fflush(stdout);
      #endif
      MPI_Isend(&(numbers[indexToSend]), BATCHSIZE, MPI_UNSIGNED_LONG, i, DATA, MPI_COMM_WORLD, &(requests[nproc - 2 + i]));
      indexToSend += BATCHSIZE;
      counter++;
    }

    while(indexToSend < inputArgument){
      MPI_Waitany (2 * nproc - 2, requests, &requestcompleted, MPI_STATUS_IGNORE);

      if(requestCompleted < (nproc - 1)){
        result += resulttemp[requestCompleted];
        counter--; // odebralismy wiadomosc i dodalismy ja do wyniku
        
        // czekamy az zwolni sie kanal do komunikacji
        MPI_Wait (&(requests[nproc - 1 + requestcompleted]), MPI_STATUS_IGNORE);
        #ifdef DEBUG
        printf("Master sending batch [%d, %d] to process %d\n", indexToSend, indexToSend + BATCHSIZE); //tutaj zostawilem
        fflush(stdout);
        #endif

      }
    }


  }
  else { //-----------SLAVE-----------

  }


  // synchronize/finalize your computations

  if (!myrank) {
    gettimeofday(&ins__tstop, NULL);
    ins__printtime(&ins__tstart, &ins__tstop, ins__args.marker);
  }
  
  MPI_Finalize();

}
