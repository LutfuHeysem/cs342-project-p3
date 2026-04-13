LIBS = -L. -lrsm -lpthread -lrt

all: librsm.a app myapp

librsm.a:  rsm.c
	gcc -Wall -c rsm.c
	ar -cvq librsm.a rsm.o
	ranlib librsm.a

app: app.c librsm.a
	gcc -Wall -o app app.c $(LIBS)

myapp: myapp.c librsm.a
	gcc -Wall -o myapp myapp.c $(LIBS)

clean: 
	rm -fr *.o *.a *~ a.out app myapp rsm.o rsm.a librsm.a
