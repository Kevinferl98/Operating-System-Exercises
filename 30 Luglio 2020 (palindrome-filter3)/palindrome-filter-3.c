#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>

#define MAX_LEN 64

#define SEM_R 0
#define SEM_P 1
#define SEM_W 2

int WAIT(int sem_des, short unsigned int num_semaforo){
    struct sembuf operazioni[1]={{num_semaforo,-1,0}};
    return semop(sem_des,operazioni,1);
}

int SIGNAL(int sem_des, short unsigned int num_semaforo){
    struct sembuf operazioni[1]={{num_semaforo,+1,0}};
    return semop(sem_des,operazioni,1);
}

/* controlla se la stringa passata è palindroma */
int check(char *buffer){
    int length=strlen(buffer);
    int flag=0;

    for(int i=0; i<length; i++){
        if(buffer[i]!=buffer[length-i-1]){
            flag=1;
            break;
        }
    }
    if(flag){
        return 0;
    }
    else 
        return 1;
}

void reader(char *memoria, char *input, int sem_id){
    printf("Avvio il figlio Reader\n");
    FILE *file;
    char buffer[MAX_LEN];

    /* apre il file in sola lettura con uno stream */
    if((file=fopen(input,"r"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* legge dal file riga per riga */
    while(fgets(buffer,MAX_LEN,file)!=NULL){
        WAIT(sem_id,SEM_R);
        /* copia la riga in memoria condivisa */
        strcpy(memoria,buffer);
        //printf("[READER] Parola copiata nella memoria: %s\n",memoria);
        SIGNAL(sem_id,SEM_P);
    }
    WAIT(sem_id,SEM_R);
    /* copia in memoria condivisa il carattere di terminazione */
    strcpy(memoria,"-1");
    //printf("[READER] Parola copiata nella memoria: %s\n",memoria);
    SIGNAL(sem_id,SEM_P);
    SIGNAL(sem_id,SEM_P);

    exit(0);
}

void padre(char *memoria, int sem_id){
    printf("Avvio il padre\n");
    char buffer[MAX_LEN];
    int non_finito=1;

    while(non_finito==1){
        WAIT(sem_id,SEM_P);
        /* legge dalla memoria condivisa */
        strcpy(buffer,memoria);
        /* controlla se il reader ha terminato */
        if(strcmp(buffer,"-1")==0){
            non_finito=0;
        }
        //printf("[PADRE] Parola trovata nella memoria: %s\n",buffer);
        /* controlla se la stringa è palindroma */
        if(buffer[strlen(buffer)-1]=='\n')
            buffer[strlen(buffer)-2]='\0';
        if(check(buffer)){
            /* se palindroma copia la stringa sulla memoria condivisa */
            strcpy(memoria,buffer);
            SIGNAL(sem_id,SEM_W);
        }
        else{
            /* altrimenti aspetta un'altra stringa da controllare */
            SIGNAL(sem_id,SEM_R);
        }
    }
    WAIT(sem_id,SEM_P);
    /* copia in memoria condivisa il carattere di terminazione */
    strcpy(memoria,"-1");
    //printf("[PADRE] Parola copiata nella memoria: %s\n",memoria);
    SIGNAL(sem_id,SEM_W);

    exit(0);
}

void writer(char *memoria, int sem_id, char *output){
    printf("Avvio il figlio Writer\n");
    char buffer[MAX_LEN];
    FILE *file;

    if((file=fopen(output,"w"))==NULL){
        perror("fopen");
        exit(1);
    }

    while(true){
        WAIT(sem_id,SEM_W);
        /* legge dalla memoria condivisa */
        strcpy(buffer,memoria);
        if(strcmp(buffer,"-1")==0){
            break;
        }
        fputs(buffer,file);
        fputs("\n",file);
        //printf("[WRITER] Parola palindroma: %s\n",buffer);
        /* Controlla se il padre ha terminato */
        SIGNAL(sem_id,SEM_R);
    }

    exit(0);
}

int main(int argc, char *argv[]){
    char *input;
    int sem_id;
    int mem_id;
    char *memoria;

    /* Controlla gli argomenti passati */
    if(argc<2){
        printf("Utilizzo: %s [input file] [output file]\n",argv[0]);
        return -1;
    }

    strcpy(input,argv[1]);

    /* crea la memoria condivisa */
    if((mem_id=shmget(IPC_PRIVATE,MAX_LEN,IPC_CREAT|IPC_EXCL|0600))==-1){
        perror("shmget");
        return -1;
    }

    /* annette la memoria condivisa */
    if((memoria=(char *)shmat(mem_id,NULL,0))==(char *)-1){
        perror("shmat");
        return -1;
    }

    /* crea i semafori */
    if((sem_id=semget(IPC_PRIVATE,4,IPC_CREAT|IPC_EXCL|0600))==-1){
        perror("semget");
        return -1;
    }

    /* inizializza i semafori */
    semctl(sem_id,0,SETVAL,1);
    semctl(sem_id,1,SETVAL,0);
    semctl(sem_id,2,SETVAL,0);

    if(fork()==0){
        reader(memoria,input,sem_id);
    }
    if(fork()==0){
        writer(memoria,sem_id,argv[2]);
    }
    padre(memoria,sem_id);

    /* aspetta la terminazione dei processi figli */
    wait(NULL);
    wait(NULL);

    /* rimuove memoria condivisa e semafori */
    shmctl(mem_id,IPC_RMID,NULL);
    semctl(sem_id,IPC_RMID,0);
}