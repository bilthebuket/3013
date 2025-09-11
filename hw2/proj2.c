#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

#define NUM_STATS 5

// these are indicides in stats arrays (i.e. stats[NUM_ASCII] = the number of ascii characters read from a file)
#define NUM_ASCII 0
#define NUM_UPPER 1
#define NUM_LOWER 2
#define NUM_DIGIT 3
#define NUM_SPACE 4

#define NUM_BYTES 5

#define DEFAULT_CHUNK_SIZE 1024

#define MAX_NUM_PROCESSES 16

/*
 * Gets the stats from an array of characters
 * params:
 * buf: the array of characters to retrieve the stats from
 * bufsize: size of buf
 * returns:
 * array of integers that stores the retrieved stats
*/
int* get_stats(unsigned char* buf, int bufsize);

/*
 * Reads a file using the read() system call and gets the stats
 * params:
 * fd: the file descriptor of the file to read
 * bufsize: the size of the buffer to use for read()
 * returns:
 * array of stats
*/
int* use_read(int fd, int bufsize);

/*
 * Reads a files using mmap() system call and gets the stats
 * params:
 * fd: the file descriptor of the file to read
 * num_processes: the number of concurrent processes to use to read the file
 * returns:
 * array of stats
*/
int* use_mmap(int fd, int num_processes);

int main(int argc, char* argv[])
{
	if (argc > 1)
	{
		int fd = open(argv[1], O_RDONLY);

		if (fd == -1)
		{
			printf("Could not open file\n");
			return 1;
		}

		if (argc == 2)
		{
			int* stats = use_read(fd, DEFAULT_CHUNK_SIZE);
			printf("ascii=%d, upper=%d, lower=%d, digit=%d, space=%d out of %d bytes\n", stats[NUM_ASCII], stats[NUM_UPPER], stats[NUM_LOWER], stats[NUM_DIGIT], stats[NUM_SPACE], stats[NUM_BYTES]);
			free(stats);
		}
		else
		{
			if (argv[2][0] == 'p')
			{
				int num_processes;
				if (sscanf(argv[2], "p%d", &num_processes) != 1)
				{
					close(fd);
					printf("Could not parse the third argument\n");
					return 1;
				}
				if (num_processes > MAX_NUM_PROCESSES || num_processes < 1)
				{
					printf("Invalid number of processes, must be >= 1 and <= 16\n");
					close(fd);
					return 1;
				}
				use_mmap(fd, num_processes);
			}
			else
			{
				if (!strcmp(argv[2], "mmap"))
				{
					int* stats = use_mmap(fd, 0);
					printf("ascii=%d, upper=%d, lower=%d, digit=%d, space=%d out of %d bytes\n", stats[NUM_ASCII], stats[NUM_UPPER], stats[NUM_LOWER], stats[NUM_DIGIT], stats[NUM_SPACE], stats[NUM_BYTES]);
					free(stats);
				}
				else
				{
					int chunk_size = atoi(argv[2]);
					int* stats = use_read(fd, chunk_size);
					printf("ascii=%d, upper=%d, lower=%d, digit=%d, space=%d out of %d bytes\n", stats[NUM_ASCII], stats[NUM_UPPER], stats[NUM_LOWER], stats[NUM_DIGIT], stats[NUM_SPACE], stats[NUM_BYTES]);
					free(stats);
				}
			}
		}

		close(fd);
	}
	else
	{
		printf("Not enough arguments\n");
	}
}

int* use_mmap(int fd, int num_processes)
{
	struct stat st;

	if (fstat(fd, &st) == -1)
	{
		printf("Could not get file stats\n");
		return NULL;
	}
	if (st.st_size == 0)
	{
		printf("Empty file\n");
		return NULL;
	}

	if (num_processes)
	{
		int chunk_size = st.st_size / num_processes;

		// unless the size of the file divides perfectly into the number of processes we need to add one
		// to a certain number of the processes to make sure we get the remainder from the division
		int num_processes_to_add_one = st.st_size % num_processes;

		int* pids = malloc(sizeof(int) * num_processes);

		for (int i = 0; i < num_processes; i++)
		{
			pids[i] = fork();

			if (pids[i] < 0)
			{
				printf("Could not fork\n");
				free(pids);
				return NULL;
			}
			if (pids[i] == 0)
			{
				free(pids);

				int incrementer = 0;
				if (i < num_processes_to_add_one)
				{
					incrementer = 1;
				}

				// because we aren't supposed to use shared memory to communicate between processes we have 
				// to calculate where we are in the file for each processes, taking into account the one byte 
				// correction that some processes might have because the size of the file is not necessary divisible 
				// by the number of processes
				int where_are_we = 0;
				for (int j = 0; j < i; j++)
				{
					int incremeneter = 0;
					if (j < num_processes_to_add_one)
					{
						incremeneter = 1;
					}
					where_are_we += chunk_size + incremeneter;
				}
				
				int page_size = sysconf(_SC_PAGESIZE);
				int delta = where_are_we % page_size;

				unsigned char* buf = mmap(NULL, chunk_size + incrementer + delta, PROT_READ, MAP_PRIVATE, fd, where_are_we - delta);

				if (buf == MAP_FAILED)
				{
					printf("could not map memory: %s\n", strerror(errno));
					exit(0);
				}

				buf += delta;

				int* stats = get_stats(buf, chunk_size + incrementer);

				printf("Process %d: ascii=%d, upper=%d, lower=%d, digit=%d, space=%d out of %d bytes\n", i + 1, stats[NUM_ASCII], stats[NUM_UPPER], stats[NUM_LOWER], stats[NUM_DIGIT], stats[NUM_SPACE], chunk_size + incrementer);
				
				free(stats);
				munmap(buf - delta, chunk_size + incrementer);
				exit(0);
			}
			else
			{
				continue;
			}
		}

		for (int i = 0; i < num_processes; i++)
		{
			int status;
			waitpid(pids[i], &status, 0);
		}

		return NULL;
	}
	else
	{
		int* r = malloc(sizeof(int) * (NUM_STATS + 1));

		unsigned char* buf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

		if (buf == MAP_FAILED)
		{
			printf("could not map memory: %s\n", strerror(errno));
			exit(0);
		}

		int* stats = get_stats(buf, st.st_size);
		r[NUM_ASCII] = stats[NUM_ASCII];
		r[NUM_UPPER] = stats[NUM_UPPER];
		r[NUM_LOWER] = stats[NUM_LOWER];
		r[NUM_DIGIT] = stats[NUM_DIGIT];
		r[NUM_SPACE] = stats[NUM_SPACE];
		free(stats);

		r[NUM_BYTES] = st.st_size;
		munmap(buf, st.st_size);

		return r;
	}
}


int* use_read(int fd, int bufsize)
{
	int* r = malloc(sizeof(int) * (NUM_STATS + 1));
	r[NUM_ASCII] = 0;
	r[NUM_UPPER] = 0;
	r[NUM_LOWER] = 0;
	r[NUM_DIGIT] = 0;
	r[NUM_SPACE] = 0;
	r[NUM_BYTES] = 0;

	unsigned char buf[bufsize];

	int bytes_read;

	do
	{
		bytes_read = read(fd, &buf, bufsize);

		int* stats = get_stats(buf, bytes_read);
		r[NUM_ASCII] += stats[NUM_ASCII];
		r[NUM_UPPER] += stats[NUM_UPPER];
		r[NUM_LOWER] += stats[NUM_LOWER];
		r[NUM_DIGIT] += stats[NUM_DIGIT];
		r[NUM_SPACE] += stats[NUM_SPACE];
		free(stats);

		r[NUM_BYTES] += bytes_read;
	}
	while (bytes_read == bufsize);

	return r;
}


int* get_stats(unsigned char* buf, int bufsize)
{
	int* r = malloc(sizeof(int) * NUM_STATS);
	r[NUM_ASCII] = 0;
	r[NUM_UPPER] = 0;
	r[NUM_LOWER] = 0;
	r[NUM_DIGIT] = 0;
	r[NUM_SPACE] = 0;

	for (int i = 0; i < bufsize; i++)
	{
		if (isascii(buf[i]))
		{
			r[NUM_ASCII]++;

			if (isupper(buf[i]))
			{
				r[NUM_UPPER]++;
			}
			else if (islower(buf[i]))
			{
				r[NUM_LOWER]++;
			}
			else if (isdigit(buf[i]))
			{
				r[NUM_DIGIT]++;
			}
			else if (buf[i] == ' ')
			{
				r[NUM_SPACE]++;
			}
		}
	}

	return r;
}
