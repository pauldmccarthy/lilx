
default: test

test: test.o stack.o lilx.o
	gcc -o lilxtest test.o stack.o lilx.o

clean: 
	rm -f *.o lilxtest
