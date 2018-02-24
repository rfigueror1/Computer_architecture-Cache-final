all: sim

sim: main.o cache.o
	gcc -o sim main.o cache.o -lm

main.o:
	gcc -c main.c
cache.o:
	gcc -c cache.c
clean:
	rm main.o cache.o sim
