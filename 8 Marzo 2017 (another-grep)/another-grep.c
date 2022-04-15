#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_BUFFER 2048

typedef struct{
    long type;
    char text[MAX_BUFFER];
} msg;

void reader(const char *input, int pipe_fd){
    FILE *input_fs, *output_fs;
    char buffer[MAX_BUFFER];

    /* apre il file di input */
    if((input_fs=fopen(input,"r"))==NULL){
        perror("input");
        exit(1);
    }

    /* associa la pipe con il file di output */
    if((output_fs=fdopen(pipe_fd,"w"))==NULL){
        perror("pipe");
        exit(1);
    }

    /* legge dal file e invia alla pipe */
    while(fgets(buffer, MAX_BUFFER, input_fs)!=NULL){
        fputs(buffer,output_fs);
    }
    exit(0);
}

void padre(int pipe_fd, char *parola, int msgid){
    FILE *input_fs;
    char buffer[MAX_BUFFER];
    msg messaggio;

    /* apre il file input dalla pipe */
    if((input_fs=fdopen(pipe_fd,"r"))==NULL){
        perror("pipe");
        exit(1);
    }

    /* legge dal file e prende soltanto le parole contententi la parola passata */
    while(fgets(buffer, MAX_BUFFER, input_fs)!=NULL){
        if(strstr(buffer, parola)!=NULL){ /* se contine la parola tutta la riga viene copiata nel messaggio */
            messaggio.type=1;
            strcpy(messaggio.text,buffer);
            if((msgsnd(msgid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
                perror("msgsnd");
                exit(1);
            }
        }
    }
    messaggio.type=1;       /* viene messo -1 per segnalare la fine dei messaggi al writer */
    strcpy(messaggio.text,"-1");
    if((msgsnd(msgid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
        perror("msgsnd");
        exit(1);
    }
    fclose(input_fs);
    exit(0);
}

void writer(int msgid){
    msg messaggio;
    
    /* riceve i messaggi dal padre e li stampa sulla standard output */
    while(1){   
        if(msgrcv(msgid,&messaggio,sizeof(messaggio)-sizeof(long),1,0)==-1){
            perror("msgrcv");
            exit(1);
        }
        if(strcmp(messaggio.text,"-1")==0){     /* se il messaggi contine -1, i messaggi sono finiti */
            break;
        }
        printf("%s",messaggio.text);
    }
    printf("\n");
}

int main(int argc, char *argv[]){
    int pipe_fd[2];
    int msgid;

    if(argc!=3){
        printf("Utilizzo: %s <word> <file>\n",argv[0]);
        exit(1);
    }

    /* crea la pipe */
    if(pipe(pipe_fd)==-1){ 
        perror("pipe");
        exit(1);
    }

    /* crea una coda di messaggi */
    if((msgid=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("msgget");
        exit(1);
    }

    if(fork()==0){
        //reader
        close(pipe_fd[0]);
        reader(argv[2],pipe_fd[1]);
    }
    else{
        if(fork()==0){
            //writer
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            writer(msgid);
        }
        else{
            close(pipe_fd[1]);
            padre(pipe_fd[0],argv[1],msgid);
        }
    }
    wait(NULL);
    wait(NULL);
    if(msgctl(msgid,IPC_RMID,NULL)==-1){   /* rimuove la coda di messaggi */
        perror("msgctl");
        exit(1);
    }
    exit(0);
}