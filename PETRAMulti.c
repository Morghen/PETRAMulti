#include <stdlib.h>
#include <signal.h>
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
} u_capt;

int fd_petra_in, fd_petra_out ;
caddr_t pShm;
caddr_t pShmIO;
caddr_t pSema;
int value;
pid_t pidIO;
//struct sigaction interrupt;


//		0			1				2
sem_t 	*SemShm, 	*SemChariot, 	*SemArbre, *SemDisp, *SemAct;

int readCapt(void);
int writeAct(void);
int readAct(void);

caddr_t initSema();
caddr_t initShm();

int ProcessPiece(void);

int nanoWait(int sec, long nsec);
int CheckDispenser(void);
int CheckPlace(void);
void affAll(void);
int processIO(void);

void handlerInterrupt(int s);

/***********************************
 *
 * 		 	MAIN    !!!!!!
 *
 ***********************************
 */
int main(int argc, char *argv[])
{
	fd_petra_out = open ( "/dev/actuateursPETRA", O_RDWR );
	if ( fd_petra_out == -1 )
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
	
	
	fflush(stdout);
	printf("init\n");
	fflush(stdout);
	pShm = initShm();
	pSema = initSema();
	//affAll();
	pidIO= fork() ;
	nanoWait(1,0);
	if((pidIO)== 0)
	{
		processIO();
	}

	while(1)
	{
		printf("\n\nDebut while(1)\n");
		fflush(stdout);
		int Place=0;
		int FullStop = 0;
		
		FullStop = CheckDispenser();
		if(CheckDispenser() == 1)
		{
			printf("check dispenser FAIL\n");
			//u_act.byte = 0x00;
			//writeAct();
			close ( fd_petra_in );
			close ( fd_petra_out );
			exit(EXIT_SUCCESS);
		}
		printf("check place\n");
		fflush(stdout);
		while((Place = CheckPlace()) == -1)
		{
			printf("Place non disponible\n");
			fflush(stdout);
			nanoWait(5,0);
		}
		sem_getvalue(SemDisp, &value);
		printf("Place dispo  %d\n", value);
		fflush(stdout);
		
		sem_wait(SemShm);
		printf("fork\n");
		fflush(stdout);
		if((*(pShm+Place) = fork()) == -1)
		{
			perror("Error fork() Piece \n");
			exit(0);
		}
		readAct();
		u_act.act.C1 = 1;
		u_act.act.C2 = 1;
		writeAct();
		if(!(*(pShm+Place)))
		{
			ProcessPiece();
			printf("fin process piece\n");
			fflush(stdout);
			exit(0);
		}
		sem_post(SemShm);
		nanoWait(1,0);
	}

	close ( fd_petra_in );
	close ( fd_petra_out );
	return EXIT_SUCCESS;
}

int ProcessPiece(void)
{
	printf("Debut process piece\n");
	fflush(stdout);
	int Slot = 0;
	int CutOut = 0;
	int i = 0;
	/*************************
	 *  Section Prise piece
	 *************************/
	sem_wait(SemChariot);
	printf("chario pris\n");
	fflush(stdout);
	sem_wait(SemDisp);
	printf("disp pris pris\n");
	fflush(stdout);
	readAct();
	u_act.act.CP = 00;
	writeAct();
	nanoWait(0,500000000);
	// A verifier !!!!
	printf("verif c\ns");
	//affAll();
	readCapt();
	while(u_capt.capt.CS == 1)
	{
		readCapt();
	}
	// A verifier !!!!
	printf("Chariot en position 00\n");
	readAct();
	u_act.act.PA = 1;
	writeAct();
	nanoWait(2,0);
	readAct();
	u_act.act.PV = 1;
	writeAct();
	nanoWait(1,0);
	printf("Ventouse active sur piece\n");
	readAct();
	u_act.act.PA = 0;
	writeAct();
	nanoWait(2,0);
	readAct();
	u_act.act.CP = 01;
	writeAct();
	nanoWait(0,500000000);
	// A verifier !!!!
	readCapt();
	while(u_capt.capt.CS == 1)
	{
		readCapt();
	}
	// A verifier !!!!
	printf("Chariot en position 01\n");
	readAct();
	u_act.act.PV = 0;
	writeAct();
	nanoWait(0,500000000);
	printf("Piece lachee sur C1\n");
	
	
	sem_post(SemChariot);
	readAct();
	u_act.act.CP = 00;
	
	writeAct();
	nanoWait(2, 0);
	sem_post(SemDisp);
	/*****************************
	 * Section Detection Slot
	 *****************************/
	printf("Demarrage detection du slot\n");
	readCapt();
	while(u_capt.capt.S != 1)
	{
		readCapt();
	}
	
	//printf("Detection fin piece\n");
	readCapt();
	while(u_capt.capt.S != 0)
	{
		readCapt();
	}
	//printf("soit fin piece soit debut slot\n");
	readCapt();
	
	i = 0;
	Slot = 0;
	while(i<150)
	{
		if(i < 150 && u_capt.capt.S == 1)
		{
			Slot = 1;
			break;
		}
		if(i == 150)
			break;
		
		nanoWait(0, 10000000);
		i++;
		readCapt();
	}
	i=0;
	if(Slot == 1)
		printf("Piece slot bonne\n");
	else
		printf("piece slot mauvais\n");
	//	sem_post(SemDisp);
	nanoWait(2,0);
	//sem_post(SemDisp);
	// A verifier !!!!
	/***************************
	 *  Section Grappin
	 ***************************/
	printf("Utilisation du grappin\n");
	sem_wait(SemArbre);
	nanoWait(1,0);
	readAct();
	
	u_act.act.GA = 1;
	writeAct();
	nanoWait(0,500000000);
	readAct();
	u_act.act.AA = 1;
	writeAct();
	nanoWait(1,0);
	printf("AA 11\n");fflush(stdout);
	readCapt();
	while(u_capt.capt.AP != 1)
	{
		readCapt();
	}
	readAct();
	u_act.act.GA = 0;
	writeAct();
	nanoWait(1,0);
	readCapt();
	readAct();
	u_act.act.AA = 0;
	writeAct();
	while(u_capt.capt.AP != 0)
	{
		readCapt();
	}
	nanoWait(0,500000000);
	sem_post(SemArbre);
	/********************************
	 *  Section CutOut
	 ********************************/
	readCapt();
	while(u_capt.capt.L1 != 1)
	{
		readCapt();
	}
	while(u_capt.capt.L2 != 1)
	{
		readCapt();
	}
	if((u_capt.capt.L1 == 1) && (u_capt.capt.L2 == 1))
	{
		CutOut = 1;
		printf("CutOut correct\n");
	}
	else
	{
		printf("CutOut Mauvais");
	}
	/***********************************
	 *  Section Choix destination piece
	 ***********************************/
	nanoWait(2,0);
	if((Slot == 1) && (CutOut == 1))
	{
		/*********************************
		 *  Piece dans le bon bac
		 *********************************/
		printf("Piece correcte\n");
		nanoWait(2,0);
	}
	else
	{
		/**********************************
		 *  Piece dans la poubelle
		 **********************************/
		 readAct();
		u_act.act.C1 = 0;
		u_act.act.C2 = 0;
		writeAct();
		sem_wait(SemArbre);
		sem_wait(SemDisp);
		sem_wait(SemChariot);
		
		printf("Piece incorrecte\n");
		readAct();
		u_act.act.C1 = 0;
		u_act.act.C2 = 0;
		writeAct();
		
		readAct();
		u_act.act.CP = 03;
		writeAct();
		nanoWait(0,500000000);
		readCapt();
		while(u_capt.capt.CS == 1)
		{
			readCapt();
		}
		readAct();
		u_act.act.PA = 1;
		writeAct();
		nanoWait(1,0);
		readAct();
		u_act.act.PV = 1;
		writeAct();
		nanoWait(1,0);
		readAct();
		u_act.act.PA = 0;
		writeAct();
		nanoWait(1,0);
		readAct();
		u_act.act.CP = 02;
		writeAct();
		nanoWait(0,500000000);
		readCapt();
		while(u_capt.capt.CS == 1)
		{
			readCapt();
		}
		readAct();
		u_act.act.PA = 1;
		writeAct();
		nanoWait(1,0);
		readAct();
		u_act.act.PV = 0;
		writeAct();
		nanoWait(1,0);
		readAct();
		u_act.act.PA = 0;
		writeAct();
		readAct();
		u_act.act.CP = 00;
		writeAct();
		readAct();
		u_act.act.C1 = 1;
		u_act.act.C2 = 1;
		writeAct();
		nanoWait(0,500000000);
		readCapt();
		while(u_capt.capt.CS == 1)
		{
			readCapt();
		}
		sem_post(SemChariot);
		sem_post(SemDisp);
		sem_post(SemArbre);
	}
	printf("Fin processus piece\n");
	fflush(stdout);
	return 1;
}

int processIO(void)
{
	printf("process IO\n");
	fflush(stdout);
	while(1)
	{
		//
		write( fd_petra_out , (unsigned char *)pShmIO , 1);
		read(fd_petra_in,  (unsigned char *)(pShmIO+sizeof(unsigned char)), 1);
		nanoWait(0, 5000000);
	}
}

int CheckDispenser(void)
{
	int Count = 0;
	sem_wait(SemDisp);
	readCapt();
	while(u_capt.capt.DE != 0)
	{
		printf("Stock vide\n");
		sem_post(SemDisp);
		if(Count == 60)
			return 1;
		
		nanoWait(1,0);
		Count++;
		sem_wait(SemDisp);
		readCapt();
	}
	sem_post(SemDisp);
	return 0;
}

int CheckPlace(void)
{
	int i=0;
	sem_wait(SemShm);
	sem_wait(SemDisp);
	for(i=0;i<SZTAB;i++)
	{
		if(*(pShm+i) == 0)
		{
			
			sem_post(SemDisp);
			sem_post(SemShm);
			return i;
		}
	}
	sem_post(SemDisp);
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
	u_capt.byte = *(pShmIO+sizeof(unsigned char));
	return 1;
}

int writeAct(void)
{
	*pShmIO = u_act.byte;
	return 1;
}

int readAct(void)
{
	u_act.byte = *pShmIO;
	return 1;
}

void affAll(void)
{
	//readAct();
	//readCapt();
	printf("Affiche all Act puis Capt\n");
	printf("%d\n",u_act.byte);
	printf("%d\n",u_capt.byte); 	
	fflush(stdout);
}

caddr_t initShm()
{
	int i;
	caddr_t ret;

	//on mmap le Shm
	ret = (char *)mmap(0, SZTAB, PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	if (*ret == -1)
		perror("Error mmap()\n");
	
	pShmIO = mmap(0,sizeof(unsigned char) + sizeof(unsigned char), PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	if (*pShmIO == -1)
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
	caddr_t ret = (char *)mmap(0, 5*sizeof(sem_t), PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	if (*ret == -1)
		perror("Error Shm semaphore\n ");

	//pour initialiser les variables globales des 5 semaphores
	SemShm = (sem_t*)ret;
	SemChariot = (sem_t*)(ret+sizeof(sem_t));
	SemArbre = (sem_t*)(ret+2*sizeof(sem_t));
	SemDisp = (sem_t*)(ret+3*sizeof(sem_t));
	SemAct = (sem_t*)(ret+4*sizeof(sem_t));

	//on initialise les 5 semaphores
	if (sem_init(SemShm, 1, 1) == -1)
		perror("Error sem_init() sur SemBuf\n");

	if (sem_init(SemChariot, 1, 1) == -1)
		perror("Error sem_init() sur SemPlace\n");

	if (sem_init(SemArbre, 1, 1) == -1)
		perror("Error sem_init() sur SemElem\n");	if (sem_init(SemDisp, 1, 1) == -1)
		perror("Error sem_init() sur SemDisp\n");
		
	if (sem_init(SemAct, 1, 1) == -1)
		perror("Error sem_init() sur SemAct\n");
	return ret;
}