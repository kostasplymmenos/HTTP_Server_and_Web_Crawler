#ifndef _WORKER_
#define _WORKER_
#include "poll.h"

int workerMain(int fiforead,int fifowrite,struct pollfd**,int,int);
int waitForJExec(struct pollfd**,int);
void printToLogfile(FILE*,char*);

#endif
