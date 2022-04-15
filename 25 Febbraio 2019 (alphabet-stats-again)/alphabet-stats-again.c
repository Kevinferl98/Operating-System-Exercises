#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define MAX_DIM 1024
#define DEST_COUNTER 10
#define DEST_PARENT 20
#define ALPHABET_SIZE 26

/* struttura del messaggio */
typedef struct{
    long type;
    char text[1024];
    int num[ALPHABET_SIZE];
    int finito;
    int mittente;
} msg;

void Reader(char *input, int msg_id, int msg_rp, int n){
    FILE *file;
    msg messaggio;
    int stat[26];
    char buffer[MAX_DIM];

    /* apre il file in sola lettura con uno stream */
    if((file=fopen(input,"r"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* azzera la statistica globale */
    for(int i=0; i<ALPHABET_SIZE; i++){
        stat[i]=0;
    }

    /* legge dal file riga per riga */
    while(fgets(buffer,sizeof(buffer),file)!=NULL){
        /* azzera le occorrenze */
        for(int i=0; i<ALPHABET_SIZE; i++){
            messaggio.num[i]=0;
        }
        strcpy(messaggio.text,buffer);
        messaggio.type=DEST_COUNTER;
        messaggio.finito=0;
        messaggio.mittente=n;
        /* invia il messaggio al processo Counter */
        if(msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0)==-1){
            perror("msgsnd");
            exit(1);
        }
        /* riceve il messaggio dal processo Counter */
        if(msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),n,0)==-1){
            perror("msgrcv");
            exit(1);
        }
        /* aggiorna la statistica globale */
        for(int i=0; i<ALPHABET_SIZE; i++){
            stat[i]+=messaggio.num[i];
        }
    }
    
    /* invia il messaggio finale al processo Counter */
    messaggio.type=DEST_COUNTER;
    strcpy(messaggio.text,"-1");
    messaggio.finito=1;
    if(msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0)==-1){
        perror("msgsnd");
        exit(1);
    }

    /* invia il messaggio contenente la statistica globale al padre */
    messaggio.type=DEST_PARENT;
    for(int i=0; i<ALPHABET_SIZE; i++)
        messaggio.num[i]=stat[i];
    messaggio.finito=1;
    if(msgsnd(msg_rp,&messaggio,sizeof(messaggio)-sizeof(long),0)==-1){
        perror("msgsnd");
        exit(1);
    }

    exit(0);
}

void Counter(int msg_id, int n){
    msg messaggio;
    int figli_terminati=0;
    int indice=0;

    while(true){
        /* riceve i messaggi dai figlio Reader */
        if(msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),DEST_COUNTER,0)==-1){
            perror("msgrcv");
            exit(1);
        }
        /* controlla se il messaggio contiene finito=1 */
        if(messaggio.finito==1){
            figli_terminati++;
            /* se tutti i reader hanno terminato si interrompe */
            if(figli_terminati==n)
                break;
            else{
                continue;
            }
        }
        /* per ogni riga ricevuta controlla le occorrenze delle lettere */
        int count=0;
        for(int i=0; i<strlen(messaggio.text); i++){
            if(messaggio.text[i]>='a' && messaggio.text[i]<='z'){
                indice=toupper(messaggio.text[i]-'a');
                messaggio.num[indice]++;
            }
            else if(messaggio.text[i]>='A' && messaggio.text[i]<='Z'){
                indice=toupper(messaggio.text[i]-'A');
                messaggio.num[indice]++;
            }
        }
        /* invia il messaggio contenente le occorrenze al Reader */
        messaggio.type=messaggio.mittente;
        if(msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0)==-1){
            perror("msgsnd");
            exit(1);
        }
    }

    exit(0);
}

void padre(int msg_rp, int n){
    int figli_terminati=0;
    msg messaggio;

    while(true){
        /* riceve i messaggi dai figli reader */
        if(msgrcv(msg_rp,&messaggio,sizeof(messaggio)-sizeof(long),DEST_PARENT,0)==-1){
            perror("msgrcv");
            exit(1);
        }
        /* stampa la statistica globale */
        for(int i=0; i<ALPHABET_SIZE; i++){
            printf("%c:%d ",'a'+i,messaggio.num[i]);
        }
        printf("\n");
        /* Controlla se hanno terminato */
        if(messaggio.finito==1){
            figli_terminati++;
            if(figli_terminati==n)
                break;
        }
    }

    exit(1);
}

int main(int argc, char *argv[]){
    int num_figli=0;
    int msg_id, msg_rp;

    /* controlla gli argomenti passati */
    if(argc<2){
        printf("Utilizzo: %s <file-1> <file-2> ... <file-n>\n",argv[0]);
        return -1;
    }

    /* crea la prima coda di messaggi */
    if((msg_id=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0600))==-1){
        perror("msgget");
        return -1;
    }

    /* crea la seconda coda di messaggi */
    if((msg_rp=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0600))==-1){
        perror("msgget");
        return -1;
    }

    num_figli=argc-1;
    /* crea tanti processi reader tanti i file passati */
    for(int i=1; i<argc; i++){
        if(fork()==0)
            Reader(argv[i],msg_id,msg_rp,i);
    }
    if(fork()==0){
        Counter(msg_id,num_figli);
    }
    else{
        padre(msg_rp,num_figli);
    }

    for(int i=1; i<argc; i++){
        wait(NULL);
    }
    wait(NULL);

    /* rimuove le code di messaggi */
    msgctl(msg_id,IPC_RMID,NULL);
    msgctl(msg_rp,IPC_RMID,NULL);

    return 0; 
}