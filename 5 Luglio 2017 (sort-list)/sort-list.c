#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>

#define MAX_LEN 50
#define MAX_DIM 20
#define SEM_SORTER 0
#define SEM_COMPARER 1
#define SEM_FATHER 2

/* struttura della memoria condivisa */
typedef struct{
    int confronto;
    char stringa1[MAX_LEN];
    char stringa2[MAX_LEN];
} mem;

int WAIT(int sem_des, short unsigned int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
    return semop(sem_des, operazioni, 1);
}

int SIGNAL(int sem_des, short unsigned int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
    return semop(sem_des, operazioni, 1);
}

void sorter(char *input, int mem_id, int sem_id){
    FILE *file;
    mem *memoria;
    char parole[MAX_DIM][MAX_LEN];
    char buffer[MAX_LEN];
    int i=0; 
    int scambi=0;

    /* apre il file con uno stream in sola lettura */
    if((file=fopen(input,"r"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* annette la memoria condivisa */
    if((memoria=(mem *)shmat(mem_id,NULL,0))==(mem *)-1){
        perror("shmat");
        exit(1);
    }

    /* copia il contenuto del file dentro l'array di stringhe */
    while(fgets(buffer,MAX_LEN,file)){
        strcpy(parole[i],buffer);
        i++;
    }

    /* algoritmo di ordinamento */
    do{
        scambi=0;
        for(int j=0; j<MAX_DIM; j++){
            /* copia in memoria le due stringhe da confrontare e segnala il comparer */
            WAIT(sem_id,SEM_SORTER);
            strcpy(memoria->stringa1,parole[j]);
            strcpy(memoria->stringa2,parole[j+1]);
            memoria->confronto=0;
            SIGNAL(sem_id,SEM_COMPARER);

            /* controlla se le stringhe devono cambiare di posto */
            WAIT(sem_id,SEM_SORTER);
            if(memoria->confronto==1){
                char appoggio[MAX_LEN];
                strcpy(appoggio,parole[j]);
                strcpy(parole[j],parole[j+1]);
                strcpy(parole[j+1],appoggio);
                scambi++;
            }
            SIGNAL(sem_id,SEM_SORTER);
        }
    } while(scambi!=0);

    /* segnala la terminazione al processo comparer */
    WAIT(sem_id,SEM_SORTER);
    memoria->confronto=-1;
    SIGNAL(sem_id,SEM_COMPARER);

    /* copia in memoria parola per parola e segnala il padre ad ogni parola scritta */
    for(int j=0; j<MAX_DIM; j++){
        WAIT(sem_id,SEM_SORTER);
        memoria->confronto=0;
        strcpy(memoria->stringa1,parole[j]);
        SIGNAL(sem_id,SEM_FATHER);
    }

    /* segnala la terminazione al processo padre */
    WAIT(sem_id,SEM_SORTER);
    memoria->confronto=-1;
    SIGNAL(sem_id,SEM_FATHER);

    exit(0);
}

void comparer(int mem_id, int sem_id){
    mem *memoria;

    /* annette la memoria condivisa */
    if((memoria=(mem *)shmat(mem_id,NULL,0))==(mem *)-1){
        perror("shmat");
        exit(1);
    }

    while(true){
        WAIT(sem_id,SEM_COMPARER);
        /* controlla se il processo sorter ha terminato */
        if(memoria->confronto==-1){
            SIGNAL(sem_id,SEM_SORTER);
            break;
        }
        /* confronta le due stringhe */
        if(strcmp(memoria->stringa1,memoria->stringa2)>0){
            memoria->confronto=1;
        }
        else{
            memoria->confronto=0;
        }
        SIGNAL(sem_id,SEM_SORTER);
    }

    exit(0);

}

void padre(int mem_id, int sem_id){
    mem *memoria;

    /* annette la memoria condivisa */
    if((memoria=(mem *)shmat(mem_id,NULL,0))==(mem *)-1){
        perror("shmat");
        exit(1);
    }

    while(true){
        WAIT(sem_id,SEM_FATHER);
        /* controlla se il processo sorter ha terminato */
        if(memoria->confronto==-1){
            break;
        }
        /* stampa parola per parola */
        printf("%s",memoria->stringa1);
        SIGNAL(sem_id,SEM_SORTER);
    }

    exit(0);
}

int main(int argc, char *argv[]){
    int mem_id;
    int sem_id;

    /* controlla gli argomenti passati */
    if(argc!=2){
        printf("Utilizzo: %s <file>\n",argv[0]);
        exit(1);
    }

    /* crea la memoria condivisa */
    if((mem_id=shmget(IPC_PRIVATE,sizeof(mem),IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("shmget");
        exit(1);
    }

    /* crea i semafori */
    if((sem_id=semget(IPC_PRIVATE,3,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("semget");
        exit(1);
    }

    /* inizializza i semafori */
    semctl(sem_id,SEM_SORTER,SETVAL,1);
    semctl(sem_id,SEM_COMPARER,SETVAL,0);
    semctl(sem_id,SEM_FATHER,SETVAL,0);

    if(fork()==0){
        sorter(argv[1],mem_id,sem_id);
    }
    if(fork()==0){
        comparer(mem_id,sem_id);
    }
    padre(mem_id,sem_id);

    /* aspetta la terminazione dei processi figli */
    wait(NULL);
    wait(NULL);

    /* rimuove memoria condivisa e semafori */
    semctl(sem_id,0,IPC_RMID);
    shmctl(mem_id,IPC_RMID,NULL);
}