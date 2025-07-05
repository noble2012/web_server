#pragma once
#include<stdio.h>
#include<pthread.h>
typedef struct task {
	void(*function)(void* arg);
	void* arg;
}Task;
typedef struct ThreadPool
{
	//任务队列
	Task* taskQ;
	int queueCapacity;//容量
	int queuesize;//当前任务个数
	int queuefront;//队头取数据
	int queueRear;//队尾放数据
	pthread_t managerID;//管理者线程
	pthread_t* threadIDs;//工作的线程ID
	int minNum;
	int maxNum;
	int busyNum;//工作的线程个数
	int threadNum;//存在的线程
	int destroyNum;//销毁的线程个数
	pthread_mutex_t mutexpool;//锁整个线程池
	pthread_mutex_t mutexbusy;//锁busyNum变量

	bool shutdown;//是不是要销毁线程池，销毁为1，不销毁为0

	pthread_cond_t condfull;//判断是否满了
	pthread_cond_t condempty;//判断是否空了
}threadpool;

#ifndef _THREADPOOL_H
#define _THREADPOOL_H
//线程池初始化函数
threadpool* ThreadPool_Init(int max, int min, int qusize);
//销毁线程池
int threadpoolDestroy(threadpool* pool);
//添加任务
void threadpool_add(threadpool* pool, void (*function)(void*), void* arg);

//获取工作的线程个数
int threadpoolBUsyNum(threadpool* pool);
//获取创建的线程的个数
int threadpoolThreadNum(threadpool* pool);

void* threadwork(void* arg);
void* manager(void* arg);
void threadExit(ThreadPool* pool);
#endif // !_THREADPOOL_H
