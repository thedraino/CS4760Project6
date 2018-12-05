
// File name: oss.c
// Executable: oss
// Created by: Andrew Audrain
// Created on: 11/21/2018
//
// Program to simulate an OSS managing memory through use of the second-chance algorithm.

#include "project6.h"


/***** Structures *****/
// Structure to represent the process control block. New processes can be created as long as the block is not full at the 
//	time. Each instance of a Process will be stored in an array the size of MAX_PROCESSES.
typedef struct {
	int pid;		// Store the process ID of the process currently occupying this index of the block. 
	int pageTable[32];	// Store the process's page table to keep track of what frames the process's pages
				//	are stored in currently. 
	unsigned int processCreationTime[2];	//  Capture the time at which the process is added to the control block. 
} Process;

// Structure to help define an the OSS's frame table. Each instance will represent a frame in the frame table. 
//	OSS will create a an array of 256 Frames in main to represent the Frame Table. 
typedef struct {
	int occupiedBit;	// Bit to flag if the frame is currently occupied by a process. Default is 0.
	int processID;		// Stores the pid of the process currently associated with this frame. 
	int processPageFrame;	// Stores the frame of the associated process's page table that this reference is stored in. 
	int referenceBit;	// Bit to flag if the frame has been referenced before (used with second-chance algorithm).
	int dirtyBit;		// Bit to flag if the frame has anything "written" to it. Frames with their dirty bit set
				// 	"take longer" to process upon read requests.
} Frame;

// A structure to represent a queue.
typedef struct {
	int front, rear, size;
	unsigned capacity;
	int *array;  
} Queue;


/***** Function Prototypes *****/
// General Functions
void manageClock ( unsigned int clock[] , int timeElapsed );
// void clearPCBEntry ( int processID );
// void clearFrameEntries ( int processID);
// void printReport( void );
void cleanUpResources( void );

// Queue Protypte Functions
Queue* createQueue ( unsigned capacity );
int isFull ( Queue* queue ); 
int isEmpty ( Queue* queue );
void enqueue ( Queue* queue, int item );
int dequeue ( Queue* queue );
int front ( Queue* queue );
int rear ( Queue* queue );


/***** Global Variables *****/
// Constants
const int MAX_PROCESSES = 18;	// Default guard for maxCurrentProcesses value defined below.
const int MAX_MEMORY = 256; 	// 
const int KILL_TIME = 2;	// Constant time for alarm signal handling.
const int READ = 0;		// Constant to compare the request_type value to when a message is sent by USER. If
const int WRITE = 1;		//	request_type == 0, it is a read. If request_type == 1, it is a write. 
	
// Statistic trackers
int totalProcessesCreated = 0;
int totalMemoryRequests = 0;
int totalPageFaults = 0;
double numberOfMemoryAccessesPerSecond;
double numberOfPageFaultsPerMemoryAccess;
unsigned int totalSecondsProgramRan; 

// Logfile information
FILE *fp;
char logName[12] = "program.log";


/*************************************************************************************************************/
/*************************************************************************************************************/
/******************************************* Start of Main Function ******************************************/
/*************************************************************************************************************/
/*************************************************************************************************************/

int main ( int argc, char *argv[] ) {
	
	printf ( "Entering main.\n" );
	
	/* General Use Variables */
	int i, j;			// Control variables for loop logic. 
	int maxCurrentProcesses = MAX_PROCESSES;	// Controls the maximum number of processes that can be running at one time.
	int maxTotalProcesses = 100;	// Controls the maximum number of processes that can be created throughout the program. 
	srand ( time ( NULL ) );	// Seed for random number generation.
	
	fp = fopen ( logName, "w+" );	// Open up the log file for writing to. 
	bool keepLogging = true;	// Flags if the log file has reached its line limit. 
	int numberOfLines = 0;		// Tracks the number of lines in the log file. 
	
	/* Getopts */
	// Loop to implement getopt to get any command-line options and/or arguments.
	// Option -s requires ant argument.
	int opt = 0;	// Controls the getopt loop
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
		
			// Specify the maximum number of user processes spawned.
			case 's':
				maxCurrentProcesses = atoi ( optarg++ );
				if ( maxCurrentProcesses > MAX_PROCESSES ) {
					maxCurrentProcesses = MAX_PROCESSES;
				}
				break;

// 			default:
// 				maxCurrentProcesses = MAX_PROCESSES;
// 				break;
		}
	} // End of getopts
	
	printf ( "Max Processes: %d.\n", maxCurrentProcesses ); 
	
	/* Signal Handling */
	// Sets the timer alarm based on the value of KILL_TIME
	alarm ( KILL_TIME );	
	
	// Catch signals for ctrl-c input or other early termination signals.
	if ( signal ( SIGINT, sig_handle ) == SIG_ERR ) {
		perror ( "OSS: ctrl-c signal failed." );
	}
	
	// Catch the signal for the alarm.
	if ( signal ( SIGALRM, sig_handle ) == SIG_ERR ) {
		perror ( "OSS: alarm signal failed." );
	}
	
	printf ( "Setup signal handling.\n" );
	
	/* Shared Memory */
	// Create shared memory block for simulated system clock.
	if ( ( shmClockID = shmget ( shmClockKey, ( 2 * ( sizeof ( unsigned int ) ) ), IPC_CREAT | 0666 ) ) == -1 ) {
		perror ( "OSS: Failure to create shared memory space for simulated clock." );
	}
	
	// Attach to and initialize shared memory.
	if ( ( shmClock = (unsigned int *) shmat ( shmClockID, NULL, 0 ) ) < 0 ) {
		perror ( "OSS: Failure to attach to shared memory space for simulated clock." );
		
		return 1;
	}
	shmClock[0] = 0;	// Will hold the seconds value for the simulated clock.
	shmClock[1] = 1;	// Will hold the nanoseconds value for the simulated clock. 
	
	printf ( "Setup shared memory.\n" );
	
	/* Message Queue */
	// Create the message queue used for IPC.
	if ( ( messageID = msgget ( messageKey, IPC_CREAT | 0666 ) ) == -1 ) {
		perror ( "OSS: Failure to create the message queue." );
		
		return 1;
	}
	
	printf ( "Setup message queue.\n" );
	
	/* Setup for main loop */
	// Create a queue large enough to hold all of the frames in the frame table at once. 
	Queue* queue = createQueue ( MAX_MEMORY );
	
	// Frame Table
	// Declare the struct. Set the occupied bit for each index to 0. 
	Frame frameTable[MAX_MEMORY];
	for ( i = 0; i < MAX_MEMORY; ++i ) {
		frameTable[i].occupiedBit = 0;
	}
	
	printf ( "Setup frame table. Size: %d\n", sizeof ( frameTable ) );
	
	// Process Control Block
	// Declare the struct. Set the pid for each index to 0. Set each page's value to -1. 
	int pcbIndex = 0; 
	Process processControlBlock[maxCurrentProcesses];
	for ( i = 0; i < maxCurrentProcesses; ++i ) {
		processControlBlock[i].pid = 0;
		for ( j = 0; j < 32; ++j ) {
			processControlBlock[i].pageTable[j] = -1;
		}
	}
	
	printf ( "Setup process control block. Size: %d\n", sizeof (processControlBlock) );
	
	// Various variables used throughout main loop. 
	pid_t childPid;
	int currentIndex;
	int frameIndex;
	bool pagePresent;	// Flags if the requested page is already stored in the frame table.
	bool noEmptyFrame; 	// Flags if the frame table is full. 
	bool createProcess;	// Flags if it is okay to create a new process.
	unsigned int newProcessTime[2] = { 0, 0 };	// Time value at which a new process should be be created.
	
	/* Main Loop */
	while ( totalProcessesCreated <= maxTotalProcesses ) {
		printf ( "Entering main loop.\n" );
		
		/* 1. Check to make sure the log file has surpassed it maximum number of lines allowed. 
		If it has, close the file and set the flag to false so no more writes will be done. */
		if ( numberOfLines >= 10000 ) {
			keepLogging = false; 
			fclose ( fp );
		}
		
		/* 2. Check to see if it is time to create a new process (shmClock needs to have passed
		the time stored in newProcessTime). */
		if ( ( shmClock[0] == newProcessTime[0] && shmClock[1] >= newProcessTime[1] ) || 
		    shmClock[0] >= newProcessTime[0] ) {
			createProcess = false;	// Flag is reset to false each run through the loop. 
			
			// Check to make sure there is an available location in the process control block. 
			for ( i = 0; i < maxCurrentProcesses; ++i ) {
				if ( processControlBlock[i].pid == 0 ) {
					createProcess = true; 
					pcbIndex = i;	// Set the current index of the process control block
					childPid = fork();
					break;
				}
			} // End of checking for room in process control block
			
			// If there is room...
			if ( createProcess == true ) {
				printf ( "Creating process.\n" );
				// Error check...
				if ( childPid == -1 ) {
					perror ( "OSS: Failure to fork process correctly." );
					kill ( getpid(), SIGINT );
				}
				
				// Inside the child process...
				if ( childPid == 0 ) {
					execl ( "./user", "user", NULL );
				}
				
				// Inside OSS...
				processControlBlock[pcbIndex].pid = childPid; 
				processControlBlock[pcbIndex].processCreationTime[0] = shmClock[0];
				processControlBlock[pcbIndex].processCreationTime[1] = shmClock[1];
				totalProcessesCreated++;
				
				// Generate new time for the next process to be created. 
				newProcessTime[0] = shmClock[0]; 
				newProcessTime[1] = ( rand() %  5000000 ) + 1000000 + shmClock[1]; 
				newProcessTime[0] += newProcessTime[1] / 1000000000;
				newProcessTime[1] = newProcessTime[1] % 1000000000;
			}
		} // End of creating new process
		
		/* 3. Wait for a message from a child with a memory request. */
		msgrcv ( messageID, &message, sizeof ( message ), getpid(), 1 );
		totalMemoryRequests++;
// 		printf ( "Message received.\n" );

		
		// Set the following flags to false each run through the loop...
		noEmptyFrame = false; 
		
		/* 4. Get the index for the process in the process control block */
		for ( i = 0; i < maxCurrentProcesses; ++i ) {
			if ( processControlBlock[i].pid == message.pid ) {
				currentIndex = i; 
				break;
			}
		}
		
		/* 5. Check for termination notice from USER. */
		if ( message.terminate == true ) {
			if ( keepLogging ) {
				fprintf( fp, "OSS: Process %d terminated at %d:%d\n", message.pid, 
				       message.sentTime[0], message.sentTime[1] );
				numberOfLines++;
			}
			kill ( message.pid, SIGTERM ); 
			wait ( NULL );
			
			// Clear entry in process control block.
			processControlBlock[currentIndex].pid = 0;
			for ( i = 0; i < 32; ++i ) {
				processControlBlock[currentIndex].pageTable[i] = -1;
			}
			
			// Mark any associated frame in frame table as unoccupied. 
			for ( i = 0; i < MAX_MEMORY; ++i ){
				if ( frameTable[i].processID == message.pid ) {
					frameTable[i].occupiedBit = 0; 
					frameTable[i].processID = 0;
					frameTable[i].processPageFrame = -1;
					frameTable[i].referenceBit = 0;
					frameTable[i].dirtyBit = 0;
				}
			}
			continue; 
		} else {
			if ( keepLogging ) {
				fprintf( fp, "OSS: Process %d is requesting access to Page %d at %d:%d\n", 
				       message.pid, message.pageReferenced, shmClock[0], shmClock[1] );
				numberOfLines++;
			}
		}
		
		/* 6. Check for page fault */
		// Need to check search the process's page table for the correct mapping of the requested
		//	page to its location in the frame table. 
		// If the page is present...
		pagePresent = false; 
		if ( processControlBlock[currentIndex].pageTable[message.pageReferenced] != -1 ) {
			pagePresent = true; 
			
			// Set the index within the page table that maps to the appropriate frame. 
			frameIndex = processControlBlock[currentIndex].pageTable[message.pageReferenced];
			
			// Set reference bit for the frame back to 1 since it was recently referenced. 
			frameTable[frameIndex].referenceBit = 1; 
		
			// If request_type was a write, set the frame's dirty bit. 
			if ( message.request_type == WRITE ) {
				if ( keepLogging ) {
					fprintf( fp, "OSS: Process %d is writing to Page %d in Frame %d\n", 
						message.pid, message.pageReferenced, frameIndex );
					numberOfLines++;
				}
				frameTable[frameIndex].dirtyBit = 1; 
			}
			
			// If request_type was a write, increment the clock based on if the frame's dirty bit is 
			//	set or not. A set dirty bit will result in slight processing time as opposed to 
			//	requesting to read a blank frame. 
			if ( message.request_type == READ ) {
				if ( frameTable[frameIndex].dirtyBit == 1 ) {
					if ( keepLogging ) {
						fprintf( fp, "OSS: Process %d is reading from Page %d (dirty) in Frame %d\n", 
							message.pid, message.pageReferenced, frameIndex );
						numberOfLines++;
					}
					manageClock ( shmClock , 50 );
				} else {
					if ( keepLogging ) {
						fprintf( fp, "OSS: Process %d is reading from Page %d in Frame %d\n", 
							message.pid, message.pageReferenced, frameIndex );
						numberOfLines++;
					}
				}
			}
			manageClock ( shmClock, 10 );
		} // End of 6.1
		
		// If page is not present...
		if ( !pagePresent ) {
			totalPageFaults++; 
			if ( message.request_type == WRITE ) {
				if ( keepLogging ) {
					fprintf( fp, "OSS: Process %d requested to write to Page %d at %d:%d\n", 
						message.pid, message.pageReferenced, shmClock[0], shmClock[1] );
					fprintf( fp, "OSS: Process %d's Page %d is not in a frame. PAGE FAULT.\n",
						message.pid, message.pageReferenced );
					numberOfLines++;
					numberOfLines++;
				}
			}
			
			if ( message.request_type == READ ) {
				if ( keepLogging ) {
					fprintf( fp, "OSS: Process %d requested to read from Page %d at %d:%d\n", 
						message.pid, message.pageReferenced, shmClock[0], shmClock[1] );
					fprintf( fp, "OSS: Process %d's Page %d is not in a frame. PAGE FAULT.\n",
						message.pid, message.pageReferenced );
					numberOfLines++;
					numberOfLines++;
				}
			}
			
			// Check to see if there is currently room in the frame table for a page to be stored. 
			for ( i = 0; i < MAX_MEMORY; ++i ) {
				// If current frame is unoccupied... 
				if ( frameTable[i].occupiedBit == 0 ) {
					// Initialize characteristics to reflect new process/page info.
					frameTable[i].occupiedBit = 1; 
					frameTable[i].processID = message.pid;
					frameTable[i].processPageFrame = message.pageReferenced;
					frameTable[i].referenceBit = 1;
					frameTable[i].dirtyBit = 0;
					
					// Update the process's page table with the coordinating index from the frame table.
					processControlBlock[currentIndex].pageTable[message.pageReferenced] = i; 
					
					// Add this frame index to the queue to track the order in which frames are populated.
					enqueue ( queue, i );
					
					manageClock ( shmClock, 500 ); 
					
					// Update log file.
					if ( keepLogging ) {
						fprintf( fp, "OSS: Process %d's Page %d was loaded into Frame %d at %d:%d.\n", 
							message.pid, message.pageReferenced, i, shmClock[0], shmClock[1] );
						if ( message.request_type == WRITE ) {
							fprintf( fp, "OSS: Process %d is writing to Page %d in Frame %d\n", 
								message.pid, message.pageReferenced, i );
							numberOfLines++;
						}
						if ( message.request_type == READ ) {
							fprintf( fp, "OSS: Process %d is reading from Page %d in Frame %d\n", 
								message.pid, message.pageReferenced, i );
							numberOfLines++;
						}
						numberOfLines++;
					} 
					
					manageClock ( shmClock, 1500000 ); 
					break; 
				} 
				// If not already-empty frame was found, set the flag to true. 
				if ( i == ( MAX_MEMORY - 1 ) ) {
					noEmptyFrame = true;
				}
			} // End of logic regarding having found an empty frame		
			
			// If there are no empty frames found during the above loop, then the second-chance algorithm
			//	is used to clear one of the frames to make room for this new request. See README for 
			//	more info on how the algorithm works in general. 
			if ( noEmptyFrame == true ) {
				bool searching = true;	// Flag to control when the search is over.
				int potentialFrame;	// Temp index of the frameTable to use while searching. 
				int emptiedFrame;	// Index of the frameTable that will be removed to make room. 
				
				// Loop to simulate the second-chance algorithm. 
				do {
					// Looks at frames based on the order in which they were occupied. (Essentially sorts
					// from oldest to newest info). 
					potentialFrame = front ( queue );
					dequeue ( queue );
					
					// Looks at the referenceBit value. If the bit is set to 1, then the bit is reset
					// 	to 0, and then put back into the queue (second-chance).
					if ( frameTable[potentialFrame].referenceBit == 1 ) {
						frameTable[potentialFrame].referenceBit = 0; 
						manageClock ( shmClock, 500 );
						enqueue ( queue, potentialFrame );
					}
					
					// If the bit is set to 0, then the oldest frame having been referenced the least
					//	recently is selected to be removed from the frame table. 
					if ( frameTable[potentialFrame].referenceBit == 0 ) {
						emptiedFrame = potentialFrame;
						searching = false; 
						manageClock ( shmClock, 450 );
						break;
					}										
				} while ( searching ); // End of do-while loop
				
				// Update the system structures now that a frame has been chosen. 
				int removedPageOwner; 
				for ( i = 0; i < maxCurrentProcesses; ++i ) {
					if ( processControlBlock[i].pid == frameTable[emptiedFrame].processID ) {
						removedPageOwner = i; 
						break;
					}
				}
				
				// Reset the page table index for the removed frame reference. 
				processControlBlock[removedPageOwner].pageTable[frameTable[emptiedFrame].processPageFrame] = -1;
				
				// Update frame with new page info. 
				frameTable[emptiedFrame].occupiedBit = 1; 
				frameTable[emptiedFrame].processID = message.pid;
				frameTable[emptiedFrame].processPageFrame = message.pageReferenced;
				frameTable[emptiedFrame].referenceBit = 1;
				frameTable[emptiedFrame].dirtyBit = 0;				

				// Update page table with new frame mapping.
				processControlBlock[currentIndex].pageTable[message.pageReferenced] = emptiedFrame; 
				
				// Put newly occupied frame back in the queue. 
				enqueue ( queue , emptiedFrame );
				
				// Update log. 
				if ( keepLogging ) {
					fprintf( fp, "OSS: Clearing Frame %d and swapping in Process %d Page %d.\n", 
						potentialFrame, message.pid, message.pageReferenced );
					numberOfLines++;
				}
				
				if ( message.request_type == WRITE ) {
					if ( keepLogging ) {
						fprintf( fp, "OSS: Process %d is writing to Page %d in Frame %d at %d:%d\n", 
							message.pid, message.pageReferenced, emptiedFrame, shmClock[0], shmClock[1]  );
						numberOfLines++;
					}
					frameTable[emptiedFrame].dirtyBit = 1; 
				}
				
				if ( message.request_type == READ ) {
					if ( frameTable[emptiedFrame].dirtyBit == 1 ) {
						if ( keepLogging ) {
							fprintf( fp, "OSS: Process %d is reading from Page %d (dirty) in Frame %d at %d:%d.\n", 
								message.pid, message.pageReferenced, emptiedFrame, shmClock[0], shmClock[1] );
							numberOfLines++;
						}
						manageClock ( shmClock , 50 );
					} else {
						if ( keepLogging ) {
							fprintf( fp, "OSS: Process %d is reading from Page %d in Frame %d at %d:%d.\n", 
								message.pid, message.pageReferenced, emptiedFrame, shmClock[0], shmClock[1] );
							numberOfLines++;
						}
					}
				}
			} // End of implemenation of second-chance algorithm
			
		} // End of 6.2 
		
		/* 7. Send a message to the child to inform it the memory request was granted. */
		message.msg_type = message.pid; 
		
		if ( msgsnd ( messageID, &message, sizeof ( message ), 0 ) == -1 ) {
			perror ( "OSS: Failure to send message to USER process." );
		}
		
	} // End of main loop
	
	wait ( NULL ); 
	
	cleanUpResources();
	
	return 0;
}

/*************************************************************************************************************/
/*************************************************************************************************************/
/******************************************* End of Main Function ********************************************/
/*************************************************************************************************************/
/*************************************************************************************************************/


// Function that increments the clock by some amount of time at different points to simulate processing time or 
// overhead time. Also makes sure that nanoseconds are converted to seconds if/when necessary.
void manageClock ( unsigned int clock[], int timeElapsed ) {
	clock[1] += timeElapsed;

	clock[0] += clock[1] / 1000000000;
	clock[1] = clock[1] % 1000000000;
}

// int totalProcessesCreated = 0;
// int totalMemoryRequests = 0;
// int totalPageFaults = 0;
// double numberOfMemoryAccessesPerSecond;
// double numberOfPageFaultsPerMemoryAccess;
// unsigned int totalSecondsProgramRan;

// Function to print the after-run report showing any relevant statistics. 
// void printReport() {
// 	totalSecondsProgramRan = shmClock[0]; 
// 	numberOfMemoryAccessesPerSecond = totalMemoryRequests / totalSecondsProgramRan;
// 	numberOfPageFaultsPerMemoryAccess = totalPageFaults / totalMemoryRequests;
	
// 	printf ( "Total processes created: %d.\n", totalProcessesCreated );
// 	printf ( "Total memory requests processed: %d.\n", totalMemoryRequests );
// 	printf ( "Total page faults: %d.\n", totalPageFaults );
// 	printf ( "Number of memory accesses per second: %f.\n", numberOfMemoryAccessesPerSecond );
// 	printf ( "Number of page faults per second: %f.\n", numberOfPageFaultsPerMemoryAccess );
// }

// Function to terminate all shared memory and message queue upon completion or to be used with signal handling.
void cleanUpResources() {
// 	printReport();
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
		printf ( "Total processes created: %d\n", totalProcessesCreated ); 
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
	queue->rear = capacity - 1;	// This is important, see the enqueue
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
