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

int sumCounter(int * counter, int nproc){
  int sum = 0;
  for(int i = 0; i < nproc - 1; i++){
    sum += counter[i];
  }
  return sum;
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
    // for(int i = 0; i < inputArgument; i++){
    //   numbers[i] = i;
    // }
  }

  // run your computations here (including MPI communication)
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

  unsigned long int result = 0; 
  if(myrank == 0 ){
    MPI_Request *send_requests;
    MPI_Request *reciv_requests;
    MPI_Status status;
	  int *resulttemp;
    int *counter;
    int indexToSend = 0; 
    int requestCompleted;
    unsigned long int *lastBatch;

    counter = (int *) malloc((nproc - 1) * sizeof(int));
    memset(counter, 0, (nproc -1 ) * sizeof(int));
    send_requests = (MPI_Request *) malloc ((nproc - 1) * sizeof(MPI_Request));
    reciv_requests = (MPI_Request *) malloc ((nproc - 1) * sizeof(MPI_Request));
    resulttemp = (int *) malloc((nproc - 1) * sizeof(int));
    lastBatch = (unsigned long int*) malloc((BATCHSIZE)*sizeof(unsigned long int));
    
    if (!send_requests || !reciv_requests || !resulttemp || !lastBatch){
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

        //trzeba sprawdzic czy nie wysylamy za duzo
        if(indexToSend + BATCHSIZE > inputArgument){
          printf("Master ran out of work during overfeeding\n");
          int remaining = inputArgument - indexToSend;
          if(remaining > 0){
            //skopiuj pozostale dane
            memcpy(lastBatch, &numbers[indexToSend], remaining * sizeof(unsigned long int));
            
            //wypelnij reszte zerami
            memset(lastBatch + remaining, 0, (BATCHSIZE - remaining) * sizeof(unsigned long int));
            
            printf("Master has some work left to do, sending last batch: (");
            for(int k = 0; k < BATCHSIZE; k++){
              printf("%lu,",lastBatch[k]);
            }
            printf(") to process: %d\n", j);

            MPI_Send(lastBatch, BATCHSIZE, MPI_UNSIGNED_LONG, j, DATA, MPI_COMM_WORLD);
          }
          overfed = 1;
          break;
        }
        
        #ifdef DEBUG
        printf("Master sending overfeeding batch (");
        for(int k = indexToSend; k < indexToSend + BATCHSIZE; k++){
          printf("%lu,",numbers[k]);
        }
        printf(") to slave %d\n", j);
        fflush(stdout);
        #endif

        MPI_Send(&(numbers[indexToSend]), BATCHSIZE, MPI_UNSIGNED_LONG, j, DATA, MPI_COMM_WORLD);
        indexToSend += BATCHSIZE;
        counter[j-1]++;
        #ifdef DEBUG
        printf("counter status for slave: %d is: (overall/slave): %d/%d\n", j, sumCounter(counter,nproc), counter[j-1]);
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
      counter[requestCompleted]--; // odebralismy wiadomosc i dodalismy ja do wyniku
      #ifdef DEBUG
      printf("Master recieved result:%d from slave:%d, current result: %ld\n", resulttemp[requestCompleted], requestCompleted + 1, result);
      #endif
        
      // czekamy az zwolni sie kanal do komunikacji
      MPI_Wait (&(send_requests[requestCompleted]), MPI_STATUS_IGNORE);
      #ifdef DEBUG
      printf("Master sending batch [%d, %d] to process %d\n", indexToSend, indexToSend + BATCHSIZE, requestCompleted + 1);
      fflush(stdout);
      #endif
        
        //trzeba sprawdzic czy mozemy wyslac na pewno paczke o rozmiarze batchsize
      if(indexToSend + BATCHSIZE > inputArgument){
        #ifdef DEBUG
        printf("Master has last incomplete batch to send, padding will be used\n");
        #endif
        int remaining = inputArgument - indexToSend;
        memcpy(lastBatch, &numbers[indexToSend], remaining * sizeof(unsigned long int));
        memset(lastBatch + remaining, 0, (BATCHSIZE - remaining) * sizeof(unsigned long int));
          
        MPI_Isend(lastBatch, BATCHSIZE, MPI_UNSIGNED_LONG, requestCompleted + 1, DATA, MPI_COMM_WORLD, &(send_requests[requestCompleted]));
        MPI_Irecv(&(resulttemp[requestCompleted]), 1, MPI_INT, requestCompleted + 1, RESULT, MPI_COMM_WORLD, &(reciv_requests[requestCompleted]));
        counter[requestCompleted]++;
        break;
      }

      MPI_Isend(&(numbers[indexToSend]), BATCHSIZE, MPI_UNSIGNED_LONG, requestCompleted + 1, DATA, MPI_COMM_WORLD, &(send_requests[requestCompleted]));
			indexToSend += BATCHSIZE;
      counter[requestCompleted]++;

      // trzeba wydac odpowiedni kwitek na odebranie wyniku tej wysylki
      MPI_Irecv(&(resulttemp[requestCompleted]), 1, MPI_INT, requestCompleted + 1, RESULT, MPI_COMM_WORLD, &(reciv_requests[requestCompleted]));
    }
    #ifdef DEBUG
    printf("Master ran out of work to send, %d messages awaiting result\n", sumCounter(counter, nproc)); 
    fflush(stdout);
    #endif

    

    // odbierz rezultaty od slaveow
    //sposob blokujacy do debugowania
    // for(int i = 0; i < nproc - 1; i++){
    //   while(counter[i]>0){
    //     MPI_Wait(&(reciv_requests[i]), MPI_STATUS_IGNORE);
    //     result += resulttemp[i];
    //     counter[i]--; // odebralismy wiadomosc i dodalismy ja do wyniku
    //     #ifdef DEBUG
    //     printf("Master recieved result:%d from slave:%d, waiting for (overall/slave) messages: (%d, %d)\n",
    //        resulttemp[i], i + 1, sumCounter(counter,nproc), counter[i]);
    //     #endif
    //     if(counter[i] > 0){
    //       MPI_Irecv(&(resulttemp[i]), 1, MPI_INT, i + 1, RESULT, MPI_COMM_WORLD, &(reciv_requests[i]));
    //     } 
    //   }
    // }

    // sposob nieblokujacy
    while(sumCounter(counter, nproc)>0){  
      MPI_Waitany (nproc-1, reciv_requests, &requestCompleted, MPI_STATUS_IGNORE);

      result += resulttemp[requestCompleted];
      counter[requestCompleted]--; // odebralismy wiadomosc i dodalismy ja do wyniku
      #ifdef DEBUG
      printf("Master recieved result:%d from slave:%d, waiting for (overall/slave) messages: (%d, %d)\n",
         resulttemp[requestCompleted], requestCompleted + 1, sumCounter(counter,nproc), counter[requestCompleted]);
      #endif
      if(counter[requestCompleted]>0){       
        MPI_Irecv(&(resulttemp[requestCompleted]), 1, MPI_INT, requestCompleted + 1, RESULT, MPI_COMM_WORLD, &(reciv_requests[requestCompleted]));  
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

    #ifdef DEBUG
    int done;
    for(int i = 0; i < nproc - 1; i++){
      done = 0;
      MPI_Test(&send_requests[i], &done, MPI_STATUSES_IGNORE);
      printf("send_requests: %d status is: %d\n", i, done);
    }
    for(int i = 0; i < nproc - 1; i++){
      done = 0;
      MPI_Test(&reciv_requests[i], &done, MPI_STATUSES_IGNORE);
      printf("reciv_requests: %d status is: %d\n", i, done);
    }
    #endif

    MPI_Waitall(nproc - 1 , send_requests, MPI_STATUSES_IGNORE);
    MPI_Waitall(nproc - 1, reciv_requests, MPI_STATUSES_IGNORE);

    printf("Master recieved all results from slaves, the result is: %ld\n", result);
    free(send_requests);
    free(reciv_requests);
    free(resulttemp);
    free(lastBatch);
  }
  else { //-----------SLAVE-----------
    MPI_Request recv_req = MPI_REQUEST_NULL, send_req = MPI_REQUEST_NULL;
    MPI_Status recv_status, send_status;
    int resulttemp;
    unsigned long int* batch = (unsigned long int *) malloc(BATCHSIZE * sizeof(unsigned long int));
    int recv_ready = 0, send_ready = 1, finished = 0, recv_done = 0, send_done = 0;
    // recv_ready to flaga która mówi nam czy możemy odpalić znowu odbieranie Irecv. 
    // 0 oznacza że tak
    // 1 oznacza że jesteśmy w trakcie odbierania, czyli został odpalony i czekamy na przetworzenie tych danych
    // send_ready to flaga która mówi nam czy mozemy wysłac wiadomosc
    // 0 nie bo jedna wysylka wlasnie trwa
    // 1 tak 

    if(!batch){
      printf ("\nNot enough memory");
	    MPI_Finalize ();
	    return -1;
    }

    while(finished == 0){
      if(recv_ready == 0){
        MPI_Irecv(batch, BATCHSIZE, MPI_UNSIGNED_LONG, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &recv_req);
        recv_ready = 1;
      }

      //czyli czy jestesmy w trakcie odbierania paczki
      if(recv_ready == 1){
        recv_done = 0;
        MPI_Test(&recv_req, &recv_done, &recv_status); // sprawdzamy czy skonczylo sie odbieranie paczki
        
        if(recv_done == 1){
          recv_ready = 0; //czyli odebralismy paczke i mozna odebrac kolejna
          
          //zobacz czy dostales sygnal koniec
          if(recv_status.MPI_TAG == FINISH){
            finished = 1;
            #ifdef DEBUG
            printf("Slave:%d, recieved Finish signal, stopping work\n", myrank);
            #endif
            break;
          }

          #ifdef DEBUG
          printf("Slave:%d recieved batch to process: (",myrank);
          for(int i = 0; i < BATCHSIZE; i++){
            printf("%lu,", batch[i]);
          }
          printf(")\n");
          fflush(stdout);
          #endif

          //oblicz wynik
          resulttemp = NumberOfPrimes(batch);

          //poczekaj na poprzednia wysylke 
          if(send_ready == 0){
            MPI_Wait(&send_req, &send_status);
          }

          #ifdef DEBUG
          printf("Slave:%d sending result: %d\n", myrank, resulttemp);
          #endif
          MPI_Isend(&resulttemp, 1, MPI_INT, 0, RESULT, MPI_COMM_WORLD, &send_req);
          send_ready = 0; //wlasnie wyslalismy paczke, nie mozemy wysylac dalej
        }

      }

      // sprawdz czy ostatnia wysylka sie zakonczyla
      if(send_ready == 0){
        send_done = 0; 
        MPI_Test(&send_req, &send_done, &send_status);
        if(send_done == 1){
          send_ready = 1;
        }
      }

    }

    //upewnic sie ze ostatni wynik zostal wyslany
    if(send_ready == 0){
      MPI_Wait(&send_req, &send_status);
    }
    #ifdef DEBUG
    printf("Slave:%d is stopping\n",myrank);
    #endif
    free(batch);
  }


  // synchronize/finalize your computations

  if (!myrank) {
    gettimeofday(&ins__tstop, NULL);
    ins__printtime(&ins__tstart, &ins__tstop, ins__args.marker);
  }
  
  MPI_Finalize();

}
