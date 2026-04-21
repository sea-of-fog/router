build:
	gcc -g -std=gnu18 -Wall -Wextra -c data.c
	gcc -g -std=gnu18 -Wall -Wextra -c network.c
	gcc -g -std=gnu18 -Wall -Wextra router.c network.o data.o -o router

clean:
	rm *.o 

distclean:
	rm router 
	rm *.o 
