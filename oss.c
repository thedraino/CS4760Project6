
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
	int occupiedBit;	// Bit to flag if the frame is currently occupied by a process.  
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
void clearPCBEntry ( int processID );
void clearFrameEntries ( int processID);
void printReport();
void cleanUpResources();

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
int numberOfLines; 


/*************************************************************************************************************/
/*************************************************************************************************************/
/******************************************* Start of Main Function ******************************************/
/*************************************************************************************************************/
/*************************************************************************************************************/

int main ( int argc, char *argv[] ) {
	
	/* General Use Variables */
	int i, j;			// Control variables for loop logic. 
	int maxCurrentProcesses;	// Controls the maximum number of processes that can be running at one time.
	int maxTotalProcesses = 100;	// Controls the maximum number of processes that can be created throughout the program. 
	long ossID = getpid();		// Store the pid for OSS.
	unsigned int newProcessTime[2] = { 0, 0 };	// Time value at which a new process should be be created.
	fp = fopen ( logName, "w+" );	// Open up the log file for writing to. 
	numberOfLines = 0;
	srand ( time ( NULL ) );	// Seed for random number generation.
	
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

			default:
				maxCurrentProcesses = MAX_PROCESSES;
				break;
		}
	} // End of getopts
	
// 	printf ( "Max Processes: %d.\n", maxCurrentProcesses ); 
	
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
	
	/* Message Queue */
	// Create the message queue used for IPC.
	if ( ( messageID = msgget ( messageKey, IPC_CREAT | 0666 ) ) == -1 ) {
		perror ( "OSS: Failure to create the message queue." );
		
		return 1;
	}
	
	
	
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
	int processingTime = timeElapsed;
	clock[1] += processingTime;

	clock[0] += clock[1] / 1000000000;
	clock[1] = clock[1] % 1000000000;
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
