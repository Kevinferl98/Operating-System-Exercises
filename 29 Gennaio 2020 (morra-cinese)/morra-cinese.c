#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>

#define SEM_G 0
#define SEM_P1 1
#define SEM_P2 2
#define SEM_T 3

int WAIT(int sem_des, short unsigned int num_semaforo){
    struct sembuf operazioni[1]={{num_semaforo,-1,0}};
    return semop(sem_des,operazioni,1);
}

int SIGNAL(int sem_des, short unsigned int num_semaforo){
    struct sembuf operazioni[1]={{num_semaforo,+1,0}};
    return semop(sem_des,operazioni,1);
}

/* struttura della memoria condivisa */
typedef struct{
    char mossa_p1;
    char mossa_p2;
    int vincitore;
    int partite_rimanenti;
} mem;

/* confronta le mosse e ritorna il vincitore */
int getVincitore(mem *memoria){
    if(memoria->mossa_p1==memoria->mossa_p2)
        return 0;

    switch(memoria->mossa_p1){
        case 'S':
            switch(memoria->mossa_p2){
                case 'C': return 2;
                case 'F': return 1;
            }
        case 'C':
            switch(memoria->mossa_p2){
                case 'S': return 1;
                case 'F': return 2;
            }
        case 'F':
            switch(memoria->mossa_p2){
                case 'C': return 1;
                case 'S': return 2;
            }
    }
}

/* genera una mossa random */
char random_mossa(){
    int val=rand()%3;
    
    switch(val){
        case 0: return 'S';
        case 1: return 'C';
        case 2: return 'F';
    }
}

void giocatore(int id, mem *memoria, int sem_id){
    srand(time(NULL)+id);

    /* gioca finche le partite non terminano */
    while(memoria->partite_rimanenti>0){
        /* ricava la mossa da random_mossa */
        char mossa=random_mossa();

        printf("P%d: mossa %c\n",id,mossa);
        /* il giocatore 1 o 2 inserisce in memoria la mossa */
        if(id==1){
            memoria->mossa_p1=mossa;
        }
        else if(id==2){
            memoria->mossa_p2=mossa;
        }

        /* segnala la sua terminazione al giudice */
        SIGNAL(sem_id, SEM_G);
        /* il giocatore 1 o 2 attendono di riniziare la partita */
        if(id==1){
            WAIT(sem_id,SEM_P1);
        }
        else if(id==2){
            WAIT(sem_id,SEM_P2);
        }
    }

    exit(0);
}

/* valuta il vincitore e invia il risultato al tabellone */
void giudice(mem *memoria, int sem_id){
    int num=0;

    while(memoria->partite_rimanenti>0){
        num++;
        /* aspetta che tutti e due i giocatori terminano la partita */
        WAIT(sem_id,SEM_G);
        WAIT(sem_id,SEM_G);
        /* ricava il vincitore da getVincitore */
        int vincitore=getVincitore(memoria);
        /* se sono in pareggio lancia una nuova partita */
        if(vincitore==0){
            printf("G: partita n.%d patta e quindi da ripetere\n");
            SIGNAL(sem_id,SEM_P1);
            SIGNAL(sem_id,SEM_P2);
            continue;
        }
        /* decrementa il numero di partite rimanenti */
        memoria->partite_rimanenti--;
        /* invia il risultato al tabellone e lancia una nuova partita */
        printf("G: partita n.%d vinta da P%d\n",num,vincitore);
        memoria->vincitore=vincitore;
        SIGNAL(sem_id,SEM_T);
        WAIT(sem_id,SEM_G);
        SIGNAL(sem_id,SEM_P1);
        SIGNAL(sem_id,SEM_P2);
    }

    exit(0);
}

/* Tiene la classifica delle vittore dei giocatori */
void tabellone(mem *memoria, int sem_id){
    int p1_vittorie=0, p2_vittorie=0;

    while(memoria->partite_rimanenti>0){
        /* aspetta il risultato dal giudice */
        WAIT(sem_id,SEM_T);
        switch(memoria->vincitore){
            case 1:
                p1_vittorie++;
                break;
            case 2:
                p2_vittorie++;
                break;
        }
        printf("T: classifica temporanea: P1=%d P2=%d\n",p1_vittorie,p2_vittorie);
        SIGNAL(sem_id,SEM_G);
    }

    printf("T: classifica finale: P1=%d P2=%d\n",p1_vittorie,p2_vittorie);
    if(p1_vittorie>p2_vittorie)
        printf("T: vincitore del torneo: P1\n");
    else if(p2_vittorie>p1_vittorie)
        printf("T: vincitore del torneo: P2\n");

    exit(0);
}

int main(int argc, char *argv[]){
    int num_partite;
    int mem_id;
    mem *memoria;
    int sem_id;

    /* controlla gli argomenti passati */
    if(argc!=2){
        printf("Utilizzo: %s <numero-partite>\n",argv[0]);
        exit(1);
    }

    num_partite=atoi(argv[1]);

    /* crea la memoria condivisa */
    if((mem_id=shmget(IPC_PRIVATE,sizeof(memoria), IPC_CREAT|IPC_EXCL|0600))==-1){
        perror("shmget");
        exit(1);
    }

    /* annette la memoria condivisa */
    if((memoria=(mem *)shmat(mem_id,NULL,0))==(mem *)-1){
        perror("shmat");
        exit(1);
    }

    memoria->partite_rimanenti=num_partite;

    /* crea i semafori */
    if((sem_id=semget(IPC_PRIVATE,4,IPC_CREAT|IPC_EXCL|0600))==-1){
        perror("semget");
        exit(1);
    }

    /* inizializza i semafori */
    semctl(sem_id,0,SETVAL,0);
    semctl(sem_id,1,SETVAL,0);
    semctl(sem_id,2,SETVAL,0);
    semctl(sem_id,3,SETVAL,0);

    if(fork()==0){
        giocatore(1,memoria,sem_id);
    }
    if(fork()==0){
        giocatore(2,memoria,sem_id);
    }
    if(fork()==0){
        giudice(memoria,sem_id);
    }
    tabellone(memoria,sem_id);

    wait(NULL);
    wait(NULL);
    wait(NULL);

    /* rimuove la memoria condivisa */
    shmctl(mem_id,IPC_RMID,NULL);
    /* rimuove i semafori */
    semctl(sem_id,IPC_RMID,0);
}