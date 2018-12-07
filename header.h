// File name: header.h
// Header file
// Created by: Andrew Audrain
// Created on: 11/21/2018
//
// Header file for use by oss.c and user.c

#ifndef header_h
#define header_h

/* Included Libraries */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdbool.h>


/* Structures */
// Structure of message used in message queue.
typedef struct {
    long msg_type;
    long pid;
    int blockIndex;
    int pageRef;
    int memoryAddress; 
    int requestType;
    int terminate;
    unsigned int sentTime[2];
} Message;


/* Function Prototypes */
void sig_handle ( int sig_num );


/* Shared Memory */
int shmClockID;
int *shmClock;
key_t shmKey = 1993;


/* Message Queue */
Message message;
int messageID;
key_t messageKey = 1995; 

#endif
