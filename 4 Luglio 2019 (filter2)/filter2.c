#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h> 
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

#define MAX_LEN 1024

enum cmd{UP,LOW,SUB};

int WAIT(int sem_des, short unsigned int num_semaforo){
    struct sembuf operazioni[1]={{num_semaforo,-1,0}};
    return semop(sem_des,operazioni,1);
}

int SIGNAL(int sem_des, short unsigned int num_semaforo){
    struct sembuf operazioni[1]={{num_semaforo,+1,0}};
    return semop(sem_des,operazioni,1);
}

void filter(char *memoria, int sem_id, int n, enum cmd c, char *parola1, char *parola2){
    printf("ENTER FILTER-%i: <%s> <%s>\n",n,parola1,parola2);
    char *tmp;      //puntatore temporaneo per la ricerca all'interno della riga
    char *curr;     //puntatore al carattere corrente all'interno della memoria condivisa
    char tmp_buf[MAX_LEN];  //dove manteniamo il resto della stringa quando sostituiamo una parola
    char end=0;

    while(!end){
        WAIT(sem_id,n);
        /* controlla se il file è finito */
        if(memoria[0]==EOF){
            end=1;
            continue;
        }

        /* controlla il filtro da utilizzare */
        switch(c){
            case UP:
                tmp=memoria;
                /* controlla se è presente la sottostringa */
                while((curr=strstr(tmp,parola1))!=NULL){
                    /* trasforma la sottostringa in maiuscolo */
                    for(int i=0; i<strlen(parola1); i++){
                        curr[i]=toupper(curr[i]);
                    }
                    tmp=curr+strlen(parola1);
                }
                break;
            case LOW:
                tmp=memoria;
                /* controlla se è presenta la sottostringa */
                while((curr=strstr(tmp,parola1))!=NULL){
                    /* trasforma la sottostringa in minuscolo */
                    for(int i=0; i<strlen(parola1); i++)
                        curr[i]=tolower(curr[i]);
                    tmp=curr+strlen(parola1);
                }
                break;
            case SUB:
                tmp=memoria;
                while((curr=strstr(tmp,parola1))!=NULL){
                    strcpy(tmp_buf,curr+strlen(parola1));
                    strcpy(curr,parola2);
                    strcat(tmp,tmp_buf);
                    tmp=curr+strlen(parola2);
                }
                break;
        }
        SIGNAL(sem_id,n+1);
    }
    SIGNAL(sem_id, n+1);
    exit(0);
}

int main(int argc, char *argv[]){
    int fd;
    struct stat statbuf;
    FILE *file;
    int memoria_id, sem_id;
    char *memoria;
    int n_filter=0;
    enum cmd to_send;
    char *parola1;
    char *parola2;
    int max_diff;

    /* Controlla gli argomenti passati */
    if(argc<3){
        printf("Utilizzo: %s <file.txt> <filter-1> [filter-2] [...]\n",argv[0]);
        return -1;    
    }

    /* apre il file passato in sola lettura */
    if((fd=open(argv[1],O_RDONLY))==-1){
        perror("open");
        return -1;
    }

    /* ricava informazioni sul file */
    if((fstat(fd,&statbuf))==-1){
        perror("fstat");
        return -1;
    }

    /* controlla se è un file regolare */
    if((!S_ISREG(statbuf.st_mode))){
        perror("S_ISREG");
        return -1;
    }

    /* apre uno stream associato al file */
    if((file=fdopen(fd,"r"))==NULL){
        perror("fdopen");
        return -1;
    }

    /* crea la memoria condivisa */
    if((memoria_id=shmget(IPC_PRIVATE,MAX_LEN,IPC_CREAT|IPC_EXCL|0700))==-1){
        perror("shmget");
        return -1;
    }

    /* annette la memoria condivisa */
    if((memoria=(char *)shmat(memoria_id,NULL,0))==(char *)-1){
        perror("shmat");
        return -1;
    }

    /* crea i semafori */
    if((sem_id=semget(IPC_PRIVATE,argc-1,IPC_CREAT|IPC_EXCL|0700))==-1){
        perror("semget");
        return -1;
    }

    for(int i=2; i<argc; i++){
        parola1=&argv[i][1];
        parola2=NULL;

        if(strlen(argv[i])==1)
            continue;

        switch(argv[i][0]){
            case '^':
                to_send=UP;
                break;
            case '_':
                to_send=LOW;
                break;
            case '%':
                to_send=SUB;
                parola2=strchr(&argv[i][1],'|');
                *parola2='\0';
                parola2++;
                if((strlen(parola2)-strlen(parola1))>max_diff){
                    max_diff=(strlen(parola2)-strlen(parola1));
                }
                break;
            default:
                continue;
        }
        semctl(sem_id,n_filter,SETVAL,0);

        if(fork()==0){
            filter(memoria,sem_id,n_filter,to_send,parola1,parola2);
            exit(0);
        }
        n_filter++;
    }
    if(n_filter==0){
        printf("DIO MERDA SAI LEGGERE???\n");
        exit(1);
    }

    semctl(sem_id,n_filter,SETVAL,0);

    while(fgets(memoria,MAX_LEN-max_diff,file)!=NULL){
        SIGNAL(sem_id,0);
        WAIT(sem_id,n_filter);
        printf("%s",memoria);
    }
    memoria[0]=EOF;
    SIGNAL(sem_id,0);
    WAIT(sem_id,n_filter);

    shmctl(memoria_id,IPC_RMID,NULL);
    semctl(sem_id,0,IPC_RMID);

    return 0;
}