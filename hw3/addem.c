#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "mailbox.h"

#define RANGE 1
#define ALLDONE 2
#define ERROR 3

void* worker_func(void* id);

int main(int argc, char* argv[])
{
	if (argc != 3)
	{
		printf("Incorrect argmuents. Usage: /addem <num_threads> <value>\n");
		return 1;
	}

	int num_threads = atoi(argv[1]);
	int val = atoi(argv[2]);

	if (num_threads <= 0 || val <= 0)
	{
		printf("Invalid argmuents passed. Both arguments should be positive integers\n");
		return 1;
	}
	
	pthread_t threads[num_threads];

	int leftover = val % num_threads;
	int amount_for_each = (val - leftover) / num_threads;
	int where_are_we = 1;

	init_boxes(num_threads + 1);

	msg msg;
	msg.iSender = 0;
	msg.type = RANGE;

	for (int i = 0; i < num_threads; i++)
	{
		int* id = malloc(sizeof(int));
		*id = i + 1;
		if (pthread_create(&threads[i], NULL, worker_func, id) != 0)
		{
			printf("Error creating thread.\n");
			return 1;
		}

		msg.value1 = where_are_we;
		
		where_are_we += amount_for_each;
		if (i < leftover)
		{
			where_are_we++;
		}

		msg.value2 = where_are_we;

		SendMsg(*id, &msg);
	}

	int total = 0;

	for (int i = 0; i < num_threads; i++)
	{
		RecvMsg(0, &msg);
		if (msg.type == ALLDONE)
		{
			total += msg.value1;
			pthread_join(threads[msg.iSender - 1], NULL);
		}
	}

	printf("The total for 1 to %d using %d threads is %d.\n", val, num_threads, total);

	free_boxes(num_threads + 1);
}

void* worker_func(void* id)
{
	msg msg;
	RecvMsg(*(int*) id, &msg);

	if (msg.type == RANGE && msg.iSender == 0)
	{
		int total = 0;
		for (int i = msg.value1; i < msg.value2; i++)
		{
			total += i;
		}

		msg.iSender = *(int*) id;
		msg.type = ALLDONE;
		msg.value1 = total;

		SendMsg(0, &msg);
	}
	else
	{
		msg.iSender = *(int*) id;
		msg.type = ERROR;
		msg.value1 = 1;

		SendMsg(0, &msg);
	}

	free(id);
}
