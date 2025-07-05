#include"threadpool.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
const int thread_num = 1;
void* threadwork(void* arg)
{
	threadpool* pool = (threadpool*)arg;

	while (1)
	{
		pthread_mutex_lock(&pool->mutexpool);
		while (pool->queuesize == 0 && !pool->shutdown)
		{
			//阻塞工作线程
			pthread_cond_wait(&pool->condempty, &pool->mutexpool);
			if (pool->destroyNum > 0)
			{
				pool->destroyNum--;
				if (pool->threadNum > pool->minNum)
				{
					pool->threadNum--;
					pthread_mutex_unlock(&pool->mutexpool);
					threadExit(pool);
				}
			}
		}
		//判断是否线程池关闭
		if (pool->shutdown)
		{
			pthread_mutex_unlock(&pool->mutexpool);
			threadExit(pool);
			pool->threadNum--;
		}
		Task task;
		task.function = pool->taskQ[pool->queuefront].function;
		task.arg = pool->taskQ[pool->queuefront].arg;
		//移动queuefront
		pool->queuefront=(pool->queuefront + 1) % pool->queueCapacity;
		pool->queuesize--;

		pthread_cond_signal(&pool->condfull);
		pthread_mutex_unlock(&pool->mutexpool);

		pthread_mutex_lock(&pool->mutexbusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexbusy);
		task.function(task.arg);
		free(task.arg);
		task.arg = NULL;
		pthread_mutex_lock(&pool->mutexbusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexbusy);
	}
	return NULL;
}
void* manager(void *arg) {

	threadpool* pool = (threadpool*)arg;
	while (!pool->shutdown)
	{
		//每5s检测一次
		sleep(1);
		pthread_mutex_lock(&pool->mutexpool);
		int quequesize = pool->queuesize;
		int threadnum = pool->threadNum;
		pthread_mutex_unlock(&pool->mutexpool);

		pthread_mutex_lock(&pool->mutexbusy);
		int busynum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexbusy);
		//添加线程，当已经存在的线程数<任务数&&小于最大线程数
		if (quequesize > threadnum-busynum && threadnum < pool->maxNum)
		{
			pthread_mutex_lock(&pool->mutexpool);
			int counter = 0;
			for (int i = 0; i < pool->maxNum && counter < thread_num && pool->threadNum < pool->maxNum; ++i)
			{
				if (pool->threadIDs[i] == 0)
				{
					pthread_create(&pool->threadIDs[i], NULL, threadwork, pool);
					counter++;
					pool->threadNum++;
				}
			}
			pthread_mutex_unlock(&pool->mutexpool);
		}
		//销毁线程
		//忙的线程*2<存活的线程数&&存活的线程>最小线程
		if (busynum * 2 < threadnum && threadnum > pool->minNum)
		{
			pthread_mutex_lock(&pool->mutexpool);
			pool->destroyNum = thread_num;
			pthread_mutex_unlock(&pool->mutexpool);
			for (int i = 0; i < thread_num; i++)
			{
				pthread_cond_signal(&pool->condempty);
			}
		}
	}
	return NULL;
}
threadpool* ThreadPool_Init(int max, int min, int qusize) 
{
		threadpool* pool = (threadpool*)malloc(sizeof(threadpool));
	do{	if (pool == NULL)
		{
			printf("threadpool has failed to build\n");
			break;
		}

		pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);
		if (pool->threadIDs == NULL)
		{
			printf("threadpool's id has failed to build\n");
			break;
		}
		memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
		pool->maxNum = max;
		pool->minNum = min;
		pool->queuesize = 0;
		pool->busyNum = 0;
		pool->threadNum = min;//刚创建，等于最小线程数
		pool->destroyNum = 0;

		if (pthread_mutex_init(&pool->mutexpool, NULL) != 0 || pthread_mutex_init(&pool->mutexbusy, NULL) != 0
			|| pthread_cond_init(&pool->condempty, NULL) != 0 || pthread_cond_init(&pool->condfull, NULL) != 0)
		{
			printf("Init has been failed\n");
			break;
		}

		pool->taskQ = (Task*)malloc(sizeof(Task) * qusize);
		pool->queueCapacity = qusize;
		pool->queuefront = 0;
		pool->queueRear = 0;
		pool->shutdown = 0;

		//创建管理者线程
		pthread_create(&pool->managerID, NULL, manager, pool);
		//创建工作的线程
		for (int i = 0; i < min; ++i)
		{
			pthread_create(&pool->threadIDs[i], NULL, threadwork, pool);
		}
		return pool;
	} while (0);
	//释放资源
	if(pool&&pool->threadIDs)
	free(pool->threadIDs);
	if(pool&&pool->taskQ)
	free(pool->taskQ);
	if (pool)
	free(pool);
	return NULL;
}
void threadExit(ThreadPool* pool)
{
	pthread_t tid = pthread_self();
	for (int i = 0; i < pool->maxNum; ++i)
	{
		if (pool->threadIDs[i] == tid)
		{
			pool->threadIDs[i] = 0;
			break;
		}
	}
	printf("%d\n", pool->threadNum);
	printf("thread %ld exiting\n", pthread_self());
	pthread_exit(NULL);
}
void threadpool_add(threadpool* pool, void(*func)(void*), void* arg)
{
	pthread_mutex_lock(&pool->mutexpool);
	while (pool->queuesize == pool->queueCapacity&&!pool->shutdown)
	{
		//阻塞生产者线程
		pthread_cond_wait(&pool->condfull, &pool->mutexpool);
	}
	if (pool->shutdown)
	{
		pthread_mutex_unlock(&pool->mutexpool);
		return;
	}
	//添加任务
	pool->taskQ[pool->queueRear].function = func;
	pool->taskQ[pool->queueRear].arg = arg;
	pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
	pool->queuesize++;

	pthread_cond_signal(&pool->condempty);
	pthread_mutex_unlock(&pool->mutexpool);
}
int threadpoolBUsyNum(threadpool* pool)
{
	pthread_mutex_lock(&pool->mutexbusy);
	int numbusy = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexbusy);
	return numbusy;
}
int threadpoolThreadNum(threadpool* pool)
{
	pthread_mutex_lock(&pool->mutexpool);
	int numthread = pool->threadNum;
	pthread_mutex_unlock(&pool->mutexpool);
	return numthread;
}
int threadpoolDestroy(threadpool* pool)
{
	if (pool == NULL)
		return -1;
	//关闭线程
	pool->shutdown = 1;
	//回收管理者线程
	pthread_join(pool->managerID, NULL);
	for (int i = 0; i < pool->threadNum; ++i)
	{
		pthread_cond_signal(&pool->condempty);
	}
	//释放pool里面申请的堆内存
	if (pool->taskQ)
	{
		free(pool->taskQ);
	}
	if (pool->threadIDs)
	{
		free(pool->threadIDs);
	}
	pthread_mutex_destroy(&pool->mutexpool);
	pthread_mutex_destroy(&pool->mutexbusy);
	pthread_cond_destroy(&pool->condempty);
	pthread_cond_destroy(&pool->condfull);
	free(pool);
	pool = NULL;

	return 0;
}