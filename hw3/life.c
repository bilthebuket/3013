#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include "mailbox.h"

#define MAXGRID 40

#define RANGE 1
#define ALLDONE 2
#define GO 3
#define GENDONE 4
#define ALLDEAD 5
#define SAMEASLAST 6

bool even_grid[MAXGRID][MAXGRID];
bool odd_grid[MAXGRID][MAXGRID];

int num_rows;
int num_columns;

void* worker_func(void* id);

void print_gen(int gen_number, bool last);

int main(int argc, char* argv[])
{
	if (argc != 4 && argc != 5)
	{
		printf("Incorrect usage. Proper usage: ./life <num_threads> <filename> <num_generations> <OPTIONAL:print(y/n)>\n");
		return 1;
	}

	int num_threads;
	int num_generations;

	num_threads = atoi(argv[1]);
	num_generations = atoi(argv[3]);

	if (num_threads <= 0 || num_generations <= 0)
	{
		printf("Number of threads and number of generations must be a positive integer.\n");
		return 1;
	}

	bool print = false;

	if (argc == 5)
	{
		if (argv[4][0] == 'y')
		{
			print = true;
		}
	}

	FILE* f = fopen(argv[2], "r");

	if (f == NULL)
	{
		printf("Could not open file.\n");
		return 1;
	}

	char buf[MAXGRID * 2 + 1];

	int rows = 0;
	int cols;
	while (fgets(buf, MAXGRID * 2 + 1, f))
	{
		cols = 0;

		if (rows >= 40)
		{
			printf("Number of rows must not exceed 40.\n");
			fclose(f);
			return 1;
		}

		int i;
		for (i = 0; buf[i] != '\0'; i++)
		{
			if (buf[i] == '1' || buf[i] == '0')
			{
				even_grid[rows][cols] = (buf[i] == '1');
				cols++;
			}
		}

		if (i == MAXGRID * 2 && buf[MAXGRID * 2 - 1] != '\n')
		{
			printf("Number of columns must not exceed 40.\n");
			fclose(f);
			return 1;
		}

		rows++;
	}

	num_rows = rows;
	num_columns = cols;

	pthread_t threads[num_threads];
	msg msg;
	msg.iSender = 0;
	msg.type = RANGE;

	int where_are_we = 0;
	int remainder = rows % num_threads;
	int num_rows_for_each = (rows - remainder) / num_threads;

	init_boxes(num_threads + 1);

	for (int i = 0; i < num_threads; i++)
	{
		int* id = malloc(sizeof(int));
		*id = i + 1;

		if (pthread_create(*threads[i], NULL, worker_func, id) != 0)
		{
			printf("Error creating thread.\n");
			return 1;
		}

		msg.iSender = 0;
		msg.value1 = where_are_we;
		
		where_are_we += num_rows_for_each;

		if (i < remainder)
		{
			where_are_we++;
		}

		msg.value2 = where_are_we;

		SendMsg(*id, &msg);
	}

	for (int i = 0; i < num_generations; i++)
	{
		int num_same_as_last = 0;
		int num_all_dead = 0;
		int num_joined = 0;
		for (int j = 0; j < num_threads; j++)
		{
			RecvMsg(0, &msg);

			if (msg.type == ALLDONE)
			{
				pthread_join(threads[msg.iSender - 1]);
				num_joined++;
			}
			else if (msg.type == SAMEASLAST)
			{
				num_same_as_last++;
			}
			else if (msg.type == ALLDEAD)
			{
				num_all_dead++;
			}
		}

		msg.iSender = 0;

		if (num_same_as_last == num_threads || num_all_dead == num_threads || num_joined > 0)
		{
			msg.type = ALLDONE;
			print_gen(i, true);
		}
		else
		{
			msg.type = GO;
			print_gen(i, false);
		}

		if (num_joined > 0)
		{
			if (num_joined != num_threads)
			{
				printf("Error: some threads are done but others arent.\n");
				free_boxes(num_threads + 1);
				return 1;
			}
		}
		else
		{
			for (int j = 0; j < num_threads; j++)
			{
				SendMsg(j + 1, &msg);

				if (msg.type == ALLDONE)
				{
					pthread_join(threads[j]);
				}
			}

			if (msg.type == ALLDONE)
			{
				break;
			}
		}
	}

	free_boxes(num_threads + 1);
}

void* worker_func(void* id)
{
	msg msg;
	RecvMsg(*(int*) id, &msg);

	int start_row;
	int end_row;

	bool[][] prev = even_grid;
	bool[][] next = odd_grid;

	start_row = msg.value1;
	end_row = msg.value2;

	do
	{
		bool same_as_last = true;
		bool add_dead = true;
		for (int i = start_row; i < end_row; i++)
		{
			for (int j = 0; j < num_columns; j++)
			{
				int num_adj = 0;

				int y = i - 1;
				int x = j - 1;

				int n = i + 1;
				int m = j + 1;

				if (i == 0)
				{
					y = 0;
				}
				else if (i == num_rows - 1)
				{
					n = i;
				}

				if (j == 0)
				{
					x = 0;
				}
				else if (j == num_columns - 1)
				{
					m = j;
				}

				for (; y < n; y++)
				{
					for (; x < m; x++)
					{
						if (prev[y][x])
						{
							num_adj++;
						}
					}
				}

				if (prev[i][j])
				{
					if (num_adj == 2 || num_adj == 3)
					{
						next[i][j] = true;
					}
					else
					{
						next[i][j] = false;
					}
				}
				else
				{
					if (num_adj == 3)
					{
						next[i][j] = true;
					}
					else
					{
						next[i][j] = false;
					}
				}

				if (next[i][j])
				{
					all_dead = false;
				}
				if (prev[i][j] != next[i][j])
				{
					same_as_last = false;
				}
			}
		}

		msg.iSender = *(int*) id;
		msg.type = GENDONE;
		
		if (same_as_last)
		{
			msg.type = SAMEASLAST;
		}
		if (all_dead)
		{
			msg.type = ALLDEAD;
		}
		
		SendMsg(0, &msg);
		RecvMsg(*(int*) id, &msg);
	}
	while (msg.type == GO);

	msg.iSender = *(int*) id;
	msg.type = ALLDONE;

	SendMsg(0, &msg);
}

void print_gen(int gen_number, bool last)
{
	if (last)
	{
		printf("The game ends after %d generations with:\n", gen_number);
	}
	else
	{
		printf("Generation %d:\n", gen_number);
	}
	for (int i = 0; i < num_rows; i++)
	{
		for (int j = 0; j < num_columns; j++)
		{
			if (gen_number % 2 == 0)
			{
				if (even_grid[i][j])
				{
					printf("1 ");
				}
				else
				{
					printf("0 ");
				}
			}
			else
			{
				if (odd_grid[i][j])
				{
					printf("1 ");
				}
				else
				{
					printf("0 ");
				}
			}
		}

		printf("\n");
	}

	printf("\n");
}
