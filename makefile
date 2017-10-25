CFLAGS = -Wall -Werror -pthread -g
CC = gcc-7

main: main.c random_provider.o thread_pool.o queue.o
	$(CC) main.c random_provider.o thread_pool.o queue.o -o main $(CFLAGS)

random_provider.o: random_provider.c random_provider.h
	$(CC) -c random_provider.c $(CFLAGS)

thread_pool.o: thread_pool.c thread_pool.h
	$(CC) -c thread_pool.c $(CFLAGS)

queue.o: queue.c queue.h
	$(CC) -c queue.c $(CFLAGS)

clean:
	rm -rf tests *.o *.gcov *.dSYM *.gcda *.gcno *.swp