#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h> 
#include <sys/wait.h>

#define MAX_SIZE 255
#define SEM_READER 0
#define SEM_FATHER 1
#define SEM_FATHER2 2
#define SEM_WRITER 3

int WAIT(int sem_des, short unsigned int num_semaforo){
    struct sembuf operazioni[1]={{num_semaforo,-1,0}};
    return semop(sem_des,operazioni,1);
}
int SIGNAL(int sem_des, short unsigned int num_semaforo){
    struct sembuf operazioni[1]={{num_semaforo,+1,0}};
    return semop(sem_des,operazioni,1);
}

/* controlla se una data stringa è palindroma */
int check(char *buffer){
    int length=strlen(buffer);
    int flag=0;

    for(int i=0; i<length; i++){
        if(buffer[i]!=buffer[length-i-1])
            flag=1;
            break;
    }
    if(flag){
        return 0;
    }
    else 
        return 1;
}

void reader(int file, int size, char *memoria, int sem_id){
    printf("Sono il processo lettore\n");
    char *input;
    char letto[MAX_SIZE];
    char *token;

    /* mappa il file in memoria */
    if((input=(char *)mmap(NULL,size,PROT_READ|PROT_EXEC,MAP_PRIVATE,file,0))==MAP_FAILED){
        perror("mmap");
        exit(1);
    }

    int i=0, fine=0, fineparola=0;
    /* copia ogni parola del file dentro la memoria condivisa */
    while(i<size){
        fineparola=0;
        WAIT(sem_id, SEM_READER);

        while(fineparola==0){
            letto[i-fine]=input[i];
            if(input[i]=='\n'){
                fineparola=1;
                letto[i-fine]='\0';
            }
            i++;
        }
        strcpy(memoria,letto);
        printf("[reader] scrivo sulla memoria: %s\n",memoria);
        fine=i;
        SIGNAL(sem_id, SEM_FATHER);
    }
    /* messaggio che indica la fine del file */
    WAIT(sem_id, SEM_READER);
    strcpy(memoria, "-1");
    printf("[reader] scrivo sulla memoria: %s\n",memoria);
    SIGNAL(sem_id, SEM_FATHER);
    //rimuove la mappatura del file */
    munmap(input,size);
    exit(0);
}

void writer(int file, char *memoria, int sem_id){
    printf("Sono il processo scrittore\n");
    int non_finito=1;
    char scrivi[MAX_SIZE];
    
    /* ciclo per scirvere sul file */
    while(non_finito==1){
        WAIT(sem_id,SEM_WRITER);
        /* controlla trova il carattere di terminazione del file */
        if(strcmp(memoria,"-1")==0){
            /* se presente esce dal ciclo e termina */
            non_finito=0;
        }
        else{
            /* copia in una stringa la parola contenuta in memoria */
            strcpy(scrivi,memoria);
            strcat(scrivi,"\n");
            /* scrive sul file la stringa */
            if(write(file,scrivi,strlen(scrivi))==-1){
                perror("write");
                exit(1);
            }
        }
        SIGNAL(sem_id, SEM_FATHER2);
    }
    exit(0);
}

void parent(char *memoriaRP, char *memoriaWP, int size, int sem_id){
    printf("Sono il processo padre\n");
    char stringa[MAX_SIZE];
    int palindroma=0;
    int non_finito=1;

    /* ciclo per leggere dalla memoria e scrivere sulla memoria */
    while(non_finito==1){
        WAIT(sem_id, SEM_FATHER);
        printf("[parent] trovo sulla memoria: %s\n",memoriaRP);
        /* scrive su stringa il contenuto della memoria condivisa R-P */
        strcpy(stringa, memoriaRP);
        //SIGNAL(sem_id, MUTEXRP);
        SIGNAL(sem_id, SEM_READER);

        /* se contine il carattere di terminazione del file termina il ciclo */
        if(strcmp(stringa,"-1")==0){
            non_finito=0;
            palindroma=1;
        }
        /* controlla se la parola è palindroma */
        else{
            printf("sono entrato\n");
            if(check(memoriaRP)==1){
                palindroma=1;
            }
        }
        /* se palindroma scrive la parola sulla memoria condivisa P-W */
        if(palindroma==1){
            WAIT(sem_id, SEM_FATHER2);
            printf("[parent] scrivo sulla memoria: %s\n",memoriaRP);
            strcpy(memoriaWP,memoriaRP);
            //SIGNAL(sem_id, MUTEXWP);
            SIGNAL(sem_id, SEM_WRITER);
        }
    }
    exit(0);
}

int main(int argc, char *argv[]){
    int file_in, file_out;
    int shmRP, shmPW;
    int sem_id;
    char *memoriaPW;
    char *memoriaRP;
    struct stat statbuf;

    if(argc<2 || argc>3){
        printf("Utilizzo: %s <file-input> [file-output]\n",argv[0]);
        exit(1);
    }

    /* apre il file di input */
    if((file_in=open(argv[1],O_RDONLY))==-1){
        perror("open");
        exit(1);
    }

    /* apre il file di output */
    if(argc==3){
        if((file_out=open(argv[2],O_CREAT|O_WRONLY|O_TRUNC,0660))==-1){
            perror("open");
            exit(1);
        }
    }
    else{
        /* se non è presente il file di output, allora stampa sulla standard output */
        file_out=1;
    }

    /* prende informazioni sul file di input */
    if(stat(argv[1],&statbuf)==-1){
        perror("stat");
        exit(1);
    }

    /* crea le aree di memoria condivisa */
    if((shmPW=shmget(IPC_PRIVATE,MAX_SIZE,IPC_CREAT|0660))==-1){
        perror("shmget");
        exit(1);
    }
    if((shmRP=shmget(IPC_PRIVATE,MAX_SIZE,IPC_CREAT|0660))==-1){
        perror("shmget");
        exit(1);
    }

    /* annette le aree di memoria */
    if((memoriaRP=(char *)shmat(shmRP,NULL,0))==(char *)-1){
        perror("shmat");
        exit(1);
    }
    if((memoriaPW=(char *)shmat(shmPW,NULL,0))==(char *)-1){
        perror("shmat");
        exit(1);
    }

    /* crea i semafori */
    if((sem_id=semget(IPC_PRIVATE,4,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("semget");
        exit(1);
    }

    /* imposta i valori iniziali dei semafori */
    semctl(sem_id,SEM_READER,SETVAL,1);
    semctl(sem_id,SEM_FATHER,SETVAL,0);
    semctl(sem_id,SEM_FATHER2,SETVAL,1);
    semctl(sem_id,SEM_WRITER,SETVAL,0);

    if(fork()==0){
        reader(file_in,statbuf.st_size,memoriaRP,sem_id);
    }
    else{
        if(fork()==0){
            writer(file_out,memoriaPW,sem_id);
        }
        else{
            parent(memoriaRP,memoriaPW,statbuf.st_size,sem_id);
            wait(NULL);
            wait(NULL);
            /* chiude il file in input */
            close(file_in);

            /* libera le aree di memoria */
            if(shmctl(shmRP,IPC_RMID,NULL)==-1){
                perror("shmctl");
                exit(1);
            }
            if(shmctl(shmPW,IPC_RMID,NULL)==-1){
                perror("shmctl");
                exit(1);
            }

            /* rimuove i semafori */
            semctl(sem_id,SEM_READER,IPC_RMID,0);
            semctl(sem_id,SEM_FATHER,IPC_RMID,0);
            semctl(sem_id,SEM_FATHER2,IPC_RMID,0);
            semctl(sem_id,SEM_WRITER,IPC_RMID,0);

            exit(0);
        }
    }
}