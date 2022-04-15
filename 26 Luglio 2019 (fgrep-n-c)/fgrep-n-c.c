#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define MAX_LEN 1024
#define MAX_DIM 50
#define MAX_NUM 10
#define DEST_PARENT 10

/* struttura dei messaggi */
typedef struct{
    long type;
    char parola[MAX_LEN];
    char file[MAX_LEN];
    int terminato;
    int num_riga;
    char testo[MAX_LEN];
    int processo;
} msg;

void figlio(char *input, int msg_id, int msg_id2, int n){
    FILE *file;
    msg messaggio;
    char buffer[MAX_LEN];
    char parole[MAX_DIM][MAX_LEN]; 
    int riga=1;    
    int j=0;                                                    

    /* apre il file in sola letture */
    if((file=fopen(input,"r"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* legge il file e lo importa dentro un array di stringhe */
    while(fgets(buffer,MAX_LEN,file)){
        strcpy(parole[j],buffer);
        j++;
    }

    while(true){
        /* riceve il messaggio dal padre */
        if((msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),n,0))==-1){
            perror("msgrcv");
            exit(1);
        }
        /* controlla se il padre ha terminato di inviare messaggi */
        if(messaggio.terminato==1){
            break;
        }
        /* legge il contenuto dell'array */
        for(int i=0; i<MAX_DIM; i++){
            /* se trova la parola nella riga invia il messaggio al padre */
            if(strstr(parole[i],messaggio.parola)){
                messaggio.processo=n;
                messaggio.type=DEST_PARENT;
                messaggio.num_riga=riga;
                strcpy(messaggio.testo,parole[i]);
                strcpy(messaggio.file,input);
                if((msgsnd(msg_id2,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
                    perror("msgsnd");
                    exit(1);
                }
            }
            riga++;
        }
    }
    /* invia il messaggio per comunicare la sua terminazione */
    messaggio.type=DEST_PARENT;
    messaggio.terminato=1;
    if((msgsnd(msg_id2,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
        perror("msgsnd");
        exit(1);
    }

    exit(0);
}

void padre(int msg_id, int msg_id2, char **parole, int num_parole, int num_figli){
    msg messaggio;
    int figli_terminati=0;

    /* invia i messaggi ai figli */
    for(int i=0; i<num_figli; i++){
        for(int j=0; j<num_parole; j++){
            messaggio.type=i+1;
            strcpy(messaggio.parola,parole[j]);
            if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
                perror("msgsnd");
                exit(1);
            }
        }
    }
    /* invia i messaggi per far terminare i figli */
    for(int i=0; i<num_figli; i++){
        messaggio.type=i+1;
        messaggio.terminato=1;
        if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
            perror("msgsnd");
            exit(1);
        }
    }
    while(true){
        /* riceve i messaggi dai figli */
        if((msgrcv(msg_id2,&messaggio,sizeof(messaggio)-sizeof(long),DEST_PARENT,0))==-1){
            perror("msgrcv");
            exit(1);
        }
        /* controlla se i figli hanno terminato */
        if(messaggio.terminato==1){
            figli_terminati++;
            if(figli_terminati==num_figli)
                break;
            else
                continue;
        }
        printf("%s@%s:%d:%s\n",messaggio.parola,messaggio.file,messaggio.num_riga,messaggio.testo);
    }

    exit(0);
}

int main(int argc, char *argv[]){
    int msg_id, msg_id2;
    int num_parole=0;
    int num_file=argc-2;

    /* controlla gli argomenti passati */
    if(argc<3){
        printf("Utilizzo: %s <word-1> [word-2] [...] @ <file-1> [file-2] [...]\n",argv[0]);
        exit(1);
    }

    /* crea la prima coda di messaggi */
    if((msg_id=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("msgget");
        exit(1);
    }

    /* crea la seconda coda di messaggi */
    if((msg_id2=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("msgget");
        exit(1);
    }

    int i=1;
    while(argv[i][0]!='@'){
        num_parole++;
        i++;
    }
    num_file-=num_parole;

    char *parole[num_parole];
    i=1;
    while(argv[i][0]!='@'){
        parole[i-1]=argv[i];
        i++;
    }

    printf("Numero parole: %d\n",num_parole);
    printf("Numero file: %d\n",num_file);

    for(int i=0; i<num_file; i++){
        if(fork()==0){
            figlio(argv[i+num_parole+2],msg_id,msg_id2,i+1);
        }
    }
    padre(msg_id,msg_id2,parole,num_parole,num_file);
    
    wait(NULL);
    wait(NULL);

    msgctl(msg_id,IPC_RMID,NULL);
    msgctl(msg_id2,IPC_RMID,NULL);

}