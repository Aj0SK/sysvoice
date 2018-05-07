CFLAGS = gcc -Wall -Wextra

clean:
	rm *.out

play:
	$(CFLAGS) play.c -lasound -o play.out && ./play.out < sound.raw

specialrec:
	$(CFLAGS) specialrec.c -lasound -o specialrec.out && ./specialrec.out > sound.raw