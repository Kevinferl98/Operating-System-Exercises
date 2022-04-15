#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define MAX_LEN 1024

/* controlla se la stringa passata è palindroma */
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

void reader(int *pipe_rp, char *input){
    int fd;
    FILE *out;
    char *memoria;
    struct stat statbuf;

    /* apre il file in sola lettura */
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

    /* associa lo stream con la pipe in scrittura */
    if((out=fdopen(pipe_rp[1],"w"))==NULL){
        perror("fdopen");
        exit(1);
    }
    /* scrive sulla pipe il contenuto del file */
    fprintf(out,"%s",memoria);

    exit(0);
}

void writer(int *pipe_pw){
    FILE *in;
    char buffer[MAX_LEN];

    /* associa lo stream con la pipe in lettura */
    if((in=fdopen(pipe_pw[0],"r"))==NULL){
        perror("fdopen");
        exit(1);
    }

    /* legge dalla pipe e stampa su stdout */
    while(fgets(buffer,MAX_LEN,in)){
        printf("%s",buffer);
    }

    exit(0);
}

void padre(int *pipe_rp, int *pipe_pw){
    FILE *in;
    FILE *out;
    char parola[MAX_LEN];
    
    /* associa lo stream con la prima pipe in lettura */
    if((in=fdopen(pipe_rp[0],"r"))==NULL){
        perror("fdopen");
        exit(1);
    }

    /* associa lo stream con la seconda pipe in scrittura */
    if((out=fdopen(pipe_pw[1],"w"))==NULL){
        perror("fdopen");
        exit(1);
    }

    /* legge dalla prima pipe e manda le parole palindrome alla seconda pipe */
    while(fgets(parola,MAX_LEN,in)){
        if(parola[strlen(parola)-1]=='\n')
            parola[strlen(parola)-2]='\0';
        if(check(parola)){
            fprintf(out,"%s\n",parola);
        }
    }

    exit(0);
}

int main(int argc, char *argv[]){
    int pipe_rp[2], pipe_pw[2];

    /* controlla gli argomenti passati */
    if(argc!=2){
        printf("Utilizzo: %s <input file>\n",argv[0]);
        return -1;
    }

    /* crea la prima pipe */
    if((pipe(pipe_rp))==-1){
        perror("pipe");
        exit(1);
    }

    /* crea la seconda pipe */
    if((pipe(pipe_pw))==-1){
        perror("pipe");
        exit(1);
    }

    if(fork()==0){
        close(pipe_rp[0]);
        close(pipe_pw[0]);
        close(pipe_pw[1]);
        reader(pipe_rp,argv[1]);
    }
    if(fork()==0){
        close(pipe_rp[0]);
        close(pipe_rp[1]);
        close(pipe_pw[1]);
        writer(pipe_pw);
    }
    close(pipe_rp[1]);
    close(pipe_pw[0]);
    padre(pipe_rp,pipe_pw);

    wait(NULL);
    wait(NULL);
    close(pipe_rp[0]);
    close(pipe_pw[1]);

}