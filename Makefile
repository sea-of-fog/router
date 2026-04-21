build:
	gcc -g -Wall -c data.c
	gcc -g -Wall -c network.c
	gcc -g -Wall router.c network.o data.o -o router

clean:
	rm *.o || echo 

distclean:
	rm router || echo 
	rm *.o || echo
