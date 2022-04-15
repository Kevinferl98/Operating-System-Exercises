#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define MAX_SIZE 255

/* struttura del messagio */
typedef struct{
    long type;
    char comando[MAX_SIZE];
    char param2[MAX_SIZE];
    char param3;
} msg;

/* metodo per inviare il messaggio */
int sendMessage(int msgid, int type, char *comando, char *param2, char param3){
    msg messaggio;

    strcpy(messaggio.comando,comando);
    strcpy(messaggio.param2,param2);
    messaggio.param3=param3;
    messaggio.type=(long) type;

    if((msgsnd(msgid,&messaggio,sizeof(messaggio)-sizeof(long),0))==-1){
        perror("msgsnd");
        return -1;
    }
    return 0;
}

void figlio(int msgid, int num_figlio, char *dir){
    msg messaggio;
    DIR *directory;
    struct dirent *voce;
    struct stat statbuf;
    int finito=0;

    /* prende informazioni sulla directory */
    if((stat(dir,&statbuf))==-1){
        perror("stat");
        exit(1);
    }

    /* controlla se è una directory */
    if(!S_ISDIR(statbuf.st_mode)){
        printf("Errore! l'argomento %d passato non è una directory\n",num_figlio);
        exit(1);
    }

    /* ciclo che esegue le operazioni dategli a comando */
    while(finito==0){
        /* riceve il messaggio dal padre */
        if((msgrcv(msgid,&messaggio,sizeof(messaggio)-sizeof(long),(long)num_figlio,0))==-1){
            perror("msgrcv");
            exit(1);
        }
        /* controlla se il comando è num-files */
        if(strcmp(messaggio.comando,"num-files")==0){
            /* apre la directory */
            if((directory=opendir(dir))==NULL){
                perror("opendir");
                exit(1);
            }
            long numero=0l;
            /* ciclo che legge la directory */
            while((voce=readdir(directory))!=NULL){
                /* se trova un file regolare aumenta il contatore */
                if(voce->d_type==DT_REG)
                    numero++;
            }
            /* copia il numero in una stringa e invia il messaggio */
            char stringa[MAX_SIZE];
            sprintf(stringa,"%ld file",numero);
            sendMessage(msgid,255,stringa,"0",'0');
            closedir(directory);
        }
        /* controlla se il comando è total-size */
        else if(strcmp(messaggio.comando,"total-size")==0){
            /* apre la directory */
            if((directory=opendir(dir))==NULL){
                perror("opendir");
                exit(1);
            }
            char pathvoce[2*MAX_SIZE];
            long dimensione=0l;
            /* ciclo che legge la directory */
            while((voce=readdir(directory))!=NULL){
                strcpy(pathvoce,dir);
                strcat(pathvoce,"/");
                strcat(pathvoce,voce->d_name);
                /* prende informazioni sulla voce della directory */
                if((stat(pathvoce,&statbuf))==-1){
                    perror("stat");
                    exit(1);
                }
                /* aggiunge la dimensione */
                dimensione+=statbuf.st_size;
            }
            /* copia il numero in una stringa e invia il messaggio */
            char stringa[MAX_SIZE];
            sprintf(stringa,"%ld byte",dimensione);
            sendMessage(msgid,255,stringa,"0",'0');
            closedir(directory);
        }
        /* controlla se il comando è search-char */
        else if(strcmp(messaggio.comando,"search-char")==0){
            char nome[2*MAX_SIZE];
            char *memoria;
            int dim_file, fd;
            long contatore=0;

            strcpy(nome,dir);
            strcat(nome,"/");
            strcat(nome,messaggio.param2);
            /* apre il file */
            if((fd=open(nome,O_RDWR,0660))==-1){
                perror("open");
                continue;
            }
            /* prende informazioni sul file */
            if((stat(nome,&statbuf))==-1){
                perror("stat");
                continue;
            }
            dim_file=statbuf.st_size;
            /* mappa il file in memoria */
            if((memoria=(char *)mmap(NULL,dim_file,PROT_READ,MAP_PRIVATE,fd,0))==(char *)-1){
                perror("mmap");
                continue;
            }
            /* controlla per ogni char se è presente quello cercato,
             * in caso affermativo, aumenta il contatore */
            for(int i=0; i<dim_file; i++){
                if(memoria[i]==messaggio.param3)
                    contatore++;
            }
            /* rimuove la mappatura del file */
            if((munmap(memoria,dim_file))==-1){
                perror("munmap");
                continue;
            }
            /* chiude il file precedentemente aperto */
            close(fd);

            /* copia il numero in una stringa e invia il messaggio */
            char stringa[MAX_SIZE];
            memset(stringa,0,sizeof(stringa));
            sprintf(stringa,"%ld",contatore);
            sendMessage(msgid,255,stringa,"0",'0');
        }
        /* controlla se il comando è quit */
        else if(strcmp(messaggio.comando,"quit\n")==0){
            /* esce dal ciclio e termina */
            finito=1;
        }
    }
    exit(0);
}

void padre(int msgid, int num_figli){
    char stringa[MAX_SIZE];
    char comando[MAX_SIZE];
    char param1;
    char param2[MAX_SIZE], param3;
    msg messaggio;
    int i;

    do{
        printf("file-shell2> ");
        /* salva in una stringa il comando passatto
         * dalla standard input */
        fgets(stringa,sizeof(stringa),stdin);
        i=0;
        /* copia la stringa in comando */
        while(stringa[i]!=' ' && i<strlen(stringa)){
            comando[i]=stringa[i];
            i++;
        }
        if(stringa[i]==' ')
            comando[i]='\0';
        /* controlla se il comando è num-files */
        if(strcmp(comando,"num-files")==0){
            /* estra il numero da utilizzare come parametro */
            param1=stringa[i+1];
            /* controlla la validità del comando */
            if(stringa[i+2]!='\n'){
                printf("Comando errato, troppi argomenti!\n");
            }
            else{
                /* controlla se è presente il figlio richiesto */
                if(num_figli<(int)(param1-'0')){
                    printf("Comando errato, non esiste il figlio %d!\n",(int)(param1-'0'));
                }
                else{
                    /* se è presente invia il messaggio al figlio */
                    sendMessage(msgid,(int)(param1-'0'),comando,"0",0);
                    /* riceve il messaggio contenente il risultato del comando dal figlio */
                    if((msgrcv(msgid,&messaggio,sizeof(messaggio)-sizeof(long),(long)MAX_SIZE,0))==-1){
                        perror("msgrcv");
                        exit(1);
                    }
                    /* stampa sulla standard output il risultato */
                    printf("%s\n",messaggio.comando);
                }
            }
        }
        /* controlla se il comando è total-size */
        else if(strcmp(comando,"total-size")==0){
            /* estrae il numero da utilizzare come parametro */
            param1=stringa[i+1];
            /* controlla la validità del comando */
            if(stringa[i+2]!='\n'){
                printf("Comando errato, troppi argomenti!\n");
            }
            else{
                /* controlla se è presente il figlio richiesto */
                if(num_figli<(int)(param1-'0')){
                    printf("Comando errato, non esiste il figlio %d!\n",(int)(param1-'0'));
                }
                else{
                    /* se è presente invia il messaggio al figlio */
                    sendMessage(msgid,(int)(param1-'0'),comando,"0",'0');
                    /* riceve il messaggio contenente il risultato del comando dal figlio */
                    if((msgrcv(msgid,&messaggio,sizeof(messaggio)-sizeof(long),(long)MAX_SIZE,0))==-1){
                        perror("msgrcv");
                        exit(1);
                    }
                    /* stampa sulla standard output il risultato */
                    printf("%s\n",messaggio.comando);
                }
            }
        }
        /* controlla se il comando è search-char */
        else if(strcmp(comando,"search-char")==0){
            /* estrae il numero da utilizzare come parametro */
            param1=stringa[i+1];
            /* porta l'indice all'inizio del nome del file */
            i=i+3;
            int iniz=i;
            /* copia il nome del file dentro il parametro 2 */
            while(stringa[i]!=' ' && i<strlen(stringa)){
                param2[i-iniz]=stringa[i];
                i++;
            }
            /* controlla la validità del comando */
            if(stringa[i]==' '){
                param2[i]=='\0';
                param3=stringa[i+1];
                if(stringa[i+2]!='\n'){
                    printf("Comando errato, troppi argomenti!\n");
                }
                else{
                    /* controlla se è presente il figlio richiesto */
                    if(num_figli<(int)(param1-'0')){
                        printf("Comando errato, non esiste il figlio %d\n",(int)(param1-'0'));
                    }
                    else{
                        /* se è presente invia il messaggio al figlio */
                        sendMessage(msgid,(int)(param1-'0'),comando,param2,param3);
                        /* riceve il messaggio contenente il risultato del comando dal figlio */
                        if((msgrcv(msgid,&messaggio,sizeof(messaggio)-sizeof(long),(long)MAX_SIZE,0))==-1){
                            perror("msgrcv");
                            exit(1);
                        }
                        /* stampa sulla standard output il risultato */
                        printf("%s\n",messaggio.comando);
                    }
                }
            }
            else{
                    printf("Comando errato, troppi pochi argomenti!\n");
            }
        }
        /* controlla se il comando è quit */
        else if(strcmp(comando,"quit\n")==0){
            /* manda il messaggio a tutti i figli per farli terminare */
            for(int i=1; i<=num_figli; i++){
                sendMessage(msgid,i,comando,"0",'0');
            }
            break;
        }
        else{
            printf("Comando errato!\n");
        }
    } while(strcmp(stringa,"quit\n")!=0);
    exit(0);
}

int main(int argc, char *argv[]){
    int msgid;

    /* controlla gli argomenti passati */
    if(argc<2){
        printf("Utilizzo: %s <directory-1> <directory-2> <...>\n",argv[0]);
        exit(1);
    }

    /* crea la coda di messaggi */
    msgid=msgget(IPC_PRIVATE,IPC_CREAT|IPC_EXCL|0660);

    /* crea tanti figli tanti gli argomenti passati */
    for(int i=1; i<argc; i++){
        if(fork()==0){
            figlio(msgid,i,argv[i]);
        }
    }
    /* esegue il processo padre */
    padre(msgid,argc-1);
    for(int i=1; i<argc; i++){
        wait(NULL);
    }
    /* rimuove la coda di messaggi */
    msgctl(msgid,IPC_RMID,NULL);
    return 0;
}