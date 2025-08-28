#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>

#define MAX_NUM_JOBS 30
#define MAX_COMMAND_LEN 128
#define MAX_NUM_FINISHED_JOBS 16

typedef struct Job
{
	int pid;
	char* name;
	struct rusage* usage;
	int time;
} Job;

Job** jobs;
int* num_jobs;

int* finished_pids;

int execute_command(char** args, bool background);

char* get_input(char* prompt);

char** get_args(char* cmd);

void check_finished_processes(void);

void print_stats(struct rusage* usage, int time);

int main(int argc, char* argv[])
{
	if (argc == 1)
	{
		char* prompt = malloc(sizeof(char) * 4);
		prompt[0] = '=';
		prompt[1] = '=';
		prompt[2] = '>';
		prompt[3] = '\0';

		jobs = mmap(NULL, sizeof(Job*) * MAX_NUM_JOBS, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		for (int i = 0; i < MAX_NUM_JOBS; i++)
		{
			jobs[i] = mmap(NULL, sizeof(Job), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
			jobs[i]->name = mmap(NULL, sizeof(char) * MAX_COMMAND_LEN + 1, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
			jobs[i]->usage = mmap(NULL, sizeof(struct rusage), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		}

		num_jobs = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		*num_jobs = 0;

		finished_pids = mmap(NULL, sizeof(int) * MAX_NUM_FINISHED_JOBS, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		finished_pids[0] = -1;

		while (1)
		{
			char* input = get_input(prompt);
			char** args = get_args(input);
			free(input);

			check_finished_processes();

			if (!strcmp(args[0], "exit"))
			{
				for (int i = 0; args[i] != NULL; i++)
				{
					free(args[i]);
				}

				free(args);			

				while (*num_jobs > 0)
				{
					check_finished_processes();
					usleep(500000);
				}

				break;
			}
			else if (!strcmp(args[0], "cd"))
			{
				chdir(args[1]);
			}
			else if (!strcmp(args[0], "set") && !strcmp(args[1], "prompt") && !strcmp(args[2], "="))
			{
				free(prompt);
				int len = 0;
				for (int i = 0; args[3][i] != '\0'; i++)
				{
					len++;
				}
				len++;

				prompt = malloc(sizeof(char) * len);
				
				for (int i = 0; args[3][i] != '\0'; i++)
				{
					prompt[i] = args[3][i];
				}

				prompt[len - 1] = '\0';
			}
			else if (!strcmp(args[0], "jobs"))
			{
				for (int i = 0; i < *num_jobs; i++)
				{
					printf("[%d] %d %s\n", i + 1, jobs[i]->pid, jobs[i]->name);
				}
			}
			else
			{
				int last_arg_index;
				for (last_arg_index = 0; args[last_arg_index] != NULL; last_arg_index++) {}
				last_arg_index--;

				if (!strcmp(args[last_arg_index], "&"))
				{
					args[last_arg_index] = NULL;
					execute_command(args, true);
				}
				else
				{
					execute_command(args, false);
				}
			}

			for (int i = 0; args[i] != NULL; i++)
			{
				free(args[i]);
			}

			free(args);
		}
		free(prompt);

		for (int i = 0; i < *num_jobs; i++)
		{
			munmap(jobs[i]->name, sizeof(char) * MAX_COMMAND_LEN + 1);
			munmap(jobs[i]->usage, sizeof(struct rusage));
			munmap(jobs[i], sizeof(Job));
		}
		munmap(jobs, sizeof(Job*) * MAX_NUM_JOBS);
		munmap(num_jobs, sizeof(int));

		munmap(finished_pids, sizeof(int) * MAX_NUM_FINISHED_JOBS);
	}
	else
	{
		char** args = malloc(sizeof(char*) * argc);
		
		for (int i = 1; i < argc; i++)
		{
			args[i - 1] = argv[i];
		}

		args[argc - 1] = NULL;

		execute_command(args, false);
		free(args);
	}
}

int execute_command(char** args, bool background)
{
	if (background)
	{
		bool* done = mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		*done = false;

		int pid2 = fork();

		if (pid2 < 0)
		{
			printf("Could not fork\n");
			return 1;
		}
		else if (pid2 == 0)
		{
			// child

			int pid = fork();

			if (pid < 0)
			{
				printf("Could not fork\n");
				return 1;
			}
			else if (pid == 0)
			{
				// child
				execvp(args[0], args);
				exit(0);
			}
			else
			{
				// parent

				struct timeval t0;
				struct timeval t1;
				gettimeofday(&t0, NULL);
				printf("[%d] %d\n", *num_jobs + 1, pid);
				*done = true;

				int i;
				for (i = 0; args[0][i] != '\0'; i++)
				{
					jobs[*num_jobs]->name[i] = args[0][i];
				}
				jobs[*num_jobs]->name[i] = '\0';

				jobs[*num_jobs]->pid = pid;

				*num_jobs = *num_jobs + 1;	

				int status;
				wait4(pid, &status, 0, jobs[*num_jobs - 1]->usage);

				gettimeofday(&t1, NULL);
				jobs[*num_jobs - 1]->time = ((t1.tv_sec-t0.tv_sec)*1000000 + t1.tv_usec-t0.tv_usec) / 1000;

				fflush(stdout);

				for (i = 0; finished_pids[i] != -1; i++) {}

				finished_pids[i] = pid;
				finished_pids[i + 1] = -1;
				exit(0);
			}
		}
		else
		{
			while (!(*done))
			{
				usleep(10000);
			}

			munmap(done, sizeof(bool));

			return 0;
		}
	}
	else
	{
		int pid = fork();

		if (pid < 0)
		{
			printf("Could not fork\n");
			return 1;
		}
		else if (pid == 0)
		{
			// child

			execvp(args[0], args);
			exit(0);
		}
		else
		{
			// parent
			
			struct timeval t0;
			struct timeval t1;
			gettimeofday(&t0, NULL);

			int status;
			struct rusage usage;
			wait4(pid, &status, 0, &usage);

			gettimeofday(&t1, NULL);

			fflush(stdout);
			print_stats(&usage, ((t1.tv_sec-t0.tv_sec)*1000000 + t1.tv_usec-t0.tv_usec) / 1000);
			return 0;
		}
	}
}

char* get_input(char* prompt)
{
	printf("%s", prompt);
	char* s = malloc(sizeof(char) * (MAX_COMMAND_LEN + 1));
	fgets(s, 128, stdin);
	return s;
}

char** get_args(char* cmd)
{
	char** args = malloc(sizeof(char*) * 33);

	int start_of_arg = 0;
	int num_args = 0;

	for (int i = 0; 1; i++)
	{
		if (cmd[i] == ' ' || cmd[i] == '\n')
		{
			int len = i - start_of_arg + 1;
			char* arg = malloc(sizeof(char) * len);
			arg[len - 1] = '\0';
			for (int j = i - 1; j >= start_of_arg; j--)
			{
				arg[j - i - 1 + len] = cmd[j];
			}
			args[num_args] = arg;
			num_args++;
			start_of_arg = i + 1;

			if (cmd[i] == '\n')
			{
				break;
			}
		}
	}

	args[num_args] = NULL;

	return args;
}

void check_finished_processes(void)
{
	for (int i = 0; finished_pids[i] != -1; i++)
	{
		bool found = false;
		int j;
		for (j = 0; j < *num_jobs; j++)
		{
			if (jobs[j]->pid == finished_pids[i])
			{
				found = true;
				break;
			}
		}

		if (found)
		{
			printf("[%d] %d Completed\n", j + 1, finished_pids[i]);
			print_stats(jobs[j]->usage, jobs[j]->time);

			for (; j < *num_jobs - 1; j++)
			{
				jobs[j] = jobs[j + 1];
			}
			*num_jobs = *num_jobs - 1;

			for (j = i; finished_pids[j] != -1; j++)
			{
				finished_pids[j] = finished_pids[j + 1];
			}
		}
		else
		{
			printf("Could not find pid [%d] in jobs array\n", finished_pids[i]);
			break;
		}
	}
}

void print_stats(struct rusage* usage, int time)
{
	printf("User CPU Time: %dms\n", usage->ru_utime.tv_sec);
	printf("System CPU Time: %dms\n", usage->ru_stime.tv_sec);
	printf("Wall Time: %dms\n", time);
	printf("Involuntary Premptions: %d\n", usage->ru_nivcsw);
	printf("Voluntary CPU Give Ups: %d\n", usage->ru_nvcsw);
	printf("Minor Page Faults: %d\n", usage->ru_minflt);
	printf("Major Page Faults: %d\n", usage->ru_majflt);
}
