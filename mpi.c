#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <mpi.h>

int main(int argc,char **argv) {
  Args ins__args;
  parseArgs(&ins__args, &argc, argv);

  int INITIAL_NUMBER = ins__args.start; 
  int FINAL_NUMBER = ins__args.stop;

  struct timeval ins__tstart, ins__tstop;

  int myrank,nproc;
  
  MPI_Init(&argc,&argv);

  // obtain my rank
  MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
  // and the number of processes
  MPI_Comm_size(MPI_COMM_WORLD,&nproc);

  if(!myrank) 
      gettimeofday(&ins__tstart, NULL);

  // run your computations here (including MPI communication)

  if(!myrank){
    printf("%d, %d\n", INITIAL_NUMBER, FINAL_NUMBER);
  }


  if(INITIAL_NUMBER == 0){
    double pi, pi_final;
    int mine, sign;
    int i;
      // each process performs computations on its part
    pi = 0;
    mine = myrank * 2 + 1;
    sign = (((mine - 1) / 2) % 2) ? -1 : 1;

    for (; mine < FINAL_NUMBER;)
    {
      //        printf("\nProcess %d %d %d", myrank,sign,mine);
      //        fflush(stdout);
      pi += sign / (double) mine;
      mine += 2 * nproc;
      sign = (((mine - 1) / 2) % 2) ? -1 : 1;
    }

      // now merge the numbers to rank 0
    MPI_Reduce (&pi, &pi_final, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (!myrank)
    {
      pi_final *= 4;
      printf ("pi=%.12f", pi_final);
    }
  } else {
    double pi = 0, pi_final;
    int sign = 1;
    int range = FINAL_NUMBER/nproc;
    int start = range * myrank * 2 + 1;
    int mine = start;
    
    
    for (int i = 0 ; i < range; i++){
      if(!myrank){
        //printf("%d\n", mine* sign); 
      }
      pi += sign/(double) mine;
      mine += 2;
      sign *= -1;
    }

      // now merge the numbers to rank 0
    MPI_Reduce (&pi, &pi_final, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (!myrank)
    {
      pi_final *= 4;
      printf ("pi=%.12f", pi_final);
    }
  }


  // synchronize/finalize your computations

  if (!myrank) {
    gettimeofday(&ins__tstop, NULL);
    ins__printtime(&ins__tstart, &ins__tstop, ins__args.marker);
  }
  
  MPI_Finalize();

}
