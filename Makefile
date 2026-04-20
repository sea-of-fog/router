build:
	gcc -g -c data.c
	gcc -g -c network.c
	gcc -g router.c network.o data.o -o router

clean:
	rm *.o

distclean: clean
	rm router

count:
	wc -l *
