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

// the maximum number of background process we can run simultaneously
#define MAX_NUM_JOBS 30
// maximum number of characters in a command
#define MAX_COMMAND_LEN 128
// maxiumum number of completed background processes we can have
#define MAX_NUM_FINISHED_JOBS 30

// stores information about a background process
typedef struct Job
{
	int pid;

	// the name of the command
	char* name;

	// stores information about the resources used
	struct rusage* usage;

	// the wall time it took to execute
	int time;
} Job;

// all of the following pointers are allocated on shared memory
Job** jobs;
int* num_jobs;

// note: must terminate with a -1 similar to how a string must terminate with a '\0'
int* finished_pids;

// whenever you want to modify shared memory, you must wait for this to become false, then set it to true while modifying
// the memory, then set it back when you are finished
bool* shared_mem_in_use;

/*
 * executes a bash command
 * params
 * args: the arguments of the command (ex: if you ran "gdb -x commands.txt ./program" then args[0] = "gdb" args[1] = "-x" args[2] = "commands.txt" and args[3] = "./program")
 * background: if the process should be ran in the background, this will be true. false otherwise
 * returns
 * 1 if successful, 0 otherwise
*/
int execute_command(char** args, bool background);

/*
 * gets a command from the user to run
 * params
 * prompt: the string to use as the prompt for the user
 * returns
 * the retrieved string
*/
char* get_input(char* prompt);

/*
 * parses a command to split up its arguments into separate strings
 * params
 * cmd: the command to parse
 * returns
 * the parsed command
 * note:
 * an example of a parsed command is the in the execute_command function header comment
*/
char** get_args(char* cmd);

/*
 * checks for finished processes in the finished_pids array and then removes them from the jobs array if the corresponding process is found
 * also prints the stats about the finished process
 * params none
 * returns void
*/
void check_finished_processes(void);

/*
 * prints the stats about a process
 * params
 * usage: a struct containing most of the system stats, (cpu system time and user time, page faults, etc)
 * time: contains the wall time for a process' execution
 * returns void
*/
void print_stats(struct rusage* usage, int time);

int main(int argc, char* argv[])
{
	if (argc == 1)
	{
		// setting prompt to the default, which is "==>"
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

		shared_mem_in_use = mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		*shared_mem_in_use = false;

		finished_pids = mmap(NULL, sizeof(int) * MAX_NUM_FINISHED_JOBS, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		finished_pids[0] = -1;

		// main loop: get input, parse the input, execute the input, repeat until "exit" command
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

				// getting the length of the new prompt, which is stored in the 4th element (index number 3) of the command
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
				// checking for an "&" in the last argmument which would indicate that we need to run
				// this process in the background
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
		munmap(shared_mem_in_use, sizeof(bool));
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
	// if this is a background task, we will fork twice, the first child will gather information about the execution and add the job to
	// the jobs array, the grandchild will actually perform the execution
	if (background)
	{
		// this boolean will become true when the 1st child as forked to start the execution and then printed the line telling the user
		// the pid of the background task. we need the parent to wait until this becomes true in order to prevent the parent from printing something else
		// before the 1st child prints that pid
		bool* done = mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		*done = false;

		int pid2 = fork();

		if (pid2 < 0)
		{
			printf("Could not fork\n");
			return 0;
		}
		else if (pid2 == 0)
		{
			// child

			int pid = fork();

			if (pid < 0)
			{
				printf("Could not fork\n");
				return 0;
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

				// getting the time for when the process starts, filling in information about the process
				// then waiting for the process to finish, getting the time when the process finishes, and adding it to finished_pids
				struct timeval t0;
				struct timeval t1;
				gettimeofday(&t0, NULL);

				while (*shared_mem_in_use)
				{
					usleep(1);
				}

				*shared_mem_in_use = true;

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

				*shared_mem_in_use = false;

				// we have now gathered all the pre execution information and added it to the jobs array so we will wait for the child
				// to finish the actual bash execution
				int status;
				wait4(pid, &status, 0, jobs[*num_jobs - 1]->usage);

				gettimeofday(&t1, NULL);

				while (*shared_mem_in_use)
				{
					usleep(1);
				}

				*shared_mem_in_use = true;

				// this is a failsafe for if a bump occurs in the job array from a check_finished_process(), 
				// which would make the index in the job array that we are dealing with different than *num_jobs - 1
				for (i = 0; i < *num_jobs; i++)
				{
					if (jobs[i]->pid == pid)
					{
						break;
					}
				}

				jobs[i]->time = ((t1.tv_sec-t0.tv_sec)*1000000 + t1.tv_usec-t0.tv_usec) / 1000;

				fflush(stdout);

				// this process is now complete so we add it to finished_pids
				for (i = 0; finished_pids[i] != -1; i++) {}

				finished_pids[i] = pid;
				finished_pids[i + 1] = -1;

				*shared_mem_in_use = false;

				exit(0);
			}
		}
		else
		{
			// waiting for the child to give the go ahead for continuing the execution
			while (!(*done))
			{
				usleep(1);
			}

			munmap(done, sizeof(bool));

			return 1;
		}
	}
	else
	{
		// if we aren't handling a background task this is very simple, we will just create a child to execute the process while
		// the parent gathers information about said process
		int pid = fork();

		if (pid < 0)
		{
			printf("Could not fork\n");
			return 0;
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
			return 1;
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
	while (*shared_mem_in_use)
	{
		usleep(1);
	}

	*shared_mem_in_use = true;

	// searching for each finished pid in the jobs array
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
				// this bumps the pointer to the completed job to the end of the job array
				// and moves the other elements down to replace it. if we do not bump the finished element to the end,
				// we will have a memory leak as some of the elements in the jobs array will point to same job, and others
				// will be lost (remember, these are pointers to different chunks of allocated memory which are not deep copied or ever freed/realloced until the end)
				Job* tmp = jobs[j];
				jobs[j] = jobs[j + 1];
				jobs[j + 1] = tmp;
			}
			*num_jobs = *num_jobs - 1;

			for (int k = 0; k < *num_jobs; k++)
			{
				printf("Job %d, Pid %d\n", k, jobs[k]->pid);
			}
		}
		else
		{
			printf("Could not find pid [%d] in jobs array\n", finished_pids[i]);
			break;
		}
	}

	// note: this line means that when a pid is not found in the array on its first check, it will not check again
	// this should never be an issue given that a job is always added before its added to finished_pids, but none the less
	// it is important to point out
	finished_pids[0] = -1;

	*shared_mem_in_use = false;
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
