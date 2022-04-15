#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define MAX_LEN 1024
#define DEST_COUNTER 5
#define DEST_PARENT 7
#define ALPHABET_SIZE 26

/* struttura del messaggio */
typedef struct{
    long type;
    char testo[MAX_LEN];
    int stat[ALPHABET_SIZE];
    int terminato;
} msg;

void Reader(char *input, int msg_idRC){
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
        messaggio.terminato=0;
        messaggio.type=DEST_COUNTER;
        strcpy(messaggio.testo,buffer);
        /* invia il messaggio contenente la riga al processo Counter */
        if((msgsnd(msg_idRC,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
            perror("msgsnd");
            exit(1);
        }
    }
    /* invia il messaggio finale per comunicare la sua terminazione */
    messaggio.type=DEST_COUNTER;
    messaggio.terminato=1;
    if((msgsnd(msg_idRC,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
        perror("msgsnd");
        exit(1);
    }

    exit(0);
}

void Counter(int msg_idRC, int msg_idCP, int num_reader){
    msg messaggio;
    int indice=0;
    int reader_terminati=0;

    while(true){
        /* riceve il messaggio dai processi Reader */
        if((msgrcv(msg_idRC,&messaggio,sizeof(messaggio)-sizeof(long),DEST_COUNTER,0))==-1){
            perror("msgrcv");
            exit(1);
        }
        /* azzera le occorrenze presenti nel messaggio */
        for(int i=0; i<ALPHABET_SIZE; i++){
            messaggio.stat[i]=0;
        }
        /* controlla se i processi Reader hanno terminato */
        if(messaggio.terminato==1){
            reader_terminati++;
            if(reader_terminati==num_reader)
                break;
            else
                continue;
        }
        /* Controlla per ogni riga le occorrenze delle lettere */
        for(int i=0; i<strlen(messaggio.testo); i++){
            if(messaggio.testo[i]>='A' && messaggio.testo[i]<='z'){
                indice=toupper(messaggio.testo[i]-'a');
                messaggio.stat[indice]++;
            }
        }
        /* invia il messaggio contenente le occorrenze al processo padre */
        messaggio.type=DEST_PARENT;
        if((msgsnd(msg_idCP,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
            perror("msgsnd");
            exit(1);
        }
    }
    /* invia il messaggio finale per comunicare la sua terminazione */
    messaggio.type=DEST_PARENT;
    messaggio.terminato=1;
    if((msgsnd(msg_idCP,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
        perror("msgsnd");
        exit(1);
    }

    exit(0);
}

void Padre(int msg_idCP){
    msg messaggio;
    int stat[ALPHABET_SIZE];

    /* azzera le occorrenze totali */
    for(int i=0; i<ALPHABET_SIZE; i++){
        stat[i]=0;
    }

    while(true){
        /* riceve i messaggi dal processo Counter */
        if((msgrcv(msg_idCP,&messaggio,sizeof(messaggio)-sizeof(long),DEST_PARENT,0))==-1){
            perror("msgrcv");
            exit(1);
        }
        /* controlla se il processo Counter ha terminato */
        if(messaggio.terminato==1){
            break;
        }
        /* aggiorna le occorrenze totali */
        for(int i=0; i<ALPHABET_SIZE; i++)
            stat[i]+=messaggio.stat[i];
    }
    /* stampa le occorrenze totali */
    for(int i=0; i<ALPHABET_SIZE; i++){
        printf("%c:%d\n",'a'+i,stat[i]);
    }

    exit(0);
}

int main(int argc, char *argv[]){
    int msg_idRC, msg_idCP;

    /* controlla gli argomenti passati */
    if(argc<2){
        printf("Utilizzo: %s <file-1> <file-2> ... <file-n>\n",argv[0]);
        exit(1);
    }

    /* crea la prima coda di messaggi: reader-counter */
    if((msg_idRC=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("msgget");
        exit(1);
    }

    /* crea la seconda coda di messaggi: counter-padre */
    if((msg_idCP=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("msgget");
        exit(1);
    }

    /* avvia i processi figli reader */
    for(int i=1; i<argc; i++){
        if(fork()==0){
            Reader(argv[i],msg_idRC);
        }
    }
    /* avvia il processo figlio counter */
    if(fork()==0){
        Counter(msg_idRC,msg_idCP,argc-1);
    }
    /* avvia il processo padre */
    Padre(msg_idCP);
    /* aspetta la terminazione dei processi reader
     * e del processo counter */
    for(int i=1; i<argc; i++){
        wait(NULL);
    }
    wait(NULL);

    /* rimuove le code di messaggi */
    msgctl(msg_idCP,IPC_RMID,NULL);
    msgctl(msg_idRC,IPC_RMID,NULL);
}