OMP ?= /opt/homebrew/opt/libomp
ifeq ($(shell uname),Darwin)
OMPFLAGS = -Xpreprocessor -fopenmp -I$(OMP)/include -L$(OMP)/lib -lomp
else
OMPFLAGS = -fopenmp
endif

all: ecosystem ecosystem-omp

ecosystem: ecosystem.c
	cc -O2 -Wall -Wextra -o $@ $<

ecosystem-omp: ecosystem.c
	cc -O2 -Wall -Wextra $(OMPFLAGS) -o $@ $<

test: all
	./ecosystem < ex1.in | diff - ex1.out
	./ecosystem-omp < ex1.in | diff - ex1.out
	node check_engine.js < ex1.in | diff - ex1.out
	@echo PASS

clean:
	rm -f ecosystem ecosystem-omp
