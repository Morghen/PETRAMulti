#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

struct	ACTUATEURS
{
	unsigned CP : 2; //Chariot
	unsigned C1 : 1; //Conv1
	unsigned C2 : 1; //Conv2
	unsigned PV : 1; //Ventouse
	unsigned PA : 1; //Truc sur le chariot
	unsigned AA : 1; //Arbre
	unsigned GA : 1; //Grapin sur Arbre
} ;
union
{
	struct ACTUATEURS act ;
	unsigned char byte ;
} u_act ;

struct 	CAPTEURS
{
	unsigned L1 : 1;
	unsigned L2 : 1;
	unsigned T  : 1; /* cable H */
	unsigned S  : 1;
	unsigned CS : 1;
	unsigned AP : 1;
	unsigned PP : 1;
	unsigned DE : 1;
} ;

union
{
	struct CAPTEURS capt ;
	unsigned char byte ;
} u_capt ;

int fd_petra_in, fd_petra_out ;
int readCapt(void);
int writeAct(void);
int readAct(void);

int nanoWait(int sec, long nsec);


/***********************************
 *
 * 		 	MAIN    !!!!!!
 *
 ***********************************
 */
int main(int argc, char *argv[])
{
	fd_petra_out = open ( "/dev/actuateursPETRA", O_WRONLY );
	if ( fd_petra_in == -1 )
	{
		perror ( "MAIN : Erreur ouverture PETRA_OUT" );
		return 1;
	}
	else
		printf ("MAIN: PETRA_OUT opened\n");


	fd_petra_in = open ( "/dev/capteursPETRA", O_RDONLY );
	if ( fd_petra_in == -1 )
	{
		perror ( "MAIN : Erreur ouverture PETRA_IN" );
		return 1;
	}
	else
		printf ("MAIN: PETRA_IN opened\n");

	CheckDispenser();



	u_act.byte = 0x00;
	writeAct();
	close ( fd_petra_in );
	close ( fd_petra_out );
	return 1;
}

int ProcessPiece(void)
{

}

void CheckDispenser(void)
{
	readCapt();
	while(u_capt.capt.DE != 0)
	{
		printf("Stock vide\n");
		nanoWait(1,0);
		readCapt();
	}
}



int nanoWait(int sec, long nsec)
{
	struct timespec tmp;
	tmp.tv_sec = sec;
	tmp.tv_nsec = nsec;
	return nanosleep(&tmp, NULL);
}

int readCapt(void)
{
	return read ( fd_petra_in , &u_capt.byte , 1 );
}

int writeAct(void)
{
	return write ( fd_petra_out , &u_act.byte ,1 );
}

int readAct(void)
{
	return read ( fd_petra_out , &u_act.byte , 1 );
}
