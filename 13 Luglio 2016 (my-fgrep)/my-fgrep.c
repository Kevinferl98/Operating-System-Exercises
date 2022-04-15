#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define MAX_LEN 1024

void reader(char *input, int *pipe_id){
    FILE *file, *out;
    char buffer[MAX_LEN];

    /* apre il file in sola lettura con uno stream */
    if((file=fopen(input,"r"))==NULL){
        perror("file");
        exit(1);
    }

    /* associa il secondo stream con la pipe in scrittura */
    if((out=fdopen(pipe_id[1],"w"))==NULL){
        perror("fdopen");
        exit(1);
    }

    /* legge riga per riga e scrive sulla pipe */ 
    while(fgets(buffer,MAX_LEN,file)){
        fputs(buffer,out);
    }

    exit(0);
}

void filterer(char *word, int *pipe_id, const char *pathname, int inverse_logic, int case_insensitive){
    char buffer[MAX_LEN];
    FILE *in, *out;

    /* associa il primo stream con la pipe in lettura */
    if((in=fdopen(pipe_id[0],"r"))==NULL){
        perror("fdopen");
        exit(1);
    }

    /* associa il secondo stream con la FIFO in scrittura */
    if((out=fopen(pathname,"w"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* effettua il controllo e scrive sulla FIFO */
    while(fgets(buffer,MAX_LEN,in)!=NULL){
        char *match;
        if(case_insensitive)
            match=strcasestr(buffer,word);
        else if(!case_insensitive)
            match=strstr(buffer,word);
        if((!inverse_logic && match) || (inverse_logic && !match))
            fputs(buffer,out);
    }

    exit(1);
}

void writer(const char *pathname){
    FILE *in;
    char buffer[MAX_LEN];

    /* associa lo stream con la FIFO in lettura */
    if((in=fopen(pathname,"r"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* legge dalla FIFO e strampa sulla stdout */
    while(fgets(buffer,MAX_LEN,in)){
        printf("%s",buffer);
    }

    exit(0);
}

int main(int argc, char *argv[]){
    int pipe_id[2];
    const char *pathname="/tmp/fifo";
    int inverse_logic=0;
    int case_insensitive=0;
    char *word;
    int index=0;

    /* controlla gli argomenti passati */
    if(argc<3){
        printf("Utilizzo: %s [-i] [-v] <word> [file]\n",argv[0]);
        exit(1);
    }

    /* crea la pipe */
    if(pipe(pipe_id)==-1){
        perror("pipe");
        exit(1);
    }

    /* crea la FIFO */
    unlink(pathname);
    if(mkfifo(pathname,0600)==-1){
        perror("mkfifo");
        exit(1);
    }

    for(int i=1; i<4; i++){
        if(strcmp(argv[i],"-i")==0){
            case_insensitive=1;
        }
        else if(strcmp(argv[i],"-v")==0){
            inverse_logic=1;
        }
        else{
            word=argv[i];
            index=i;
            break;
        }
    }

    if(fork()==0){
        close(pipe_id[0]);
        reader(argv[index+1],pipe_id);
    }
    if(fork()==0){
        close(pipe_id[1]);
        filterer(word,pipe_id,pathname,inverse_logic,case_insensitive);
    }
    if(fork()==0){
        close(pipe_id[0]);
        close(pipe_id[1]);
        writer(pathname);
    }
    close(pipe_id[0]);
    close(pipe_id[1]);

    wait(NULL);
    wait(NULL);
    wait(NULL);

    unlink(pathname);
}