#include <stdib.h>
#include <pthread.h>
#include <semaphore.h>
#include "mailbox.h"

#define READY_FOR_WRITE 0
#define READ_FOR_READ 1

msg** boxes;

sem_t** sems;

void init_boxes(int num_boxes)
{
	boxes = malloc(sizeof(msg*) * num_boxes);
	sems = malloc(sizeof(sem_t*) * num_boxes);
	for (int i = 0; i < num_boxes; i++)
	{
		boxes[i] = malloc(sizeof(msg));
		sems[i] = malloc(sizeof(sem_t) * 2);
		if (sem_init(&sems[i][READY_FOR_WRITE], 0, 1) != 0)
		{
			printf("Could not init semaphore\n");
		}
		if (sem_init(&sems[i][READY_FOR_READ], 0, 0) != 0)
		{
			printf("Could not init semaphore\n");
		}
	}
}

void SendMsg(int iTo, msg* pMsg)
{
	sem_wait(&sems[iTo][READY_FOR_WRITE]);
	boxes[iTo] = pMsg;
	sem_post(&sems[iTo][READY_FOR_READ]);
}

void RecvMsg(int iFrom, msg* pMsg)
{
	sem_wait(&sems[iFrom][READY_FOR_READ]);
	pMsg->iSender = boxes[iFrom]->iSender;
	pMsg->type = boxes[iFrom]->type;
	pMsg->value1 = boxes[iFrom]->value1;
	pMsg->value2 = boxes[iFrom]->value2;
	sem_post(&sems[iFrom][READY_FOR_READ]);
}
