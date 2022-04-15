#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define MAX_LEN 1024

/* struttura del messaggio */
typedef struct{
    long type;
    char path[MAX_LEN];
    int stat;
    int terminato;
} msg;

void cerca(char *dir, int msg_id){
    DIR *directory;
    struct dirent *voce;
    char fullpath[MAX_LEN];
    msg messaggio;

    /* apre la directory */
    if((directory=opendir(dir))==NULL){
        perror("opendir");
        exit(1);
    }

    /* legge la varie voci della directory */
    while((voce=readdir(directory))!=NULL){
        if((strcmp(voce->d_name,"."))==0 || (strcmp(voce->d_name,"..")==0))
            continue;
        if(dir[strlen(dir)-1]=='/')
            sprintf(fullpath,"%s%s",dir,voce->d_name);
        else
            sprintf(fullpath,"%s/%s",dir,voce->d_name);
        /* se è un file regolare manda il pathname completo all'Analyzer */
        if(voce->d_type==DT_REG){
            printf("Scanner: %s\n",fullpath);
            messaggio.type=1;
            messaggio.terminato=0;
            strcpy(messaggio.path,fullpath);
            messaggio.stat=0;
            if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
                perror("msgsnd");
                exit(1);
            }
        }
        /* se è una directory richiama la funzione */
        else if(voce->d_type==DT_DIR)
            cerca(fullpath,msg_id);
    }
}

void Scanner(char *dir, int msg_id){
    msg messaggio;
    
    cerca(dir,msg_id);

    /* manda il messaggio per comunicare la sua terminazione */
    messaggio.type=1;
    messaggio.terminato=1;
    if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
        perror("msgsnd");
        exit(1);
    }

    exit(0);
}

void Analyzer(int msg_id){
    int fd;
    struct stat statbuf;
    char *memoria;
    msg messaggio;
    int stat=0;

    while(true){
        /* riceve i messaggi dal processo Scanner */
        if((msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),1,0))==-1){
            perror("msgrcv");
            exit(1);
        }

        /* se il processo Scanner ha terminato esce dal ciclo */
        if(messaggio.terminato==1){
            break;
        }

        /* apre il file */
        if((fd=open(messaggio.path,O_RDONLY))==-1){
            perror("open");
            exit(1);
        }

        /* ricava informazioni sul file */
        if((fstat(fd,&statbuf))==-1){
            perror("fstat");
            exit(1);
        }

        /* mappa il file in mmoria */
        if((memoria=(char *)mmap(NULL,statbuf.st_size,PROT_READ,MAP_PRIVATE,fd,0))==MAP_FAILED){
            perror("mmap");
            exit(1);
        }

        /* esegue il controllo dei caratteri */
        for(int i=0; i<statbuf.st_size; i++){
            if(memoria[i]>='A' && memoria[i]<='Z')
                stat++;
            if(memoria[i]>='a' && memoria[i]<='z')
                stat++;
        }
        /* manda il risultato al processo padre */
        printf("Analyzer: %s %d\n",messaggio.path,stat);
        messaggio.type=3;
        messaggio.terminato=0;
        messaggio.stat=stat;
        if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
            perror("msgsnd");
            exit(1);
        }
    }
    /* manda il messaggio per comunicare la sua terminazione */
    messaggio.type=3;
    messaggio.terminato=1;
    if((msgsnd(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
        perror("msgsnd");
        exit(1);
    }

    exit(0);
}

void padre(int msg_id){
    int tot=0;
    msg messaggio;

    while(true){
        /* riceve i messaggi dal processo Analyzer */
        if((msgrcv(msg_id,&messaggio,sizeof(messaggio)-sizeof(long),3,0))==-1){
            perror("msgrcv");
            exit(1);
        }

        /* se il processo Analyzer ha terminato esce dal ciclo */
        if(messaggio.terminato==1){
            break;
        }

        /* aggiorna il totale */
        tot+=messaggio.stat;
    }

    printf("Padre: totale di %d caratteri alfabetici\n",tot);

    exit(0);
}

int main(int argc, char *argv[]){
    int msg_id;

    /* controlla gli argomenti passati */
    if(argc!=2){
        printf("Utilizzo: %s [directory]\n",argv[0]);
        exit(1);
    }

    /* crea la coda di messaggi */
    if((msg_id=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0660))==-1){
        perror("msgget");
        exit(1);
    }

    if(fork()==0){
        Scanner(argv[1],msg_id);
    }
    if(fork()==0){
        Analyzer(msg_id);
    }
    padre(msg_id);

    wait(NULL);
    wait(NULL);

    msgctl(msg_id,IPC_RMID,NULL);
}