search-engine: program.o index.o
	gcc program.o index.o -lm -lpthread -o search-engine

test: index.o test.o
	gcc index.o test.o -o test

index.o: index.c
	gcc -Wall -g -c index.c -o index.o

test.o: test.c
	gcc -Wall -g -c test.c -o test.o

program.o: program.c
	gcc -Wall -g -c program.c -o program.o

clean:
	rm -f *.o test
