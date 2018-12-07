// File name: user.c
// Executable: user
// Created by: Andrew Audrain
// Created on: 11/21/2018
//
// Program to simulate a USER process that requests memory access from OSS (the
//  Operating System Simulator ). See oss.c for more info.

#include "header.h"


int main ( int argc, char *argv[] ) {
    
    // ID information
    long myPID = getpid();          // Store the USER PID.
    long ossPID = getppid();        // Store the OSS's PID.
    int index = atoi( argv[1] );    // Store the argument that was passed from OSS through execl.
    
//    printf( "Process %ld created by Parent %ld is being following at index %d in the PCB.\n", myPID, ossPID, index );
    
    // Set a seed unique to each individual process for random number generation.
    time_t seed;
    srand( (int)time( &seed ) % getppid() );
    
    // General Variables
    int numberOfRequests = 0;   // Counter for the number of memory requests made by USER.
    int address;                // Store the random address creating during the main loop.
    int pageNeeded;             // Store the page associated with that random address.
    int memoryRequestType;      // Store the bit flag that represent the random type of request by USER.
    int memoryRequestRNG;       
    int terminationRNG;
    
    /* Signal Handling */
    if ( signal ( SIGINT, sig_handle) == -1 ) {
        perror ( "USER: Failure to setup signal handing." );
        return 1;
    }
    
    /* Shared Memory */
    // Connect to shared memory.
    if ( ( shmClockID = shmget(shmKey, ( 2 * sizeof ( unsigned int ) ), 0666 ) ) == -1 ) {
        perror ( "USER: Failure to find shared memory space for simulated clock." );
        return 1;
    }
    
    // Attach to shared memory.
    if ( ( shmClock = ( int *) shmat(shmClockID, NULL, 0) ) < 0 ) {
        perror ( "USER: Failure to attach to shared memory space for simulated clock." );
    }
    
    /* Message Queue */
    // Connect to message queue set up by OSS.
    if ( ( messageID = msgget( messageKey, 0666 ) ) == -1 ) {
        perror ( "USER: Failure to connect to the message queue." );
        return 1;
    }
    
    /* Main Loop */
    while ( 1 ) {
        // Prepare the message content each run.
        message.pid = myPID;
        message.msg_type = ossPID;
        message.blockIndex = index;
        message.terminate = 0;
        message.sentTime[0] = shmClock[0];
        message.sentTime[1] = shmClock[1];
        
        // Generate random numbers to determine the action for current run through loop.
        memoryRequestRNG = ( rand() % ( 100 - 0 + 1 ) + 0 );
        terminationRNG = ( rand() % ( 100 - 0 + 1 ) + 0 );
        
        /* 1 - Check if process is going to terminate. */
        // Process can potentially terminate if it has made at least 1000 memory requests. After
        //  reaching that threshold, if terminationRNG was between 80-100, USER will terminate.
        //  These numbers can be adjusted to tune exit rates of child processes.
        if ( ( numberOfRequests >= 1000 ) && ( terminationRNG >= 80 ) ) {
            message.terminate = 1;
            
            if ( msgsnd( messageID, &message, sizeof( message ), 1 ) == -1 ) {
                perror ( "USER: Failure to send termination message to OSS." );
                return 1;
            }
            
            break;  // Break out of loop and terminate.
        }
        
        /* 2 - Determine the page that USER will reference in its memory request. */
        // Generate an number between 0-31000. This will be the fake memory address USER wants to access.
        address = ( rand() % ( 31000 - 0 + 1 ) + 0 );
        
        // Divide address by 1000 to give the fake page that address is stored in with respect to the USER's entry
        //  in the Process Control Block in OSS.
        pageNeeded = ( rand() % ( 31 - 0 + 1 ) + 0 );
        
        /* 3 - Determine if the memory request will be read of write...50/50 chance. */
        // If memoryRequestRNG was less than 50, the request will be write (0). Otherwise, the request will be write (1).
        if ( memoryRequestRNG < 50 ) {
            memoryRequestType = 0;
        } else {
            memoryRequestType = 1;
        }
        
        /* 4 - Send memory request message to OSS. */
        // Prepare the rest of the message content.
        message.memoryAddress = address;
        message.pageRef = pageNeeded;
        message.requestType = memoryRequestType;
        
//        printf( "REQUEST - TO: %ld FROM: %ld PCB: %d TYPE: %d ADDRESS: %d PAGE: %d", message.msg_type, message.pid, message.blockIndex, message.requestType, message.memoryAddress, message.pageRef );
        
        // Send message to OSS.
        if ( msgsnd( messageID, &message, sizeof ( message ), 1 ) == -1 ) {
            perror( "USER: Failure to send request to OSS." );
            return 1;
        }
        
        /* 5 - Wait for response from OSS. */
        msgrcv( messageID, &message, sizeof( message ), myPID, 0 );
//        printf( "Process %ld received response from OSS and is continuing.\n" );
        
        /* 6 - Increase the counter tracking the number of memory requests made by USER. */
        numberOfRequests++;
    }
    
//    printf( "Process %ld is terminating after having made %d memory requests.\n", myPID, numberOfRequests );
    
    return 0;
} // End of main


void sig_handle ( sig_num ) {
    
    if ( sig_num == SIGINT ) {
        printf ( "Process %d received signal to terminate.\n.", getpid() );
    
        exit ( 0 );
    }
    
} // End of sig_handle
