/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

int get_frame_to_pop_fifo(int nframes);
int get_frame_to_pop_lru(int nframes);
int get_frame_to_pop_custom(int nframes);
int get_page_from_frame_table(int frame);

int FIFO = 0;
int LRU = 1;
int CUSTOM = 2;

int *frame_table;
int last_frame_index;
int custom_algorithm_counter = 1;

// Statistics
int page_faults_amount;
int disk_write_amount;
int disk_read_amount;

int replacing_algorithm;
struct disk *disk;

void page_fault_handler( struct page_table *pt, int page) {
	// Log that a page fault has been produced
	page_faults_amount ++;

	// Get current frame index and bit value at the requested page
	int frame_index;
	int bit_value;
	page_table_get_entry( pt, page, &frame_index, &bit_value );

	switch (bit_value) {
		case PROT_READ: {
			// In case the requested page is read-only, set it to read-write
			page_table_set_entry(pt, page, frame_index, PROT_READ|PROT_WRITE);

		} break;
		case PROT_NONE: {
			/*
				In case the requested page frame is not in the physical memory,
				replace one using the 'replacing_algorithm'.
			*/

			char *physmem = page_table_get_physmem(pt);
			int nframes = page_table_get_nframes(pt);

			/*
				We use 'frame_to_pop' index to select the frame to be replaced
				by the requested page frame on the physical memory.
			*/
			int frame_to_pop = 0;
			if (replacing_algorithm == FIFO) {
				frame_to_pop = get_frame_to_pop_fifo(nframes);

			} else if (replacing_algorithm == LRU) {
				frame_to_pop = get_frame_to_pop_lru(nframes);

			} else if (replacing_algorithm == CUSTOM) {
				frame_to_pop = get_frame_to_pop_custom(nframes);

			} else {
				printf("Replacing Algorithm not recognized\n");
				exit(1);
			}

			/*
				Once we have selected a page frame to be replaced, we get the
				page index.
			*/
			int page_to_pop = get_page_from_frame_table(frame_to_pop);
			frame_table[frame_to_pop] = page;

			/*
				If the physical memory has the 'page_to_pop' index used it will
				write it to disk if the page has the corresponding permissions.
				Then it will be marked in the page table as
				'not on physical memory'.
			*/
			if ( page_to_pop != -1 ) {
				int bit_to_pop;
				page_table_get_entry( pt, page_to_pop, &frame_to_pop, &bit_to_pop );

				if (bit_to_pop == (PROT_READ|PROT_WRITE)) {
					disk_write(disk, page_to_pop, &physmem[ frame_to_pop*PAGE_SIZE ]);
					disk_write_amount ++;  // Log that a disk write has been produced.
				}

				int NOT_IN_PHYSICAL = 0;
				page_table_set_entry(pt, page_to_pop, NOT_IN_PHYSICAL, PROT_NONE);
			}

			/*
				From the disk we retrieve the frame corresponding to the page
				and save it in the physical memory.
			*/
			disk_read(disk, page, &physmem[ frame_to_pop*PAGE_SIZE ]);
			disk_read_amount ++;  // Log that a disk read has been produced.

			/*
				On the page table gets logged that the 'frame_to_pop' has been
				loaded into physical memory.
			*/
			page_table_set_entry(pt, page, frame_to_pop, PROT_READ);

		} break;

		default:
			exit(1);
	}
}

int get_frame_to_pop_fifo(int nframes) {
	last_frame_index = (last_frame_index + 1) % nframes;
	return last_frame_index;
}

int get_frame_to_pop_lru(int nframes) {
	last_frame_index = (rand() + last_frame_index) % nframes;
	return last_frame_index;
}

int get_frame_to_pop_custom(int nframes) {
	last_frame_index = (last_frame_index + custom_algorithm_counter) % nframes;
	custom_algorithm_counter = !custom_algorithm_counter;

	return last_frame_index;
}

int get_page_from_frame_table(int frame) {
	int page = frame_table[frame];
	return page;
}

int main( int argc, char *argv[]) {
	if(argc!=5) {
		printf("Use: virtmem <npages> <nframes> <lru|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}

	// Load arguments
	int npages = atoi(argv[1]);
	int nframes = atoi(argv[2]);
	const char *algorithm = argv[3];
	const char *program = argv[4];

	// Statistics variables init
	page_faults_amount = 0;
	disk_read_amount = 0;
	disk_write_amount = 0;

	// Global variables initializing
  	srand(time(NULL));

	last_frame_index = -1;

	frame_table = malloc(sizeof(int) * nframes);
	for (int page = 0; page < nframes; page++) {
		frame_table[page] = -1;
	}

	if(!strncmp(algorithm, "fifo", 4)) {
		replacing_algorithm = FIFO;

	} else if(!strncmp(algorithm, "lru", 3)) {
		replacing_algorithm = LRU;

	} else if(!strncmp(algorithm, "custom", 6)) {
		replacing_algorithm = CUSTOM;

	} else {
		fprintf(stderr,"Unknown algorithm: %s\n", argv[3]);
		exit(1);
	}

	disk = disk_open("myvirtualdisk", npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n", strerror(errno));
		return 1;
	}

	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr, "couldn't create page table: %s\n", strerror(errno));
		return 1;
	}

	char *virtmem = page_table_get_virtmem(pt);
	if(!strncmp(program, "sort", 4)) {
		sort_program(virtmem, npages*PAGE_SIZE);

	} else if(!strncmp(program, "scan", 4)) {
		scan_program(virtmem, npages*PAGE_SIZE);

	} else if(!strncmp(program, "focus", 5)) {
		focus_program(virtmem, npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[4]);
		exit(1);
	}

	page_table_delete(pt);
	disk_close(disk);

	// Free frame_table malloc
	free(frame_table);

	printf("Total Faults: %d\n", page_faults_amount);
	printf("Disk Read: %d\n", disk_read_amount);
	printf("Disk Write: %d\n", disk_write_amount);

	return 0;
}
