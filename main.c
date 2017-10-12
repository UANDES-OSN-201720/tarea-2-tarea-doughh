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

int get_frame_to_pop_fifo();
int get_frame_to_pop_lru();
int get_frame_to_pop_custom();
int get_page_from_frame(int frame);

int FIFO = 0;
int LRU = 0;
int CUSTOM = 0;

int replacing_algorithm = 0;
struct disk *disk;

void page_fault_handler( struct page_table *pt, int page) {
	int bit_value;
	int frame_index;
	page_table_get_entry( pt, page, &frame_index, &bit_value );

	switch (bit_value) {
		case PROT_READ: {
			page_table_set_entry(pt, page, frame_index, PROT_READ|PROT_WRITE);
		} break;

		case PROT_NONE: {
			char *physmem = page_table_get_physmem(pt);

			// Elijo el 'frame_to_pop' frame que hay que sacar de physical
			int frame_to_pop = 0;

			if (replacing_algorithm == FIFO) {
				frame_to_pop = get_frame_to_pop_fifo();
			}
			else if (replacing_algorithm == LRU) {
				frame_to_pop = get_frame_to_pop_lru();
			}
			else if (replacing_algorithm == CUSTOM) {
				frame_to_pop = get_frame_to_pop_custom();
			} else {
				printf("Replacing Algorithm not recognized\n");
				exit(1);
			}

			// Encuentro donde hay que guardarlo en disk
			int page_to_pop = get_page_from_frame(frame_to_pop);

			if ( page_to_pop != -1 ) {
				int bit_to_pop;
				page_table_get_entry( pt, page_to_pop, NULL, &bit_to_pop );

				if (bit_to_pop == (PROT_READ|PROT_WRITE)) {

					// Saco de physical el que voy a sacar y lo guardo en disk
					disk_write(disk, page_to_pop, &physmem[ frame_to_pop*PAGE_SIZE ]);
				}

				// Modifico en virtmem que el frame sacado no esta en physical
				int NOT_IN_PHYSICAL = 0;
				page_table_set_entry(pt, page_to_pop, NOT_IN_PHYSICAL, PROT_NONE);
			}

			// Saco del disk el 'page' frame y lo guardo en 'frame_to_pop' en physical
			disk_read(disk, page, &physmem[ frame_to_pop*PAGE_SIZE ]);

			// Modifico en virtmem que el frame 'frame_to_pop' y read esta en physical en el frame 'frame_to_pop'
			page_table_set_entry(pt, page, frame_to_pop, PROT_READ);

		} break;

		default:
			exit(1);
	}
}
int get_frame_to_pop_fifo() {
	printf("fifo algorithm not implemented\n");
	return 0;
}
int get_frame_to_pop_lru() {
	printf("lru algorithm not implemented\n");
	return -1;
}
int get_frame_to_pop_custom() {
	printf("custom algorithm not implemented\n");
	return -1;
}
int get_page_from_frame(int frame) {
	return -1;
}

int main( int argc, char *argv[]) {
	if(argc!=5) {
		/* Add 'random' replacement algorithm if the size of your group is 3 */
		printf("use: virtmem <npages> <nframes> <lru|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}

	int npages = atoi(argv[1]);
	int nframes = atoi(argv[2]);
	const char *algorithm = argv[3];
	const char *program = argv[4];

	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}


	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	char *virtmem = page_table_get_virtmem(pt);

	if(!strcmp(algorithm, "fifo")) {
		replacing_algorithm = FIFO;

	} else if(!strcmp(algorithm, "lru")) {
		replacing_algorithm = LRU;

	} else if(!strcmp(algorithm, "custom")) {
		replacing_algorithm = CUSTOM;

	}

	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);

	}

	page_table_delete(pt);
	disk_close(disk);

	return 0;
}
