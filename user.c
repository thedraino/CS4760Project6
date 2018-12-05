
// File name: user.c
// Executable: user
// Created by: Andrew Audrain
// Created on: 11/21/2018
//
// Program to simulate a USER process requesting memory access from the OSS.

#include "project6.h"

int main ( int argc, char *argv[] ) {
	
	/* General Use Variables */
	long myPid = getpid();		// Store personal ID.
	long ossPid = getppid();	// Store OSS pid for messaging. 
	int numberOfReferences = 0; 	// Counter for the number of memory requests sent to OSS.
	time_t userSeed;
	srand ( (int)time ( &userSeed ) % myPid );	// Create a USER-specific seed for random number generation.
	
	/* Variables having to do with memory request loop */
	int memoryRequestType; 
	int memoryRequestRNG; 
	int terminationRNG; 
	int pageNeeded; 
	
	/* Signal Handling */
	if ( signal ( SIGINT, sig_handle ) == SIG_ERR ) {
		perror ( "USER: signal handling failed." ) ;
	}
	
	/* Shared Memory */
	// Find and attach to shared memory block for simulated system clock. 
	if ( ( shmClockID = shmget ( shmClockKey, ( 2 * ( sizeof ( unsigned int ) ) ), 0666 ) ) == -1 ) {
		perror ( "USER: Failure to create shared memory space for simulated clock." );
		
		return 1;
	}
	
	if ( ( shmClock = (unsigned int *) shmat ( shmClockID, NULL, 0 ) ) < 0 ) {
		perror ( "USER: Failure to attach to shared memory space for simulated clock." );
	
		return 1;
	}
	
	/* Message Queue */
	// Find and join the message queue set up by OSS. 
	if ( ( messageID = msgget ( messageKey, 0666 ) ) == -1 ) {
		perror ( "USER: Failure to join the message queue." );
		
		return 1;
	}
	
	// Set message.pid and message.msg_type since they will never change.
	message.pid = myPid; 
	message.msg_type = ossPid; 
	
	//  Main Loop. Only exits if process terminates. 
	while ( 1 ) {
		message.terminate = false; 
		message.sentTime[0] = shmClock[0];
		message.sentTime[1] = shmClock[1];
		memoryRequestRNG = ( rand() % 100 ) + 1;
		terminationRNG = ( rand() % 100 ) + 1; 
		
		/* 1. Check if process can terminate. */
		if ( numberOfReferences > 1000 && terminationRNG > 80 ) {
			message.terminate = true; 
			
			if ( msgsnd ( messageID, &message, sizeof ( message ), 1 ) == -1 ) {
				perror ( "USER: Failure to send message to OSS." );
			}
			
			break;
		}
		
		/* 2. Determine the page USER will want to reference. */
		pageNeeded = ( rand() % 32001 ) / 1000; 
		message.pageReferenced =  pageNeeded;
		
		/* 3. Determine if memory request will be read or write. 50/50 chance. */
		if ( memoryRequestRNG > 50 ) {
			memoryRequestType = 0;		//  0 maps to a READ in OSS.
		} else {
			memoryRequestType = 1; 		// 1 maps to o WRITE in OSS.
		}
		    
		message.request_type = memoryRequestType;
		    
		/* 4. Send memory request message to OSS. */
		if ( msgsnd ( messageID, &message, sizeof ( message ), 1 ) == -1 ) {
			perror ( "USER: Failure to send message to OSS." );
		}
		
		/* 5. Wait for message from OSS to know that memory request was granted. */
		msgrcv ( messageID, &message, sizeof ( message ), myPid, 0 );
		
		numberOfReferences++;
	} //  End of main loop
	
	return 0;
}

void sig_handle ( int sig_num ) {
	if ( sig_num == SIGINT ) {
		printf ( "Process %d was received signal to terminate.\n", getpid() );
		exit ( 0 );
	}
}
