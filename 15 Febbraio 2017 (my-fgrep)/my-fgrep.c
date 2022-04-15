#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define MAX_BUF 1024

typedef struct {
    long type;
    char text[MAX_BUF];
    char file[MAX_BUF];
} msg;

void reader(int msgid, char *input){
    FILE *file;
    msg messaggio;
    char riga[MAX_BUF];

    /* apre il file passato in input */
    if((file=fopen(input,"r"))==NULL){
        perror("fopen");
        exit(1);
    }
    
    /* invia ogni riga del file tramite messaggi */
    while(fgets(riga,sizeof(riga),file)!=NULL){
        messaggio.type=1;
        strcpy(messaggio.text,riga);
        strcpy(messaggio.file,input);
        if((msgsnd(msgid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
            perror("msgsnd");
            exit(1);
        }
    }

    /* invia il messaggio finale per comunicare la fine del file */
    messaggio.type=1;
    strcpy(messaggio.text,"-1");
    strcpy(messaggio.file,input);
    if((msgsnd(msgid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
        perror("msgsnd");
        exit(1);
    }

    /* chiude il file precedentemente aperto */
    if((fclose(file))==-1){
        perror("fclose");
        exit(1);
    }

    exit(0);
}

void filterer(int msgid, int pipe_id[2], int reverse_logic, int case_insensitive, char *stringa, int num_file){
    msg messaggio;
    int finito=0;
    int figli_terminati=0;
    FILE *file;

    /* apre la pipe in scrittura */
    if((file=fdopen(pipe_id[1],"w"))==NULL){
        perror("fdopen");
        exit(1);
    }

    while(finito==0){
        char *match;

        /* legge il messaggio ricevuto dal reader */
        if((msgrcv(msgid,&messaggio,sizeof(messaggio)-sizeof(long),1,0))==-1){
            perror("msgrcv");
            exit(1);
        }

        /* controlla se è presente il carattere di terminazione del file */
        if(strcmp(messaggio.text,"-1")==0){
            figli_terminati++;
        }
        
        /* controlla se tutti i figli hanno terminato */
        if(figli_terminati==num_file){
            finito=1;
        }

        /* controlla se è presente -v e/o -i ed applica il criterio di selezione */
        if(case_insensitive==1){
            match=strcasestr(messaggio.text,stringa);
        }
        else{
            match=strstr(messaggio.text,stringa);
        }
        if((reverse_logic && !match) || (!reverse_logic && match){
            /* scrive sulla pipe */
            if(fprintf(file,"%s: %s",messaggio.file,messaggio.text)==-1){
                perror("fprintf");
                exit(1);
            }
        }
    }
    /* manda il messaggio finale */
    if(fprintf(file,"-1: -1\n")==-1){
        perror("fprintf");
        exit(1);
    }

    /* chiude la pipe */
    if((fclose(file))==-1){
        perror("fclose");
        exit(1);
    }
    
    exit(0);
}

void padre(int pipe_id[2], int num_file){
    FILE *file;
    int finito=0;
    char stampare[MAX_BUF];

    /* apre la pipe in lettura */
    if((file=fdopen(pipe_id[0],"r"))==NULL){
        perror("fdopen");
        exit(1);
    }

    /* stampa i messaggi ricevuti sulla pipe dal filterer */
    while(finito==0){
        fgets(stampare,sizeof(stampare),file);
        if(strstr(stampare,"-1")==NULL){
            printf("%s",stampare);
        }
        else{
            finito=1;
        }
    }

    /* chiude la pipe */
    if((fclose(file))==-1){
        perror("fclose");
        exit(1);
    }

    exit(0);
}

int main(int argc, char *argv[]){
    int msgid, pipe_id[2];
    int inverse=0, case_insensitive=0;
    int num_file=argc-2;

    if(argc<3){
        printf("Utilizzo: %s [-v] [-i] [word] <file-1> [file-2] [file-3] [...]\n",argv[0]);
        exit(1);
    }

    /* crea la pipe */
    if(pipe(pipe_id)==-1){
        perror("pipe");
        exit(1);
    }

    /* crea la coda di messaggi */
    if((msgid=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("msgget");
        exit(1);
    }

    /* controlla gli argomenti passati in input */
    if(argc>3 && strcmp(argv[1],"-v")==0){
        inverse=1;
        num_file--;
    }
    else if(argc>3 && strcmp(argv[1],"-i")==0){
        case_insensitive=1;
        num_file--;
    }
    if(argc>4 && strcmp(argv[2],"-v")==0 && inverse==0 && case_insensitive==1){
        num_file--;
        inverse=1;
    }
    else if(argc>4 && strcmp(argv[2],"-i")==0 && inverse==1 && case_insensitive==0){
        num_file--;
        case_insensitive=1;
    }

    if(fork()==0){
        filterer(msgid,pipe_id,inverse,case_insensitive,argv[argc-num_file-1],num_file);
    }
    else{
        if(fork()==0){
            padre(pipe_id,num_file);
        }
        else{
            for(int i=0; i<num_file; i++){
                if(fork()==0){
                    reader(msgid,argv[argc-num_file+i]);
                }
                else{
                    wait(NULL);
                }
            }
            wait(NULL);
            wait(NULL);
        }
    }

    /* rimuove la coda di messaggi */
    if((msgctl(msgid,IPC_RMID,NULL))==-1){
        perror("msgctl");
        exit(1);
    }

}