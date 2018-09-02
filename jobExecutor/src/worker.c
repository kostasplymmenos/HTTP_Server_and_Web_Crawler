#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "string.h"
#include "limits.h"
#include "fcntl.h"
#include "signal.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "jobExecutor.h"
#include "worker.h"
#include "trie.h"

#define MSGHEADERSIZE 5 // To recognize the command to execute

void printToLogfile(FILE* logfile, char* msg){
    /*
    char datecmd[64];
    fseek(logfile,0,SEEK_END);
    sprintf(datecmd, "echo -n ""$(date)"" >> log/Worker_%d.txt", getpid());
    system(datecmd);
    fseek(logfile,-3,SEEK_END);
    fprintf(logfile,"%s",msg);
    fflush(logfile); */
}

/* Wait for parent to write to the worker's pipe */
int waitForJExec(struct pollfd** poll_list,int pipeindex){
    int retval = 0;
    while(retval != 1)
        retval = poll(poll_list[pipeindex],(unsigned long) 1,-1);
    retval = 0;
    poll_list[pipeindex]->events = POLLIN;
    poll_list[pipeindex]->revents = 0;
    return 1;
}

int workerMain(int fdread,int fdwrite,struct pollfd** poll_list,int pipeindex,int w){
    /* Initialize and define */
    char logfilename[64];
    sprintf(logfilename, "./jobExecutor/log/Worker_%d.txt", getpid());
    FILE* logfile = fopen(logfilename,"a+");
    printToLogfile(logfile,": Worker Created!\n");
    printf("I am proccess with pid: %d waiting for a command\n",getpid());

    Trie* trie;
    trieInit(&trie);
    struct dirent *pDirent;
    DIR *pDir;
    char delim[4] = "\r\n\0";
    char* msgbuf = malloc(MSGHEADERSIZE);
    /* Everything is dynamically allocated */
    char** w_path_map;  // maps index and id for paths
    int* w_ptof_map;    //maps path id and file id
    char** w_file_map; // maps index and id for files
    char** content_index;

    int total_bytes = 0;
    int total_paths = 0;
    int total_size = 0;
    int total_files = 0;
    int total_strings_found = 0;

    /* Main loop which runs when a signal has arrived */
    while(1){
        waitForJExec(poll_list,pipeindex);
        //printf("Worker_%d Woke up!\n",getpid());
        /* The first MSGHEADERSIZE bytes are the command to execute */
        if(read(fdread, msgbuf , MSGHEADERSIZE) < 0) {
            perror("w:read");
            exit(5);
        }
        if(strcmp(msgbuf,"PATH\0") == 0){
            if(read(fdread, &total_paths , sizeof(int)) < 0) {
                perror("w:read");
                exit(5);
            }
            if(read(fdread, &total_size , sizeof(int)) < 0) {
                perror("w:read");
                exit(5);
            }
            char* path_msg = malloc(total_size+1);
            if(read(fdread, path_msg , total_size+1) < 0) {
                perror("w:read");
                exit(5);
            }
            fflush(stdout);
            printf("Worker_%d> Handling Paths:\n%s",getpid(),path_msg);
            fflush(stdout);
            /* Example of the path to file mapping */
            // w_path_map     w_ptof_map           w_file_map
            // 0: path1          2                   0: file1
            // 1: path2          5 -> has files with ids 2-5
            w_path_map = malloc(total_paths*sizeof(char*));
            w_ptof_map = malloc(total_paths*sizeof(int));

            char* path_tok = strtok(path_msg,delim);
            for(int i = 0;i < total_paths;i++){
                if(path_tok!= NULL){
                    printf("path_tok: %s\n",path_tok);
                    w_path_map[i] = malloc(strlen(path_tok)+1);
                    strcpy(w_path_map[i],path_tok);
                    w_path_map[i][strlen(path_tok)] = '\0';
                }
                else
                    w_path_map[i] = NULL;
                path_tok = strtok(NULL,delim);
            }
            /* First count the number of files in the paths to allocate memory */
            for(int i = 0;i < total_paths;i++){
                if(w_path_map[i] != NULL){
                    if((pDir = opendir (w_path_map[i])) == NULL) {
                       perror("opendir");
                       printf("dir: %s\n",w_path_map[i]);
                       exit(6);
                    }
                    while((pDirent = readdir(pDir)) != NULL) {
                        if(pDirent->d_type == DT_REG)
                            total_files++;
                    }
                    closedir (pDir);
                    w_ptof_map[i] = total_files;
                }
            }
            w_file_map = malloc(total_files*sizeof(char*));
            content_index = malloc(total_files*sizeof(char*));
            int findex = 0;
            for(int i = 0;i < total_paths;i++){
                if(w_path_map[i] != NULL){
                    if((pDir = opendir (w_path_map[i])) == NULL) {
                       perror("opendir");
                       printf("dir: %s\n",w_path_map[i]);
                       exit(6);
                    }
                    while((pDirent = readdir(pDir)) != NULL) {
                        if(pDirent->d_type == DT_REG){
                            w_file_map[findex] = malloc(strlen(pDirent->d_name));
                            strcpy(w_file_map[findex],pDirent->d_name);
                            findex++;
                        }
                    }
                    closedir (pDir);
                }
            }
            /* For every file in file_map insert the text to the trie */
            int curfi = 0;
            for(int pindex = 0; pindex < total_paths; pindex++){
                for(; curfi < w_ptof_map[pindex]; curfi++){
                    fflush(stdout);
                    //printf("Worker_%d> Path: %s File: %s\n",getpid(),w_path_map[pindex],w_file_map[curfi]);
                    char filename[256];
                    strcpy(filename,w_path_map[pindex]);
                    strcat(filename,"/");
                    strcat(filename,w_file_map[curfi]);
                    FILE* file;
                    if((file = fopen(filename,"r")) == NULL) {
                       perror("fopen");
                       exit(6);
                    }
                    fseek(file,0,SEEK_END);
                    int flen = ftell(file);
                    content_index[curfi] = malloc(flen);
                    fseek(file,0,SEEK_SET);
                    size_t n = 0;
                    char* line;
                    int lnum = 0;
                    int loffset = ftell(file);
                    while(getline(&line,&n,file) != -1){
                        total_bytes += strlen(line);
                        strncpy(&content_index[curfi][loffset],line,strlen(line));
                        trieInsertLine(trie,line,pindex,curfi,lnum,loffset);
                        lnum++;
                        loffset = ftell(file);
                    }
                    //printf("\n\nStored text : %s",content_index[curfi]);
                    free(line);
                    fclose(file);
                    fflush(stdout);
                }
            }
            fflush(stdout);
            fflush(stdout);
            free(path_msg);
            if(write(fdwrite, "DONE\0" , MSGHEADERSIZE) < 0) {
                perror("w:write");
                exit(5);
            }
        }
        if(strcmp(msgbuf,"SRCH\0") == 0){
            int total_keywords = 0;

            if(read(fdread, (int*) &total_keywords, sizeof(int)) < 0) {
                perror("w:write");
                exit(5);
            }
            char** keywords_index = malloc(total_keywords*sizeof(char*));

            for(int i = 0; i < total_keywords; i++){
                int len = 0;
                if(read(fdread, (int*) &len, sizeof(int)) < 0) {
                    perror("w:write");
                    exit(5);
                }
                keywords_index[i] = malloc(len);
                if(read(fdread,  keywords_index[i], len) < 0) {
                    perror("w:write");
                    exit(5);
                }
                keywords_index[i][len-1] = '\0';
            }

            /* For every word in query search it in the trie */
            PostingList *plists[total_keywords];
            int total_paths_found = 0;
            int loglen = strlen(" : /search : ");
            for(int i = 0; i < total_keywords; i++){
                loglen += strlen(keywords_index[i]);
                trieSearchWord(trie,keywords_index[i],&plists[i]);
                if(plists[i] != NULL)
                    total_paths_found += plists[i]->nodesnum;
            }
            char** paths_found_index;
            if(total_paths_found != 0)
                paths_found_index = malloc(sizeof(char*)*total_paths_found);
            int path_i = 0;

            int path_keyword_map[total_keywords];

            for(int i = 0; i < total_keywords; i++){
                /* if word not found send -1 as the total occurences of the word and continue with the next word */
                path_keyword_map[i] = 0;
                PostingListNode* pnode = NULL;
                if(plists[i] == NULL){
                    int end = -1;
                    if(write(fdwrite, (int*) &end, sizeof(int)) < 0) {
                        perror("w:write");
                        exit(5);
                    }
                    continue;
                }
                else{
                    total_strings_found++;
                    if(write(fdwrite, (int*) &plists[i]->nodesnum, sizeof(int)) < 0) {
                        perror("w:write");
                        exit(5);
                    }
                    pnode = plists[i]->head;
                    path_keyword_map[i] = plists[i]->nodesnum;

                    /* For every file containing a word of the query store in temporarily to an array.
                    Send the results to JobExecutor */
                    while(pnode !=NULL){
                        int path_len = strlen(w_path_map[pnode->pathid])+1;
                        if(write(fdwrite, (int*) &path_len, sizeof(int)) < 0) {
                            perror("w:write");
                            exit(5);
                        }
                        if(write(fdwrite, w_path_map[pnode->pathid] , path_len) < 0) {
                            perror("w:write");
                            exit(5);
                        }
                        int file_len = strlen(w_file_map[pnode->fileid])+1;
                        if(write(fdwrite, &file_len , sizeof(int)) < 0) {
                            perror("w:write");
                            exit(5);
                        }
                        if(write(fdwrite, w_file_map[pnode->fileid] , file_len) < 0) {
                            perror("w:write");
                            exit(5);
                        }
                        paths_found_index[path_i] = malloc(path_len+file_len+4);
                        sprintf(paths_found_index[path_i],"%s/%s",w_path_map[pnode->pathid],w_file_map[pnode->fileid]);
                        strcat(paths_found_index[path_i],"\0");
                        loglen += path_len + file_len +3;
                        path_i++;

                        /* ListNode contains the line number and the first byte of the line */
                        ListNode* lnode = pnode->lhead;
                        int total_results = 0;
                        while(lnode != NULL){
                            total_results += 1;
                            lnode = lnode->next;
                        }
                        if(write(fdwrite, (int*) &total_results, sizeof(int)) < 0) {
                            perror("w:write");
                            exit(5);
                        }
                        lnode = pnode->lhead;
                        while(lnode != NULL){
                            int offset = lnode->lineoffset;
                            int len = 0;
                            while(content_index[pnode->fileid][offset+len] != '\n')
                                len++;

                            if(write(fdwrite, (int*) &len, sizeof(int)) < 0) {
                                perror("w:write");
                                exit(5);
                            }
                            if(write(fdwrite, (int*) &lnode->linenum, sizeof(int)) < 0) {
                                perror("w:write");
                                exit(5);
                            }
                            if(write(fdwrite, &content_index[pnode->fileid][offset], len) < 0) {
                                perror("w:write");
                                exit(5);
                            }
                            lnode = lnode->next;
                        }
                        pnode = pnode->next;
                    }
                }
            }

            /* Write search results to logfile */
            for(int i = 0; i < total_keywords; i++){
                char* logstr = malloc(loglen+3*total_paths_found +2000);
                sprintf(logstr," : /search : ");
                strcat(logstr,keywords_index[i]);
                strcat(logstr," ");
                if(path_keyword_map[i] == 0){
                    strcat(logstr," Not Found");
                    strcat(logstr,"\n");
                    strcat(logstr,"\0");
                    printToLogfile(logfile,logstr);
                    free(logstr);
                    continue;
                }
                for(int j = 0; j < path_keyword_map[i]; j++){
                    strcat(logstr," : ");
                    strcat(logstr,paths_found_index[j]);
                }
                strcat(logstr,"\n");
                strcat(logstr,"\0");
                printToLogfile(logfile,logstr);
                free(logstr);
            }

            /* Free Data Structures */
            for(int i = 0; i < total_keywords; i++){
                free(keywords_index[i]);
            }
            free(keywords_index);

            for(int i = 0; i < total_paths_found; i++){
                free(paths_found_index[i]);
            }
            if(total_paths_found != 0)
                free(paths_found_index);
        }
        if(strcmp(msgbuf,"WORD\0") == 0){
            if(write(fdwrite, (int*) &total_bytes, sizeof(int)) < 0) {
                perror("w:write");
                exit(5);
            }
            if(write(fdwrite, (int*) &trie->wordCount , sizeof(int)) < 0) {
                perror("w:write");
                exit(5);
            }
            if(write(fdwrite, (int*) &trie->lineCount , sizeof(int)) < 0) {
                perror("w:write");
                exit(5);
            }
        }
        /* Finds the file that has the minimum occurences of the keyword */
        if(strcmp(msgbuf,"MINK\0") == 0){
            int length = 0;
            if(read(fdread, &length , sizeof(int)) < 0) {
                perror("w:read");
                exit(5);
            }
            char keyword[length];
            if(read(fdread, keyword , length) < 0) {
                perror("w:read");
                exit(5);
            }
            int pathid; int fileid;
            int occur = findMinKeyword(trie,keyword,&pathid,&fileid);
            int path_len;
            if(occur == -1) path_len = -1;
            else path_len = strlen(w_path_map[pathid])+1;

            if(write(fdwrite, &path_len , sizeof(int)) < 0) {
                perror("w:write");
                exit(5);
            }
            if(occur != -1){
                if(write(fdwrite, w_path_map[pathid] , path_len) < 0) {
                    perror("w:write");
                    exit(5);
                }
                int file_len = strlen(w_file_map[fileid])+1;
                if(write(fdwrite, &file_len , sizeof(int)) < 0) {
                    perror("w:write");
                    exit(5);
                }
                if(write(fdwrite, w_file_map[fileid] , file_len) < 0) {
                    perror("w:write");
                    exit(5);
                }
                if(write(fdwrite, &occur , sizeof(int)) < 0) {
                    perror("w:write");
                    exit(5);
                }
            }
        }
        /* Finds the file that has the maximum occurences of the keyword */
        if(strcmp(msgbuf,"MAXK\0") == 0){
            int length = 0;
            if(read(fdread, &length , sizeof(int)) < 0) {
                perror("w:read");
                exit(5);
            }
            char keyword[length];
            if(read(fdread, keyword , length) < 0) {
                perror("w:read");
                exit(5);
            }
            int pathid; int fileid;
            int occur = findMaxKeyword(trie,keyword,&pathid,&fileid);
            int path_len;
            if(occur == -1) path_len = -1;
            else path_len = strlen(w_path_map[pathid])+1;

            if(write(fdwrite, &path_len , sizeof(int)) < 0) {
                perror("w:write");
                exit(5);
            }
            if(occur != -1){
                if(write(fdwrite, w_path_map[pathid] , path_len) < 0) {
                    perror("w:write");
                    exit(5);
                }
                int file_len = strlen(w_file_map[fileid])+1;
                if(write(fdwrite, &file_len , sizeof(int)) < 0) {
                    perror("w:write");
                    exit(5);
                }
                if(write(fdwrite, w_file_map[fileid] , file_len) < 0) {
                    perror("w:write");
                    exit(5);
                }
                if(write(fdwrite, &occur , sizeof(int)) < 0) {
                    perror("w:write");
                    exit(5);
                }
            }
        }
        /* free the data structures used, return to main to free the othe data structures and exit */
        if(strcmp(msgbuf,"EXIT\0") == 0){
            trieDelete(trie->head);
            free(msgbuf);
            for(int i = 0; i < total_paths; i++){
                if(w_path_map[i] != NULL)
                    free(w_path_map[i]);
            }
            free(w_path_map);
            free(w_ptof_map);
            for(int i = 0; i < total_files; i++){
                if(w_file_map[i] != NULL)
                    free(w_file_map[i]);
                    free(content_index[i]);
            }
            free(w_file_map);
            free(trie);
            free(content_index);
            printToLogfile(logfile,": Exited!\n");
            fclose(logfile);
            return total_strings_found;
        }
    }
}
