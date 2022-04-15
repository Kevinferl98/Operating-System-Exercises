#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_BUFFER 2048

typedef enum{REPLY_DATA,REPLY_DATA_STOP} Payload;

/* struttura del messaggio */
typedef struct{
    long int type; /* destinatario, 1: Padre, 2: Writer */
    Payload payload;
    char testo[MAX_BUFFER];
} msg;

/* controlla se una data stringa è palindroma */
int check(char *buffer){
    int length=strlen(buffer);
    int flag=0;

    for(int i=0; i<length; i++){
        if(buffer[i]!=buffer[length-i-1]){
            flag=1;
            break;
        }
    }
    if(flag){
        return 0;
    }
    else 
        return 1;
}

/* legge il file e manda il contenuto al padre */
void reader(int msgid, char *input){
    printf("Avvio il figlio reader\n");
    msg messaggio;
    FILE *fd;
    char buffer[MAX_BUFFER];

    /* apre il file in sola lettura */
    if((fd=fopen(input,"r"))==NULL){
        perror("fopen");
        exit(1);
    }
    
    /* legge il file e manda il messaggio al padre */
    while(fgets(buffer,MAX_BUFFER,fd)!=NULL){
        strcpy(messaggio.testo,buffer);
        messaggio.type=1;
        messaggio.payload=REPLY_DATA;
        if((msgsnd(msgid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
            perror("msgsnd");
            exit(1);
        }
    }
    messaggio.type=1;
    messaggio.payload=REPLY_DATA_STOP;
    if((msgsnd(msgid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
        perror("msgsnd");
        exit(1);
    }
    exit(0);
}

/* riceve i messaggi dal padre e li scrive nel file di output */
void writer(int msgid,char *dstFile){
    msg messaggio;
    FILE *fd;
    char *tmp;

    /* apre il file in sola scrittura */
    if((fd=fopen(dstFile,"w"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* riceve i messaggi e li scrive sul file di output */
    while(true){
        if((msgrcv(msgid,&messaggio,sizeof(messaggio)-sizeof(long),2,0))==-1){
            perror("msgrcv");
            exit(1);
        }
        if(messaggio.payload==REPLY_DATA_STOP)
            break;
        else{
            fputs(messaggio.testo,fd);
        }
    }
    exit(0);
}

/* riceve i messaggi dal reader e controlla se contine parole palindrome e le invia al writer */
void padre(int msgid){
    printf("Avvio il padre\n");
    msg messaggio;
    char *tmp;

    while(true){
        if(msgrcv(msgid,&messaggio,sizeof(messaggio)-sizeof(long),1,0)==-1){
            perror("msgrcv");
            exit(1);
        }
        if(messaggio.payload==REPLY_DATA_STOP)
            break;
        else{
            strcpy(tmp,messaggio.testo);
            if(tmp[strlen(tmp)-1]=='\n')
                tmp[strlen(tmp)-2]='\0';
            if(check(tmp)){
                tmp[strlen(tmp)]='\n';
                messaggio.type=2;
                messaggio.payload=REPLY_DATA;
                strcpy(messaggio.testo,tmp);
                if((msgsnd(msgid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
                    perror("msgsnd");
                    exit(1);
                }
            }
        }
    }
    messaggio.type=2;
    messaggio.payload=REPLY_DATA_STOP;
    if((msgsnd(msgid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
        perror("msgsnd");
        exit(1);   
    }
    exit(0);
}

int main(int argc, char *argv[]){
    char *input;
    char *output;
    int msgid;

    if(argc!=3){
        printf("Utilizzo: %s [input file] [output file]\n",argv[0]);
        exit(1);
    }
    
    
    if((msgid=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("msgget");
        exit(1);
    } 

    input=argv[1];
    output=argv[2];
    if(fork()==0){
        reader(msgid,input);
    }
    else if(fork()==0){
        writer(msgid,output);
    }
    else{
        padre(msgid);
    }
    exit(0);
}