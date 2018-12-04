
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


/* Structures */
// Structure used in the message queue 
typedef struct {
	long msg_type;			// Control what process can receive the message.
	int pid;			// Store the sending process's pid.
	unsigned int sentTime[2];	// Array to capture the time at which the message was sent for statistcs.
	bool read;			// Flag is set to true if child process's memory request involves a read from memory. 
	bool write;			// Flag is set to true if child process's memory request is a write to memory.
	bool terminate;			// Flag is set to true if child process is wanting to terminate. Default is false.
} Message;

/* Function prototypes */
void sig_handle ( int sig_num );	// Function to handle any early-termination signals from either OSS or USER.
  
/* Shared memory info */
Message message;
int messageID;
key_t messageKey = 1994;

/* Shared Memory Variables */
// Simulated clock
int shmClockID;
int *shmClock;
key_t shmClockKey = 1993;

#endif
