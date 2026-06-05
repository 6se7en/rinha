CCFLAGS = -Wall -O3 -std=c2x -mavx2 -march=haswell -mtune=haswell
FILES = arena.c ivf.c iter.c errors.c req_parse.c nnet.c mmap.c obj_pool.c quantization.c handler.c matrix.c normalize.c weights.c mainzinho.c 

build:
	gcc $(CCFLAGS) -o server $(FILES) weights.o -lm
	
build-lb:
	gcc $(CCFLAGS) -o lb lb_ring.c
	
compose:
	docker compose up --build --abort-on-container-exit --remove-orphans
