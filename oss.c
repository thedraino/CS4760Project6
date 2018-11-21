
// File name: oss.c
// Executable: oss
// Created by: Andrew Audrain
// Created on: 11/21/2018
//
// Purpose...

#include "project6.h"

/***** Structures *****/
// A structure to represent a queue
typedef struct {
	int front, rear, size;
	unsigned capacity;
	int *array;  
} Queue;


/***** Function Prototypes *****/
Queue* createQueue ( unsigned capacity );
int isFull ( Queue* queue ); 
int isEmpty ( Queue* queue );
void enqueue ( Queue* queue, int item );
int dequeue ( Queue* queue );
int front ( Queue* queue );
int rear ( Queue* queue );
void incrementClock ( unsigned int shmClock[] );
void printReport();
void cleanUpResources();


/***** Global Variables *****/


/*************************************************************************************************************/
/*************************************************************************************************************/
/******************************************* Start of Main Function ******************************************/
/*************************************************************************************************************/
/*************************************************************************************************************/

int main ( int argc, char *argv[] ) {
	printf ( "Hello, from OSS.\n" );
	
	return 0;
}

/*************************************************************************************************************/
/*************************************************************************************************************/
/******************************************* End of Main Function ********************************************/
/*************************************************************************************************************/
/*************************************************************************************************************/


// Function that increments the clock by some amount of time at different points to simulate processing time or 
// overhead time. Also makes sure that nanoseconds are converted to seconds if necessary.
void incrementClock ( unsigned int shmClock[] ) {
	int processingTime = 10000; // Can be changed to adjust how much the clock is incremented.
	shmClock[1] += processingTime;

	shmClock[0] += shmClock[1] / 1000000000;
	shmClock[1] = shmClock[1] % 1000000000;
}

/* Note:
   All below code regarding queue implentation was gotten from: 
   https://www.geeksforgeeks.org/queue-set-1introduction-and-array-implementation/
*/
// Function to create a queue of given capacity. It initializes size of queue as 0.
Queue* createQueue ( unsigned capacity ) {
	Queue* queue = (Queue*) malloc ( sizeof ( Queue ) );
	queue->capacity = capacity;
	queue->front = queue->size = 0;
	queue->rear = capacity - 1;	// This is important, see the enqueue.
	queue->array = (int*) malloc ( queue->capacity * sizeof ( int ) );

	return queue;
}

// Queue is full when size becomes equal to the capacity.
int isFull ( Queue* queue ) {
	return ( queue->size == queue->capacity );
}

// Queue is empty when size is 0.
int isEmpty ( Queue* queue ) { 
	return ( queue->size == 0 );
}

// Function to add an item to the queue. It changes rear and size.
void enqueue ( Queue* queue, int item ) {
	if ( isFull ( queue ) ) 
		return;
	
	queue->rear = ( queue->rear + 1 ) % queue->capacity;
	queue->array[queue->rear] = item;
	queue->size = queue->size + 1;
}

// Function to remove an item from queue. It changes front and size.
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
