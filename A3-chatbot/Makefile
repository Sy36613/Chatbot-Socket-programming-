CXX = gcc
CFLAGS = -pthread
OBJECTS = lets-talk.o list.o
EXEC = lets-talk

${EXEC}: lets-talk.o list.o
	${CXX} ${CFLAGS} ${OBJECTS} -o ${EXEC}

list.o: list.c list.h

lets-talk.o: lets-talk.c

valgrind:
	valgrind ./lets-talk 3000 127.0.0.1 3001

.PHONY: clean

clean:
	rm -f ${OBJECTS} ${EXEC}
