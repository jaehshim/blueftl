#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "blueftl_ftl_base.h"
#include "blueftl_ssdmgmt.h"
#include "blueftl_user_vdevice.h"
#include "blueftl_mapping_page.h"
#include "blueftl_gc_page.h"
#include "blueftl_util.h"
#include "blueftl_compress_rw.h"

struct ftl_base_t ftl_base_page_mapping_lab = {
	.ftl_create_ftl_context = page_mapping_create_ftl_context,
	.ftl_destroy_ftl_context = page_mapping_destroy_ftl_context,
	.ftl_get_mapped_physical_page_address = page_mapping_get_mapped_physical_page_address,
	.ftl_get_free_physical_page_address = page_mapping_get_free_physical_page_address,
	.ftl_map_logical_to_physical = page_mapping_map_logical_to_physical,
	.ftl_trigger_gc = gc_page_trigger_gc_lab,
	.ftl_trigger_merge = NULL,
	.ftl_trigger_wear_leveler = NULL,
};

uint32_t init_page_mapping_table(struct ftl_context_t *ptr_ftl_context)
{
	uint32_t init_loop = 0;
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_table_t *ptr_mapping_table =
		((struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping)->ptr_mapping_table;

	/* calculate the number of entries for the mapping table */
	ptr_mapping_table->nr_page_table_entries =
		ptr_ssd->nr_buses * ptr_ssd->nr_chips_per_bus * ptr_ssd->nr_blocks_per_chip * ptr_ssd->nr_pages_per_block;

	/* allocate the memory for the page mapping table */
	if ((ptr_mapping_table->ptr_pg_table = (uint32_t *)malloc(ptr_mapping_table->nr_page_table_entries * sizeof(uint32_t))) == NULL)
	{
		printf("blueftl_mapping_page: failed to allocate the memory for ptr_mapping_table\n");
		goto error_alloc_mapping_table;
	}

	/* init the page mapping table  */
	for (init_loop = 0; init_loop < ptr_mapping_table->nr_page_table_entries; init_loop++)
	{
		ptr_mapping_table->ptr_pg_table[init_loop] = PAGE_TABLE_FREE;
	}

	return 0;

error_alloc_mapping_table:
	printf("blueftl_mapping_page: error occurs when allocating the memory for the mapping table\n");
	return -1;
}

uint32_t init_gc_blocks(struct ftl_context_t *ptr_ftl_context)
{
	uint32_t loop_chip, loop_bus;

	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_context_t *ptr_pg_mapping =
		(struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;
	struct flash_block_t *ptr_erase_block;

	for (loop_bus = 0; loop_bus < ptr_ssd->nr_buses; loop_bus++)
	{
		for (loop_chip = 0; loop_chip < ptr_ssd->nr_chips_per_bus; loop_chip++)
		{
			if ((ptr_erase_block = ssdmgmt_get_free_block(ptr_ssd, loop_bus, loop_chip)) != NULL)
			{
				ptr_pg_mapping->ptr_gc_block = ptr_erase_block;
				ptr_erase_block->is_reserved_block = 1;
			}
			else
			{
				// OOPS!!! ftl needs a garbage collection.
				printf("blueftl_mapping_page: there is no free block that will be used for a gc block\n");
			}
		}
	}

	return 0;
}

uint32_t init_active_blocks(struct ftl_context_t *ptr_ftl_context)
{
	uint32_t loop_bus, loop_chip;
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_context_t *ptr_pg_mapping =
		(struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;
	struct flash_block_t *ptr_erase_block;

	for (loop_bus = 0; loop_bus < ptr_ssd->nr_buses; loop_bus++)
	{
		for (loop_chip = 0; loop_chip < ptr_ssd->nr_chips_per_bus; loop_chip++)
		{
			if ((ptr_erase_block = ssdmgmt_get_free_block(ptr_ssd, loop_bus, loop_chip)))
			{
				ptr_pg_mapping->ptr_active_block = ptr_erase_block;
			}
			else
			{
				// OOPS!!! ftl needs a garbage collection.
				printf("blueftl_mapping_page: there is no free block that will be used for an active block\n");
			}
		}
	}

	return 0;
}

uint32_t init_ru_blocks(struct ftl_context_t *ptr_ftl_context)
{
	uint32_t loop_bus;
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_context_t *ptr_pg_mapping =
		(struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;

	for (loop_bus = 0; loop_bus < ptr_ssd->nr_buses; loop_bus++)
	{
		ptr_pg_mapping->ptr_ru_chips[loop_bus] = ptr_ssd->nr_chips_per_bus - 1;
	}
	ptr_pg_mapping->ru_bus = ptr_ssd->nr_buses - 1;

	return 0;
}

struct ftl_context_t *page_mapping_create_ftl_context(struct virtual_device_t *ptr_vdevice)
{
	struct ftl_context_t *ptr_ftl_context = NULL;
	struct flash_ssd_t *ptr_ssd = NULL;
	struct ftl_page_mapping_context_t *ptr_pg_mapping = NULL;

	/* create the ftl context */
	if ((ptr_ftl_context = (struct ftl_context_t *)malloc(sizeof(struct ftl_context_t))) == NULL)
	{
		printf("blueftl_mapping_page: the creation of the ftl context failed\n");
		goto error_alloc_ftl_context;
	}

	/* create the ssd context */
	if ((ptr_ftl_context->ptr_ssd = ssdmgmt_create_ssd(ptr_vdevice)) == NULL)
	{
		printf("blueftl_mapping_page: the creation of the ftl context failed\n");
		goto error_create_ssd_context;
	}

	/* create the page mapping context */
	if ((ptr_ftl_context->ptr_mapping = (struct ftl_page_mapping_context_t *)malloc(sizeof(struct ftl_page_mapping_context_t))) == NULL)
	{
		printf("blueftl_mapping_page: the creation of the ftl context failed\n");
		goto error_alloc_ftl_page_mapping_context;
	}

	ptr_ssd = (struct flash_ssd_t *)ptr_ftl_context->ptr_ssd;
	ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;

	/* allocate the memory for ptr_mapping_table */
	if ((ptr_pg_mapping->ptr_mapping_table = (struct ftl_page_mapping_table_t *)malloc(sizeof(struct ftl_page_mapping_table_t))) == NULL)
	{
		printf("blueftl_mapping_page: the memory allocation for ptr_mapping_table failed\n");
		goto error_alloc_mapping_table_info;
	}
	init_page_mapping_table(ptr_ftl_context);

	init_gc_blocks(ptr_ftl_context);
	
	init_active_blocks(ptr_ftl_context);

	/* allocate the memory for the ru blocks */
	if ((ptr_pg_mapping->ptr_ru_chips =
			 (uint32_t *)malloc(sizeof(uint32_t) * ptr_ssd->nr_buses)) == NULL)
	{
		printf("blueftl_mapping_page: the memory allocation of the ru blocks failed\n");
	}
	init_ru_blocks(ptr_ftl_context);

	init_chunk_table(ptr_ftl_context);

	/* set virtual device */
	ptr_ftl_context->ptr_vdevice = ptr_vdevice;

	return ptr_ftl_context;

error_alloc_gc_block:
	free(ptr_pg_mapping->ptr_mapping_table);

error_alloc_mapping_table_info:
	free(ptr_pg_mapping);

error_alloc_ftl_page_mapping_context:
	ssdmgmt_destroy_ssd(ptr_ssd);

error_create_ssd_context:
	free(ptr_ftl_context);

error_alloc_ftl_context:
	return NULL;
}

void page_mapping_destroy_ftl_context(struct ftl_context_t *ptr_ftl_context)
{
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_context_t *ptr_pg_mapping =
		(struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;

	/* destroy ru blocks */
	if (ptr_pg_mapping->ptr_ru_chips != NULL)
	{
		free(ptr_pg_mapping->ptr_ru_chips);
	}

	/* destroy the page mapping table */
	if (ptr_pg_mapping->ptr_mapping_table != NULL && ptr_pg_mapping->ptr_mapping_table->ptr_pg_table != NULL)
	{
		free(ptr_pg_mapping->ptr_mapping_table->ptr_pg_table);
	}

	if (ptr_pg_mapping->ptr_mapping_table != NULL)
	{
		free(ptr_pg_mapping->ptr_mapping_table);
	}

	/* destroy the ftl context */
	ssdmgmt_destroy_ssd(ptr_ssd);

	if (ptr_pg_mapping != NULL)
	{
		free(ptr_pg_mapping);
	}

	if (ptr_ftl_context != NULL)
	{
		free(ptr_ftl_context);
	}
}

// functions for managing mapping table
uint32_t get_new_free_page_addr(struct ftl_context_t *ptr_ftl_context)
{
	uint32_t curr_bus, curr_chip;
	uint32_t curr_block, curr_page;
	uint32_t curr_physical_page_addr;

	struct flash_block_t *ptr_active_block = NULL;
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_context_t *ptr_pg_mapping =
		(struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;

	// (1) choose a bus to be written
	if (ptr_pg_mapping->ru_bus == (ptr_ssd->nr_buses - 1))
	{
		ptr_pg_mapping->ru_bus = 0;
	}
	else
	{
		ptr_pg_mapping->ru_bus++;
	}
	curr_bus = ptr_pg_mapping->ru_bus;

	// (2) choose a chip to be written
	if (ptr_pg_mapping->ptr_ru_chips[curr_bus] == (ptr_ssd->nr_chips_per_bus - 1))
	{
		ptr_pg_mapping->ptr_ru_chips[curr_bus] = 0;
	}
	else
	{
		ptr_pg_mapping->ptr_ru_chips[curr_bus]++;
	}
	curr_chip = ptr_pg_mapping->ptr_ru_chips[curr_bus];

	// (3) see if the bus number calculated is different from the real bus number
	ptr_active_block = ptr_pg_mapping->ptr_active_block;

	if (ptr_active_block->no_bus != curr_bus || ptr_active_block->no_chip != curr_chip)
	{
		printf("blueftl_mapping_page: the bus number for the active block is not same to the current bus number\n");
		return -1;
	}
	// (4) see if there are free pages to be used in the block
	if (ptr_active_block->nr_free_pages == 0)
	{
		// there is no free page in the block, and therefore it it necessary to allocate a new log block for this bus
		ptr_active_block = ptr_pg_mapping->ptr_active_block = ssdmgmt_get_free_block(ptr_ssd, curr_bus, curr_chip);

		if (ptr_active_block != NULL)
		{
			if (ptr_active_block->no_bus != curr_bus)
			{
				printf("blueftl_mapping_page: bus number or free block number is incorrect (%d %d)\n", ptr_active_block->no_bus, ptr_active_block->nr_free_pages);
				return -1;
			}
		}
		else
		{
			// oops! there is no free block in the bus. Hence we need a garbage collection.
			return -1;
		}
	}

	// (5) now we can know that target block and page
	curr_block = ptr_active_block->no_block;
	curr_page = ptr_ssd->nr_pages_per_block - ptr_active_block->nr_free_pages;

	// (3) get the physical page address from the layout information
	curr_physical_page_addr = ftl_convert_to_physical_page_address(curr_bus, curr_chip, curr_block, curr_page);

	return curr_physical_page_addr;
}

int8_t is_valid_address_range(struct ftl_context_t *ptr_ftl_context, uint32_t logical_page_address)
{
	struct ftl_page_mapping_table_t *ptr_mapping_table =
		((struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping)->ptr_mapping_table;

	if (logical_page_address >= ptr_mapping_table->nr_page_table_entries)
	{
		return -1;
	}
	else
	{
		return 1;
	}
}

int32_t map_logical_to_physical(struct ftl_context_t *ptr_ftl_context, uint32_t logical_page_address, uint32_t physical_page_address)
{
	struct flash_block_t *ptr_erase_block;
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_table_t *ptr_mapping_table =
		((struct ftl_page_mapping_context_t *)(ptr_ftl_context->ptr_mapping))->ptr_mapping_table;

	uint32_t curr_bus, curr_chip, curr_block, curr_page;
	uint32_t previous_physical_page_address;

	// (1) see if the given addresses are valid or not
	if (is_valid_address_range(ptr_ftl_context, logical_page_address) != 1)
	{
		printf("blueftl_mapping_page: invalid logical page range (%d)\n", logical_page_address);
		exit(1);
		return -1;
	}
	// (2) change the status of the previous block.
	// get the previous physical page address from the page mapping table
	// previous_physical_page_address = ptr_mapping_table->ptr_pg_table[logical_page_address];

	// make the previous page invalid
	// if (previous_physical_page_address != PAGE_TABLE_FREE)
	// {
	// 	/* get the physical layout from the physical page address */
	// 	ftl_convert_to_ssd_layout(previous_physical_page_address, &curr_bus, &curr_chip, &curr_block, &curr_page);

	// 	/* get the erase block from the block mapping table */
	// 	ptr_erase_block = &(ptr_ssd->list_buses[curr_bus].list_chips[curr_chip].list_blocks[curr_block]);

	// 	/* update the last modified time */
	// 	ptr_erase_block->last_modified_time = timer_get_timestamp_in_sec();

	// 	/* increase the number of invalid pages while reducing the number of valid pages */
	// 	if (ptr_erase_block->nr_valid_pages > 0)
	// 	{
	// 	//	ptr_erase_block->nr_invalid_pages++;
	// 	//	ptr_erase_block->nr_valid_pages--;

	// 		// if (ptr_erase_block->list_pages[curr_page].page_status != PAGE_STATUS_VALID)
	// 		// {
	// 		// 	printf("blueftl_mapping_page: the previous page should be a valid status\n");
	// 		// 	return -1;
	// 		// }

	// 		ptr_erase_block->list_pages[curr_page].page_status = PAGE_STATUS_INVALID;
	// 	}
	// 	else
	// 	{
	// 		printf("blueftl_mapping_page: nr_valid_pages is zero\n");
	// 		return -1;
	// 	}

	// 	/* see if the number of pages is correct */
	// 	if ((ptr_erase_block->nr_free_pages + ptr_erase_block->nr_valid_pages + ptr_erase_block->nr_invalid_pages) != ptr_ssd->nr_pages_per_block)
	// 	{
	// 		printf("blueftl_mapping_page: free:%u + valid:%u + invalid:%u is not %u\n",
	// 			   ptr_erase_block->nr_free_pages, ptr_erase_block->nr_valid_pages, ptr_erase_block->nr_invalid_pages, ptr_ssd->nr_pages_per_block);
	// 		return -1;
	// 	}
	// }
	// (3) change the status of the given block
	ftl_convert_to_ssd_layout(physical_page_address, &curr_bus, &curr_chip, &curr_block, &curr_page);

	if ((ptr_erase_block = &(ptr_ssd->list_buses[curr_bus].list_chips[curr_chip].list_blocks[curr_block])) == NULL)
	{
		printf("blueftl_mapping_page: the target block is NULL (bus: %u chip: %u, block: %u, page: %u)\n",
			   curr_bus, curr_chip, curr_block, curr_page);
		return -1;
	}

	if (ptr_erase_block->nr_free_pages > 0)
	{
		// ptr_erase_block->nr_valid_pages++;
		// ptr_erase_block->nr_free_pages--;

		/* in memory compression -> ppa 겹칠 수 있음 */
		// if (ptr_erase_block->list_pages[curr_page].page_status != PAGE_STATUS_FREE)
		// {
		// 	// the given page should be free
		// 	printf("blueftl_mapping_page: the given page should be free (%u) (%u)\n", ptr_erase_block->list_pages[curr_page].page_status, curr_page);
		// 	return -1;
		// }

		ptr_erase_block->list_pages[curr_page].page_status = PAGE_STATUS_VALID;
		ptr_erase_block->list_pages[curr_page].no_logical_page_addr = logical_page_address;

		ptr_erase_block->last_modified_time = timer_get_timestamp_in_sec();
	}
	else
	{
		printf("blueftl_mapping_page: nr_free_page is zero before writing the data into flash memory\n");
		return -1;
	}


	// CHECK: see if the sum of different types of pages is the same as the number of pages in a block
	// if ((ptr_erase_block->nr_free_pages + ptr_erase_block->nr_valid_pages + ptr_erase_block->nr_invalid_pages) != ptr_ssd->nr_pages_per_block)
	// {
	// 	printf("blueftl_mapping_page: free:%u + valid:%u + invalid:%u is not %u\n",
	// 		   ptr_erase_block->nr_free_pages, ptr_erase_block->nr_valid_pages, ptr_erase_block->nr_invalid_pages, ptr_ssd->nr_pages_per_block);
	// 	return -1;
	// }

	// (4) map the given logical page address to the physical page address
	ptr_mapping_table->ptr_pg_table[logical_page_address] = physical_page_address;

	return 0;
}

int32_t page_mapping_get_mapped_physical_page_address(
	struct ftl_context_t *ptr_ftl_context,
	uint32_t logical_page_address,
	uint32_t *ptr_bus,
	uint32_t *ptr_chip,
	uint32_t *ptr_block,
	uint32_t *ptr_page)
{
	int32_t ret = -1;
	struct ftl_page_mapping_context_t *ptr_pg_mapping;
	uint32_t physical_page_address;

	ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;

	/* see if the given addresses are valid or not */
	if (is_valid_address_range(ptr_ftl_context, logical_page_address) != 1)
	{
		printf("blueftl_mapping_page: invalid logical page range (%d)\n", logical_page_address);
		return -1;
	}

	/* obtain the physical address that corresponds to the given logical page */
	physical_page_address = ptr_pg_mapping->ptr_mapping_table->ptr_pg_table[logical_page_address];

	if (physical_page_address == PAGE_TABLE_FREE)
	{
		/* the requested page is a free page; it is not mapped to any logical page 
		   NOTE: this is not an error case because file system attempts to access the data that were not written before.
		 */
		//printf("[NOT A ERROR] blueftl_mapping_page: the requested page is a free page; it is not mapped to any logical page\n");
		ret = -1;
	}
	else
	{
		/* ok. no problem */
		ftl_convert_to_ssd_layout(physical_page_address, ptr_bus, ptr_chip, ptr_block, ptr_page);
		ret = 0;
	}

	return ret;
}

int32_t page_mapping_get_free_physical_page_address(
	struct ftl_context_t *ptr_ftl_context,
	uint32_t logical_page_address,
	uint32_t *ptr_bus,
	uint32_t *ptr_chip,
	uint32_t *ptr_block,
	uint32_t *ptr_page)
{
	int32_t ret = -1;
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_context_t *ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;
	uint32_t physical_page_address;

	if ((physical_page_address = get_new_free_page_addr(ptr_ftl_context)) == -1)
	{
		/* get the target bus and chip for garbage collection */
		*ptr_bus = ptr_pg_mapping->ru_bus;
		*ptr_chip = ptr_pg_mapping->ptr_ru_chips[ptr_pg_mapping->ru_bus];
		*ptr_block = -1;
		*ptr_page = -1;

		/* getting a new free page faild, so we move to the previous bus */
		if (ptr_pg_mapping->ptr_ru_chips[ptr_pg_mapping->ru_bus] == 0)
		{
			ptr_pg_mapping->ptr_ru_chips[ptr_pg_mapping->ru_bus] = ptr_ssd->nr_chips_per_bus - 1;
		}
		else
		{
			ptr_pg_mapping->ptr_ru_chips[ptr_pg_mapping->ru_bus]--;
		}

		if (ptr_pg_mapping->ru_bus == 0)
		{
			ptr_pg_mapping->ru_bus = ptr_ssd->nr_buses - 1;
		}
		else
		{
			ptr_pg_mapping->ru_bus--;
		}

		/* now, it is time to reclaim garbage */
		ret = -1;
	}
	else
	{
		/* decoding the physical page address */
		ftl_convert_to_ssd_layout(physical_page_address, ptr_bus, ptr_chip, ptr_block, ptr_page);

		ret = 0;
	}

	return ret;
}

int32_t page_mapping_map_logical_to_physical(
	struct ftl_context_t *ptr_ftl_context,
	uint32_t logical_page_address,
	uint32_t bus,
	uint32_t chip,
	uint32_t block,
	uint32_t page)
{
	uint32_t physical_page_address = ftl_convert_to_physical_page_address(bus, chip, block, page);
	return map_logical_to_physical(ptr_ftl_context, logical_page_address, physical_page_address);
}
