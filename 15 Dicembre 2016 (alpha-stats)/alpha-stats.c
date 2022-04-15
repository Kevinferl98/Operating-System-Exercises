#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/sem.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define ALPHABET_SIZE 26
#define SEM_CHILD 0
#define SEM_FATHER 1

int WAIT(int sem_id, short unsigned int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,-1,0}};
    return semop(sem_id, operazioni, 1);
}

int SIGNAL(int sem_id, short unsigned int num_semaforo){
    struct sembuf operazioni[1] = {{num_semaforo,+1,0}};
    return semop(sem_id, operazioni, 1);
}

typedef struct{
    int stat[ALPHABET_SIZE];
    int figli_terminati;
} mem;

void figlio(char *input, int mem_id, int sem_id){
    int fd;
    struct stat statbuf;
    char *file;
    mem *memoria;
    size_t bytes=0;

    /* apre il file in sola lettura */
    if((fd=open(input,O_RDONLY))==-1){
        perror("open");
        exit(1);
    }

    /* ricava informazioni sul file */
    if((stat(input,&statbuf))==-1){
        perror("stat");
        exit(1);
    }

    /* annette la memoria condivisa */
    if((memoria=(mem *)shmat(mem_id,NULL,0))==(mem *)-1){
        perror("shmat");
        exit(1);
    }

    /* mappa il file in memoria */
    if((file=(char *)mmap(NULL,statbuf.st_size,PROT_READ,MAP_PRIVATE,fd,0))==MAP_FAILED){
        perror("mmap");
        exit(1);
    }
    
    /* controlla i caratteri presenti */
    WAIT(sem_id,SEM_CHILD);
    for(int i=0; i<statbuf.st_size; i++){
        if(file[i]>='A' && file[i]<='z'){
            int index=toupper(file[i])-'A';
            memoria->stat[index]++;
        }
    }
    SIGNAL(sem_id,SEM_FATHER);

    /* segnala la sua terminazione */
    WAIT(sem_id,SEM_CHILD);
    memoria->figli_terminati++;
    SIGNAL(sem_id,SEM_FATHER);

    exit(0);
}

void padre(int mem_id, int sem_id, int num_figli){
    mem *memoria;
    int frequenze[ALPHABET_SIZE];

    /* annette la memoria condivisa */
    if((memoria=(mem *)shmat(mem_id,NULL,0))==(mem *)-1){
        perror("shmat");
        exit(1);
    }

    /* azzera le occorrenze totali */
    for(int i=0; i<ALPHABET_SIZE; i++){
        frequenze[i]=0;
    }

    /* azzera le occorrenze in memoria condivisa */
    WAIT(sem_id,SEM_FATHER);
    for(int i=0; i<ALPHABET_SIZE; i++)
        memoria->stat[i]=0;
    SIGNAL(sem_id,SEM_CHILD);

    while(true){
        WAIT(sem_id,SEM_FATHER);

        /* controlla se tutti i figli hanno terminato */
        if(memoria->figli_terminati==num_figli)
            break;

        /* aggorna le occorrenze totali */
        for(int i=0; i<ALPHABET_SIZE; i++){
            frequenze[i]+=memoria->stat[i];
        }
        SIGNAL(sem_id,SEM_CHILD);
    } 

    printf("frequenze: \n");
    for(int i = 0; i < ALPHABET_SIZE; ++i) {
        printf("%c = %f%%\n", i +'a', (float)((float)frequenze[i]/26*100));
    }

    exit(0);
}

int main(int argc, char *argv[]){
    int mem_id;
    int sem_id;

    /* controlla gli argomenti passati */
    if(argc<2){
        printf("Utilizzo: %s <file-1> [file-2] [file-3] [...]\n",argv[0]);
        exit(1);
    }

    /* crea la memoria condivisa */
    if((mem_id=shmget(IPC_PRIVATE,sizeof(mem),IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("shmget");
        exit(1);
    }

    /* crea i semafori */
    if((sem_id=semget(IPC_PRIVATE,2,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("semget");
        exit(1);
    }

    /* inizializza i semafori */
    semctl(sem_id,SEM_FATHER,SETVAL,1);
    semctl(sem_id,SEM_CHILD,SETVAL,0);

    for(int i=1; i<argc; i++){
        if(fork()==0){
            figlio(argv[i],mem_id,sem_id);
        }
    }
    padre(mem_id,sem_id,argc-1);

    for(int i=1; i<argc; i++){
        wait(NULL);
    }
    
    shmctl(mem_id,IPC_RMID,0);
}