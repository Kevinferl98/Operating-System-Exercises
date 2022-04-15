#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>

#define GIUDICE 3

/* struttura del messaggio */
typedef struct{
    long type;
    char mossa;
    int giocatore;
    int gioco_terminato;
} msg;

/* genera una mossa random */
char random_mossa(){
    int val=rand()%3;

    switch(val){
        case 0: return 'S';
        case 1: return 'C';
        case 2: return 'F';
    }
}

/* confronta le mosse e ritorna il vincitore */
int getVincitore(char mossa_g1, char mossa_g2){
    if(mossa_g1==mossa_g2)
        return 0;
    
    switch(mossa_g1){
        case 'S':
            switch(mossa_g2){
                case 'C': return 2;
                case 'F': return 1;
            }
        case 'C':
            switch(mossa_g2){
                case 'S': return 1;
                case 'F': return 2;
            }
        case 'F':
            switch(mossa_g2){
                case 'C': return 1;
                case 'S': return 2;
            }
    }
}

void giocatore(int num, int msg_id){
    msg messaggio;
    srand(time(NULL)+num);

    while(true){
        /* riceve il messaggio dal giudice per iniziare la partita */
        if((msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),num,0))==-1){
            perror("msgrcv");
            exit(1);
        }
        /* se il messaggio ha gioco_terminato=1 il giocatore termina */
        if(messaggio.gioco_terminato==1){
            break;
        }
        char mossa=random_mossa();
        printf("P%d: mossa %c\n",num,mossa);
        messaggio.type=GIUDICE;
        messaggio.giocatore=num;
        messaggio.mossa=mossa;
        /* invia il messaggio al giudice contenente la mossa scelta */
        if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
            perror("msgsnd");
            exit(1);
        }
    }

    exit(0);
}

void giudice(int msg_id, const char *pathname, int num_partite){
    msg messaggio;
    int fd;
    int n=1;
    char mossa_g1;
    char mossa_g2;
    int term=-1;

    /* apre la FIFO in scrittura */
    if((fd=open(pathname,O_WRONLY))==-1){
        perror("open");
        exit(1);
    }

    /* ripete finche le partite non terminano */
    while(num_partite>0){
        printf("G: inizio partita n.%d\n",n);
        /* invia i messaggi iniziali ai giocatori */
        for(int i=0; i<2; i++){
            messaggio.type=i+1;
            if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
                perror("msgsnd");
                exit(1);
            }
        }
        /* riceve i messaggi da tutti e 2 i giocatori e memorizza le mosse scelte*/
        for(int i=0; i<2; i++){
            if((msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),GIUDICE,0))==-1){
                perror("msgrcv");
                exit(1);
            }
            if(messaggio.giocatore==1){
                mossa_g1=messaggio.mossa;
            }
            else if(messaggio.giocatore==2){
                mossa_g2=messaggio.mossa;
            }
        }
        /* ricava il vincitore */
        int vincitore=getVincitore(mossa_g1,mossa_g2);
        /* se sono in pareggio lancia una nuova partita */
        if(vincitore==0){
            continue;
        }
        printf("G: partita n.%d vinta da P%d\n",n,vincitore);
        /* scrive sulla pipe il risultato della partita */
        write(fd,&vincitore,sizeof(vincitore));
        n++;
        num_partite--;
    }
    /* manda i messaggi per far terminare i giocatori */
    for(int i=0; i<2; i++){
        messaggio.type=i+1;
        messaggio.gioco_terminato=1;
        if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
            perror("msgsnd");
            exit(1);
        }
    }
    /* scrive sulla FIFO per far terminare il tabellone */
    write(fd,&term,sizeof(term));

    exit(0);
}

int tabellone(const char *pathname){
    int fd;
    int p1_vittorie=0, p2_vittorie=0;
    int  n;

    /* apre la FIFO in lettura */
    if((fd=open(pathname,O_RDONLY))==-1){
        perror("open");
        exit(1);
    }

    while(true){
        /* legge dalla FIFO il risultato della partita */
        read(fd,&n,sizeof(n));
        /* se contiene -1 termina */
        if(n==-1)
            break;
        if(n==1)
            p1_vittorie++;
        else if(n==2)
            p2_vittorie++;
        printf("T: classifica temporanea: P1=%d P2=%d\n",p1_vittorie,p2_vittorie);
    }
    /* stampa la classifica finale e il vincitore */
    printf("T: classifica finale: P1=%d P2=%d\n",p1_vittorie,p2_vittorie);
    if(p1_vittorie>p2_vittorie)
        printf("T: vincitore del torner: P1\n");
    else if(p1_vittorie<p2_vittorie)
        printf("T: vincitore del tornero: P2\n");

    exit(0);
}

int main(int argc, char *argv[]){
    int msg_id;
    const char *pathname="/tmp/fifo";
    int num_partite;

    /* controlla gli argomenti passati */
    if(argc!=2){
        printf("Utilizzo: %s <numero-partite>\n",argv[0]);
        exit(1);
    }
    num_partite=atoi(argv[1]);

    /* crea la coda di messaggi */
    if((msg_id=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0600))==-1){
        perror("msgget");
        exit(1);
    }

    /* usa unlink per eliminare una precedente FIFO */
    unlink(pathname);
    /* crea la FIFO */
    if((mkfifo(pathname,0660))==-1){
        perror("mkfifo");
        exit(1);
    }

    if(fork()==0){
        giocatore(1,msg_id);
    }
    if(fork()==0){
        giocatore(2,msg_id);
    }
    if(fork()==0){
        giudice(msg_id,pathname,num_partite);
    }
    tabellone(pathname);

    /* aspetta la terminazione dei processi figli */
    wait(NULL);
    wait(NULL);
    wait(NULL);

    /* rimuove la coda di messaggi */
    msgctl(msg_id,IPC_RMID,NULL);
    /* rimuove la FIFO */
    unlink(pathname);

}