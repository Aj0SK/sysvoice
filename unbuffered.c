#include <stdio.h>
#include <unistd.h>
#include <termios.h>

int main()
{
	struct termios old_tio, new_tio;
	unsigned char c;

	tcgetattr(STDIN_FILENO,&old_tio);

	new_tio=old_tio;

	new_tio.c_lflag &=(~ICANON & ~ECHO);

	tcsetattr(STDIN_FILENO,TCSANOW,&new_tio);

	do {
		 c=getchar();
		 printf("%d ",c);
	} while(c!='q');
	
	tcsetattr(STDIN_FILENO,TCSANOW,&old_tio);

	return 0;
}