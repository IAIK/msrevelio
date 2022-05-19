all:
	gcc -shared -fPIC libfixcpuid.c -O3 -o libfixcpuid.so -ldl
	gcc test.c -O3 -o test -lm
	gcc bench.c -O3 -o bench -lm

clean:
	-rm bench libfixcpuid.so test	
