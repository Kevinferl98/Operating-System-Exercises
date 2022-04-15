#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#define GIUDICE 7

/* struttura del messaggio */
typedef struct{
    long type;
    int mossa;
    int richiesta;
    int partite_rimanenti;
    int mittente;
} msg;

void giocatore(int id, int msg_id){
    srand(time(NULL)+id);
    msg messaggio;
    int n;
    
    while(true){
        /* riceve il messaggio dal giudice */
        if(msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),id+1,0)==-1){
            perror("msgrcv");
            exit(1);
        }
        /* controlla se le partite sono terminate */
        if(messaggio.partite_rimanenti==0){
            break;
        }
        /* genera una mossa e la invia al giudice */
        if(messaggio.richiesta==1){
            //srand(time(NULL)+id);
            n=rand()%10;
            messaggio.type=7;
            messaggio.mossa=n;
            messaggio.richiesta=0;
            messaggio.mittente=id;
            printf("P%d: mossa %d\n",id,n);
            if(msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0)==-1){
                perror("msgsnd");
                exit(1);
            }
        }
    }

    exit(0);
}

void giudice(int msg_id, int num_giocatori, int num_partite){
    msg messaggio;
    int messaggi_ricevuti=0;
    int tot=0;
    int mosse[num_giocatori]={0};
    int vittorie[num_giocatori]={0};
    int vincitore=0;
    int n=1;
    int ripeti=0;

    while(num_partite>0){
        /* manda il messaggio inziale ai giocatori per farli iniziare */
        for(int i=0; i<num_giocatori; i++){
            messaggio.type=i+1;
            messaggio.richiesta=1;
            messaggio.partite_rimanenti=num_partite;
            if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
                perror("msgsnd");
                exit(1);
            }
        }
        /* riceve i messaggi contenteti le mosse dai giocatori */
        for(int i=0; i<num_giocatori; i++){
            if(msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),GIUDICE,0)==-1){
                perror("msgrcv");
                exit(1);
            }
            tot+=messaggio.mossa;
            mosse[i]=messaggio.mossa;
            for(int j=0; j<i+1; j++){
                if(messaggio.mossa==mosse[j]){
                    printf("J: partita n.%d patta e quindi da ripetere\n",n);
                    ripeti=1;
                    break;
                }
            }
            if(ripeti==1){
                break;
            }
        }
        if(ripeti==1){
            continue;
        }
        /* decreta il vincitore */
        vincitore=(tot)%num_giocatori;
        vittorie[vincitore]++;
        printf("J: partita n.%d vinta da P%d\n",n,vincitore++);
        n++;
        num_partite--;
    }
    /* manda il messaggio per far terminare i giocatori */
    for(int i=0; i<num_giocatori; i++){
        messaggio.type=i+1;
        messaggio.partite_rimanenti=0;
        messaggio.richiesta=0;
        if(msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0)==-1){
            perror("msgsnd"); 
            exit(1);
        }
    }
    printf("J: torneo finito\n");

    exit(0);
}

int main(int argc, char *argv[]){
    int num_partite;
    int num_giocatori;
    int msg_id;

    /* controlla gli argomenti passati */
    if(argc!=3){
        printf("Utilizzo: %s <n=numero-giocatori> <m=numero-partite>\n",argv[0]);
        exit(1);
    }
    
    /* controlla il numero di giocatori passati */
    num_giocatori=atoi(argv[1]);
    if(num_giocatori<2 || num_giocatori>6){
        printf("Inserire un numero di giocatori n: 2<=n>=6\n");
        exit(1);
    }

    /* controlla il numero di partite passate */
    num_partite=atoi(argv[2]);
    if(num_partite<1){
        printf("Inseire un numero di partite maggiori o uguali di 1\n");
        exit(1);
    }

    /* crea la coda di messaggi */
    if((msg_id=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0600))==-1){
        perror("msgget");
        exit(1);
    }

    for(int i=0; i<num_giocatori; i++){
        if(fork()==0){
            giocatore(i,msg_id);
        }
    }
    giudice(msg_id,num_giocatori,num_partite);

    for(int i=0; i<num_giocatori; i++){
        wait(NULL);
    }

    /* rimuove la coda di messaggi */
    msgctl(msg_id,IPC_RMID,NULL);

}