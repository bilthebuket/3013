#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define MAX_NUM_REFERENCES 1024
#define MIN_NUM_FRAMES 3
#define MAX_NUM_FRAMES 20

typedef struct Page
{
	int id;
	int num_references;
	int num_calls_since_last_called;
} Page;

typedef struct Frame
{
	int page;
	int num_calls_since_last_called;
	int reference;

	// for the lfu policy, i didnt feel like refactoring all the other policies because they only needed an int to represent which page they were storing
	Page* page_struct; 
} Frame;

typedef struct Info
{
	int num_references;
	int num_page_faults;
} Info;

Info* lru(int num_frames, int* references, bool trace);
Info* fifo(int num_frames, int* references, bool trace);
Info* min(int num_frames, int* references, bool trace);
Info* clock(int num_frames, int* references, bool trace);
Info* lfu(int num_frames, int* references, bool trace);

void all(int num_frames, int* references);

void print_trace(int reference, bool found, Frame** frames, int num_frames);

int main(int argc, char* argv[])
{
	if (argc != 4 && argc != 5)
	{
		printf("Usage: ./proj4 <policy> <num frames> <reference string filename> <Optional: trace>\n");
		return 1;
	}

	bool trace = false;

	if (argc == 5)
	{
		if (!strcmp(argv[4], "trace"))
		{
			trace = true;
		}
	}

	int num_frames = atoi(argv[2]);

	if (num_frames < MIN_NUM_FRAMES || num_frames > MAX_NUM_FRAMES)
	{
		printf("Number of frames cannot be less than 3 or greater than 20.\n");
		return 1;
	}

	FILE* f = fopen(argv[3], "r");

	if (f == NULL)
	{
		printf("Could not open file.\n");
		return 1;
	}
	
	char buf[500];
	int* references = malloc(sizeof(int) * (MAX_NUM_REFERENCES + 1));
	int i = 0;

	while (fgets(buf, 500, f) > 0)
	{
		char* ptr = &buf[0];
		int bytes_read;
		while (i < MAX_NUM_REFERENCES && ptr[0] != '\0' && sscanf(ptr, "%d%n", &references[i], &bytes_read) == 1)
		{
			ptr += bytes_read;
			i++;
		}
	}

	fclose(f);

	references[i] = -1;

	if (!strcmp("lru", argv[1]))
	{
		Info* info = lru(num_frames, references, trace);
		printf("lru policy with %d frames: %d page faults for %d references\n", num_frames, info->num_page_faults, info->num_references);
		free(info);
	}
	else if (!strcmp("fifo", argv[1]))
	{
		Info* info = fifo(num_frames, references, trace);
		printf("fifo policy with %d frames: %d page faults for %d references\n", num_frames, info->num_page_faults, info->num_references);
		free(info);
	}
	else if (!strcmp("min", argv[1]))
	{
		Info* info = min(num_frames, references, trace);
		printf("min policy with %d frames: %d page faults for %d references\n", num_frames, info->num_page_faults, info->num_references);
		free(info);
	}
	else if (!strcmp("clock", argv[1]))
	{
		Info* info = clock(num_frames, references, trace);
		printf("clock policy with %d frames: %d page faults for %d references\n", num_frames, info->num_page_faults, info->num_references);
		free(info);
	}
	else if (!strcmp("lfu", argv[1]))
	{
		Info* info = lfu(num_frames, references, trace);
		printf("lfu policy with %d frames: %d page faults for %d references\n", num_frames, info->num_page_faults, info->num_references);
		free(info);
	}
	else if (!strcmp("all", argv[1]))
	{
		all(num_frames, references);
	}
	else
	{
		printf("Invalid policy.\n");
		free(references);
		return 1;
	}

	free(references);
	return 0;
}

Info* lru(int num_frames, int* references, bool trace)
{
	Frame** frames = malloc(sizeof(Frame*) * num_frames);

	for (int i = 0; i < num_frames; i++)
	{
		frames[i] = malloc(sizeof(Frame));
		frames[i]->page = -1;
	}

	int i = 0;
	int num_page_faults = 0;
	while (references[i] != -1)
	{
		bool found = false;

		int j = 0;

		for (; j < num_frames; j++)
		{
			frames[j]->num_calls_since_last_called++;
		}

		j = 0;

		for (; j < num_frames; j++)
		{
			if (frames[j]->page == references[i])
			{
				found = true;
				frames[j]->num_calls_since_last_called = 0;
				break;
			}
			if (frames[j]->page == -1)
			{
				break;
			}
		}

		if (!found)
		{
			if (j < num_frames)
			{
				frames[j]->page = references[i];
				frames[j]->num_calls_since_last_called = 0;
			}
			else
			{
				int index = 0;

				for (int j = 1; j < num_frames; j++)
				{
					if (frames[j]->num_calls_since_last_called > frames[index]->num_calls_since_last_called)
					{
						index = j;
					}
				}

				frames[index]->page = references[i];
				frames[index]->num_calls_since_last_called = 0;
			}

			num_page_faults++;
		}

		if (trace)
		{
			print_trace(references[i], found, frames, num_frames);
		}

		i++;
	}

	Info* r = malloc(sizeof(Info));
	r->num_references = i;
	r->num_page_faults = num_page_faults;

	for (int i = 0; i < num_frames; i++)
	{
		free(frames[i]);
	}

	free(frames);

	return r;
}

Info* fifo(int num_frames, int* references, bool trace)
{
	Frame** frames = malloc(sizeof(Frame*) * num_frames);

	for (int i = 0; i < num_frames; i++)
	{
		frames[i] = malloc(sizeof(Frame));
		frames[i]->page = -1;
	}

	int i = 0;
	int num_page_faults = 0;
	int next_to_remove = 0;

	while (references[i] != -1)
	{
		bool found = false;

		int j = 0;
		for (; j < num_frames; j++)
		{
			if (references[i] == frames[j]->page)
			{
				found = true;
				break;
			}
			if (frames[j]->page == -1)
			{
				break;
			}
		}

		if (!found)
		{
			if (j < num_frames)
			{
				frames[j]->page = references[i];
			}
			else
			{
				frames[next_to_remove]->page = references[i];
				next_to_remove++;
				if (next_to_remove == num_frames)
				{
					next_to_remove = 0;
				}
			}
			
			num_page_faults++;
		}

		if (trace)
		{
			print_trace(references[i], found, frames, num_frames);
		}

		i++;
	}

	Info* r = malloc(sizeof(Info));
	r->num_references = i;
	r->num_page_faults = num_page_faults;

	for (int i = 0; i < num_frames; i++)
	{
		free(frames[i]);
	}

	free(frames);

	return r;
}

Info* min(int num_frames, int* references, bool trace)
{
	Frame** frames = malloc(sizeof(Frame*) * num_frames);

	for (int i = 0; i < num_frames; i++)
	{
		frames[i] = malloc(sizeof(Frame));
		frames[i]->page = -1;
	}

	int i = 0;
	int num_page_faults = 0;

	while (references[i] != -1)
	{
		bool found = false;

		int j = 0;
		for (; j < num_frames; j++)
		{
			if (references[i] == frames[j]->page)
			{
				found = true;
				break;
			}
			if (frames[j]->page == -1)
			{
				break;
			}
		}

		if (!found)
		{
			if (j < num_frames)
			{
				frames[j]->page = references[i];
			}
			else
			{
				int index;
				int how_far_away = -1;
				for (j = 0; j < num_frames; j++)
				{
					int k = i + 1;
					bool found2 = false;

					// highly inefficent but whatever
					// better implementation would be to maintain how far away each frame is from its next reference,
					// then decrement that value every time a reference is made, then take the highest of those values
					// every time a replacement is necessary
					for (; references[k] != -1; k++)
					{
						if (references[k] == frames[j]->page)
						{
							found2 = true;

							if (k - i - 1 > how_far_away)
							{
								how_far_away = k - i - 1;
								index = j;
							}

							break;
						}
					}

					if (!found2)
					{
						index = j;
						break;
					}
				}

				frames[index]->page = references[i];
			}

			num_page_faults++;
		}

		if (trace)
		{
			print_trace(references[i], found, frames, num_frames);
		}

		i++;
	}

	Info* r = malloc(sizeof(Info));
	r->num_references = i;
	r->num_page_faults = num_page_faults;

	for (int i = 0; i < num_frames; i++)
	{
		free(frames[i]);
	}

	free(frames);

	return r;
}

Info* clock(int num_frames, int* references, bool trace)
{
	Frame** frames = malloc(sizeof(Frame*) * num_frames);

	for (int i = 0; i < num_frames; i++)
	{
		frames[i] = malloc(sizeof(Frame));
		frames[i]->page = -1;
	}

	int i = 0;
	int num_page_faults = 0;
	int index = 0;

	while (references[i] != -1)
	{
		int j = 0;
		bool found = false;

		for (; j < num_frames; j++)
		{
			if (references[i] == frames[j]->page)
			{
				found = true;
				frames[j]->reference = 1;
				break;
			}
			if (frames[j]->page == -1)
			{
				break;
			}
		}

		if (!found)
		{
			if (j < num_frames)
			{
				frames[j]->page = references[i];
				frames[j]->reference = 1;
			}
			else
			{
				for (; frames[index]->reference == 1; index++)
				{
					frames[index]->reference = 0;
					if (index == num_frames - 1)
					{
						index = -1;
					}
				}

				frames[index]->page = references[i];
				frames[index]->reference = 1;

				index++;
				if (index == num_frames)
				{
					index = 0;
				}
			}

			num_page_faults++;
		}

		if (trace)
		{
			print_trace(references[i], found, frames, num_frames);
		}

		i++;
	}


	Info* r = malloc(sizeof(Info));
	r->num_references = i;
	r->num_page_faults = num_page_faults;

	for (int i = 0; i < num_frames; i++)
	{
		free(frames[i]);
	}

	free(frames);

	return r;
}

Info* lfu(int num_frames, int* references, bool trace)
{
	int num_unique_references = 0;
	int unique_references[MAX_NUM_REFERENCES];

	for (int i = 0; references[i] != -1; i++)
	{
		bool found = false;

		for (int j = 0; j < num_unique_references; j++)
		{
			if (unique_references[j] == references[i])
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			unique_references[num_unique_references] = references[i];
			num_unique_references++;
		}
	}

	Page** frames = malloc(sizeof(Page*) * num_frames);
	Page** pages = malloc(sizeof(Page*) * num_unique_references);

	for (int i = 0; i < num_frames; i++)
	{
		frames[i] = NULL;
	}
	for (int i = 0; i < num_unique_references; i++)
	{
		pages[i] = malloc(sizeof(Page));
		pages[i]->id = unique_references[i];
		pages[i]->num_references = 0;
		pages[i]->num_calls_since_last_called = 0;
	}

	int i = 0;
	int num_page_faults = 0;
	int index = 0;

	while (references[i] != -1)
	{
		int j = 0;
		bool found = false;

		for (; j < num_frames; j++)
		{
			if (frames[j] == NULL)
			{
				break;
			}
			frames[j]->num_calls_since_last_called++;
		}

		j = 0;

		for (; j < num_frames; j++)
		{
			if (frames[j] == NULL)
			{
				break;
			}
			if (frames[j]->id == references[i])
			{
				found = true;
				frames[j]->num_references++;
				frames[j]->num_calls_since_last_called = 0;
				break;
			}
		}

		if (!found)
		{
			if (j < num_frames)
			{
				for (int k = 0; k < num_unique_references; k++)
				{
					if (pages[k]->id == references[i])
					{
						frames[j] = pages[k];
						frames[j]->num_calls_since_last_called = 0;
						break;
					}
				}
			}
			else
			{
				int min_references = frames[0]->num_references;
				int index = 0;
				j = 1;
				for (; j < num_frames; j++)
				{
					if (frames[j]->num_references < min_references)
					{
						min_references = frames[j]->num_references;
						index = j;
					}
					else if (frames[j]->num_references == min_references)
					{
						if (frames[j]->num_calls_since_last_called > frames[index]->num_calls_since_last_called)
						{
							min_references = frames[j]->num_references;
							index = j;
						}
					}
				}

				for (int k = 0; k < num_unique_references; k++)
				{
					if (pages[k]->id == references[i])
					{
						frames[index] = pages[k];
						frames[index]->num_calls_since_last_called = 0;
						break;
					}
				}
			}

			num_page_faults++;
		}

		if (trace)
		{
			printf("%d ", references[i]);

			if (found)
			{
				printf("- ");
			}
			else
			{
				printf("P ");
			}

			printf("Frames:");

			int j;

			for (j = 0; j < num_frames && frames[j] != NULL; j++)
			{
				printf(" %d", frames[j]->id);
			}

			for (; j < num_frames; j++)
			{
				printf(" -");
			}

			printf("\n");
		}

		i++;
	}

	Info* r = malloc(sizeof(Info));
	r->num_references = i;
	r->num_page_faults = num_page_faults;

	for (int i = 0; i < num_unique_references; i++)
	{
		free(pages[i]);
	}

	free(pages);
	free(frames);

	return r;
}

void all(int num_frames, int* references)
{
	FILE* f = fopen("output.csv", "w");
	fprintf(f, "frames,lru,fifo,min,clock,lfu\n");

	for (int i = 3; i <= num_frames; i++)
	{
		fprintf(f, "%d,", i);

		Info* info = lru(i, references, false);
		fprintf(f, "%d,", info->num_page_faults);
		free(info);

		info = fifo(i, references, false);
		fprintf(f, "%d,", info->num_page_faults);
		free(info);

		info = min(i, references, false);
		fprintf(f, "%d,", info->num_page_faults);
		free(info);

		info = clock(i, references, false);
		fprintf(f, "%d,", info->num_page_faults);
		free(info);

		info = lfu(i, references, false);
		fprintf(f, "%d\n", info->num_page_faults);
		free(info);
	}

	fclose(f);
}

void print_trace(int reference, bool found, Frame** frames, int num_frames)
{
	printf("%d ", reference);

	if (found)
	{
		printf("- ");
	}
	else
	{
		printf("P ");
	}

	printf("Frames:");

	int j;

	for (j = 0; j < num_frames && frames[j]->page != -1; j++)
	{
		printf(" %d", frames[j]->page);
	}

	for (; j < num_frames; j++)
	{
		printf(" -");
	}

	printf("\n");
}
