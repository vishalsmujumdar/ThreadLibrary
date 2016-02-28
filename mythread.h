#ifndef MYTHREAD_H
#define MYTHREAD_H

typedef void *MyThread;
typedef void *MySemaphore;

MyThread MyThreadCreate(void(*start_funct)(void *), void *args);

void MyThreadYield(void);

int MyThreadJoin(MyThread thread);

void MyThreadJoinAll(void);

void MyThreadExit(void);

MySemaphore MySemaphoreInit(int initialValue);

void MySemaphoreSignal(MySemaphore sem);

void MySemaphoreWait(MySemaphore sem);

int MySemaphoreDestroy(MySemaphore sem);

void MyThreadInit(void(*start_funct)(void *), void *args);

#endif
