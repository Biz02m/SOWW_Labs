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


main: openmp.c
	${CC} ${CFLAGS} ${OMPFLAGS} $< -o openmp ${LIBS}

run:
	./openmp $(RUN_ARGS)

clean:
	rm openmp