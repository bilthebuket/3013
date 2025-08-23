#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <sys/mman.h>

#define MAX_NUM_JOBS 30
#define MAX_COMMAND_LEN 128
#define SHARED_MEM_SIZE 64

typedef struct Job
{
	int pid;
	char* name;
} Job;

Job** jobs;
int num_jobs = 0;

int* finished_pids;

int execute_command(char** args, bool background);

char* get_input(char* prompt);

char** get_args(char* cmd);

int main(int argc, char* argv[])
{
	if (argc == 1)
	{
		jobs = malloc(sizeof(Job*) * MAX_NUM_JOBS);

		char* prompt = malloc(sizeof(char) * 4);
		prompt[0] = '=';
		prompt[1] = '=';
		prompt[2] = '>';
		prompt[3] = '\0';

		finished_pids = mmap(NULL, SHARED_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		finished_pids[0] = -1;

		while (1)
		{
			char* input = get_input(prompt);
			char** args = get_args(input);
			free(input);

			if (!strcmp(args[0], "exit"))
			{
				for (int i = 0; args[i] != NULL; i++)
				{
					free(args[i]);
				}

				free(args);			
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
				for (int i = 0; i < num_jobs; i++)
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

			for (int i = 0; finished_pids[i] != -1; i++)
			{
				bool found = false;
				int j;
				for (j = 0; j < num_jobs; j++)
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

					free(jobs[j]);
					for (; j < num_jobs - 1; j++)
					{
						jobs[j] = jobs[j + 1];
					}
					num_jobs--;

					for (j = i; finished_pids[j] != -1; j++)
					{
						finished_pids[j] = finished_pids[j + 1];
					}
				}
				else
				{
					printf("Could not find pid in jobs array\n");
					break;
				}
			}


			for (int i = 0; args[i] != NULL; i++)
			{
				free(args[i]);
			}

			free(args);
		}
		free(prompt);

		for (int i = 0; i < num_jobs; i++)
		{
			free(jobs[i]);
		}
		free(jobs);

		munmap(finished_pids, SHARED_MEM_SIZE);
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
			return 0;
		}
		else
		{
			// parent
			
			if (background)
			{
				Job* job = malloc(sizeof(Job));

				int len;
				for (len = 0; args[0][len] != '\0'; len++) {}
				len++;

				printf("[%d] %d\n", num_jobs + 1, pid);
				char* name = malloc(sizeof(char) * len);

				for (int i = 0; i < len; i++)
				{
					name[i] = args[0][i];
				}

				job->name = name;
				job->pid = pid;

				jobs[num_jobs] = job;
				num_jobs++;

				int pid2 = fork();

				if (pid2 < 0)
				{
					printf("Could not fork\n");
					return 1;
				}
				else if (pid2 == 0)
				{
					// child

					int* status_ptr;
					if (waitpid(pid, status_ptr, 0) == -1)
					{
						perror("waitpid");
						return 1;
					}

					int i;
					for (i = 0; finished_pids[i] != -1; i++) {}

					finished_pids[i] = pid;
					finished_pids[i + 1] = -1;
					exit(0);
				}
				else
				{
					return 0;
				}
			}
			else
			{
				wait(0);
			}
			return 0;
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
