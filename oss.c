// File name: oss.c
// Executable: oss
// Created by: Andrew Audrain
// Created on: 11/21/2018
//
// For general program discussion, see README.
//
// Program to simulate an OS managing memory through by paging. OSS will receive
//  and manage memory request from USER processes. See user.c for more info.

#include "header.h"


/* Global Variables */
// General variables
const int READ = 0;
const int WRITE = 1;
const int MAX_PROCESSES = 18;
const int MEMORY = 256;
const int KILL_TIME = 2;
int maxCurrentProcesses = 0;
pid_t pid;

// Logfile info
FILE *fp;
char logName[15] = "program.log";

// Statistic trackers
int totalProcessesCreated = 0;
int totalMemoryRequests = 0;
int totalPageFaults = 0;
float memoryAccessesPerSecond = 0;
float pageFaultsPerMemoryAccess = 0;
int totalRuntime = 0;


/* Structures */
// Process Control Block
// Structure to represent the process control block. New processes can be created as long as the block is not full at the
//    time. Each instance of a Process will be stored in an array the size of MAX_PROCESSES.
typedef struct {
    int pageTable[32];
} Process;

// Frame Table
// Structure to help define an the OSS's frame table. Each instance will represent a frame in the frame table.
//    OSS will create a an array of 256 frames in main to represent the frame table.
typedef struct {
    int pid;
    int occupiedBit;
    int dirtyBit;
    int referenceBit;
    int processPage;
} Frame;

// Queue member
// Structure to represent a queue.
typedef struct {
    int front;
    int rear;
    int size;
    unsigned capacity;
    int * array;
} Queue;

/* Function prototypes */
// General functions
void manageClock ( unsigned int clock[] );
void cleanUpResources ( void );
void printReport ( void );

// Queue prototypes
Queue* createQueue ( unsigned capacity );
int isFull ( Queue* queue );
int isEmpty ( Queue* queue );
void enqueue ( Queue* queue, int item );
int dequeue ( Queue* queue );
int front ( Queue* queue );
int rear ( Queue* queue );



/***********************************/
/********** MAIN FUNCTION **********/
/***********************************/

int main ( int argc, char *argv[] ) {
    
    // General variables
    int i, j;                               // Control variables for loop logic.
    maxCurrentProcesses = MAX_PROCESSES;    // Default value for the max number of processes that can be running at one time.
    int maxTotalProcesses = 100;            // Guard value for the max number of processes that can be created over the course of the program.
    
    // Log file setup
    fp = fopen( logName, "w+" );    // Opens up log file for writing to. File will be overwritten during each new run of the program.
    int numberOfLines = 0;          // Tracks the number of lines in the log file.
    bool keepLogging = true;        // Flag to show when the log file has reached its line limit.
    
    
    /* Getopts */
    // Loop to implement getopt to get any command-line options and/or arguments.
    // Option -s requires ant argument.
    int opt = 0;    // Controls the getopt loop
    while ( ( opt = getopt ( argc, argv, "hs:" ) ) != -1 ) {
        switch ( opt ) {
            // Display the help message.
            case 'h':
                printf ( "Program: ./oss\n" );
                printf ( "Options:\n" );
                printf ( "\t-h : display help message (currently viewing)\n" );
                printf ( "\t-s : specify the maximum number of user processes allowed by the system at any given time\n" );
                printf ( "\tNote: -s requires an argument\n" );
                printf ( "\tNote: oss does not require any options. Default values are provided if not specified.\n" );
                printf ( "Example usage:\n" );
                printf ( "\t./oss -s 3\n" );
                printf ( "\toss will restrict the system to never have more than 3 child processes running at the\n" );
                printf ( "\tsame time.\n" );
                exit ( 0 );
                break;
                
            // Specify the maximum number of user process to be running at one time.
            case 's':
                maxCurrentProcesses = atoi ( optarg++ );
                if ( maxCurrentProcesses > MAX_PROCESSES ) {
                    maxCurrentProcesses = MAX_PROCESSES;
                }
                break;
                
             default:
                 break;
        }
    } // End of getopts
    
    
    /* Signal Handling */
    // Sets the timer alarm based on the value of KILL_TIME
    alarm ( KILL_TIME );
    
    // Catch signals for ctrl-c input or other early termination signals.
    if ( signal ( SIGINT, sig_handle ) == SIG_ERR ) {
        perror ( "OSS: ctrl-c signal failed." );
        return 1;
    }
    
    // Catch the signal for the alarm.
    if ( signal ( SIGALRM, sig_handle ) == SIG_ERR ) {
        perror ( "OSS: alarm signal failed." );
        return 1;
    }
    
    
    /* Shared Memory */
    // Create shared memory block for simulated system clock.
    if ( ( shmClockID = shmget ( shmKey, ( 2 * ( sizeof ( unsigned int ) ) ), IPC_CREAT | 0666 ) ) == -1 ) {
        perror ( "OSS: Failure to create shared memory space for simulated clock." );
        return 1;
    }
    
    // Attach to and initialize shared memory.
    if ( ( shmClock = (int *) shmat ( shmClockID, NULL, 0 ) ) < 0 ) {
        perror ( "OSS: Failure to attach to shared memory space for simulated clock." );
        return 1;
    }
    shmClock[0] = 0;    // Seconds value for the simulated clock.
    shmClock[1] = 1;    // Nanoseconds value for the simulated clock.
    

    /* Message Queue */
    // Create the message queue used for IPC.
    if ( ( messageID = msgget ( messageKey, IPC_CREAT | 0666 ) ) == -1 ) {
        perror ( "OSS: Failure to create the message queue." );
        return 1;
    }
    
    
    /* Setup for main loop */
    // Create a queue large enough to hold all of the frames in the frame table at once.
    Queue* frameQueue = createQueue ( MEMORY );
    
    // Array to store the pids of any currently active processes. Updated by OSS.
    //  After initializing, set the value at each index to -1.
    int pidArray[maxCurrentProcesses];
    for ( i = 0; i < maxCurrentProcesses; ++i ) {
        pidArray[i] = -1;
    }
    
    // Frame Table
    // After initializing, set the occupied bit to 0 for each index to start off to show every index
    //  is unoccupied.
    Frame frameTable[MEMORY];
    for ( i = 0; i < MEMORY; ++i ) {
        frameTable[i].occupiedBit = 0;
    }
    
    // Process Control Block
    // After initializing, set the page value for each index's page table to -1.
    Process pcb[maxCurrentProcesses];
    for ( i = 0; i < maxCurrentProcesses; ++i ) {
        for ( j = 0; j < 32; ++j ) {
            pcb[i].pageTable[j] = -1;
        }
    }
    
    // Various variables to be used within the main loop below.
    bool createProcess = false;         // Flags if it is okay to create a new process.
    bool pagePresent;                   // Flags if the page requested by USER is currently loaded in the frame table somewhere.
    bool noEmptyFrame;                  // Flags if the frame table is currently full.
    unsigned int newProcessTime[2] = { 0, 0 };  // Timer to set a time for a new process to be created after.
    
    fprintf( fp, "Beginning Main Loop...\n" );
    fflush( fp );
    
    /* Main Loop */
    while ( totalProcessesCreated <= maxTotalProcesses ) {
        /* 1. Check to make sure the log file has surpassed it maximum number of lines allowed.
         If it has, close the file and set the flag to false so no more writes will be done. */
        if ( ( keepLogging == true ) && ( numberOfLines >= 10000 ) ) {
            printf( "Log file has reached its maximum size.\n" );
            fprintf( fp, "Log file has reached its maximum size.\n" );
        
            keepLogging = false;
        } // End of checking the log file status
        
        /* 2 - Check to see if it is time to create a new process (shmClock needs to have passed
         the time stored in newProcessTime). */
        if ( ( ( shmClock[0] == newProcessTime[0] ) && ( shmClock[1] >= newProcessTime[1] ) ) || ( shmClock[0] > newProcessTime[0] ) )  {
            // OSS needs to find an available location in the PCB by checking the pidArray.
            for ( i = 0; i < maxCurrentProcesses; ++i ){
                if ( pidArray[i] == 0 ) {
                    createProcess = true;
                    pid = fork();   // If there was room, fork the process.
                    
                    break;
                }
            } // End of checking for room in pidArray
        
            // If the process was forked, manage the rest of the process creation pieces.
            if ( createProcess == true ) {
                // Error checking
                if ( pid == -1 ) {
                    perror( "OSS: Failure to fork the child process." );
                    kill( getpid(), SIGINT );
                }
                // In the child process...
                else if ( pid == 0 ) {
                    // Create a buffer for the child's index to in the PCB to pass with execl.
                    char indexBuffer[3];
                    sprintf( indexBuffer, "%d", i );
                    execl( "./user", "user", indexBuffer, NULL );
                }
                // In OSS...
                else {
                    pidArray[i] = pid;
                    if ( keepLogging == true) {
                        fprintf( fp, "OSS: Created Process: %d. Stored in PCB at Index: %d. Time: %d:%d.\n", pid, i, shmClock[0], shmClock[1] );
                        fflush( fp );
                        numberOfLines++;
                    }
                    
                    totalProcessesCreated++;
                }
            }
            
            // Set a new time for the next process to be created after.
            newProcessTime[0] = shmClock[0];
            newProcessTime[1] = ( rand() % ( 5000000 - 1000000 + 1 ) + 1000000 ) + shmClock[1];
            manageClock( newProcessTime );
        } // End of child creation flow.
        
        /* 3 - Check for a message from a child with a memory request. */
        msgrcv( messageID, &message, sizeof( message ), getpid(), 0 );
        totalMemoryRequests++;  // Increase the request counter once a message is received.

        /* 4 - Check for termination notice from USER. */
        if ( message.terminate == 1 ) {
            if ( keepLogging == true ) {
                fprintf( fp, "OSS: Process %ld terminated at %d:%d.\n", message.pid, message.sentTime[0], message.sentTime[1] );
                fflush( fp );
                numberOfLines++;
            }
            
            // Reset its location PID vector
            pidArray[message.blockIndex] = 0;
            
            // Clear any associated frames in the frame table based on what was stored in the PCB.
            for ( i = 0; i < 32; ++i ) {
                if ( pcb[message.blockIndex].pageTable[i] != -1 ) {
                    // Reset the frame that maps to this page in the process's page table.
                    frameTable[pcb[message.blockIndex].pageTable[i]].pid = 0;
                    frameTable[pcb[message.blockIndex].pageTable[i]].occupiedBit = 0;
                    frameTable[pcb[message.blockIndex].pageTable[i]].dirtyBit = 0;
                    frameTable[pcb[message.blockIndex].pageTable[i]].referenceBit = 0;
                    frameTable[pcb[message.blockIndex].pageTable[i]].processPage = -1;
                    
                    // Reset the page in the process's page table
                    pcb[message.blockIndex].pageTable[i] = -1;
                }
            }
            
            // Make sure the process terminated.
            kill( message.pid, SIGTERM );
            wait ( NULL );
            
            // If the process did terminate, then continue is used to skip the rest of the loop code and begin the next run through
            //  since none of the below code will impact the terminated process.
            continue;
        } // End of checking for termination
        
        /* 5 - Check for page fault. Need to check search the process's page table for the correct mapping of
         the requested page to its location in the frame table. */
        pagePresent = false;
        noEmptyFrame = false;

        // Need to search the process's page table for the correct mapping of the requested page to its frame table index.
        
        // 5a - If the page is found in the frame table...(no page fault)...
        if ( pcb[message.blockIndex].pageTable[message.pageRef] != -1 ) {
            // Flag that the page is already loaded into a frame in the frame table.
            pagePresent = true;
            
            // Reset the reference bit to 1 indicating that the frame just been referenced.
            frameTable[pcb[message.blockIndex].pageTable[message.pageRef]].referenceBit = 1;
            
            // If memory request was a read...
            if ( message.requestType == READ ) {
                if ( keepLogging == true ) {
                    fprintf( fp, "OSS: Process %ld requesting READ of address %d at time %d:%d.\n", message.pid, message.memoryAddress, message.sentTime[0], message.sentTime[1] );
                    fflush( fp );
                    numberOfLines++;
                }
                
                // If the frame's dirty bit is not set...
                if (  frameTable[pcb[message.blockIndex].pageTable[message.pageRef]].dirtyBit == 0 ) {
                    if ( keepLogging == true ) {
                        fprintf( fp, "OSS: Address %d in Frame %d. Giving data to Process %ld at time %d:%d.\n", message.memoryAddress, pcb[message.blockIndex].pageTable[message.pageRef], message.pid, shmClock[0], shmClock[1] );
                        fflush( fp );
                        numberOfLines++;
                        
                        shmClock[1] += 10;
                    }
                }
                // If the frame's dirty bit is not set...Takes slightly longer to read since there was something
                //  written to the address.
                else {
                    if ( keepLogging == true ) {
                        fprintf( fp, "OSS: Address %d in Frame %d. Dirty bit was set. Giving data to Process %ld at time %d:%d.\n", message.memoryAddress, pcb[message.blockIndex].pageTable[message.pageRef], message.pid, shmClock[0], shmClock[1] );
                        fflush( fp );
                        numberOfLines++;
                        
                        shmClock[1] += 15;
                    }
                }
            }
            
            // If memory request was a write...
            if ( message.requestType == WRITE ) {
                if ( keepLogging == true ) {
                    fprintf( fp, "OSS: Process %ld requesting WRITE to address %d at time %d:%d.\n", message.pid, message.memoryAddress, message.sentTime[0], message.sentTime[1] );
                    fflush( fp );
                    
                    fprintf( fp, "OSS: Address %d in Frame %d. Giving data to Process %ld at time %d:%d.\n", message.memoryAddress, pcb[message.blockIndex].pageTable[message.pageRef], message.pid, shmClock[0], shmClock[1] );
                    fflush( fp );
                    numberOfLines += 2;
                    
                    shmClock[1] += 10;
                }
            }
            
            manageClock( shmClock );
        } // End of 5a (no page fault)
        
        // 5b - if the page is not found in the frame table...(page fault/second-chance algorithm)...
        if ( !pagePresent ) {
            totalPageFaults++;
            
            // Since the page was not loaded anywhere in the frame table, need to first check to see if
            //  there is room to load a new page without unloading another.
            for ( i = 0; i < MEMORY; ++i ) {
                // If an unoccupied frame is found, load the page info into the frame.
                if ( frameTable[i].occupiedBit == 0 ) {
                    frameTable[i].pid = message.pid;
                    frameTable[i].occupiedBit = 1;
                    frameTable[i].dirtyBit = 0;
                    frameTable[i].referenceBit = 1;
                    frameTable[i].processPage = pcb[message.blockIndex].pageTable[message.pageRef];
                   
                    // Place frame index into queue to track how long it has been in the system.
                    enqueue( frameQueue, i );
                    
                    // Store frame index in the process's page to correctly map its location in the frame table.
                    pcb[message.blockIndex].pageTable[message.pageRef] = i;
                } else if ( i == ( MEMORY + 1 ) ) {
                    noEmptyFrame = true;
                }
            } // End of looking for an empty frame.
        
            if ( noEmptyFrame ) {
                // This will hold the frame that will eventually be replaced by the new frame info.
                int removedFrame = 0;
                
                // Temporary frame value to hold data while going through algorithm.
                int possibleFrame;
                
                // Flag to control algorithm.
                bool searching = true;
                
                do {
                    possibleFrame = front( frameQueue );
                    dequeue( frameQueue );
                    
                    // If frame is dequeued with a reference bit == 0, then it is the oldest-populated frame
                    //  having been reference the least-recently, so it is chosen to be replaced.
                    if ( frameTable[possibleFrame].referenceBit == 0 ) {
                        removedFrame = possibleFrame;
                        searching = false;
                        break;
                    }
                    
                    // If frame is dequeued with a reference bit == 1, then it has not been hit by the selection
                    //  algorithm yet and is given a second chance.
                    if ( frameTable[possibleFrame].referenceBit == 1 ) {
                        frameTable[possibleFrame].referenceBit = 0;
                    }
                    
                    // If the frame didn't get removed, place it back in the queue and run through the loop again.
                    enqueue( frameQueue, possibleFrame );
                } while ( searching );
                
                if ( keepLogging ) {
                    fprintf( fp, "OSS: Clearing frame %d and swapping in Process %ld Page %d.\n", removedFrame, message.pid, message.pageRef );
                    fflush( fp );
                    numberOfLines++;
                }
                
                // Update the page for the process whose page info was just unloaded.
                int tempIndex;
                int tempFrameIndex;
                for ( i = 0; i < maxCurrentProcesses; ++i ) {
                    if ( pidArray[i] == frameTable[removedFrame].pid ) {
                        tempIndex = i;
                        break;
                    }
                }
                tempFrameIndex = frameTable[removedFrame].processPage;
                pcb[tempIndex].pageTable[tempFrameIndex] = -1;
                
                // Update frame with info of new page.
                if ( message.requestType == 0 ) {
                    frameTable[removedFrame].dirtyBit = 0;
                } else {
                    frameTable[removedFrame].dirtyBit = 1;
                }
                
                frameTable[removedFrame].pid = message.pid;
                frameTable[removedFrame].occupiedBit = 1;
                frameTable[removedFrame].processPage = message.pageRef;
                frameTable[removedFrame].referenceBit = 1;
            } // End of selecting the frame to replace
        
            shmClock[1] += 150000;
            manageClock( shmClock );
        
        } // End of 5b (second chance algorithm)
        
        /* 6 - Send a message to the child to inform it that its memory request was granted. */
        message.msg_type = message.pid;
        if ( msgsnd( messageID, &message, sizeof( message ), 0) == -1 ) {
            perror( "OSS: Failure to send response message to USER." );
            return 1;
        }
        
    } // End of main loop
    
    wait ( NULL );
    
    cleanUpResources();
    
    return 0;
}

/***********************************/
/*********** END OF MAIN ***********/
/***********************************/



/* Function Definitions */

// Function that increments the clock by some amount of time at different points to simulate processing time or
//  overhead time. Also makes sure that nanoseconds are converted to seconds if/when necessary.
void manageClock ( unsigned int clock[] ) {
    clock[0] += clock[1] / 1000000000;
    clock[1] = clock[1] % 1000000000;
}

 // Function to print the after-run report showing any relevant statistics.
 void printReport() {
     totalRuntime = shmClock[0];
     memoryAccessesPerSecond = totalMemoryRequests / totalRuntime;
     pageFaultsPerMemoryAccess = totalPageFaults / totalMemoryRequests;

     printf ( "Total processes created: %d.\n", totalProcessesCreated );
     fprintf( fp, "Total processes created: %d.\n", totalProcessesCreated );
     
     printf ( "Total memory requests processed: %d.\n", totalMemoryRequests );
     fprintf( fp, "Total memory requests processed: %d.\n", totalMemoryRequests );
     
     printf ( "Total page faults: %d.\n", totalPageFaults );
     fprintf( fp, "Total page faults: %d.\n", totalPageFaults );
     
     printf ( "Number of memory accesses per second: %f.\n", memoryAccessesPerSecond );
     fprintf( fp, "Number of memory accesses per second: %f.\n", memoryAccessesPerSecond );
     
     printf ( "Number of page faults per second: %f.\n", pageFaultsPerMemoryAccess );
     fprintf( fp, "Number of page faults per second: %f.\n", pageFaultsPerMemoryAccess );
 }

// Function to terminate all shared memory and message queue upon completion or to be used with signal handling.
void cleanUpResources() {
    printReport();
    
    // Close the file.
    fclose ( fp );

    // Detach from shared memory.
    shmdt ( shmClock );

    // Destroy shared memory.
    shmctl ( shmClockID, IPC_RMID, NULL );

    // Destroy message queue.
    msgctl ( messageID, IPC_RMID, NULL );
}

// Function to handle signal handling. See comments above in code where this is setup to get more information.
void sig_handle ( int sig_num ) {
    if ( sig_num == SIGINT || sig_num == SIGALRM ) {
        printf ( "Signal to terminate was received.\n" );
        cleanUpResources();
        kill ( 0, SIGKILL );
        wait ( NULL );
        exit ( 0 );
    }
}

// Function to create a queue of given capacity.
// It initializes size of queue as 0.
Queue* createQueue ( unsigned capacity ) {
    Queue* queue = (Queue*) malloc ( sizeof ( Queue ) );
    queue->capacity = capacity;
    queue->front = queue->size = 0;
    queue->rear = capacity - 1;    // This is important, see the enqueue
    queue->array = (int*) malloc ( queue->capacity * sizeof ( int ) );
    
    return queue;
}

// Queue is full when size becomes equal to the capacity
int isFull ( Queue* queue ) {
    return ( queue->size == queue->capacity );
}

// Queue is empty when size is 0
int isEmpty ( Queue* queue ) {
    return ( queue->size == 0 );
}

// Function to add an item to the queue.
// It changes rear and size.
void enqueue ( Queue* queue, int item ) {
    if ( isFull ( queue ) )
        return;
    
    queue->rear = ( queue->rear + 1 ) % queue->capacity;
    queue->array[queue->rear] = item;
    queue->size = queue->size + 1;
}

// Function to remove an item from queue.
// It changes front and size.
int dequeue ( Queue* queue ) {
    if ( isEmpty ( queue ) )
        return INT_MIN;
    
    int item = queue->array[queue->front];
    queue->front = ( queue->front + 1 ) % queue->capacity;
    queue->size = queue->size - 1;
    
    return item;
}

// Function to get front of queue.
int front ( Queue* queue ) {
    if ( isEmpty ( queue ) )
        return INT_MIN;
    
    return queue->array[queue->front];
}

// Function to get rear of queue.
int rear ( Queue* queue ) {
    if ( isEmpty ( queue ) )
        return INT_MIN;
    
    return queue->array[queue->rear];
}
