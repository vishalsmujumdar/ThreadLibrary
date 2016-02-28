#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ucontext.h>
#include "mythread.h"
#define debug 0
#define queues 0
#define THREAD_STACK_SIZE 8000

int THREAD_COUNTER = 0;
int SEMAPHORE_COUNTER = 0;
ucontext_t main_context;

typedef struct Thread Thread;

typedef struct Semaphore Semaphore;

typedef struct MyQueue {
	char name;
	Thread * front;
	Thread * rear;
} MyQueue ;

struct Thread {
	int id;
	int state;
	ucontext_t * context;

	struct Thread * next_r;
	struct Thread * next_b;
	struct Thread * next_s;
	struct Thread * next_sem;

	MyQueue * children;

	Thread * parent;

	bool joined_by_parent;
	bool join_all;
};

struct Semaphore {
	int id;
	int initialValue;
	MyQueue * waitingQ;
};

MyQueue * readyQ;
MyQueue * blockedQ;

Thread * running_thread;

MyQueue * NewQueue(char name){
	MyQueue * queue = malloc(sizeof(MyQueue));
	queue->name = name;
	queue->front = NULL;
	queue->rear = NULL;
	return queue;
}

bool isEmpty(MyQueue * queue) {
	if (queue->front == NULL && queue->rear == NULL) {
		return true;
	} else {
		return false;
	}
}

void Enqueue(Thread * thread, MyQueue * queue, int mode) {
	if(queue->front == NULL && queue->rear == NULL) {
		queue->front = queue->rear = thread;
		return;
	}
	if(mode==0)	queue->rear->next_r = thread;
	else if(mode==1)	queue->rear->next_b = thread;
	else if(mode==2)	queue->rear->next_s = thread;
	else if(mode==3)	queue->rear->next_sem = thread;

	queue->rear = thread;
}

void Dequeue(MyQueue * queue, int mode) {
	Thread * thread = queue->front;
	if (queue->front==NULL) return;
	if (queue->front==queue->rear) {
		queue->front=queue->rear=NULL;
	} else {
		if(mode==0)	queue->front = queue->front->next_r;
		else if(mode==1)	queue->front = queue->front->next_b;
		else if(mode==2)	queue->front = queue->front->next_s;
		else if(mode==3)	queue->front = queue->front->next_sem;
	}
	if(mode==0)	thread->next_r=NULL;
	else if(mode==1)	thread->next_b=NULL;
	else if(mode==2)	thread->next_s=NULL;
	else if(mode==3)	thread->next_sem=NULL;
}

Thread * GetFromQueue(Thread * thread, MyQueue * queue, int mode) {
	Thread * checking_thread = queue->front;
	if (isEmpty(queue)) return NULL;

	while(checking_thread != NULL){
		if(checking_thread==thread) return thread;
		if(mode==0)	checking_thread = checking_thread->next_r;
		else if(mode==1)	checking_thread = checking_thread->next_b;
		else if(mode==2)	checking_thread = checking_thread->next_s;
		else if(mode==3)	checking_thread = checking_thread->next_sem;
	}
	return NULL;
}

void DeleteFromQueue(Thread * thread, MyQueue * queue, int mode) {
	if (!isEmpty(queue)) {
		Thread * checking_thread = queue->front;
		Thread * previous_thread = queue->front;

		while (checking_thread != NULL && checking_thread != thread) {		// Find the thread node
			previous_thread = checking_thread;
			if(mode==0)	checking_thread = checking_thread->next_r;
			else if(mode==1)	checking_thread = checking_thread->next_b;
			else if(mode==2)	checking_thread = checking_thread->next_s;
			else if(mode==3)	checking_thread = checking_thread->next_sem;
		}
		if (checking_thread == queue->front && checking_thread == queue->rear) {
			queue->front = queue->rear = NULL;
		} else if (checking_thread == queue->front) {
			if(mode==0)	queue->front = checking_thread->next_r;
			else if(mode==1)	queue->front = checking_thread->next_b;
			else if(mode==2)	queue->front = checking_thread->next_s;
			else if(mode==3)	queue->front = checking_thread->next_sem;
		} else if (checking_thread == queue->rear) {
			queue->rear = previous_thread;
		} else {
			if(mode==0)	previous_thread->next_r = checking_thread->next_r;
			else if(mode==1)	previous_thread->next_b = checking_thread->next_b;
			else if(mode==2)	previous_thread->next_s = checking_thread->next_s;
			else if(mode==3)	previous_thread->next_sem = checking_thread->next_sem;
		}
		if(mode==0)	checking_thread->next_r = NULL;
		else if(mode==1)	checking_thread->next_b = NULL;
		else if(mode==2)	checking_thread->next_s = NULL;
		else if(mode==3)	checking_thread->next_sem = NULL;
	}
}

void PrintQueue(MyQueue * queue, int mode) {
	if (isEmpty(queue)) {
		printf("%c Queue is empty\n", queue->name);
		return;
	} else {
		printf("%c: ", queue->name);
		Thread * current_thread = queue->front;
		while(current_thread != NULL) {
			printf("%d -> ", current_thread->id);
			if(mode==0)	current_thread = current_thread->next_r;
			else if(mode==1)	current_thread = current_thread->next_b;
			else if(mode==2)	current_thread = current_thread->next_s;
			else if(mode==3)	current_thread = current_thread->next_sem;
		}
		printf("\n");
	}
}

void RelinquishThread(Thread * thread) {
	if (thread->parent != NULL) {
		MyQueue * family = thread->parent->children;
		DeleteFromQueue(thread, family, 2);
		thread->parent = NULL;
	}
	Thread * child = thread->children->front;
	while (child != NULL) {
		child->parent = NULL;
		if (child->next_s == NULL) {
			child = NULL;
		} else {
			child = child->next_s;
		}

	}
	thread->children = NULL;
	free(thread->context);
	free(thread);
}

void MyThreadInit(void(*start_funct)(void *), void *args){
	if (debug) printf("***** MyThreadInit *****\n");
	readyQ = NewQueue('r');
	blockedQ = NewQueue('b');
	ucontext_t * context = malloc(sizeof(ucontext_t));
	getcontext(context);
	context->uc_link=0;
	context->uc_stack.ss_sp=malloc(THREAD_STACK_SIZE);
	context->uc_stack.ss_size=THREAD_STACK_SIZE;
	context->uc_flags=0;

	makecontext(context, (void *)start_funct, 1, args);

	Thread * thread_0 = malloc(sizeof(Thread));
	thread_0->id = THREAD_COUNTER++;
	thread_0->state=1;
	thread_0->context=context;
	thread_0->next_r 	 = NULL;
	thread_0->next_b	 = NULL;
	thread_0->next_s 	 = NULL;
	thread_0->next_sem = NULL;
	thread_0->join_all = false;
	thread_0->joined_by_parent= false;
	thread_0->children = NewQueue('c');
	thread_0->parent   = NULL;

	running_thread = thread_0;

	swapcontext(&main_context, running_thread->context);
}

MyThread MyThreadCreate(void(*start_funct)(void *), void *args){
	if (debug) printf("***** MyThreadCreate *****\n");
	if (debug) printf("***** Running   %i   *****\n", running_thread->id);
	ucontext_t * new_context = malloc(sizeof(ucontext_t));
	getcontext(new_context);
	new_context->uc_link=0;
	new_context->uc_stack.ss_sp=malloc(THREAD_STACK_SIZE);
	new_context->uc_stack.ss_size=THREAD_STACK_SIZE;
	new_context->uc_flags=0;

	makecontext(new_context, (void *)start_funct, 1, args);

	Thread * thread = malloc(sizeof(Thread));
	thread->id 		 = THREAD_COUNTER++;
	thread->state	 = 0;
	thread->context  = new_context;
	thread->next_r 	 = NULL;
	thread->next_b	 = NULL;
	thread->next_s 	 = NULL;
	thread->next_sem = NULL;
	thread->join_all = false;
	thread->joined_by_parent= false;
	thread->children = NewQueue('c');
	thread->parent   = running_thread;

	Enqueue(thread, readyQ, 0);
	Enqueue(thread, running_thread->children, 2);

	if (debug) printf("***** Created   %i   *****\n", thread->id);
	if (queues) PrintQueue(readyQ, 0);
	if (queues) PrintQueue(blockedQ, 1);

	return (void*)thread;
}

void MyThreadYield(void) {
	if (debug) printf("***** MyThreadYield *****\n");
	if (debug) printf("***** Running   %i *****\n", running_thread->id);
	if (!isEmpty(readyQ)) {
		running_thread->state = 0;
		Enqueue(running_thread, readyQ, 0);

		Thread * yielded_thread = running_thread;

		running_thread = readyQ->front;
		running_thread->state=1;
		Dequeue(readyQ, 0);

		if (debug) printf("***** Yielded   %i   *****\n", yielded_thread->id);
		if (debug) printf("***** Running   %i  *****\n", running_thread->id);

		if (queues) PrintQueue(readyQ, 0);
		if (queues) PrintQueue(blockedQ, 1);
		swapcontext(yielded_thread->context, running_thread->context);
	}
}

int MyThreadJoin(MyThread thread) {
	if (debug) printf("***** MyThreadJoin *****\n");
	if (debug) printf("***** Running   %i *****\n", running_thread->id);

	Thread * child_thread = (Thread *)thread;

	if (child_thread->parent == running_thread) {
		running_thread->state = 2;
		Enqueue(running_thread, blockedQ, 1);

		child_thread->joined_by_parent = true;

		Thread * blocked_thread = running_thread;

		running_thread = readyQ->front;
		running_thread->state = 1;
		Dequeue(readyQ, 0);

		if (debug) printf("***** Blocked   %i   *****\n", blocked_thread->id);
		if (debug) printf("***** Running   %i   *****\n", running_thread->id);

		if (queues) PrintQueue(readyQ, 0);
		if (queues) PrintQueue(blockedQ, 1);

		swapcontext(blocked_thread->context, running_thread->context);
		return 0;
	} else {
		return -1;
	}
}

void MyThreadJoinAll(void) {
	if (debug) printf("***** MyThreadJoinAll *****\n");
	if (debug) printf("***** Running   %i *****\n", running_thread->id);
	MyQueue * children = running_thread->children;
	if(!isEmpty(children)) {
		running_thread->state = 2;
		running_thread->join_all = true;
		Enqueue(running_thread, blockedQ, 1);

		Thread * blocked_thread = running_thread;

		running_thread = readyQ->front;
		Dequeue(readyQ, 0);
		running_thread->state = 1;

		if (debug) printf("***** Blocked   %i   *****\n", blocked_thread->id);
		if (debug) printf("***** Running   %i *****\n", running_thread->id);

		if (queues) PrintQueue(readyQ, 0);
		if (queues) PrintQueue(blockedQ, 1);
		swapcontext(blocked_thread->context, running_thread->context);
	}
}


void MyThreadExit(void) {
	if (debug) printf("***** MyThreadExit *****\n");
	if (debug) printf("***** Running   %i *****\n", running_thread->id);

	Thread * exiting_thread = running_thread;

	exiting_thread->state = 3;

	if (exiting_thread->parent != NULL && exiting_thread->parent->join_all) {
		if (exiting_thread->parent->children->front == exiting_thread && exiting_thread->parent->children->rear == exiting_thread) {
			Thread * unblocked_node = exiting_thread->parent;
			unblocked_node->state = 0;
			DeleteFromQueue(unblocked_node, blockedQ, 1);
			Enqueue(unblocked_node, readyQ, 0);
		}
	} else if (exiting_thread->joined_by_parent) {
		Thread * unblocked_node = exiting_thread->parent;
		unblocked_node->state = 0;
		DeleteFromQueue(unblocked_node, blockedQ, 1);
		Enqueue(unblocked_node, readyQ, 0);
	}
	if (queues) PrintQueue(readyQ, 0);
	if (queues) PrintQueue(blockedQ, 1);
	if (debug) printf("***** Exited   %i   *****\n", exiting_thread->id);

	RelinquishThread(exiting_thread);

	if(!isEmpty(readyQ)) {
		Thread * next_thread = readyQ->front;
		next_thread->state = 1;
		running_thread = next_thread;
		Dequeue(readyQ, 0);
		if (debug) printf("***** Running   %i *****\n", running_thread->id);

		setcontext(running_thread->context);
	} else {
		setcontext(&main_context);
	}
}

MySemaphore MySemaphoreInit(int initialValue) {
	Semaphore * sem = malloc(sizeof(Semaphore));
	sem->id = SEMAPHORE_COUNTER++;
	sem->initialValue = initialValue;
	sem->waitingQ = NewQueue('w');

	return sem;
}
void MySemaphoreWait(MySemaphore sem) {
	if (debug) printf("***** MySemaphoreWait *****\n");
	if (debug) printf("***** Running   %i *****\n", running_thread->id);
	Semaphore * semaphore = (Semaphore *)sem;
	if (semaphore->initialValue > 0) {
		semaphore->initialValue--;
		if (debug) printf("***** Continuing   %i *****\n", running_thread->id);
	} else {
		semaphore->initialValue--;
		running_thread->state = 2;
		Enqueue(running_thread, semaphore->waitingQ, 3);
		Thread * blocked_thread = running_thread;

		running_thread = readyQ->front;
		Dequeue(readyQ, 0);
		if (debug) printf("***** Blocked on Sem   %i *****\n", blocked_thread->id);
		if (debug) printf("***** Running   %i *****\n", running_thread->id);

		if (queues) PrintQueue(readyQ, 0);
		if (queues) PrintQueue(blockedQ, 1);
		if (queues) PrintQueue(semaphore->waitingQ, 3);

		swapcontext(blocked_thread->context, running_thread->context);
	}
}

void MySemaphoreSignal(MySemaphore sem) {
	if (debug) printf("***** MySemaphoreSignal *****\n");
	if (debug) printf("***** Running   %i *****\n", running_thread->id);

	Semaphore * semaphore = (Semaphore *)sem;
	if (!isEmpty(semaphore->waitingQ)) {
		Thread * unblocked_thread = semaphore->waitingQ->front;
		Dequeue(semaphore->waitingQ, 3);
		unblocked_thread->state = 0;
		Enqueue(unblocked_thread, readyQ, 0);
		if (debug) printf("***** Unblocked on Sem   %i *****\n", unblocked_thread->id);
		if (queues) PrintQueue(readyQ, 0);
		if (queues) PrintQueue(blockedQ, 1);
		if (queues) PrintQueue(semaphore->waitingQ, 3);
		semaphore->initialValue++;
	} else {
		semaphore->initialValue++;
	}
}

int MySemaphoreDestroy(MySemaphore sem) {
	if (debug) printf("***** MySemaphoreDestroy *****\n");
	if (debug) printf("***** Running   %i *****\n", running_thread->id);

	Semaphore * semaphore = (Semaphore *)sem;
	if (isEmpty(semaphore->waitingQ)) {
		semaphore->waitingQ = NULL;
		free(semaphore);
		return 0;
	} else {
		return -1;
	}
}
