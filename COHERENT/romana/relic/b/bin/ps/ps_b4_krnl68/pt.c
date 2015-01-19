/* Attempt to find page tables in /dev/mem.  */
#include <stdio.h>
#define ONE_CLICK 0x400	/* Number of longs in a click.  */
#define MEM_SIZE 0x460000L /* Size of memory on qiqineb.  */
#define PRESENT 0x00000001L	/* Present bit for Page Table Entries.  */
#define TRUE (1==1)
#define FALSE (1==2)

main()
{
	static unsigned long page_buffer[ONE_CLICK]; /* Load pages here.  */

	int fd;	/* Handle for /dev/mem.  */

	unsigned long *base;	/* Loop through all possible clicks.  */
	unsigned long *walker;	/* Loop through each long in a click.  */

	int nonzero_seen;	/* Seen a nonzero element in this click?  */

	if (-1 == (fd = open("/dev/mem", 0))) {
		perror("Can't open /dev/mem");
		exit (1);
	}
	for (base = 0; base < (unsigned long *) MEM_SIZE; base += ONE_CLICK) {

		if (-1 == read(fd, page_buffer, ONE_CLICK*sizeof(long)) ) {
			perror("Read error");
			exit (1);
		}

		nonzero_seen = FALSE;

		for (walker = page_buffer;
		     walker < page_buffer + ONE_CLICK;
		     ++walker) {

			/* There must be at least one nonzero entry.  */
			if (0 != *walker) {
				nonzero_seen = TRUE;

				/* All nonzero PTE's must be present.  */
				if (0L == (*walker & PRESENT)) {
					break;
				}
			}

			/* All PTE's must point at existant memory.  */
			if (*walker > MEM_SIZE) {
				break;
			}
		}

		if (nonzero_seen && (page_buffer + ONE_CLICK == walker)) {
			printf("Possible page table: %x\n", base);
		}
	}
} /* main() */
