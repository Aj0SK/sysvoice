all: testsound clean

testsound: load play
	./load.out > sound.raw && ./play.out < sound.raw

load:
	gcc load.c -lasound -o load.out

play:
	gcc play.c -lasound -o play.out

clean:
	rm *.out
	
client:
	gcc client.c -o client.out && ./client.out
	
