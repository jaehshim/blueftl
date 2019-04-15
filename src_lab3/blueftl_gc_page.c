#ifdef KERNEL_MODE

#include <linux/vmalloc.h>
#include <time.h>
#include <stdbool.h>

#include "blueftl_ftl_base.h"
#include "blueftl_mapping_page.h"
#include "blueftl_gc_page.h"
#include "blueftl_util.h"
#include "blueftl_wl_dual_pool.h"

#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

#include "blueftl_ftl_base.h"
#include "blueftl_mapping_page.h"
#include "blueftl_gc_page.h"
#include "blueftl_util.h"
#include "blueftl_user_vdevice.h"
#include "blueftl_wl_dual_pool.h"

#endif


void select_victim_block(struct flash_ssd_t * ptr_ssd, uint32_t victim_bus, uint32_t victim_chip, uint32_t * victim_block, int select_case)
{
	int i;
	struct flash_block_t* ptr_victim_block = NULL;
	uint32_t min, age, ctime;
	double val, min_val, util;
	switch (select_case)
	{
	case 1: // Greedy
		min = 200;
		*victim_block = 0;
		for (i = 0; i < ptr_ssd->nr_blocks_per_chip; i++)
		{
			ptr_victim_block = &ptr_ssd->list_buses[victim_bus].list_chips[victim_chip].list_blocks[i];

			if (ptr_victim_block->nr_free_pages == ptr_ssd->nr_pages_per_block)
				continue;

			if (min > ptr_victim_block->nr_valid_pages)
			{
				min = ptr_victim_block->nr_valid_pages;
				*victim_block = i;
			}
		}
		break;
	case 2: // Random
		srand(time(NULL));
		*victim_block = rand() % ptr_ssd->nr_blocks_per_chip;
		/* Avoid free block to be selected as victim */
		while (ptr_ssd->list_buses[victim_bus].list_chips[victim_chip].list_blocks[*victim_block].nr_free_pages == ptr_ssd->nr_pages_per_block) {
			*victim_block = rand() % ptr_ssd->nr_blocks_per_chip;
		}
		break;

	case 3: // Cost-Benefit
		ctime = timer_get_timestamp_in_us();
		min_val = (double)ctime;
		*victim_block = 0;
		for (i = 0; i < ptr_ssd->nr_blocks_per_chip; i++)
		{
			ptr_victim_block = &ptr_ssd->list_buses[victim_bus].list_chips[victim_chip].list_blocks[i];

			if (ptr_victim_block->nr_free_pages == ptr_ssd->nr_pages_per_block)
				continue;

			util = (double)ptr_victim_block->nr_valid_pages / ptr_ssd->nr_pages_per_block;
			age = ctime - ptr_victim_block->last_modified_time;
			val = (double)util / ((1-util) * age);
			if (min_val > val)
			{
				min_val = val;
				*victim_block = i;
			}
		}
		break;
	default:
		break;
	}
}

int32_t gc_page_trigger_gc_lab(
	struct ftl_context_t *ptr_ftl_context,
	uint32_t victim_bus,
	uint32_t victim_chip)
{
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;
	struct flash_block_t* ptr_victim_block = NULL;

	uint8_t* ptr_page_buff = NULL;
	uint32_t victim_block;
	uint32_t loop_page = 0;
	int32_t ret = 0;

	uint8_t* ptr_block_buff = NULL;

	select_victim_block(ptr_ssd, victim_bus, victim_chip, &victim_block, 3); // 1:Greedy, 2:Random, 3:Cost-Benefit

	/* get the victim block information */
	if ((ptr_victim_block = &(ptr_ssd->list_buses[victim_bus].list_chips[victim_chip].list_blocks[victim_block])) == NULL) {
		printf ("blueftl_gc_block: oops! 'ptr_victim_block' is NULL\n");
		return -1;
	}

	/* allocate the memory for the block merge & memset buffer */
	if ((ptr_block_buff = (uint8_t*)malloc (ptr_ssd->nr_pages_per_block * ptr_vdevice->page_main_size)) == NULL) {
		printf ("blueftl_gc_block: the memory allocation for the merge buffer failed (%u)\n",
				ptr_ssd->nr_pages_per_block * ptr_vdevice->page_main_size);
		return -1;
	}
	memset (ptr_block_buff, 0xFF, ptr_ssd->nr_pages_per_block * ptr_vdevice->page_main_size);

	/* MERGE-STEP1: read all the valid pages from the target block */
	for (loop_page = 0; loop_page < ptr_ssd->nr_pages_per_block; loop_page++) {
		if (ptr_victim_block->list_pages[loop_page].page_status == 3) { // new_page_offset not needed
			/* read the valid page data from the flash memory */
			ptr_page_buff = ptr_block_buff + (loop_page * ptr_vdevice->page_main_size);

			blueftl_user_vdevice_page_read (
				_ptr_vdevice,
				victim_bus, victim_chip, victim_block, loop_page, 
				ptr_vdevice->page_main_size,
				(char*)ptr_page_buff);

			perf_gc_inc_page_copies ();
		}
	}

	/* MERGE-STEP3: erase the victim block */
	blueftl_user_vdevice_block_erase (ptr_vdevice, victim_bus, victim_chip, victim_block);
	ptr_victim_block->nr_erase_cnt++;
	ptr_victim_block->nr_recent_erase_cnt++;
	perf_gc_inc_blk_erasures ();

	/* MERGE-STEP4: copy the data in the buffer to the block again */
	for (loop_page = 0; loop_page < ptr_ssd->nr_pages_per_block; loop_page++) {
		if (ptr_victim_block->list_pages[loop_page].page_status == 3) { // new_page_offset not needed
			ptr_page_buff = ptr_block_buff + (loop_page * ptr_vdevice->page_main_size);

			blueftl_user_vdevice_page_write ( 
					ptr_vdevice,
					victim_bus, victim_chip, victim_block, loop_page, 
					ptr_vdevice->page_main_size, 
					(char*)ptr_page_buff);

			if (ptr_victim_block->list_pages[loop_page].page_status == 1) { // 사실 필요 없음
				/* increase the number of valid pages in the block */
				ptr_victim_block->nr_valid_pages++;
				ptr_victim_block->nr_free_pages--;

				/* make the page status valid */
				ptr_victim_block->list_pages[loop_page].page_status = 3;
			}
		}
	}

	check_max_min_nr_erase_cnt(ptr_ftl_context, ptr_victim_block);

	if (check_cold_data_migration())
		cold_data_migration(ptr_ftl_context);
/*	if (check_cold_pool_adjustment())
		cold_pool_adjustment(ptr_ftl_context);
	if (check_hot_pool_adjustment())
		hot_pool_adjustment(ptr_ftl_context);

*/
	/* free the block buffer */
	if (ptr_block_buff != NULL)
	{
		free(ptr_block_buff);
	}

	return ret;
}