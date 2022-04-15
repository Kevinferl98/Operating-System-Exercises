#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>

#define MEM_SIZE 1024
#define MAX_PATH_LEN 512
#define STOP_SEQ "\1"
#define SEM_SCANNER 0
#define SEM_FATHER 1
#define SEM_STATER 2

int WAIT(int sem_des, short unsigned int num_semaforo){
    struct sembuf operazioni[1]={{num_semaforo,-1,0}};
    return semop(sem_des,operazioni,1);
}
int SIGNAL(int sem_des, short unsigned int num_semaforo){
    struct sembuf operazioni[1]={{num_semaforo,+1,0}};
    return semop(sem_des,operazioni,1);
}

void cerca(char *Path, char* memoria, int sem_id){
    DIR *dir;

    /* apre la directory */
    if((dir=opendir(Path))==NULL){
        perror("opendir");
        exit(1);
    }

    struct dirent *dirEntry=readdir(dir); /* legge la directory */
    while(dirEntry!=NULL){
        if(strcmp(dirEntry->d_name,".")!=0 && strcmp(dirEntry->d_name,"..")!=0){
            char fullPath[MEM_SIZE];
            if(Path[strlen(Path)-1]=='/')
                sprintf(fullPath,"%s%s",Path,dirEntry->d_name); /* aggiorna il path */
            else
                sprintf(fullPath,"%s/%s",Path,dirEntry->d_name); /* aggiorna il path */
            if(dirEntry->d_type==DT_DIR){  /* controlla se è una directory */
                printf("[scanner] found new directory %s\n",fullPath);
                cerca(fullPath,memoria,sem_id); /* richiama la funzione */
            }
            else if(dirEntry->d_type==DT_REG){ /* se è un file lo manda nella memoria condivisa */
                printf("[scanner] found new file %s\n",fullPath);

                WAIT(sem_id, SEM_SCANNER);
                strcpy(memoria,fullPath);
                SIGNAL(sem_id, SEM_STATER);
            }
        }
        dirEntry=readdir(dir); /* legge la directory successiva */
    }
}

/* richiama cerca per trovare i file della directory */
void scanner(char *Path, char *memoria, int sem_id){
    cerca(Path,memoria,sem_id);

    /* scrive in memoria per comunicare la sua terminazione */
    WAIT(sem_id, SEM_SCANNER);
    strcpy(memoria,STOP_SEQ);
    SIGNAL(sem_id, SEM_STATER);

    exit(0);
}

/* determina lo spazio occupato da un file e lo manda al padre */
void stater(char *memoria, int num_scanner, int sem_id){
    struct stat statbuf;
    char buffer[MEM_SIZE];

    int num_corrente=num_scanner;
    while(num_corrente>0){
        WAIT(sem_id, SEM_STATER);
        printf("[stater] received %s\n",memoria);
        /* controlla se la memoria contiene STOP_SEQ */
        if(strcmp(memoria,STOP_SEQ)!=0){
            /* ricava informazioni sul file */
            stat(memoria,&statbuf);
            /* scrive in memoria la dimensione del file */
            sprintf(buffer,"%lu",statbuf.st_blocks);
            printf("[stater] buffer is %s\n",buffer);
            strcpy(memoria + MAX_PATH_LEN,buffer);
        }
        else{
            num_scanner--;
            if(num_scanner==0){
                strcpy(memoria,STOP_SEQ);
            }
        }
        SIGNAL(sem_id, SEM_FATHER);
    }
    exit(0);
}

void father(char *memoria, int sem_id){
    int run=1;

    while(run){
        WAIT(sem_id, SEM_FATHER);
        printf("[father] received %s, %s\n",memoria, memoria+MAX_PATH_LEN);
        run=strcmp(memoria,STOP_SEQ);
        SIGNAL(sem_id, SEM_SCANNER);
    }
    exit(0);
}

int main(int argc, char *argv[]){
    int memoria_id;
    char *memoria;
    int sem_id;
    short semvals[3]={1,0,0};

    if(argc<2){
        printf("Utilizzo: %s [path-1] [path-2] [...]\n",argv[0]);
        exit(1);
    }

    if((memoria_id=shmget(IPC_PRIVATE,MEM_SIZE,IPC_CREAT|IPC_EXCL|0600))==-1){
        perror("shmget");
        exit(1);
    }

    if((memoria=(char *)shmat(memoria_id,NULL,0))==(void *)-1){
        perror("shmat");
        exit(1);
    }

    if((sem_id=semget(IPC_PRIVATE,3,0600|IPC_CREAT|IPC_EXCL))==-1){
        perror("semget");
        exit(1);
    }

    if(semctl(sem_id,0,SETALL,semvals)!=0){
        perror("semctl");
        exit(1);
    }    

    for(int i=1; i<argc; i++){
        if(fork()==0){
            scanner(argv[1],memoria,sem_id);
        }
    }
    if(fork()==0){
        stater(memoria,argc-1,sem_id);
    }
    father(memoria,sem_id);

    wait(NULL);
    for(int i=1; i<argc; i++){
        wait(NULL);
    }
}