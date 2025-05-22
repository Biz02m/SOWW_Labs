MPICC=mpicc
CFLAGS=-O2
LIBS=-lm
OMPFLAGS=-fopenmp

ifeq (run,$(firstword $(MAKECMDGOALS)))
  # use the rest as arguments for "run"
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  # ...and turn them into do-nothing targets
  $(eval $(RUN_ARGS):;@:)
endif


main: mpi+openmp.c
	${MPICC} ${CFLAGS} ${OMPFLAGS} $< -o openmp+mpi ${LIBS}

run:
	mpirun -bind-to none -np 2 ./openmp+mpi $(RUN_ARGS)

clean:
	rm openmp+mpi
