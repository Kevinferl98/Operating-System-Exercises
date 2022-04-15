#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>

#define MAX_LEN 1024
#define SEM_FIGLIO 0
#define SEM_PADRE 1
#define SEM_DB 2
#define SEM_OUT 3

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
    char parola[MAX_LEN];
    int val;
    int terminato;
    int processo;
} mem;

void IN(char *input, int mem_id, int sem_id, int n){
    FILE *file;
    char buffer[MAX_LEN];
    mem *memoria;
    int i=1;

    /* apre il file in sola lettura */
    if((file=fopen(input,"r"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* annette la memoria condivisa */
    if((memoria=(mem *)shmat(mem_id,NULL,0))==(mem *)-1){
        perror("shmat");
        exit(1);
    }

    /* legge riga per riga dal file */
    while(fgets(buffer,MAX_LEN,file)){
        /* pone la riga letta dentro la memoria condivisa */
        if(buffer[strlen(buffer)-1]=='\n')
            buffer[strlen(buffer)-2]='\0';
        WAIT(sem_id,SEM_FIGLIO);
        memoria->terminato=0;
        memoria->processo=n;
        strcpy(memoria->parola,buffer);
        printf("IN%d: inviata query n.%d %s\n",n,i,memoria->parola);
        i++;
        SIGNAL(sem_id,SEM_PADRE);
    }
    /* comunica la terminazione al DB */
    WAIT(sem_id,SEM_FIGLIO);
    memoria->terminato=1;
    SIGNAL(sem_id,SEM_PADRE);
    
    exit(0);
}

void DB(char *input, int mem_id, int mem_id2, int sem_id){
    FILE *file;
    mem *memoria;
    mem *memoria2;
    char buffer[MAX_LEN];
    char *token;
    int val;
    int figli_terminati=0;

    /* apre il file in sola lettura */
    if((file=fopen(input,"r"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* annette la prima memoria condivisa: IN-DB */
    if((memoria=(mem *)shmat(mem_id,NULL,0))==(mem *)-1){
        perror("shmat");
        exit(1);
    }

    /* annette la seconda memoria condivisa: DB-OUT */
    if((memoria2=(mem *)shmat(mem_id2,NULL,0))==(mem *)-1){
        perror("shmat");
        exit(1);
    }

    while(true){
        WAIT(sem_id,SEM_PADRE);
        /* controlla se i figli IN hanno terminato */
        if(memoria->terminato==1){
            figli_terminati++;
            if(figli_terminati==2)
                break;
        }
        /* legge il database riga per riga */
        while(fgets(buffer,MAX_LEN,file)){
            /* estra la parola e il valore per ogni riga del database */
            token=strtok(buffer,":");
            val=atoi(strtok(NULL,":"));
            /* se la stringa è presenta invia la parola e il valore al processo OUT */
            if(strcmp(token,memoria->parola)==0){
                printf("DB: query %s trovata con valore %d\n",token,val);
                WAIT(sem_id,SEM_DB);
                memoria2->terminato=0;
                memoria2->processo=memoria->processo;
                strcpy(memoria2->parola,memoria->parola);
                memoria2->val=val;
                SIGNAL(sem_id,SEM_OUT);
            }
        }
        SIGNAL(sem_id,SEM_FIGLIO);
        rewind(file);
    }
    /* comunica la terminazione al processo OUT */
    WAIT(sem_id,SEM_DB);
    memoria2->terminato=1;
    SIGNAL(sem_id,SEM_OUT);

    exit(1);
}

void OUT(int mem_id2, int sem_id){
    mem *memoria;
    int val1=0, val2=0;
    int tot1=0, tot2=0;

    /* annette la memoria condivisa */
    if((memoria=(mem *)shmat(mem_id2,NULL,0))==(mem *)-1){
        perror("shmat");
        exit(1);
    }

    int i=1; 
    while(true){
        WAIT(sem_id,SEM_OUT);
        /* controlla se il DB ha terminato */
        if(memoria->terminato==1){
            break;
        }
        if(memoria->processo==1){
            val1++;
            tot1+=memoria->val;
        }
        else if(memoria->processo==2){
            val2++;
            tot2+=memoria->val;
        }
        SIGNAL(sem_id,SEM_DB);
        i++;
    }
    printf("OUT: ricevuti n.%d valori validi per IN1 con totale %d\n",val1,tot1);
    printf("OUT: ricevuti n.%d valori validi per IN2 con totale %d\n",val2,tot2);

    exit(0);
}

int main(int argc, char *argv[]){
    int mem_id, mem_id2;
    int sem_id;

    /* controlla gli argomenti passati */
    if(argc!=4){
        printf("Utilizzo: %s <db-file> <query-file-1> <query-file-2>\n",argv[0]);
        exit(1);
    }

    /* crea la prima area di memoria: IN-DB */
    if((mem_id=shmget(IPC_PRIVATE,sizeof(mem),IPC_PRIVATE|IPC_EXCL|0660))==-1){
        perror("shmget");
        exit(1);
    }

    /* crea la seconda area di memoria: DB-OUT */
    if((mem_id2=shmget(IPC_PRIVATE,sizeof(mem),IPC_PRIVATE|IPC_EXCL|0660))==-1){
        perror("shmget");
        exit(1);
    }

    /* crea i semafori */
    if((sem_id=semget(IPC_PRIVATE,4,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("semget");
        exit(1);
    }

    /* inizializza i semafori */
    semctl(sem_id,SEM_FIGLIO,SETVAL,1);
    semctl(sem_id,SEM_PADRE,SETVAL,0);
    semctl(sem_id,SEM_DB,SETVAL,1);
    semctl(sem_id,SEM_OUT,SETVAL,0);

    /* avvia tutti i processi figli */
    if(fork()==0){
        IN(argv[2],mem_id,sem_id,1);
    }
    if(fork()==0){
        IN(argv[3],mem_id,sem_id,2);
    }
    if(fork()==0){
        DB(argv[1],mem_id,mem_id2,sem_id);
    }
    if(fork()==0){
        OUT(mem_id2, sem_id);
    }
    /* aspetta la terminazione di tutti i processi */
    wait(NULL);
    wait(NULL);
    wait(NULL);
    wait(NULL);

    /* rimuove le aree di memoria e i semafori */
    shmctl(mem_id,IPC_RMID,NULL);
    shmctl(mem_id,IPC_RMID,NULL);
    semctl(sem_id,0,IPC_RMID);

}