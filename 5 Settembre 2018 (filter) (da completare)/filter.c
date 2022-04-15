#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define MAX_LEN 1024
#define DEST_PARENT 100

/* struttura del messaggio */
typedef struct{
    long type;
    char testo[MAX_LEN];
    int terminato;
} msg;

void filter(char *filtro, int msg_id, int n, int num_figli){
    printf("Filter avviato\n");
    msg messaggio;
    char *curr;
    char parola[MAX_LEN];
    char parola2[MAX_LEN];
    char *stringa;

    while(true){
        /* riceve i messaggi contentente il testo da modificare
         * se è il primo figlio riceve i messaggi dal padre 
         * altrimenti riceve i messaggi dal figlio precendente */
        if((msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),n,0))==-1){
            perror("msgrcv");
            exit(1);
        }
        //printf("[Filter]Messaggio ricevuto: %s\n",messaggio.testo);
        /* controlla se il testo è terminato */
        if(messaggio.terminato==1){
            /* se è l'ultimo figlio interrompe il ciclo e termina */
            if(n==num_figli)
                break;
            else{
                /* altrimenti invia il messaggio di terminazione al figlio successivo e termina */
                messaggio.terminato=1;
                messaggio.type=n+1;
                if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
                    perror("msgsnd");
                    exit(1);
                }
                break;
            }
        }
        /* controlla il filtro da applicare */
        if(filtro[0]=='^'){
            //printf("SONO DENTRO\n");
            parola[0]=filtro[1];
            char *ret;
            /* estrae la parola da modificare */
            for(int i=0; i<strlen(filtro)-1; i++){ 
                parola[i]=filtro[i+1];         
            }
            //printf("Parola: %s\n",parola);
            /* sostituisce la parola */
            while((ret=strstr(messaggio.testo,parola))!=NULL){
                for(int i=0; i<strlen(parola); i++)
                    ret[i]=toupper(ret[i]);
                //printf("RET: %s\n",ret);
            }
        }
        else if(filtro[0]=='_'){
            parola[0]=filtro[1];
            char *ret;
            /* estrae la parola da modificare */
            for(int i=0; i<strlen(filtro)-1; i++){
                parola[i]=filtro[i+1];
            }
            /* sostituisce la parola */
            while((ret=strstr(messaggio.testo,parola))!=NULL){
                for(int i=0; i<strlen(parola); i++)
                    ret[i]=tolower(ret[i]);
            }
        }
        else if(filtro[0]=='%'){
            parola[0]=filtro[1];

            int i=0; 
            while(filtro[i]!='|'){
                parola[i]=filtro[i+1];
                i++;
            }
            for(int j=i+1; j<strlen(filtro); i++){
                parola2[i]=filtro[i];
            }

            printf("Parola 1: %s\n",parola);
            printf("Parola 2: %s\n",parola2);
        }
        /* se è l'ultimo figlio invia il messaggio al padre */
        if(n==num_figli){
            messaggio.type=DEST_PARENT;
            if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
                perror("msgsnd");
                exit(1);
            }
        }
        /* altrimenti invia il messaggio al figlio successivo */
        else{
            messaggio.type=n+1;
            if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
                perror("msgsnd");
                exit(1);
            }
        }
    }

    exit(0);
}

void padre(char *input, int msg_id){
    printf("Padre avviato\n");
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
        /* invia il messaggio al primo figlio */
        if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
            perror("msgsnd");
            exit(1);
        }
        //printf("[Padre]Messaggio inviato: %s\n",messaggio.testo);
        /* aspetta di riceve il messaggio prima di passare alla riga successiva */
        if((msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),DEST_PARENT,0))==-1){
            perror("msgsnd");
            exit(1);
        }
        /* stampa il messaggio modificato */
        printf("[Padre]Messaggio finale: %s\n",messaggio.testo);
    }
    /* invia il messaggio per far terminare i figli */
    messaggio.type=1;
    messaggio.terminato=1;
    if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
        perror("msgsnd");
        exit(1);
    }

    exit(0);
}

int main(int argc, char *argv[]){
    int msg_id;

    /* controlla gli argomenti passati */
    if(argc<3){
        printf("Utilizzo: %s <file.txt> <filter-1> [filter-2] [...]\n",argv[0]);
        exit(1);
    }

    /* crea la coda di messaggi */
    if((msg_id=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("msgget");
        exit(1);
    }

    /* avvia i processi filter */
    for(int i=0; i<argc-2; i++){
        if(fork()==0)
            filter(argv[i+2],msg_id,i+1,argc-2);
    }
    /* avvia il padre */
    padre(argv[1],msg_id);
    /* aspetta la terminazione dei processi filter */
    for(int i=0; i<argc-2; i++){
        wait(NULL);
    }
    /* rimuove la coda di messaggi */
    msgctl(msg_id,IPC_RMID,NULL);
}