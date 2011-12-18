all: publisher subscriber 

subscriber: subscriber.o util.o
	gcc -g -Oo subscriber.o util.o -o subscriber -largtable2

publisher: publisher.o util.o
	gcc -g -Oo publisher.o util.o -o publisher -largtable2

subscriber.o: subscriber.c
	gcc -g -c subscriber.c

publisher.o: publisher.c
	gcc -g -c publisher.c

util.o: util.c
	gcc -g -c util.c

clean:
	rm -rf *o publisher subscriber
