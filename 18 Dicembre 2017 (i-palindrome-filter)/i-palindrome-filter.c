#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define MAX_LEN 1024

/* funzione per controllare se una data parola è palindroma */
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

void reader(char *input, const char *pathname_rp){
    int fd;
    FILE *file;
    char *memoria;
    struct stat statbuf;

    /* apre il file di input in sola lettura */
    if((fd=open(input,O_RDONLY))==-1){
        perror("open");
        exit(1);
    }

    /* ricava informazioni sul file */
    if((stat(input,&statbuf))==-1){
        perror("stat");
        exit(1);
    }

    /* mappa il file in memoria */
    if((memoria=(char *)mmap(NULL,statbuf.st_size,PROT_READ,MAP_SHARED,fd,0))==MAP_FAILED){
        perror("mmap");
        exit(1);
    }

    /* associa la FIFO con lo stream*/
    if((file=fopen(pathname_rp,"w"))==NULL){
        perror("fopen");
        exit(1);
    }
    /* scrive sulla FIFO il contenuto del file */
    fprintf(file,"%s",memoria);

    exit(0);
}

void padre(const char *pathname_rp, const char *pathname_pw){
    FILE *in;
    FILE *out;
    char buffer[MAX_LEN];

    /* associa la prima FIFO con lo stream di input */
    if((in=fopen(pathname_rp,"r"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* associa la seconda FIFO con lo stream di output */
    if((out=fopen(pathname_pw,"w"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* legge dalla prima FIFO */
    while(fgets(buffer,MAX_LEN,in)){
        if(buffer[strlen(buffer)-1]=='\n')
            buffer[strlen(buffer)-2]='\0';
        /* controlla se la parola prelevata è palindroma */
        if(check(buffer)){
            /* se palindroma la scrive sulla seconda FIFO */
            fprintf(out,"%s\n",buffer);
        }
    }

    exit(0);
}

int writer(const char *pathname_pw){    
    FILE *in;
    char buffer[MAX_LEN];

    /* associa la FIFO con lo stream di input */
    if((in=fopen(pathname_pw,"r"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* estra dalla FIFO e stampa su standard output */
    while(fgets(buffer,MAX_LEN,in)){
        printf("%s",buffer);
    }

    exit(0);
}

int main(int argc, char *argv[]){
    const char *pathname_rp="/tmp/fifo_rp";
    const char *pathname_pw="/tmp/fifo_pw";

    /* controlla gli argomenti passati */
    if(argc!=2){
        printf("Utilizzo: %s <input file>\n",argv[0]);
        exit(1);
    }

    unlink(pathname_pw);
    unlink(pathname_rp);

    /* crea la prima FIFO */
    if(mkfifo(pathname_rp,0660)==-1){
        perror("mkfifo");
        exit(1);
    }

    /* crea la seconda FIFO */
    if(mkfifo(pathname_pw,0660)==-1){
        perror("mkfifo");
        exit(1);
    }

    if(fork()==0){
        /* esegue il figlio reader */
        reader(argv[1],pathname_rp);
    }
    if(fork()==0){
        /* esegue il figlio writer */
        writer(pathname_pw);
    }
    /* esegue il padre */
    padre(pathname_rp,pathname_pw);

    wait(NULL);
    wait(NULL);

    unlink(pathname_pw);
    unlink(pathname_rp);

}