#include "stdio.h"
#include "string.h"
#include "jobExecutor.h"
#include "worker.h"
#include "stdlib.h"
#include "unistd.h"
#include "fcntl.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "signal.h"
#include "poll.h"
#include <sys/wait.h>

#define INPUT_BUFSIZE 256
#define MSGHEADERSIZE 5

extern int child_flag;
extern int exit_flag;
extern char* pipeout;

/* Counts the number of paths in a file(document) and returns that number */
int getPathCount(FILE* file){
    char ch;
    int lines = 0;
    while((ch = getc(file)) != EOF){
        if(ch == '\n')
            lines++;
    }
    return lines;
}

/* Returns the size of a text in bytes*/
int getPathLength(FILE* file){
    char ch;
    int length = 0;
    while((ch = getc(file)) != '\n')
        length++;
    return length;
}

/* Send the paths which the killed worker had */
int recoverWorker(int w,struct pollfd** poll_list,char **path_map,int total_paths,int index){
    int paths_num = 0;
    int paths_size = 0;

    for(int j = 0; j < total_paths; j++){
        int iworker = j % w;
        if(iworker == index){
            paths_size += strlen(path_map[j]);
            paths_num++;
        }
    }
    /* Go to the pipe's end to discard any unwritten or unread indormation */
    lseek(poll_list[index]->fd,0,SEEK_END);
    lseek(poll_list[index+w]->fd,0,SEEK_END);

    if((write(poll_list[index]->fd, "PATH\0", MSGHEADERSIZE)) ==  -1){
        perror("JE: write");
        exit(4);
    }
    if((write(poll_list[index]->fd, (int*) &paths_num, sizeof(int))) ==  -1){
        perror("JE: write");
        exit(4);
    }
    if((write(poll_list[index]->fd, (int*) &paths_size, sizeof(int))) ==  -1){
        perror("JE: write");
        exit(4);
    }
    for(int j = 0; j < total_paths; j++){
        int iworker = j % w;
        if(iworker == index){   //Choose the paths based on the index of the worker who is dead
            if((write(poll_list[iworker]->fd, path_map[j] , strlen(path_map[j]))) ==  -1){
                perror("JE: write");
                exit(4);
            }
        }
    }
    if((write(poll_list[index]->fd, "\0" , 1)) ==  -1){
        perror("JE: write");
        exit(4);
    }
    /* Wait to respond with DONE */
    int retval = poll(poll_list[index+w],(unsigned long) 1,-1);
    poll_list[index+w]->events = POLLIN;
    poll_list[index+w]->revents = 0;
    char code[5];
    if(read(poll_list[index+w]->fd, code ,MSGHEADERSIZE) < 0) {
        perror("j:read");
        exit(5);
    }

}

/* Dispatch the paths to each worker based on their index of creation */
int pathDispatch(int w,struct pollfd** poll_list,char **path_map,int total_paths){
    int paths_per_worker = total_paths/w +1;
    int *size_per_worker = malloc(w*sizeof(int));
    for(int i = 0; i < w; i++)
        size_per_worker[i] = 0;
    for(int j = 0; j < total_paths; j++){
        int iworker = j % w;
        size_per_worker[iworker] += strlen(path_map[j]);
    }
    for(int i = 0; i < w; i++){
        if((write(poll_list[i]->fd, "PATH\0", MSGHEADERSIZE)) ==  -1){
            perror("JE: write");
            exit(4);
        }
        if((write(poll_list[i]->fd, (int*) &paths_per_worker, sizeof(int))) ==  -1){
            perror("JE: write");
            exit(4);
        }
        if((write(poll_list[i]->fd, (int*) &size_per_worker[i], sizeof(int))) ==  -1){
            perror("JE: write");
            exit(4);
        }
    }
    for(int j = 0; j < total_paths; j++){
        int nwrite = 0; int iworker = j % w;
        if((nwrite = write(poll_list[iworker]->fd, path_map[j] , strlen(path_map[j]))) ==  -1){
            perror("JE: write");
            exit(4);
        }
    }
    for(int i = 0; i < w; i++){
        if((write(poll_list[i]->fd, "\0" , 1)) ==  -1){
            perror("JE: write");
            exit(4);
        }
    }
    free(size_per_worker);
}

/* Wait for all the workers to write in their pipes using poll() Returns the number of the workers who responded within timeout*/
int waitForWorkers(int w, struct pollfd** poll_list,int timeout,int pipe_resp[]){
    int retval = 0;
    int n_responses = 0;
    for (int i = 0; i < w; i++){
        retval = poll(poll_list[i+w],(unsigned long) 1,timeout*1000);
        if(retval > 0){
            if(pipe_resp != NULL)
                pipe_resp[n_responses] = i+w;
            n_responses++;
        }
        retval = 0;
    }
    for (int i = 0; i < w; i++){
        poll_list[i+w]->events = POLLIN;
        poll_list[i+w]->revents = 0;
    }
    return n_responses;
}

void appUserInterface(struct pollfd** poll_list,int w,int total_paths,char** path_map,pid_t* worker_pid){
    printf("=== JobExecutor Starting ===\n\n");
    char code[5];
    char input[INPUT_BUFSIZE];
    waitForWorkers(w,poll_list,-1,NULL);    //Wait for workers to take their paths and send DONE
    for(int i = 0; i < w; i++){
        if(read(poll_list[i+w]->fd, code ,MSGHEADERSIZE) < 0) {
            perror("j:read");
            exit(5);
        }
    }
    /* JobExecutor Main Loop */
    int pipeout_fd;
    printf("\n=== Welcome to JobExecutor ===\n\n");
    while(1){
        char cmd[20] = "";
        char* rcvbuf;
        if(pipeout == NULL){
            printf("jobExecutor> ");
            fgets(input,INPUT_BUFSIZE,stdin);
            sscanf(input,"%s",cmd);
        }
        else{
            printf("JobExecutor waiting for command from pipe\n");
            if((pipeout_fd = open(pipeout, O_RDONLY)) < 0){
                perror("open");
                printf("Cant open pipe. Exiting.\n");
                exit(10);
            }
            int msg_len_rcv = 0;
            if(read(pipeout_fd, &msg_len_rcv ,sizeof(int)) < 0) {
                perror("j:read");
            }
            rcvbuf = malloc(msg_len_rcv+1);
            if(read(pipeout_fd, rcvbuf ,msg_len_rcv) < 0) {
                perror("j:read");
            }
            close(pipeout_fd);
            rcvbuf[msg_len_rcv] = '\0';
            printf("Read from pipe : %s\n",rcvbuf);
            sscanf(rcvbuf,"%s",cmd);
        }

        if(child_flag == 1){    // It means that a worker has been killed so replace him
            pid_t killedpid = waitpid(-1, NULL, WNOHANG);
            printf("Worker with pid: %d killed!\n",killedpid);

            for(int i = 0; i < w; i++){
                if(killedpid == worker_pid[i]){
                    pid_t newpid = fork();
                    if(newpid == 0){
                        int tot = workerMain(poll_list[i]->fd,poll_list[i+w]->fd,poll_list,i,w);
                        printf("Total Strings found from Worker_%d : %d\n",getpid(),tot);
                        printf("Worker_%d Exiting. Bye!\n",getpid());
                        return;
                    }
                    else{
                        recoverWorker(w,poll_list,path_map,total_paths,i);
                        worker_pid[i] = newpid;
                        child_flag = 0;
                    }
                }
            }
        }
        if(exit_flag == 1)  // When a signal to quit received exit normally
            strcpy(cmd,"/exit");

        if(strcmp(cmd,"")==0)
            continue;
        if(strcmp(cmd,"/help") == 0){
            printf("Type /search q1 q2 ... qi where qi the query words to search for a \
text\nType /maxcount [word] to print the word with the highest document frequency\nType /maxcount [word] to print the word with the highest document \
frequency\nType /wc to print the size in bytes of all files\nType /clear to clear command line interface\nType /exit to exit minisearch\n");
        }
        else if(strcmp(cmd,"/exit") == 0){
            for(int i = 0; i < w; i++){
                if((write(poll_list[i]->fd, "EXIT\0", MSGHEADERSIZE)) ==  -1){
                    perror("JE: write");
                    exit(4);
                }
            }
            for(int i = 0; i < w; i++){
                printf("waiting for pid: %d to exit\n",worker_pid[i]);
                waitpid(worker_pid[i],NULL,0);  //wait for workers to exit
            }
            child_flag = 0;
            break;
        }
        else if(strcmp(cmd,"/search") == 0){
            char* query;
            if(pipeout == NULL)
                query = input+8;//+8 bytes to exclude command
            else
                query = rcvbuf+8;
            char* word = strtok(query," \n");
            int timeout = -1;
            char* keywords[10];
            for(int i = 0; i < 10; i++)
                keywords[i] = NULL;
            int wordindex = 0;
            int deadline_exists = 0;    // when -d not used default timeout to 5 seconds
            while(word != NULL){
                if(strcmp(word,"-d") == 0){
                    deadline_exists = 1;
                    char* num = strtok(NULL," \n");
                    timeout = atoi(num);
                    if(num == NULL)
                        timeout = 5;
                    break;
                }
                keywords[wordindex] = word;
                wordindex++;
                word = strtok(NULL," \n");
            }
            if(deadline_exists == 0){
                timeout = 5;
                printf("Usage: /search word1 ... wordN -d (int) deadline\nSetting default deadline to 5 seconds\n");
            }
            for(int i = 0; i < w; i++){
                if((write(poll_list[i]->fd, "SRCH\0", MSGHEADERSIZE)) ==  -1){
                    perror("JE: write");
                    exit(4);
                }
                if((write(poll_list[i]->fd, (int*) &wordindex, sizeof(int))) ==  -1){
                    perror("JE: write");
                    exit(4);
                }
                for(int j = 0; j < wordindex; j++){
                    int len = strlen(keywords[j])+1;
                    if((write(poll_list[i]->fd, (int*) &len, sizeof(int))) ==  -1){
                        perror("JE: write");
                        exit(4);
                    }
                    if((write(poll_list[i]->fd, keywords[j], len)) ==  -1){
                        perror("JE: write");
                        exit(4);
                    }
                }
            }
            char** path = malloc(w*sizeof(char*));
            char** file = malloc(w*sizeof(char*));
            int pipe_resp[w];
            int n_resp = waitForWorkers(w,poll_list,timeout,pipe_resp);

            int total_msg_size = 9;
            char* results_buf_old = malloc(9);
            char* results_buf_new;
            strcpy(results_buf_old,"Results:\n");

            for(int i = 0; i < n_resp; i++){
                for(int k = 0; k < 10; k++){
                    if(keywords[k] != NULL){
                        int path_len;
                        int total_occur = 0;
                        if(read(poll_list[pipe_resp[i]]->fd, &total_occur , sizeof(int)) < 0) {
                            perror("j:plen:read");
                            exit(5);
                        }
                        if(total_occur == -1){ // total_occur == -1 means Word not found so continue with next keyword
                            path[i] = NULL;
                            file[i] = NULL;
                            //printf("Word %s Not Found\n",keywords[k]);
                            fflush(stdout);
                            continue;
                        }
                        for(int k = 0; k < total_occur; k++){
                            if(read(poll_list[pipe_resp[i]]->fd, &path_len , sizeof(int)) < 0) {
                                perror("j:plen:read");
                                exit(5);
                            }
                            path[i] = malloc(path_len);
                            if(read(poll_list[pipe_resp[i]]->fd, path[i] , path_len) < 0) {
                                perror("j:path:read");
                                exit(5);
                            }
                            int file_len;
                            if(read(poll_list[pipe_resp[i]]->fd, &file_len , sizeof(int)) < 0) {
                                perror("j:flen:read");
                                exit(5);
                            }
                            file[i] = malloc(file_len);
                            if(read(poll_list[pipe_resp[i]]->fd, file[i] , file_len) < 0) {
                                perror("j:file:read");
                                exit(5);
                            }
                            path[i][path_len-1] = '\0';
                            file[i][file_len-1] = '\0';

                            fflush(stdout);
                            //printf("\n==========================================\n\nPath Found: %s/%s\n",path[i],file[i]);
                            fflush(stdout);
                            total_msg_size += 13 + path_len+file_len;
                            results_buf_new = realloc(results_buf_old,total_msg_size);
                            strcat(results_buf_new,"Path Found: ");
                            strcat(results_buf_new,path[i]);
                            strcat(results_buf_new,file[i]);
                            strcat(results_buf_new,"\n");
                            results_buf_old = results_buf_new;


                            free(path[i]);
                            free(file[i]);

                            int total_lines = 0;
                            if(read(poll_list[pipe_resp[i]]->fd, &total_lines , sizeof(int)) < 0) {
                                perror("j:plen:read");
                                exit(5);
                            }

                            for(int l = 0; l < total_lines; l++){
                                int linelen = 0;
                                if(read(poll_list[pipe_resp[i]]->fd, &linelen , sizeof(int)) < 0) {
                                    perror("j:plen:read");
                                    exit(5);
                                }
                                int linenum = 0;
                                if(read(poll_list[pipe_resp[i]]->fd, &linenum , sizeof(int)) < 0) {
                                    perror("j:plen:read");
                                    exit(5);
                                }
                                char line[linelen+1];
                                if(read(poll_list[pipe_resp[i]]->fd, line ,linelen) < 0) {
                                    perror("j:plen:read");
                                    exit(5);
                                }
                                line[linelen] = '\0';
                                fflush(stdout);
                                //printf("> Line %d : %s\n",linenum,line);
                                fflush(stdout);

                                total_msg_size += 9 + linelen;
                                results_buf_new = realloc(results_buf_old,total_msg_size);
                                strcat(results_buf_new,"> Line: ");
                                strcat(results_buf_new,line);
                                strcat(results_buf_new,"\n");
                                results_buf_old = results_buf_new;
                            }
                        }
                    }
                }
            }
            printf("\n%d out of %d workers responed within deadline\n",n_resp,w);
            free(path);
            free(file);

            if((pipeout_fd = open(pipeout, O_WRONLY)) < 0){
                perror("open");
                printf("Cant open pipe. Exiting.\n");
                exit(10);
            }
            if(write(pipeout_fd, &total_msg_size ,sizeof(int)) < 0) {
                perror("j:read");
            }
            if(write(pipeout_fd, results_buf_old ,total_msg_size) < 0) {
                perror("j:read");
            }
            close(pipeout_fd);
            printf("Search process ended\n");
        }
        else if(strcmp(cmd,"/maxcount") == 0){
            char* word = input+10;
            int length = strlen(word);
            word[length-1] = '\0';
            for(int i = 0; i < w; i++){
                if((write(poll_list[i]->fd, "MAXK\0", MSGHEADERSIZE)) ==  -1){
                    perror("JE: write");
                    exit(4);
                }
                if((write(poll_list[i]->fd, &length, sizeof(int))) ==  -1){
                    perror("JE: write");
                    exit(4);
                }
                if((write(poll_list[i]->fd, word, length)) ==  -1){
                    perror("JE: write");
                    exit(4);
                }
            }

            char** path = malloc(w*sizeof(char*));
            char** file = malloc(w*sizeof(char*));
            int occur[w];
            waitForWorkers(w,poll_list,-1,NULL);
            for(int i = 0; i < w; i++){
                int path_len;
                if(read(poll_list[i+w]->fd, &path_len , sizeof(int)) < 0) {
                    perror("j:plen:read");
                    exit(5);
                }
                if(path_len == -1){ // path_len == -1 means Word not found
                    path[i] = NULL;
                    file[i] = NULL;
                    occur[i] = 0;
                    continue;
                }
                path[i] = malloc(path_len);
                if(read(poll_list[i+w]->fd, path[i] , path_len) < 0) {
                    perror("j:path:read");
                    exit(5);
                }
                int file_len;
                if(read(poll_list[i+w]->fd, &file_len , sizeof(int)) < 0) {
                    perror("j:flen:read");
                    exit(5);
                }
                file[i] = malloc(file_len);
                if(read(poll_list[i+w]->fd, file[i] , file_len) < 0) {
                    perror("j:file:read");
                    exit(5);
                }
                if(read(poll_list[i+w]->fd, &occur[i] , sizeof(int)) < 0) {
                    perror("j:occur:read");
                    exit(5);
                }
                path[i][path_len-1] = '\0';
                file[i][file_len-1] = '\0';
            }
            int max = occur[0];
            int maxi = 0;
            for(int i = 1; i < w; i++){
                if(occur[i] > max){
                    max = occur[i];
                    maxi = i;
                }
            }
            if(max == 0){
                fflush(stdout);
                printf("\nKeyword not found!\n\n");
                fflush(stdout);
            }
            else{
                fflush(stdout);
                printf("\nKeyword found the most times [ %d ] in path: %s/%s\n\n",max,path[maxi],file[maxi]);
                fflush(stdout);
            }
            for(int i = 0; i < w; i++){
                free(path[i]);
                free(file[i]);
            }
            free(path);
            free(file);
        }
        else if(strcmp(cmd,"/mincount") == 0){
            char* word = input+10;
            int length = strlen(word);
            word[length-1] = '\0';
            for(int i = 0; i < w; i++){
                if((write(poll_list[i]->fd, "MINK\0", MSGHEADERSIZE)) ==  -1){
                    perror("JE: write");
                    exit(4);
                }
                if((write(poll_list[i]->fd, &length, sizeof(int))) ==  -1){
                    perror("JE: write");
                    exit(4);
                }
                if((write(poll_list[i]->fd, word, length)) ==  -1){
                    perror("JE: write");
                    exit(4);
                }
            }

            char** path = malloc(w*sizeof(char*));
            char** file = malloc(w*sizeof(char*));
            int occur[w];
            waitForWorkers(w,poll_list,-1,NULL);
            for(int i = 0; i < w; i++){
                int path_len;
                if(read(poll_list[i+w]->fd, &path_len , sizeof(int)) < 0) {
                    perror("j:plen:read");
                    exit(5);
                }
                if(path_len == -1){ // path_len == -1 means Word not found
                    path[i] = NULL;
                    file[i] = NULL;
                    occur[i] = 0;
                    continue;
                }
                path[i] = malloc(path_len);
                if(read(poll_list[i+w]->fd, path[i] , path_len) < 0) {
                    perror("j:path:read");
                    exit(5);
                }
                int file_len;
                if(read(poll_list[i+w]->fd, &file_len , sizeof(int)) < 0) {
                    perror("j:flen:read");
                    exit(5);
                }
                file[i] = malloc(file_len);
                if(read(poll_list[i+w]->fd, file[i] , file_len) < 0) {
                    perror("j:file:read");
                    exit(5);
                }
                if(read(poll_list[i+w]->fd, &occur[i] , sizeof(int)) < 0) {
                    perror("j:occur:read");
                    exit(5);
                }
                path[i][path_len-1] = '\0';
                file[i][file_len-1] = '\0';
            }
            int min = occur[0];
            int mini = 0;
            for(int i = 1; i < w; i++){
                if(occur[i] < min){
                    min = occur[i];
                    min = i;
                }
            }
            if(min == 0){
                fflush(stdout);
                printf("\nKeyword not found!\n\n");
                fflush(stdout);
            }
            else{
                fflush(stdout);
                printf("\nKeyword found the least times [ %d ] in path: %s/%s\n\n",min,path[mini],file[mini]);
                fflush(stdout);
            }
            for(int i = 0; i < w; i++){
                free(path[i]);
                free(file[i]);
            }
            free(path);
            free(file);
        }
        else if(strcmp(cmd,"/wc") == 0){

            for(int i = 0; i < w; i++){
                if((write(poll_list[i]->fd, "WORD\0", MSGHEADERSIZE)) ==  -1){
                    perror("JE: write");
                    exit(4);
                }
            }

            waitForWorkers(w,poll_list,-1,NULL);
            int total_bytes = 0; int kc = 0;
            int total_words = 0; int wc = 0;
            int total_lines = 0; int lc = 0;

            for(int i = 0; i < w; i++){
                if(read(poll_list[i+w]->fd, &kc , sizeof(int)) < 0) {
                    perror("j:read");
                    exit(5);
                }
                if(read(poll_list[i+w]->fd, &wc , sizeof(int)) < 0) {
                    perror("j:read");
                    exit(5);
                }
                if(read(poll_list[i+w]->fd, &lc , sizeof(int)) < 0) {
                    perror("j:read");
                    exit(5);
                }
                total_bytes += kc;
                total_words += wc;
                total_lines += lc;
            }
            printf("\nTotal Bytes: %d\tTotal Words: %d\tTotal Lines: %d\n\n",total_bytes,total_words,total_lines);

        }
        else if(strcmp(cmd,"/clear") == 0)
            system("clear");
        else
            printf("jobExecutor> Unknown command\n");
    }

}
