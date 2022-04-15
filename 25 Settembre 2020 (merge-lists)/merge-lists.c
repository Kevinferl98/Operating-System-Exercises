#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define MAX_LEN 1024
#define NUM_PAROLE 50

/* struttura del messaggio */
typedef struct{
    long type;
    char testo[MAX_LEN];
    int terminato;
} msg;


void reader(char *input, int msg_id){
    FILE *file;
    msg messaggio;
    char buffer[MAX_LEN];

    /* apre il file in sola lettura */
    if((file=fopen(input,"r"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* legge riga per riga dal file */
    while(fgets(buffer,MAX_LEN,file)){
        messaggio.type=1;
        strcpy(messaggio.testo,buffer);
        /* invia la riga letta al padre */
        if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
            perror("msgsnd");
            exit(1);
        }
    }
    /* invia il messaggio per comunicare la sua terminazione */
    messaggio.terminato=1;
    if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
        perror("msgsnd");
        exit(1);
    }

    exit(0);
}

int padre(int msg_id, int *pipe_id){
    msg messaggio;
    int reader_terminati=0;
    char buffer[MAX_LEN];
    char parole[NUM_PAROLE][MAX_LEN]={""};
    int j=0;


    while(true){
        int ripeti=0;
        /* riceve i messaggi dai processi figli */
        if((msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0,0))==-1){
            perror("msgrcv");
            exit(1);
        }
        /* controlla se hanno terminato */
        if(messaggio.terminato==1){
            reader_terminati++;
            if(reader_terminati==2)
                break;
            else
                continue;
        }
        /* controlla se la parola era gia presente */
        for(int i=0; i<NUM_PAROLE; i++){
            if(strcasecmp(messaggio.testo,parole[i])==0){
                ripeti=1;
                break;
            }
        }
        if(ripeti==1){
            continue;
        }
        strcpy(parole[j],messaggio.testo);
        /* invia sulla pipe la parola al processo writer */
        if((write(pipe_id[1],messaggio.testo,MAX_LEN))==-1){
            perror("write");
            exit(1);
        } 
        j++;  
    }
    /* invia per comunicare al writer la sua terminazione */
    if((write(pipe_id[1],"-1",sizeof("-1")))==-1){
        perror("write");
        exit(1);
    }

    exit(0);
}

void writer(int *pipe_id){
    char buffer[MAX_LEN];

    while(true){
        /* legge dalla pipe le parole inviate dal padre */
        if((read(pipe_id[0],buffer,MAX_LEN))==-1){
            perror("read");
            exit(1);
        }
        /* controlla se il processo padre ha terminato */
        if(strcmp(buffer,"-1")==0){
            break;
        }
        /* stampa le parole */
        printf("%s",buffer);
    }
    printf("\n");

    exit(0);
}

int main(int argc, char *argv[]){
    int msg_id; 
    int pipe_id[2];

    /* controlla gli argomenti passati */
    if(argc!=3){
        printf("Utilizzo: %s <file-1> <file-2>\n",argv[0]);
        exit(1);
    }

    /* crea la coda di messaggi */
    if((msg_id=msgget(IPC_PRIVATE,IPC_EXCL|IPC_CREAT|0660))==-1){
        perror("msgget");
        exit(1);
    }

    /* crea la pipe */
    if((pipe(pipe_id))==-1){
        perror("pipe");
        exit(1);
    }

    /* avvia il primo processo reader */
    if(fork()==0){
        close(pipe_id[0]);
        close(pipe_id[1]);
        reader(argv[1],msg_id);
    }
    /* avvia il secondo processo reader */
    if(fork()==0){
        close(pipe_id[0]);
        close(pipe_id[1]);
        reader(argv[2],msg_id);
    }
    /* avvia il processo writer */
    if(fork()==0){
        close(pipe_id[1]);
        writer(pipe_id);
    }
    /* avvia il processo padre */
    close(pipe_id[0]);
    padre(msg_id,pipe_id);
    
    /* attende la terminazione dei processi figli */
    wait(NULL);
    wait(NULL);
    wait(NULL);

    /* rimuove la coda di messaggi */
    msgctl(msg_id,IPC_RMID,NULL);

}