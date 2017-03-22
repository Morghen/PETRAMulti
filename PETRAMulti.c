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

#define SZTAB (3)

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
caddr_t pShm;
caddr_t pSema;

//		0			1				2
sem_t 	*SemShm, 	*SemChariot, 	*SemArbre;

int readCapt(void);
int writeAct(void);
int readAct(void);

caddr_t initSema();
caddr_t initShm();

int ProcessPiece(void);

int nanoWait(int sec, long nsec);
void CheckDispenser(void);
int CheckPlace(void);


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

	pShm = initShm();
	pSema = initSema();

	while(1)
	{
		int Place=0;
		CheckDispenser();
		while((Place = CheckPlace()) != -1)
		{
			printf("Place non disponible\n");
			nanoWait(1,0);
		}
		sem_wait(SemShm);
		if((*(pShm+Place) = fork()) == -1)
		{
			perror("Error fork() Piece \n");
			exit(0);
		}

		if(!(*(pShm+Place)))
		{
			ProcessPiece();
			exit(0);
		}
		sem_post(SemShm);



		u_act.byte = 0x00;
		writeAct();
	}

	close ( fd_petra_in );
	close ( fd_petra_out );
	return EXIT_SUCCESS;
}

int ProcessPiece(void)
{
	return 1;
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

int CheckPlace(void)
{
	int i=0;
	sem_wait(SemShm);
	for(i=0;i<SZTAB;i++)
	{
		if(*(pShm+i) == 0)
		{
			return i;
		}
	}
	sem_post(SemShm);
	return -1;
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

caddr_t initShm()
{
	int i;
	caddr_t ret;

	//on mmap le Shm
	ret = (char *)mmap(0, SZTAB, PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	if (*ret == -1)
		perror("Error mmap()\n");

	//on init le buffer
	for (i = 0; i < SZTAB; i++)
	{
		*(ret+i) = 0;
	}
	return ret;
}

caddr_t initSema()
{
	caddr_t ret = (char *)mmap(0, 3*sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	if (*ret == -1)
		perror("Error Shm semaphore\n ");

	//pour initialisé les variable global au 3 semaphore
	SemShm = (sem_t*)ret;
	SemChariot = (sem_t*)(ret+sizeof(sem_t));
	SemArbre = (sem_t*)(ret+2*sizeof(sem_t));

	//on initialise les 3 semaphore
	if (sem_init(SemShm, 1, 1) == -1)
		perror("Error sem_init() sur SemBuf\n");

	if (sem_init(SemChariot, 1, 1) == -1)
		perror("Error sem_init() sur SemPlace\n");

	if (sem_init(SemArbre, 1, 1) == -1)
		perror("Error sem_init() sur SemElem\n");

	return ret;
}
