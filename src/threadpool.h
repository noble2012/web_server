#pragma once
#include<stdio.h>
#include<pthread.h>
typedef struct task {
	void(*function)(void* arg);
	void* arg;
}Task;
typedef struct ThreadPool
{
	//�������
	Task* taskQ;
	int queueCapacity;//����
	int queuesize;//��ǰ�������
	int queuefront;//��ͷȡ����
	int queueRear;//��β������
	pthread_t managerID;//�������߳�
	pthread_t* threadIDs;//�������߳�ID
	int minNum;
	int maxNum;
	int busyNum;//�������̸߳���
	int threadNum;//���ڵ��߳�
	int destroyNum;//���ٵ��̸߳���
	pthread_mutex_t mutexpool;//�������̳߳�
	pthread_mutex_t mutexbusy;//��busyNum����

	bool shutdown;//�ǲ���Ҫ�����̳߳أ�����Ϊ1��������Ϊ0

	pthread_cond_t condfull;//�ж��Ƿ�����
	pthread_cond_t condempty;//�ж��Ƿ����
}threadpool;

#ifndef _THREADPOOL_H
#define _THREADPOOL_H
//�̳߳س�ʼ������
threadpool* ThreadPool_Init(int max, int min, int qusize);
//�����̳߳�
int threadpoolDestroy(threadpool* pool);
//�������
void threadpool_add(threadpool* pool, void (*function)(void*), void* arg);

//��ȡ�������̸߳���
int threadpoolBUsyNum(threadpool* pool);
//��ȡ�������̵߳ĸ���
int threadpoolThreadNum(threadpool* pool);

void* threadwork(void* arg);
void* manager(void* arg);
void threadExit(ThreadPool* pool);
#endif // !_THREADPOOL_H
