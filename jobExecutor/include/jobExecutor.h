#ifndef _JOBEXEC_
#define _JOBEXEC_
#include "poll.h"
#include "unistd.h"

int getPathCount(FILE* file);
int getPathLength(FILE* file);
void appUserInterface(struct pollfd**,int,int total_paths,char**,pid_t*);
int pathDispatch(int,struct pollfd**,char**,int);
int recoverWorker(int,struct pollfd**,char**,int,int);
int waitForWorkers(int w, struct pollfd** poll_list,int timeout,int[]);
#endif
