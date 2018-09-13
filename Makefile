CC = gcc
CFLAGS  = -g -Wall

all: bl-server bl-client

bl-server:	bl-server.o server.o
	$(CC) $(CFLAGS) -o bl-server bl-server.o server.o -lpthread -pthread

bl-client:	bl-client.o simpio.o blather.h
	$(CC) $(CFLAGS) -o bl-client bl-client.o simpio.o -pthread

server.o:	server.c blather.h
	$(CC) $(CFLAGS) -c server.c -pthread -lpthread

bl-server.o:	bl-server.c server.c blather.h
	$(CC) $(CFLAGS) -c bl-server.c server.c -lpthread -pthread

bl-client.o:	bl-client.c simpio.c blather.h
	$(CC) $(CFLAGS) -c bl-client.c simpio.c -pthread

simpio.o:	simpio.c blather.h
	$(CC) $(CFLAGS) -c simpio.c -pthread

clean:
	rm -f *.o *.fifo *.out

make shell-tests : shell_tests.sh shell_tests_data.sh cat-sig.sh clean-tests
	chmod u+rx shell_tests.sh shell_tests_data.sh normalize.awk filter-semopen-bug.awk cat-sig.sh
	./shell_tests.sh

clean-tests :
	rm -f test-*.log test-*.out test-*.expect test-*.diff test-*.valgrindout
