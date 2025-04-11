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

int isPrime(unsigned long int n) {
  if (n < 2) return 0;
  if (n == 2) return 1;
  if (n % 2 == 0) return 0;
  for (int i = 3; i * i <= n; i += 2) {
      if (n % i == 0) return 0;
  }
  return 1;
}

int NumberOfPrimes(unsigned long int* range) {
  int count = 0;
  for (int i = 0; i < BATCHSIZE; i++) {
      if (isPrime(range[i])) {
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
    for(int i = 0; i < inputArgument; i++){
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
  MPI_Request *requests;
  MPI_Status status;
  unsigned long int result = 0; 
  if(myrank == 0 ){
	  int *resulttemp;
    int counter = 0;
    int indexToSend = 0; 
    int requestCompleted;
    int customBatchSize = BATCHSIZE;

    requests = (MPI_Request *) malloc (3 * (nproc - 1) * sizeof (MPI_Request));
    resulttemp = (int *) malloc((nproc - 1) * sizeof(int));
    
    if (!requests || !resulttemp){
      printf ("\nNot enough memory");
	    MPI_Finalize ();
	    return -1;
	  }

	  for (int i = 0; i < 2 * (nproc - 1); i++){
      requests[i] = MPI_REQUEST_NULL;	// none active at this point
    }

    //przekarmienie slaveow
    int overfed = 0;
    for(int i = 0; i < FEED; i++){
      for(int j = 1 ; j < nproc; j++){
        customBatchSize = indexToSend + BATCHSIZE > inputArgument ? inputArgument - indexToSend : BATCHSIZE;
        if(customBatchSize != BATCHSIZE){
          overfed = 1;
          break;
        }
        
        #ifdef DEBUG
        printf("Master sending overfeeding batch (");
        for(int k = indexToSend; k < indexToSend + customBatchSize; k++){
          printf("%lu,",numbers[k]);
        }
        printf(") to process %d\n", j);
        fflush(stdout);
        #endif

        MPI_Send(&(numbers[indexToSend]), customBatchSize, MPI_UNSIGNED_LONG, j, DATA, MPI_COMM_WORLD);
        indexToSend += BATCHSIZE;
        counter++;
      }
      if(overfed){
        break;
      }
    }

    // zaczynamy odbierac juz 
    for(int i = 1; i < nproc; i++){
      MPI_Irecv (&(resulttemp[i - 1]), 1, MPI_UNSIGNED_LONG, i, RESULT, MPI_COMM_WORLD, &(requests[i - 1]));
    }

    // odpalamy wysylke 
    for(int i = 1; i < nproc; i++){
      customBatchSize = indexToSend + BATCHSIZE > inputArgument ? inputArgument - indexToSend  : BATCHSIZE;
      if(customBatchSize != BATCHSIZE){
        break;
      }

      #ifdef DEBUG
      printf("Master sending batch [%d, %d] to process %d\n", indexToSend, indexToSend + BATCHSIZE, i);
      fflush(stdout);
      #endif
      MPI_Isend(&(numbers[indexToSend]), customBatchSize, MPI_UNSIGNED_LONG, i, DATA, MPI_COMM_WORLD, &(requests[nproc - 2 + i]));
      indexToSend += BATCHSIZE;
      counter++;
    }

    while(indexToSend < inputArgument){
      MPI_Waitany (2 * nproc - 2, requests, &requestCompleted, MPI_STATUS_IGNORE);

      if(requestCompleted < (nproc - 1)){
        result += resulttemp[requestCompleted];
        counter--; // odebralismy wiadomosc i dodalismy ja do wyniku
        
        // czekamy az zwolni sie kanal do komunikacji
        MPI_Wait (&(requests[nproc - 1 + requestCompleted]), MPI_STATUS_IGNORE);
        #ifdef DEBUG
        printf("Master sending batch [%d, %d] to process %d\n", indexToSend, indexToSend + BATCHSIZE, requestCompleted + 1);
        fflush(stdout);
        #endif
        
        customBatchSize = indexToSend + BATCHSIZE > inputArgument ? inputArgument - indexToSend  : BATCHSIZE;

        MPI_Isend(&(numbers[indexToSend]), customBatchSize, MPI_UNSIGNED_LONG, requestCompleted + 1, DATA, MPI_COMM_WORLD, &(requests[nproc - 1 + requestCompleted]));
				indexToSend += BATCHSIZE;
        counter++;

        // trzeba wydac odpowiedni kwitek na odebranie wyniku tej wysylki
        MPI_Irecv(&(resulttemp[requestCompleted]), 1, MPI_UNSIGNED_LONG, requestCompleted + 1, RESULT, MPI_COMM_WORLD, &(requests[requestCompleted]));
      }
    }
    #ifdef DEBUG
    printf("Master ran out of work to send, %d messages awaiting result\n", counter); 
    fflush(stdout);
    #endif

    // skonczyly sie dane do przetwarzania - trzeba wyslac sygnal do slaveow o tym
    unsigned long int finishSignaltmp = 0;
    for(int i = 1; i < nproc; i++){
      MPI_Isend(&finishSignaltmp, 1, MPI_UNSIGNED_LONG, i, FINISH, MPI_COMM_WORLD, &(requests[2 * nproc - 3 + i]));
    }

    // moze zsynchronizowac?
    MPI_Waitall(3 * nproc - 3, requests, MPI_STATUSES_IGNORE);

    while(counter>0){
      for(int i = 0; i < nproc - 1; i++){
        MPI_Recv(&(resulttemp[i]), 1, MPI_UNSIGNED_LONG, i + 1, RESULT, MPI_COMM_WORLD, &status);
        result += resulttemp[i];
        counter--;
      }
    }

  }
  else { //-----------SLAVE-----------
    requests = (MPI_Request *) malloc(2 * sizeof (MPI_Request));
		requests[0] = requests[1] = MPI_REQUEST_NULL;
		int resulttemp;
    unsigned long int* batch;
    batch = (unsigned long int *) malloc(BATCHSIZE * sizeof(unsigned long int));

    if(!requests || !batch){
      printf ("\nNot enough memory");
	    MPI_Finalize ();
	    return -1;
    }

    while(status.MPI_TAG != FINISH) //keep sendiing/receiving until finish
		{
			MPI_Irecv(&batch, BATCHSIZE, MPI_UNSIGNED_LONG, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &(requests[0]));
      #ifdef DEBUG
      printf("Slave:%d recieved batch to process: (",myrank);
      for(int i = 0; i < BATCHSIZE; i++){
        printf("%lu,",batch[i]);
      }
      printf(")\n");
      fflush(stdout);
      #endif

      // przeprocesuj paczke
      resulttemp = NumberOfPrimes(batch);

			MPI_Wait(&(requests[1]), MPI_STATUS_IGNORE);
			MPI_Wait(&(requests[0]), &status);

			MPI_Isend(&resulttemp, 1, MPI_UNSIGNED_LONG, 0, RESULT, MPI_COMM_WORLD, &(requests[1]));
		}

		MPI_Wait(&(requests[1]), MPI_STATUS_IGNORE); //wait for last send
    
  }


  // synchronize/finalize your computations

  if (!myrank) {
    gettimeofday(&ins__tstop, NULL);
    ins__printtime(&ins__tstart, &ins__tstop, ins__args.marker);
  }
  
  MPI_Finalize();

}
