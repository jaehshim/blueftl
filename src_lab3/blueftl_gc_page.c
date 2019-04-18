#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blueftl_ftl_base.h"
#include "blueftl_mapping_page.h"
#include "blueftl_gc_page.h"
#include "blueftl_util.h"
#include "blueftl_user_vdevice.h"

unsigned char gc_buff[FLASH_PAGE_SIZE];

struct flash_block_t* gc_page_select_victim_greedy (
	struct ftl_context_t* ptr_ftl_context,
	int32_t gc_target_bus, 
	int32_t gc_target_chip)
{
	struct flash_block_t* ptr_victim_block = NULL;
	struct flash_ssd_t* ptr_ssd = ptr_ftl_context->ptr_ssd;

	uint32_t nr_max_invalid_pages = 0;
	uint32_t nr_cur_invalid_pages;
	uint32_t loop_block;

	for (loop_block = 0; loop_block < ptr_ssd->nr_blocks_per_chip; loop_block++) 
	{
		nr_cur_invalid_pages = 
			ptr_ssd->list_buses[gc_target_bus].list_chips[gc_target_chip].list_blocks[loop_block].nr_invalid_pages;

		if (nr_cur_invalid_pages == ptr_ssd->nr_pages_per_block) 
		{
			// all the pages that belong to the block are invalid
			ptr_victim_block = &(ptr_ssd->list_buses[gc_target_bus].list_chips[gc_target_chip].list_blocks[loop_block]); 
			return ptr_victim_block;
		}

		/* hot max이거나 cold min이면 victim에서 제외 */
		if (&(ptr_ssd->list_buses[gc_target_bus].list_chips[gc_target_chip].list_blocks[loop_block]) == ptr_ftl_context->hot_block_ec_max ||
			&(ptr_ssd->list_buses[gc_target_bus].list_chips[gc_target_chip].list_blocks[loop_block]) == ptr_ftl_context->cold_block_ec_min)
			continue;

		if (nr_max_invalid_pages < nr_cur_invalid_pages) 
		{
			nr_max_invalid_pages = nr_cur_invalid_pages;
			ptr_victim_block = &(ptr_ssd->list_buses[gc_target_bus].list_chips[gc_target_chip].list_blocks[loop_block]); 
		}
	}

	/* TODO: need a way to handle such a case more nicely */
	if (ptr_victim_block == NULL) {
		printf ("blueftl_gc_page: oops! 'ptr_victim_block' is NULL.\nBlueFTL will die.\n");
	}

	return ptr_victim_block;
}

int32_t gc_page_trigger_gc_lab (
	struct ftl_context_t* ptr_ftl_context,
	int32_t gc_target_bus,
	int32_t gc_target_chip)
{
	int ret = -1;
	uint32_t loop_page = 0;
	uint32_t loop_page_victim = 0;
	uint32_t loop_page_gc = 0;
	uint32_t logical_page_address;

	struct flash_block_t* ptr_gc_block = NULL;
	struct flash_block_t* ptr_victim_block = NULL;

	struct flash_ssd_t* ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_context_t* ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;
	struct virtual_device_t* ptr_vdevice = ptr_ftl_context->ptr_vdevice;

	// (1) get the victim block using the greedy policy
	if ((ptr_victim_block = gc_page_select_victim_greedy (ptr_ftl_context, gc_target_bus, gc_target_chip)) == NULL) 
	{
		printf ("blueftl_gc: victim block is not selected\n");
		return ret;
	}

	if (ptr_victim_block->nr_invalid_pages == 0) 
	{
		printf("[ERROR] the victim block at least has 1 invalid page.\n");
		return ret;
	}

	// (2) get the free block reserved for the garbage collection
	ptr_gc_block = ptr_pg_mapping->ptr_gc_block;

	// (3) check error cases
	if(ptr_gc_block->nr_free_pages != ptr_ssd->nr_pages_per_block) 
	{
		printf("[ERROR] the gc block should be empty.\n");
		return ret;
	}

	ret = 0;

	// (4) copy flash pages from the victim block to the gc block
	if(ptr_victim_block->nr_valid_pages > 0) 
	{
		for(loop_page_victim = 0; loop_page_victim < ptr_ssd->nr_pages_per_block; loop_page_victim++) 
		{
			if(ptr_victim_block->list_pages[loop_page_victim].page_status == PAGE_STATUS_VALID) 
			{
				// get a physical page address of the page to be copied.
				logical_page_address = ptr_victim_block->list_pages[loop_page_victim].no_logical_page_addr;

				// copy the valid page on the victim block to the next new page on gc block reserved.
				blueftl_user_vdevice_page_read(
					ptr_vdevice,
					ptr_victim_block->no_bus, 
					ptr_victim_block->no_chip,
					ptr_victim_block->no_block ,
					loop_page_victim, 
					FLASH_PAGE_SIZE, 
					(char*) gc_buff);

				blueftl_user_vdevice_page_write(
					ptr_vdevice,
					ptr_gc_block->no_bus, 
					ptr_gc_block->no_chip,
					ptr_gc_block->no_block,
					loop_page_gc, 
					FLASH_PAGE_SIZE, 
					(char*) gc_buff);

				/*page_mapping_map_logical_to_physical (ptr_ftl_context, logical_page_address, physical_page_address);*/
				page_mapping_map_logical_to_physical (
					ptr_ftl_context, 
					logical_page_address, 
					ptr_gc_block->no_bus, 
					ptr_gc_block->no_chip, 
					ptr_gc_block->no_block, 
					loop_page_gc);

				// check whether the page has been moved succesfully.
				if(ptr_victim_block->list_pages[loop_page_victim].page_status != PAGE_STATUS_INVALID) 
				{
					printf("[ERROR] the victim page is not invalidated\n");
				}

				if(ptr_gc_block->list_pages[loop_page_gc].page_status != PAGE_STATUS_VALID) 
				{
					printf("[ERROR] the gc page is not valid\n");
				}

				loop_page_gc++;

				/* update the profiling information */
				perf_gc_inc_page_copies ();
			}
		}
	} 

	// (5) erase the victim block
	blueftl_user_vdevice_block_erase (
		ptr_vdevice,
		ptr_victim_block->no_bus, 
		ptr_victim_block->no_chip,
		ptr_victim_block->no_block);

	perf_gc_inc_blk_erasures ();

	// reset erased block
	ptr_victim_block->nr_free_pages = ptr_ssd->nr_pages_per_block;
	ptr_victim_block->nr_valid_pages = 0;
	ptr_victim_block->nr_invalid_pages = 0;
	ptr_victim_block->nr_erase_cnt++;
	ptr_victim_block->nr_recent_erase_cnt++;
	ptr_victim_block->last_modified_time = 0;
	ptr_victim_block->is_reserved_block = 1;

	for(loop_page = 0; loop_page < ptr_ssd->nr_pages_per_block; loop_page++) {
		ptr_victim_block->list_pages[loop_page].no_logical_page_addr = -1;
		ptr_victim_block->list_pages[loop_page].page_status = PAGE_STATUS_FREE;
	}

	// (6) the victim block becomes the new gc block
	ptr_pg_mapping->ptr_gc_block = ptr_victim_block;

	// (7) the gc block becomes the new active block
	ptr_pg_mapping->ptr_active_block = ptr_gc_block;
	ptr_gc_block->is_reserved_block = 0;

	// check_max_min_nr_erase_cnt(ptr_ftl_context, ptr_gc_block);

	// if (check_cold_data_migration(ptr_ftl_context))
	// 	cold_data_migration(ptr_ftl_context);

	// if (check_cold_pool_adjustment(ptr_ftl_context))
	// 	cold_pool_adjustment(ptr_ftl_context);

	// if (check_hot_pool_adjustment(ptr_ftl_context))
	// 	hot_pool_adjustment(ptr_ftl_context);


	for(loop_page = 0; loop_page < 1024; loop_page++) {
		update_erase_cnt(loop_page, ptr_ssd->list_buses[0].list_chips[0].list_blocks[loop_page].nr_erase_cnt);
	}

	return ret;
}
