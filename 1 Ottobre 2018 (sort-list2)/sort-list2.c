#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_LEN 255
#define MAX_DIM 50
#define DEST_SORTER 1
#define DEST_COMPARER 2

/* struttura del messaggio */
typedef struct{
    long type;
    char stringa1[MAX_LEN];
    char stringa2[MAX_LEN];
    int confronto;
    int terminato;
} msg;
 
void sorter(char *input, int msg_id, int *pipe_id){
    FILE *file, *out;
    char buffer[MAX_LEN];
    char parole[MAX_DIM][MAX_LEN];
    int j=0;
    int scambi=0;
    msg messaggio;
    
    /* apre il file con uno stream in sola lettura */
    if((file=fopen(input,"r"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* associa lo stream con la pipe in scrittura */
    if((out=fdopen(pipe_id[1],"w"))==NULL){
        perror("fdopen");
        exit(1);
    }

    /* legge le parole e le carica nel vettore di stringhe parole */
    while(fgets(buffer,MAX_DIM,file)){
        strcpy(parole[j],buffer);
        j++;
    }

    /* algoritmo di ordinamento */
    do{
        scambi=0;
        for(int i=0; i<j; i++){
            messaggio.type=DEST_COMPARER;
            strcpy(messaggio.stringa1,parole[i]);
            strcpy(messaggio.stringa2,parole[i+1]);
            messaggio.confronto=0;
            messaggio.terminato=0;
            /* manda il messaggio al comparer con le due stringhe */
            if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
                perror("msgsnd");
                exit(1);
            }
            /* riceve il messaggio dal comparer */
            if((msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),DEST_SORTER,0))==-1){
                perror("msgsnd");
                exit(1);
            }
            /* controlla se le stringhe devono cambiare di posto */
            if(messaggio.confronto==1){
                char appoggio[MAX_DIM];
                strcpy(appoggio,parole[i]);
                strcpy(parole[i],parole[i+1]);
                strcpy(parole[i+1],appoggio);
                scambi++;
            }
        }
    } while(scambi!=0);

    /* segnala la terminazione al processo comparer */
    messaggio.type=DEST_COMPARER;
    messaggio.terminato=1;
    if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
        perror("msgsnd");
        exit(1);
    }

    /* scrive le parole ordinate nella pipe */
    for(int i=0; i<MAX_DIM; i++){
        fputs(parole[i],out);
    }

    exit(0);
}

void comparer(int msg_id){
    msg messaggio;

    while(true){
        /* riceve i messaggi dal processo sorter */
        if((msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),DEST_COMPARER,0))==-1){
            perror("msgrcv");
            exit(1);
        }
        /* controlla se il processo sorter ha terminato */
        if(messaggio.terminato==1){
            break;
        }
        /* confronta le due stringhe */
        if(strcmp(messaggio.stringa1,messaggio.stringa2)>0){
            messaggio.confronto=1;
        }
        else{
            messaggio.confronto=0;
        }
        /* manda il messaggio al processo sorter */
        messaggio.type=DEST_SORTER;
        if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
            perror("msgsnd");
            exit(1);
        }
    }

    exit(0);
}

void padre(int *pipe_id){
    FILE *in;
    int i=0, count=0;
    char buffer[MAX_LEN];

    /* associa lo stream con la pipe in lettura */
    if((in=fdopen(pipe_id[0],"r"))==NULL){
        perror("fdopen");
        exit(1);
    }

    /* legge dalla pipe e stampa su stdout */
    while(fgets(buffer,MAX_LEN,in)){
        printf("%s",buffer);
    }

    exit(0);
}

int main(int argc, char *argv[]){
    int pipe_id[2];
    int msg_id;
    int fd;

    /* controlla gli argomenti passati */
    if(argc!=2){
        printf("Utilizzo: %s <file>\n",argv[0]);
        exit(1);
    }

    /* crea la pipe */
    if(pipe(pipe_id)==-1){
        perror("pipe");
        exit(1);
    }

    /* crea la coda di messaggi */
    if((msg_id=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("msgget");
        exit(1);
    }

    if(fork()==0){
        close(pipe_id[0]);
        sorter(argv[1],msg_id,pipe_id);
    }
    if(fork()==0){
        close(pipe_id[0]);
        close(pipe_id[1]);
        comparer(msg_id);
    }
    close(pipe_id[1]);
    padre(pipe_id);


    wait(NULL);
    wait(NULL);

    close(pipe_id[0]);

    if((msgctl(msg_id,IPC_RMID,NULL))==-1){
        perror("msgctl");
        exit(1);
    }

}