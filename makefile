CFLAGS = -Wall -Werror -pthread -g
CC = gcc-7

main: main.c graph.o queue.o random_provider.o random_chunk.o \
			 salesman.o thread_pool.o
	$(CC) main.c graph.o queue.o random_provider.o random_chunk.o \
	salesman.o thread_pool.o -o main $(CFLAGS)

graph.o: graph.c graph.h
	$(CC) -c graph.c $(CFLAGS)

queue.o: queue.c queue.h
	$(CC) -c queue.c $(CFLAGS)

random_provider.o: random_provider.c random_provider.h
	$(CC) -c random_provider.c $(CFLAGS)

random_chunk.o: random_chunk.c random_chunk.h
	$(CC) -c random_chunk.c $(CFLAGS)

salesman.o: salesman.c salesman.h
	$(CC) -c salesman.c $(CFLAGS)

thread_pool.o: thread_pool.c thread_pool.h
	$(CC) -c thread_pool.c $(CFLAGS)

clean:
	rm -rf tests *.o *.gcov *.dSYM *.gcda *.gcno *.swp