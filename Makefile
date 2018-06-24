BINEXEC=t2

all: ${BINEXEC} run

run:
	./t2 $(id)

${BINEXEC}: t2.c
	gcc t2.c -Wall -lpthread -o t2

order:
	rm -f *.o

clean:
	rm -f *.o ${BINEXEC}
