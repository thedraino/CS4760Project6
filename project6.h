
// File name: project6.h
// Header file
// Created by: Andrew Audrain
// Created on: 11/21/2018
//
// Header file for any information or resources needed in both oss.c and user.c

#ifndef PROJECT6_HEADER_FILE
#define PROJECT6_HEADER_FILE

/* Included libraries */
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

/* Macro variables */
define MAX_CURRENT_PROCESSES 18

/* Structures */
// Structure used in the message queue 
typedef struct {
	long msg_type;		// Controls who can receive the message.
	int pid;		// Store the pid of the sender.
	int tableIndex;		// Store the index of the child process from USER.
	int request;		// Some value from 0-19 if the child process is requesting a resource from OSS.
	int release;		// Some value from 0-19 if the child is notifying OSS that it is releasing a resource.
	bool terminate;		// Default is false. Gets changed to true when child terminates. 
	bool resourceGranted;	// Default is false. Gets changed to true when OSS approves the resource request from USER.
	unsigned int messageTime[2];	// Will store the simulated clock's time at the time a message is sent
} Message;
  
/* Shared memory info */

/* Function prototypes */
void handle ( int sig_num );

#endif
