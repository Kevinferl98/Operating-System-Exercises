#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define PIPE_BUF 2048
#define MAX_LEN 2048

void Reader(char *file, int *pipe_id){
    int fd;
    struct stat statbuf;
    char *memoria;
    FILE *out;

    /* apre il file in sola lettura */
    if((fd=open(file,O_RDONLY))==-1){
        perror("open");
        exit(1);
    }

    /* associa lo stream con la prima pipe in scrittura */
    if((out=fdopen(pipe_id[1],"w"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* ricava informazioni sul file */
    if(stat(file,&statbuf)==-1){
        perror("stat");
        exit(1);
    }

    /* controlla se e un file regolare */
    if(!S_ISREG(statbuf.st_mode)){
        perror("l'input non è un file regolare");
        exit(1);
    }

    /* mappa il file in memoria */
    if((memoria=(char *)mmap(NULL,statbuf.st_size,PROT_READ,MAP_PRIVATE,fd,0))==MAP_FAILED){
        perror("mmap");
        exit(1);
    }

    /* scrive sulla pipe il contenuto del file */
    fprintf(out,"%s",memoria);

    exit(0);
}

void padre(int *pipe_id, int *pipe_id2, const char *word, int case_insensitive, int inverse_logic){
    int n;
    char *token;
    FILE *in, *out;
    char buffer[MAX_LEN];
    char stringa[MAX_LEN]=" ";
    char str[MAX_LEN];

    /* associa il primo stream con la prima pipe in lettura */
    if((in=fdopen(pipe_id[0],"r"))==NULL){
        perror("fdopen");
        exit(1);
    }

    /* associa il secondo stream con la seconda pipe in scrittura */
    if((out=fdopen(pipe_id2[1],"w"))==NULL){
        perror("fdopen");
        exit(1);
    }
    
    /* legge riga per riga dal primo stream e scrive sul secondo stream */
    while(fgets(buffer,MAX_LEN,in)){
        char *match;
        if(case_insensitive)
            match=strcasestr(buffer,word);
        else
            match=strstr(buffer,word);
        if((inverse_logic && !match) || (!inverse_logic && match)){
            fprintf(out,"%s",buffer);
        }
    }

    exit(0);
}

void Outputer(int *pipe_id2){
    char buffer[MAX_LEN];
    FILE *in;
    int n;

    /* associa lo stream con la seconda pipe in lettura */
    if((in=fdopen(pipe_id2[0],"r"))==NULL){
        perror("fopen");
        exit(1);
    }

    /* legge riga per riga dallo stream e stampa */
    while(fgets(buffer,MAX_LEN,in)){
        printf("%s",buffer);
    }
   // printf("%s\n",buffer);

    exit(0);
}

int main(int argc, char *argv[]){
    int pipe_id[2];
    int pipe_id2[2];
    int case_insensitive=0;
    char inverse_logic=0;
    char *word=NULL;
    char *input=NULL;
    struct stat statbuf;
    int num_file=argc-1;
    int index=0;

    /* crea la prima pipe */
    if(pipe(pipe_id)==-1){
        perror("pipe");
        return -1;
    }

    /* crea la seconda pipe */
    if(pipe(pipe_id2)==-1){
        perror("pipe");
        return -1;
    }

    /* controlla gli argomenti passati */
    if(argc<3){
        printf("Utilizzo: %s [-v] [-i] [word] <file-1> [file-2] [file-3] [...]\n",argv[0]);
        return -1;
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

    for(int i=index+1; i<argc; i++){
        if(fork()==0){
            close(pipe_id[0]);
            close(pipe_id2[0]);
            close(pipe_id2[1]);
            Reader(argv[i],pipe_id);
        }
    }
    if(fork()==0){
        close(pipe_id2[1]);
        close(pipe_id[0]);
        close(pipe_id[1]);
        Outputer(pipe_id2);
    }
    close(pipe_id[1]);
    close(pipe_id2[0]);
    padre(pipe_id,pipe_id2,word,case_insensitive,inverse_logic);

    wait(NULL);
    wait(NULL);
    close(pipe_id[0]);
    close(pipe_id2[1]);

    return 0;
}