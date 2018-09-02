#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "limits.h"
#include "fcntl.h"
#include "signal.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "poll.h"
#include "jobExecutor.h"
#include "worker.h"

#define NO_FILE -1
#define FILE_ERR -2
#define W_ERR -3
#define FORK_ERR -4
#define MKFIFO_ERR -5
#define FIFO_OPEN_ERR -6

#define INPUT_BUFSIZE 256
#define PERM 0666

int child_flag = 0; // to check if a worker is killed
int exit_flag = 0; // if SIGINT SIGQUIT or SIGTERM received exit normally
char* pipeout;

void signal_handler2(int sig){
    //printf("SIGCHLD received!\n");
    child_flag = 1;
}
void signal_handler3(int sig){
    printf("Signal to terminate received!\nPress enter to exit safely.\n");
    exit_flag = 1;
}

int main(int argc,char* argv[]){

    /* Args Management */
    pipeout = NULL;     //for stdout
    char filename[NAME_MAX];
    int optchar;
    FILE* docfile;
    int workersnum = 4;
    char filegiven = 0;
    while((optchar = getopt(argc,argv,"d:w:p:")) != -1){
        if(optchar == 'd'){
            strcpy(filename,(const char*) optarg);
            if((docfile = fopen(filename,"r")) == NULL){
                perror("fopen");
                exit(FILE_ERR);
            }
            filegiven = 1;
        }
        if(optchar == 'w')
            workersnum = atoi(optarg);
        if(optchar == 'p'){
            pipeout = malloc(strlen(optarg)+1);
            strcpy(pipeout,optarg);
            pipeout[strlen(optarg)] = '\0';
            
            printf("Sending results through pipe %s\n",pipeout);
        }
    }
    if(filegiven == 0){
        printf("No input file\nUsage: ./jobExecutor -d docfile -w (integer > 0)\nExiting...\n");
        exit(NO_FILE);
    }
    if(workersnum <= 0){
        printf("Argument w <= 0\nUsage: ./jobExecutor -d docfile -w (integer > 0)\nExiting...\n");
        exit(W_ERR);
    }
    if(workersnum > 10){
        printf("Too many Workers\nExiting...\n");
        exit(W_ERR);
    }
    printf("=== Creating: %d workers ===\n=== Reading Document: %s ===\n\n",workersnum,filename);

    /* Reading Document */
    int total_paths = getPathCount(docfile);
    fseek(docfile,0,SEEK_SET);  // Read file 2 times to dynamically allocate memory, rewind to the beginning
    char** path_map = malloc(total_paths*sizeof(char*));
    for(int i = 0;i < total_paths;i++){
        int path_size = getPathLength(docfile);
        fseek(docfile,-path_size-1,SEEK_CUR);
        path_map[i] = malloc(path_size+2);  // has \n\0 at the end
        fgets(path_map[i],path_size+2,docfile); //stores both ID and text but it can be managed because the data is formated
        printf("Path: %s\n",path_map[i]);
    }

    /* Define Signals Pipes and Polling Lists */
    signal(SIGCHLD,signal_handler2);
    signal(SIGINT,signal_handler3);
    signal(SIGTERM,signal_handler3);
    signal(SIGQUIT,signal_handler3);
    char *fifoname[2*workersnum];
    int fifofd[2*workersnum];
    for(int i = 0; i < 2*workersnum; i++){
        fifoname[i] = malloc(30*sizeof(char));
        strcpy(fifoname[i],"./jobExecutor/je_fifo_");
        fifoname[i][22] = i + 'a';
        fifoname[i][23] = '\0';
        if(mkfifo(fifoname[i], PERM) == -1){
            perror("mkfifo");
            exit(MKFIFO_ERR);
        }
    }

    /* Store descriptors in polling list */
    /* JobExecutor writes to pipe with index i and reads from pipe with index i+workersnum */
    struct pollfd* poll_list[2*workersnum];
    for(int i = 0; i < workersnum; i++){
        poll_list[i] = malloc(sizeof(struct pollfd));
        poll_list[i+workersnum] = malloc(sizeof(struct pollfd));
    }
    for(int i = 0; i < workersnum; i++){
        if((poll_list[i]->fd = open(fifoname[i] , O_RDWR)) < 0){
            perror("JEwr: open");
            exit(FIFO_OPEN_ERR);
        }
        if((poll_list[i+workersnum]->fd = open(fifoname[i+workersnum] , O_RDWR)) < 0){
            perror("JErd: open");
            exit(FIFO_OPEN_ERR);
        }
        poll_list[i]->events = POLLIN;
        poll_list[i+workersnum]->events = POLLIN;
    }

    /* Spawn Worker Proccesses */
    pid_t* worker_pid = malloc(workersnum*sizeof(pid_t));

    for(int i = 0; i < workersnum; i++){
        worker_pid[i] = fork();
        if(worker_pid[i] == -1){
            perror("fork");
            exit(FORK_ERR);
        }
        else if(worker_pid[i] == 0) break;
    }

    /* Worker Functionality */
    for(int i = 0; i < workersnum; i++){
        if(worker_pid[i] == 0){
            /* workerMain is worker's main loop */
            int total;
            total = workerMain(poll_list[i]->fd,poll_list[i+workersnum]->fd,poll_list,i,workersnum);
            printf("Total Strings found from Worker_%d : %d\n",getpid(),total);
            /* When it's time for a worker to exit return here and free resources ,then exit*/
            for(int i = 0;i < total_paths;i++)
                free(path_map[i]);
            free(path_map);
            for(int i = 0; i < workersnum; i++){
                close(poll_list[i]->fd);
                close(poll_list[i+workersnum]->fd);
                free(poll_list[i]);
                free(poll_list[i+workersnum]);
                free(fifoname[i]);
                free(fifoname[i+workersnum]);
            }
            fclose(docfile);
            free(worker_pid);
            printf("Worker_%d Exiting. Bye!\n",getpid());
            exit(EXIT_SUCCESS);
        }
    }
    /* Send paths to workers */
    pathDispatch(workersnum,poll_list,path_map,total_paths);

    /*Application Interface and JobExecutor Main Loop*/
    appUserInterface(poll_list,workersnum,total_paths,path_map,worker_pid);     //returns with /exit command

    /* Deallocation and closing for JobExecutor*/
    for(int i = 0;i < total_paths;i++){
        free(path_map[i]);
    }
    free(path_map);
    for(int i = 0; i < workersnum; i++){
        close(poll_list[i]->fd);
        close(poll_list[i+workersnum]->fd);
        free(poll_list[i]);
        free(poll_list[i+workersnum]);
        remove(fifoname[i]);
        remove(fifoname[i+workersnum]);
        free(fifoname[i]);
        free(fifoname[i+workersnum]);
    }
    fclose(docfile);
    free(worker_pid);
    exit(EXIT_SUCCESS);
}
