#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>
#include <omp.h>
#include "numgen.c"

#define DATA 0
#define RESULT 1
#define FINISH 2
#define PARTIAL 3
#define BATCHSIZE 10
//#define DEBUG
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

  //set number of threads
  omp_set_num_threads(ins__args.n_thr);
  
  //program input argument
  long inputArgument = ins__args.arg; 

  struct timeval ins__tstart, ins__tstop;

  int threadsupport;
  int myrank,nproc;
  unsigned long int *numbers;
  // Initialize MPI with desired support for multithreading -- state your desired support level

  MPI_Init_thread(&argc, &argv,MPI_THREAD_FUNNELED,&threadsupport); 

  if (threadsupport<MPI_THREAD_FUNNELED) {
    printf("\nThe implementation does not support MPI_THREAD_FUNNELED, it supports level %d\n",threadsupport);
    MPI_Finalize();
    return -1;
  }
  
  // obtain my rank
  MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
  // and the number of processes
  MPI_Comm_size(MPI_COMM_WORLD,&nproc);

  if(!myrank){
      gettimeofday(&ins__tstart, NULL);
	numbers = (unsigned long int*)malloc(inputArgument * sizeof(unsigned long int));
  	numgen(inputArgument, numbers);
  }
  // run your computations here (including MPI communication and OpenMP stuff)

  //MASTER

  unsigned long int result = 0; 
  if(myrank == 0 ){
    printf("nproc is %d", nproc);
    MPI_Request *send_requests;
    MPI_Request *reciv_requests;
    MPI_Status status;
	  int *resulttemp;
    int counter = 0;
    int indexToSend = 0; 
    int requestCompleted;

    send_requests = (MPI_Request *) malloc ((nproc - 1) * sizeof(MPI_Request));
    reciv_requests = (MPI_Request *) malloc ((nproc - 1) * sizeof(MPI_Request));
    resulttemp = (int *) malloc((nproc - 1) * sizeof(int));
    
    if (!send_requests || !reciv_requests || !resulttemp){
      printf ("\nNot enough memory");
	    MPI_Finalize ();
	    return -1;
	  }

	  for (int i = 0; i < nproc - 1; i++){
      reciv_requests[i] = MPI_REQUEST_NULL;	// none active at this point
      send_requests[i] = MPI_REQUEST_NULL;
    }

    //przekarmienie slaveow
    int overfed = 0; //flaga sprawdzajaca czy doszlo juz do wyczerpania danych
    for(int i = 0; i < FEED; i++){
      for(int j = 1 ; j < nproc; j++){
        #ifdef DEBUG
        printf("Master sending overfeeding batch (");
        for(int k = indexToSend; k < indexToSend + BATCHSIZE; k++){
          printf("%lu,",numbers[k]);
        }
        printf(") to slave %d\n", j);
        fflush(stdout);
        #endif

        MPI_Wait(&(send_requests[j-1]), MPI_STATUS_IGNORE);

        if(indexToSend + BATCHSIZE > inputArgument){
          MPI_Isend(&(numbers[indexToSend]), inputArgument - indexToSend, MPI_UNSIGNED_LONG, j, PARTIAL, MPI_COMM_WORLD, &(send_requests[j-1]));
          overfed = 1;
        }
        else{
          MPI_Isend(&(numbers[indexToSend]), BATCHSIZE, MPI_UNSIGNED_LONG, j, DATA, MPI_COMM_WORLD, &(send_requests[j-1]));
        }

        indexToSend += BATCHSIZE;
        counter++;
        #ifdef DEBUG
        printf("counter value is: %d\n", counter);
        #endif
      }
      if(overfed){
        break;
      }
    }

    // zaczynamy odbierac juz 
    for(int i = 1; i < nproc; i++){
      MPI_Irecv (&(resulttemp[i-1]), 1, MPI_INT, i, RESULT, MPI_COMM_WORLD, &(reciv_requests[i-1]));
    }

    while(indexToSend < inputArgument){
      MPI_Waitany (nproc - 1, reciv_requests, &requestCompleted, MPI_STATUS_IGNORE);

      result += resulttemp[requestCompleted];
      counter--; // odebralismy wiadomosc i dodalismy ja do wyniku
      #ifdef DEBUG
      printf("Master recieved result:%d from slave:%d, current result: %ld\n", resulttemp[requestCompleted], requestCompleted + 1, result);
      #endif
        
      // czekamy az zwolni sie kanal do komunikacji
      MPI_Wait (&(send_requests[requestCompleted]), MPI_STATUS_IGNORE);
      #ifdef DEBUG
      printf("Master sending batch [%d, %d] to process %d\n", indexToSend, indexToSend + BATCHSIZE, requestCompleted + 1);
      fflush(stdout);
      #endif
        
      if(indexToSend + BATCHSIZE > inputArgument){
        MPI_Isend(&(numbers[indexToSend]), inputArgument - indexToSend, MPI_UNSIGNED_LONG, requestCompleted + 1, PARTIAL, MPI_COMM_WORLD, &(send_requests[requestCompleted]));
      }
      else{
        MPI_Isend(&(numbers[indexToSend]), BATCHSIZE, MPI_UNSIGNED_LONG, requestCompleted + 1, DATA, MPI_COMM_WORLD, &(send_requests[requestCompleted]));
      }
      indexToSend += BATCHSIZE;
      counter++;

      // trzeba wydac odpowiedni kwitek na odebranie wyniku tej wysylki
      MPI_Irecv(&(resulttemp[requestCompleted]), 1, MPI_INT, requestCompleted + 1, RESULT, MPI_COMM_WORLD, &(reciv_requests[requestCompleted]));
    }
    #ifdef DEBUG
    printf("Master ran out of work to send, %d messages awaiting result\n", counter); 
    fflush(stdout);
    #endif

    while(counter>0){  
      MPI_Waitany(nproc-1, reciv_requests, &requestCompleted, MPI_STATUS_IGNORE);

      result += resulttemp[requestCompleted];
      counter--; // odebralismy wiadomosc i dodalismy ja do wyniku
      #ifdef DEBUG
      printf("Master recieved result:%d from slave:%d, waiting for %d messages\n",resulttemp[requestCompleted], requestCompleted + 1, counter);
      #endif
      if(counter>0){       
        MPI_Irecv(&(resulttemp[requestCompleted]), 1, MPI_INT, MPI_ANY_SOURCE, RESULT, MPI_COMM_WORLD, &(reciv_requests[requestCompleted]));  
      }
      else {
        reciv_requests[requestCompleted] = MPI_REQUEST_NULL;
      }
    }

    // wysylamy sygnal stop do slaveow
    unsigned long int finishSignaltmp = 0;
    for(int i = 1; i < nproc; i++){
      MPI_Wait (&(send_requests[i-1]), MPI_STATUS_IGNORE);
      MPI_Isend(&finishSignaltmp, 1, MPI_UNSIGNED_LONG, i, FINISH, MPI_COMM_WORLD, &(send_requests[i-1]));
    }

    MPI_Waitall(nproc - 1 , send_requests, MPI_STATUSES_IGNORE);
    
    printf("Master recieved all results from slaves, the result is: %ld\n", result);
    free(send_requests);
    free(reciv_requests);
    free(resulttemp);
  }
  else { // -----------SLAVE-----------
    MPI_Request recv_req = MPI_REQUEST_NULL, send_req = MPI_REQUEST_NULL;
    MPI_Status recv_status, send_status;
    int resulttemp;
    unsigned long int* batch = NULL;
    int send_ready = 1, recv_in_progress = 0, finished = 0;

    while (!finished) {
        // Jeśli nie mamy aktywnego odbioru, sprawdzamy czy coś przyszło
        if (!recv_in_progress) {
            int flag = 0;
            MPI_Iprobe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &recv_status);
            if (flag) {
                int count;
                MPI_Get_count(&recv_status, MPI_UNSIGNED_LONG, &count);

                // Obsługa sygnału zakończenia
                if (recv_status.MPI_TAG == FINISH) {
                    finished = 1;
                    #ifdef DEBUG
                    printf("Slave:%d got FINISH signal\n", myrank);
                    #endif
                    break;
                }

                // Alokuj odpowiednio duży bufor
                if (batch) free(batch);
                batch = (unsigned long int*)malloc(count * sizeof(unsigned long int));
                if (!batch) {
                    printf("Slave:%d allocation failed!\n", myrank);
                    MPI_Abort(MPI_COMM_WORLD, 1);
                }

                // Rozpocznij odbiór nieblokujący
                MPI_Irecv(batch, count, MPI_UNSIGNED_LONG, 0, recv_status.MPI_TAG, MPI_COMM_WORLD, &recv_req);
                recv_in_progress = 1;
            }
        }

        // Jeśli odbiór trwa, sprawdź czy się zakończył
        if (recv_in_progress) {
            int done = 0;
            MPI_Test(&recv_req, &done, &recv_status);
            if (done) {
                recv_in_progress = 0;

                int count;
                MPI_Get_count(&recv_status, MPI_UNSIGNED_LONG, &count);

                // Przetwarzanie
                #ifdef DEBUG
                printf("Slave:%d received batch of size %d\n", myrank, count);
                #endif

                resulttemp = 0;
                #pragma omp parallel for reduction(+:resulttemp)
                for (int i = 0; i < count; i++) {
                    if (isPrime(batch[i])) resulttemp++;
                }

                // Poczekaj na zakończenie poprzedniej wysyłki
                if (!send_ready) {
                    MPI_Wait(&send_req, &send_status);
                    send_ready = 1;
                }

                MPI_Isend(&resulttemp, 1, MPI_INT, 0, RESULT, MPI_COMM_WORLD, &send_req);
                send_ready = 0;
            }
        }

        // Sprawdź czy ostatnia wysyłka się zakończyła
        if (!send_ready) {
            int send_done = 0;
            MPI_Test(&send_req, &send_done, &send_status);
            if (send_done) send_ready = 1;
        }
    }

    // Upewnij się, że ostatnia wiadomość została wysłana
    if (!send_ready) {
        MPI_Wait(&send_req, &send_status);
    }

    if (batch) free(batch);

    #ifdef DEBUG
    printf("Slave:%d exiting cleanly\n", myrank);
    #endif
  }


  // synchronize/finalize your computations

  if (!myrank) {
    gettimeofday(&ins__tstop, NULL);
    ins__printtime(&ins__tstart, &ins__tstop, ins__args.marker);
  }
    
  MPI_Finalize();
  
}
