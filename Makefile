CFLAGS = gcc -Wall -Wextra

clean:
	rm *.out

program:
	$(CFLAGS) program.c -lasound -o program.out

run:	program
	./program.out 192.168.1.29 3334