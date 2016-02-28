mythread.a: mythread.o
	ar rcs mythread.a mythread.o

mythread.o: mythread.c
	gcc -c mythread.c

clean:
	rm -rf *.o mythread.a
