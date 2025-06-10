#include "utility.h"
#include <stdio.h>
#include <stdlib.h>
#include <cuda_runtime.h>
#include <sys/time.h>
#include "numgen.c"


__host__ //Runs on the CPU (host)
void errorexit(const char *s) {
    printf("\n%s", s);
    exit(EXIT_FAILURE);
}


__device__ //Runs on the GPU (device)
int isPrime(unsigned long int n) {
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    if (n % 2 == 0 || n % 3 == 0) return 0;
    for (unsigned long int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return 0;
    }
    return 1;
}


__global__ //Runs on the GPU (device) but is launched from the CPU
void countPrimes(unsigned long int *numbers, int *count, long size) {
    long idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < size) {
        if (isPrime(numbers[idx])) {
            atomicAdd(count, 1);
        }
    }
}


int main(int argc,char **argv) {

  Args ins__args;
  parseArgs(&ins__args, &argc, argv);
  
  //program input argument
  long inputArgument = ins__args.arg; 
  unsigned long int *numbers = (unsigned long int*)malloc(inputArgument * sizeof(unsigned long int));
  numgen(inputArgument, numbers);

  struct timeval ins__tstart, ins__tstop;
  gettimeofday(&ins__tstart, NULL);
  
  // run your CUDA kernel(s) here
  
  unsigned long int *device_numbers = NULL;
  int *device_prime_count = NULL;
  int host_prime_count = 0;
  int threadsinblock=1024;
  int blocksingrid= (threadsinblock + inputArgument -1)/threadsinblock;	
  
  if (cudaSuccess != cudaMalloc((void **)&device_numbers, inputArgument * sizeof(unsigned long int))) {
  	errorexit("Error allocating device memory for numbers array");
  }
  if (cudaSuccess != cudaMalloc((void **)&device_prime_count, sizeof(int))) {
  	errorexit("Error allocating device memory for prime counter");
  }
  if (cudaSuccess != cudaMemcpy(device_numbers, numbers, inputArgument * sizeof(unsigned long int), cudaMemcpyHostToDevice)) {
  	errorexit("Error copying numbers to device");
  }
    
	countPrimes<<<blocksingrid, threadsinblock>>>(device_numbers, device_prime_count, inputArgument);

  // synchronize/finalize your CUDA computations
  
  cudaDeviceSynchronize();
  cudaMemcpy(&host_prime_count, device_prime_count, sizeof(int), cudaMemcpyDeviceToHost);
	
	printf("\nPrime numbers: %d\n", host_prime_count);

  cudaFree(device_numbers);
	free(numbers);

  gettimeofday(&ins__tstop, NULL);
  ins__printtime(&ins__tstart, &ins__tstop, ins__args.marker);

  return 0;
}
