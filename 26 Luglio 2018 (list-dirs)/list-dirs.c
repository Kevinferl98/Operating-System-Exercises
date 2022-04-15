#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_SIZE 1024
#define SEM_MUTEX 0
#define SEM_FILE 1
#define SEM_DIR 2

int WAIT(int sem_id, short unsigned int num_semaforo){
    struct sembuf operazioni[1]={{num_semaforo,-1,0}};
    return semop(sem_id,operazioni,1);
}
int SIGNAL(int sem_id, short unsigned int num_semaforo){
    struct sembuf operazioni[1]={{num_semaforo,+1,0}};
    return semop(sem_id,operazioni,1);
}

/* struttura della memoria condivisa */
typedef struct{
    char filename[MAX_SIZE];
    char dir_name[MAX_SIZE];
    int size;
    int exit;
} memoria;

int Reader(memoria *mem, char *directory, int sem_id){
    DIR *dir;
    struct dirent *voce;
    struct stat statbuf;

    /* apre la directory */
    if((dir=opendir(directory))==NULL){
        perror("opendir");
        return 1;
    }

    /* legge la directory */
    while((voce=readdir(dir))!=NULL){
        if(strcmp(voce->d_name,".")!=0 && strcmp(voce->d_name,"..")!=0){
            char fullpath[MAX_SIZE];
            if(directory[strlen(directory)-1]=='/')
                sprintf(fullpath,"%s%s",directory,voce->d_name);
            else
                sprintf(fullpath,"%s/%s",directory,voce->d_name);
            /* ricava informazione sul file o directory */
            if(lstat(fullpath,&statbuf)==-1){
                perror("lstat");
                return 1;
            }
            /* se è un file regolare */
            if(voce->d_type==DT_REG){
                WAIT(sem_id,SEM_MUTEX);
                /* inserisce in memoria il nome e la dimensione */
                strcpy(mem->filename,fullpath);
                mem->size=statbuf.st_size;
                SIGNAL(sem_id,SEM_FILE);
            }
            /* se è una directory */
            else if(voce->d_type==DT_DIR){
                WAIT(sem_id,SEM_MUTEX);
                /* inserisce in memoria il nome */
                strcpy(mem->dir_name,fullpath);
                SIGNAL(sem_id,SEM_DIR);
            }
        }
    }
    /* manda il messaggio finale */
    WAIT(sem_id,SEM_MUTEX);
    mem->exit+=1;
    SIGNAL(sem_id,SEM_FILE);
    SIGNAL(sem_id,SEM_DIR);

    return 0;
}

int file_consumer(memoria *mem, int sem_id, int n){

    while(true){
        WAIT(sem_id,SEM_FILE);
        /* controlla se ha terminato */
        if(mem->exit==n){
            printf("[file_consumer]: ho terminato\n");
            break;
        }
        /* stampa il nome del file e la dimensione */
        printf("%s [file di %d bytes]\n",mem->filename,mem->size);
        SIGNAL(sem_id,SEM_MUTEX);
    }

    return 0;
}

int dir_consumer(memoria *mem, int sem_id, int n){

    while(true){
        WAIT(sem_id,SEM_DIR);
        /* controlla se ha terminato */
        if(mem->exit==n){
            printf("[dir_consumer]: ho terminato\n");
            break;
        }
        /* stampa il nome della directory */
        printf("%s [directory]\n",mem->dir_name);
        SIGNAL(sem_id,SEM_MUTEX);
    }

    return 0;
}

int main(int argc, char *argv[]){
    int memoria_id;
    memoria *mem;
    int sem_id;
    //char *directory="/mnt/c/Users/Kevin/OneDrive/Desktop/pippo";

    /* controlla gli argomenti passati */ 
    if(argc<2){
        printf("Utilizzo: %s <dir1> <dir2> <...>\n",argv[0]);
        return 1;
    }

    /* crea la memoria condivisa */
    if((memoria_id=shmget(IPC_PRIVATE,sizeof(memoria),0660|IPC_CREAT|IPC_EXCL))==-1){
        perror("shmget");
        return 1;
    }

    /* annette la memoria condivisa */
    if((mem=(memoria*)shmat(memoria_id,NULL,0))==(memoria *)-1){
        perror("shmat");
        return 1;
    }

    /* crea i semafori */
    if((sem_id=semget(IPC_PRIVATE,3,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("semget");
        return 1;
    }

    /* inizializza i semafori */
    semctl(sem_id,SEM_MUTEX,SETVAL,1);
    semctl(sem_id,SEM_FILE,SETVAL,0);
    semctl(sem_id,SEM_DIR,SETVAL,0);

    /* Crea tanti reader tante le directory passate */
    for(int i=1; i<argc; i++){
        if(fork()==0){
            Reader(mem,argv[i],sem_id);
        }
    }

    int n=argc-1;
    if(fork()==0){
        file_consumer(mem,sem_id,n);
    }
    if(fork()==0){
        dir_consumer(mem,sem_id,n);
    }

    for(int i=1; i<argc; i++){
        wait(NULL);
    }
    wait(NULL);
    wait(NULL);

    /* rimuvoe i semafori e la memoria condivisa */
    semctl(sem_id,0,IPC_RMID,0);
    shmctl(memoria_id,IPC_RMID,NULL);

    return 0;
}




