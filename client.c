#include <sys/select.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>

int siet;
fd_set mnozina;
//partnerove nastavenia siete
int port = 1047;
char * adresa = "127.0.0.1";
//moje nastavenia
int myport = 2042;

void spracuj(int fd){
  char buf[81];
  int precitane;
  struct sockaddr_in odkoho;
  socklen_t adrlen = sizeof(odkoho);
  if ( fd == 0 ) precitane = read(fd, buf, 80);
  else precitane = recvfrom(fd,buf,80, 0, (struct sockaddr*)&odkoho, &adrlen );

  if ( precitane < 0){
    perror("chyba pri citani, neprecitali sme a mali sme");
    exit(1);
  }
  if (precitane > 0){
    buf[precitane] = 0;
    if (fd != 0){
      //citam zo siete, vypis na konzolu
      char * adresa = inet_ntoa(odkoho.sin_addr);
      write(1, "Dostal som od " , 14 );
      write(1, adresa , strlen(adresa) );
      write(1, ": ", 2);
      write(1, buf, precitane );
    }else{
      //citam z konzoly, idem poslat servru
      //!!! Toto nie je uplne korektne. Moze sa stat ze sa neposlu vsetky data.
      //Lepsie by to bolo v cykle posielat kym sa neposle vsetko
      if (send(siet,buf,precitane,0) == -1){
	perror("chyba pri zapise z konzoly na siet");
	exit(1);
      }
    }
  }
  return;
}

int main(int argc, char ** args){
  if (argc > 4){
    printf("Prilis velky pocet argumentov, najviac 3 = ich adresa, ich port, nas port\n");
    exit(1);
  }
  //druhy parameter adresa
  if (argc > 1) if ( strcmp(args[1],"-") != 0){
    adresa = args[1];
  }
  //treti parameter port
  if (argc > 2){
    if (atoi(args[2]) != 0) port = atoi(args[2]);
    else{
      printf("nespravny ich port, pouzivam defaultny 1047\n");
    }
  }
  if (argc > 3){
    if (atoi(args[3]) != 0) myport = atoi(args[3]);
    else{
      printf("nespravny nas port, pouzivam defaultny 2042\n");
    }
  }

  siet = socket( PF_INET , SOCK_DGRAM, 0);
  if (siet < 0){
    perror("vytvaranie socketu");
    exit(1);
  }

  struct sockaddr_in me;
  me.sin_family = AF_INET;
  me.sin_addr.s_addr = INADDR_ANY;
  me.sin_port = htons(myport);
  if (bind( siet,  (struct sockaddr*)&me, sizeof(me) )  == -1){
    perror("vysledok bindu");
    exit(1);
  }

  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = inet_addr(adresa);
  if ( sa.sin_addr.s_addr == INADDR_NONE){
    printf("neplatna adresa\n");
    exit(1);
  }
  sa.sin_port = htons( port );
  if (connect(siet, (struct sockaddr*)&sa, sizeof(sa) ) == -1){
    perror("vysledok connectu");
    exit(1);
  }

  while(1){
    //pripravi mnozinu s file deskriptormi
    FD_ZERO( &mnozina );
    FD_SET( 0, &mnozina );
    FD_SET( siet, &mnozina );
   
    int read = select( siet + 1 , &mnozina , NULL, NULL, NULL );
    //0 by vobec nemala nastat, lebo nemame timeout
    if (read <= 0){
      perror("chyba pri selecte");
      exit(1);
    }
    if ( FD_ISSET( 0, &mnozina ) ) spracuj(0);
    if ( FD_ISSET( siet, &mnozina ) ) spracuj(siet);
  }
  return 0;
}