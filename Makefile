CFLAGS = gcc -Wall -Wextra

testsound: load play
	./load.out > sound.raw && ./play.out < sound.raw
	
clean:
	rm *.out

load:
	$(CFLAGS) load.c -lasound -o load.out

play:
	$(CFLAGS) play.c -lasound -o play.out && ./play.out < sound.raw
	
client:
	$(CFLAGS) client.c -o client.out && ./client.out

specialrec:
	$(CFLAGS) specialrec.c -lasound -o specialrec.out && ./specialrec.out > sound.raw