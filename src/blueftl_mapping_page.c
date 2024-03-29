#ifdef KERNEL_MODE

#include <linux/module.h>

#include "blueftl_ftl_base.h"
#include "blueftl_ssdmgmt.h"
#include "bluessd_vdevice.h"
#include "blueftl_mapping_page.h"
#include "blueftl_gc_page.h"
#include "blueftl_util.h"

#else

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "blueftl_ftl_base.h"
#include "blueftl_ssdmgmt.h"
#include "blueftl_user_vdevice.h"
#include "blueftl_mapping_page.h"
#include "blueftl_gc_page.h"
#include "blueftl_util.h"

#endif

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

/* create the page mapping table */
/* 초기값들 모두 할당 및 설정, block table 할당, 초기화 작업 */
struct ftl_context_t *page_mapping_create_ftl_context(
	struct virtual_device_t *ptr_vdevice)
{
	uint32_t init_pg_loop = 0;

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
	/* ptr_mapping = {nr_pg_table_entries, ptr_pg_table} */
	if ((ptr_ftl_context->ptr_mapping = (struct ftl_page_mapping_context_t *)malloc(sizeof(struct ftl_page_mapping_context_t))) == NULL)
	{
		printf("blueftl_mapping_page: the creation of the ftl context failed\n");
		goto error_alloc_ftl_page_mapping_context;
	}

	// init recent page info
	ptr_ftl_context->recent_bus = VIRGIN;
	ptr_ftl_context->recent_chip = VIRGIN;
	ptr_ftl_context->recent_block = VIRGIN;
	ptr_ftl_context->recent_page = VIRGIN;

	/* set virtual device */
	ptr_ftl_context->ptr_vdevice = ptr_vdevice;

	ptr_ssd = ptr_ftl_context->ptr_ssd;
	ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;

	/* TODO: implement page-level FTL */

	ptr_pg_mapping->nr_pg_table_entries =
		ptr_ssd->nr_buses * ptr_ssd->nr_chips_per_bus * ptr_ssd->nr_blocks_per_chip * ptr_ssd->nr_pages_per_block;

	/* allocate ptr_blk_table -> entry 개수만큼 */
	if ((ptr_pg_mapping->ptr_pg_table = (uint32_t *)malloc(ptr_pg_mapping->nr_pg_table_entries * sizeof(uint32_t))) == NULL)
	{
		printf("blueftl_mapping_page: failed to allocate the memory for ptr_mapping_table\n");
		goto error_alloc_mapping_table;
	}

	/* 할당된 ptr_blk_table 모두 free 설정 */
	for (init_pg_loop = 0; init_pg_loop < ptr_pg_mapping->nr_pg_table_entries; init_pg_loop++)
	{
		ptr_pg_mapping->ptr_pg_table[init_pg_loop] = PAGE_TABLE_FREE;
	}

	/* TODO: end */

	return ptr_ftl_context;

error_alloc_mapping_table:
	free(ptr_ftl_context->ptr_mapping);

error_alloc_ftl_page_mapping_context:
	ssdmgmt_destroy_ssd(ptr_ssd);

error_create_ssd_context:
	free(ptr_ftl_context);

error_alloc_ftl_context:
	return NULL;
}

/* destroy the page mapping table */
void page_mapping_destroy_ftl_context(struct ftl_context_t *ptr_ftl_context)
{
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_context_t *ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;

	/* TODO: implement page-level FTL */
	if (ptr_pg_mapping->ptr_pg_table != NULL)
		free(ptr_pg_mapping->ptr_pg_table);
	/* TODO: end */

	/* destroy the page mapping context */
	if (ptr_pg_mapping != NULL)
		free(ptr_pg_mapping);

	/* destroy the ssd context */
	ssdmgmt_destroy_ssd(ptr_ssd);

	/* destory the ftl context */
	if (ptr_ftl_context != NULL)
		free(ptr_ftl_context);
}

/* get a physical page number that was mapped to a logical page number before */
int32_t page_mapping_get_mapped_physical_page_address(
	struct ftl_context_t *ptr_ftl_context,
	uint32_t logical_page_address,
	uint32_t *ptr_bus,
	uint32_t *ptr_chip,
	uint32_t *ptr_block,
	uint32_t *ptr_page)
{
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_context_t *ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;

	uint32_t physical_page_address;
	//   uint32_t page_offset;

	int32_t ret = -1;

	/* get the logical block number and the page offset */
	//logical_block_address = logical_page_address / ptr_ssd->nr_pages_per_block;

	/* obtain the physical page address using the page mapping table */
	physical_page_address = ptr_pg_mapping->ptr_pg_table[logical_page_address];

	if (physical_page_address == PAGE_TABLE_FREE)
	{
		/* the requested logical page is not mapped to any physical page */
		*ptr_bus = *ptr_chip = *ptr_block = *ptr_page = -1;
		ret = -1;
	}
	else
	{
		struct flash_page_t *ptr_erase_page = NULL;

		/* decoding the physical block address */
		ftl_convert_to_ssd_layout(physical_page_address, ptr_bus, ptr_chip, ptr_block, ptr_page);

		ptr_erase_page = &ptr_ssd->list_buses[*ptr_bus].list_chips[*ptr_chip].list_blocks[*ptr_block].list_pages[*ptr_page];
		if (ptr_erase_page->page_status == PAGE_STATUS_FREE)
		{ // page 존재 X
			/* the logical page must be mapped to the corresponding physical page */
			*ptr_bus = *ptr_chip = *ptr_block = *ptr_page = -1;
			ret = -1;
		}
		else
		{
			ret = 0;
		}
	}

	return ret;
}

/* get a free physical page address */
int32_t page_mapping_get_free_physical_page_address(
	struct ftl_context_t *ptr_ftl_context,
	uint32_t logical_page_address,
	uint32_t *ptr_bus,
	uint32_t *ptr_chip,
	uint32_t *ptr_block,
	uint32_t *ptr_page)
{
	struct flash_block_t *ptr_erase_block;

	struct flash_page_t *ptr_erase_page;
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_context_t *ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;

	uint32_t physical_page_address;
	int bus_num, chip_num, block_num, page_num;
	
	/* obtain the physical block address using the block mapping table */
	physical_page_address = ptr_pg_mapping->ptr_pg_table[logical_page_address];

	if (physical_page_address != PAGE_TABLE_FREE)
	{
		// mw: 요청이 들어온 lpa에 매핑된 ppa가 있다. -> free page 갖다줘야함.
		/* encoding the ssd layout to a phyical block address 
         NOTE: page must be 0 in the block-level mapping */
		ftl_convert_to_ssd_layout(physical_page_address, ptr_bus, ptr_chip, ptr_block, ptr_page);

		/* see if the logical page can be written to the physical block found */
		ptr_erase_block = &ptr_ssd->list_buses[*ptr_bus].list_chips[*ptr_chip].list_blocks[*ptr_block];
		ptr_erase_page = &ptr_ssd->list_buses[*ptr_bus].list_chips[*ptr_chip].list_blocks[*ptr_block].list_pages[*ptr_page];

		// conflict page invalid
		if (ptr_erase_page->page_status != PAGE_STATUS_INVALID)
		{
			ptr_erase_page->page_status = PAGE_STATUS_INVALID;
			ptr_erase_block->nr_valid_pages--;
			ptr_erase_block->nr_invalid_pages++;
			// ptr_erase_block->last_modified_time 추후 gc 용도?
		}

		if (ptr_ftl_context->recent_bus == VIRGIN && ptr_ftl_context->recent_chip == VIRGIN && ptr_ftl_context->recent_block == VIRGIN) // 처음 쓰는 경우
		{
			// 기존에 쓰던 블락이 없거나 꽉 찼기 때문에, 이 경우 새 free block의 첫 번째 페이지를 갖다 줘야 함
			ptr_erase_block = ssdmgmt_get_free_block(ptr_ssd, 0, 0); // 해당 칩 안에서 free block 찾음
			if (ptr_erase_block != NULL) // free block 발견
			{
				*ptr_bus = ptr_erase_block->no_bus;
				*ptr_chip = ptr_erase_block->no_chip;
				*ptr_block = ptr_erase_block->no_block;
				*ptr_page = 0; // 프리 블럭의 첫 페이지
			}
			else
				goto need_gc;
		}
		else if (ptr_ssd->list_buses[ptr_ftl_context->recent_bus].list_chips[ptr_ftl_context->recent_chip].list_blocks[ptr_ftl_context->recent_block].nr_free_pages == 0) {
			// 기존에 쓰던 블락이 없거나 꽉 찼기 때문에, 이 경우 새 free block의 첫 번째 페이지를 갖다 줘야 함
			ptr_erase_block = ssdmgmt_get_free_block(ptr_ssd, 0, 0); // 해당 칩 안에서 free block 찾음
			if (ptr_erase_block != NULL) // free block 발견
			{
				*ptr_bus = ptr_erase_block->no_bus;
				*ptr_chip = ptr_erase_block->no_chip;
				*ptr_block = ptr_erase_block->no_block;
				*ptr_page = 0; // 프리 블럭의 첫 페이지
			}
			else
				goto need_gc;
		}
		else // 이전에 쓰던 블락에 자리가 남은 경우
		{
			*ptr_bus = ptr_ftl_context->recent_bus;
			*ptr_chip = ptr_ftl_context->recent_chip;
			*ptr_block = ptr_ftl_context->recent_block;
			*ptr_page = ptr_ftl_context->recent_page + 1; // 다음 페이지
		}
	}
	else
	{
		// mw: 요청이 들어온 lpa에 매핑된 ppa가 없다. -> free page 갖다줘야함. 기존의 erase_ 어쩌구 하는게 free page나 block을 의미하는 듯
		if (ptr_ftl_context->recent_bus == VIRGIN && ptr_ftl_context->recent_chip == VIRGIN && ptr_ftl_context->recent_block == VIRGIN) // 처음 쓰는 경우
		{
			// 기존에 쓰던 블락이 없거나 꽉 찼기 때문에, 이 경우 새 free block의 첫 번째 페이지를 갖다 줘야 함
			ptr_erase_block = ssdmgmt_get_free_block(ptr_ssd, 0, 0); // 해당 칩 안에서 free block 찾음
			if (ptr_erase_block != NULL) // free block 발견
			{
				*ptr_bus = ptr_erase_block->no_bus;
				*ptr_chip = ptr_erase_block->no_chip;
				*ptr_block = ptr_erase_block->no_block;
				*ptr_page = 0; // 프리 블럭의 첫 페이지
			}
			else
				goto need_gc;
		}
		else if (ptr_ssd->list_buses[ptr_ftl_context->recent_bus].list_chips[ptr_ftl_context->recent_chip].list_blocks[ptr_ftl_context->recent_block].nr_free_pages == 0) {
			// 기존에 쓰던 블락이 없거나 꽉 찼기 때문에, 이 경우 새 free block의 첫 번째 페이지를 갖다 줘야 함
			ptr_erase_block = ssdmgmt_get_free_block(ptr_ssd, 0, 0); // 해당 칩 안에서 free block 찾음
			if (ptr_erase_block != NULL) // free block 발견
			{
				*ptr_bus = ptr_erase_block->no_bus;
				*ptr_chip = ptr_erase_block->no_chip;
				*ptr_block = ptr_erase_block->no_block;
				*ptr_page = 0; // 프리 블럭의 첫 페이지
			}
			else 
				goto need_gc;
		}
		else // 이전에 쓰던 블락에 자리가 남은 경우
		{
			*ptr_bus = ptr_ftl_context->recent_bus;
			*ptr_chip = ptr_ftl_context->recent_chip;
			*ptr_block = ptr_ftl_context->recent_block;
			*ptr_page = ptr_ftl_context->recent_page + 1; // 다음 페이지
		}
	}

	ptr_ftl_context->recent_bus = *ptr_bus;
	ptr_ftl_context->recent_chip = *ptr_chip;
	ptr_ftl_context->recent_block = *ptr_block;
	ptr_ftl_context->recent_page = *ptr_page;

	return 0;

need_gc:
	for (bus_num = 0; bus_num < ptr_ssd->nr_buses; bus_num++)
	{
		for (chip_num = 0; chip_num < ptr_ssd->nr_chips_per_bus; chip_num++)
		{
			for (block_num = 0; block_num < ptr_ssd->nr_blocks_per_chip; block_num++)
			{
				for (page_num = 0; page_num < ptr_ssd->nr_pages_per_block; page_num++)
				{
					if (ptr_ssd->list_buses[bus_num].list_chips[chip_num].list_blocks[block_num].list_pages[page_num].page_status == PAGE_STATUS_FREE)
					{
						*ptr_bus = bus_num;
						*ptr_chip = chip_num;
						*ptr_block = block_num;
						*ptr_page = page_num;

						return 0;
					}
				}
			}
		}
	} // 모든 page에 대해 free page 검사

	*ptr_bus = *ptr_chip = *ptr_block = *ptr_page = 0; // bus, chip, block, page 값 초기화
	return -1;
}

/* map a logical page address to a physical page address */
int32_t page_mapping_map_logical_to_physical(
	struct ftl_context_t *ptr_ftl_context,
	uint32_t logical_page_address,
	uint32_t bus,
	uint32_t chip,
	uint32_t block,
	uint32_t page)
{
	struct flash_block_t *ptr_erase_block;
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_context_t *ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;

	uint32_t physical_page_address;

	int32_t ret = -1;

	/* get the physical page address using the page mapping table */
	physical_page_address = ptr_pg_mapping->ptr_pg_table[logical_page_address];

	/* see if the given logical page is alreay mapped or not */
	if (physical_page_address == PAGE_TABLE_FREE)
	{
		physical_page_address = ftl_convert_to_physical_page_address(bus, chip, block, page);

		/* update the page mapping table */
		ptr_pg_mapping->ptr_pg_table[logical_page_address] = physical_page_address;
	}

	/* see if the given page is already written or not */
	ptr_erase_block = &ptr_ssd->list_buses[bus].list_chips[chip].list_blocks[block];
	if (ptr_erase_block->list_pages[page].page_status == PAGE_STATUS_FREE)
	{
		/* make it valid */
		ptr_erase_block->list_pages[page].page_status = PAGE_STATUS_VALID;

		/* increase the number of valid pages while decreasing the number of free pages */
		ptr_erase_block->nr_valid_pages++;
		ptr_erase_block->nr_free_pages--;

		physical_page_address = ftl_convert_to_physical_page_address(bus, chip, block, page);
		ptr_pg_mapping->ptr_pg_table[logical_page_address] = physical_page_address;

		ret = 0;

		// recent time 기록 비교 how?
		ptr_erase_block->last_modified_time = timer_get_timestamp_in_us();
	//	printf("time : %d\n",  ptr_erase_block->last_modified_time);
	}
	else
	{
		/*hmmm... the physical page that corresponds to the logical page must be free */
		printf("blueftl_mapping_page: the physical page address is already used (%u:%u,%u,%u,%u)\n",
			   physical_page_address, bus, chip, block, page);
		ret = -1;
	}

	return ret;
}