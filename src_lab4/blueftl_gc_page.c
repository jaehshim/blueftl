#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "blueftl_ftl_base.h"
#include "blueftl_mapping_page.h"
#include "blueftl_gc_page.h"
#include "blueftl_util.h"
#include "blueftl_user_vdevice.h"
#include "blueftl_wl_dual_pool.h"
#include "blueftl_compress_rw.h"
#include "lzrw3.h"

int32_t lpa_arr[64 * CHUNK_SIZE];
uint8_t buff[FLASH_PAGE_SIZE * CHUNK_SIZE * 64];

uint8_t *write_buff;
uint8_t *compress_buff;

int write_cnt = 0; //write 요청 들어온 lpa 개수 count

struct flash_block_t *gc_page_select_victim_greedy(
	struct ftl_context_t *ptr_ftl_context,
	int32_t gc_target_bus,
	int32_t gc_target_chip)
{
	struct flash_block_t *ptr_victim_block = NULL;
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;

	uint32_t nr_max_invalid_pages = 0;
	uint32_t nr_cur_invalid_pages;
	uint32_t loop_block;

	// for (loop_block = 0; loop_block < ptr_ssd->nr_blocks_per_chip; loop_block++)
	// {
	// 	nr_cur_invalid_pages =
	// 		ptr_ssd->list_buses[gc_target_bus].list_chips[gc_target_chip].list_blocks[loop_block].nr_invalid_pages;

	// 	if (nr_max_invalid_pages < nr_cur_invalid_pages)
	// 	{
	// 		nr_max_invalid_pages = nr_cur_invalid_pages;
	// 		ptr_victim_block = &(ptr_ssd->list_buses[gc_target_bus].list_chips[gc_target_chip].list_blocks[loop_block]);
	// 	}
	// }

	srand(time(NULL));
	loop_block = rand() % ptr_ssd->nr_blocks_per_chip;
	/* Avoid free block to be selected as victim */
	while (ptr_ssd->list_buses[0].list_chips[0].list_blocks[loop_block].nr_invalid_pages == 0)
	{
		loop_block = rand() % ptr_ssd->nr_blocks_per_chip;
	}
	ptr_victim_block = &ptr_ssd->list_buses[0].list_chips[0].list_blocks[loop_block];
	/* TODO: need a way to handle such a case more nicely */
	if (ptr_victim_block == NULL)
	{
		printf("blueftl_gc_page: oops! 'ptr_victim_block' is NULL.\nBlueFTL will die.\n");
	}

	return ptr_victim_block;
}

int32_t gc_page_trigger_gc_lab(
	struct ftl_context_t *ptr_ftl_context,
	int32_t gc_target_bus,
	int32_t gc_target_chip)
{
	int ret = -1;
	uint32_t loop_page = 0;
	uint32_t loop_page_victim = 0;
	uint32_t loop_page_gc = 0;
	uint32_t logical_page_address;
	uint32_t bus, chip, block, page;

	struct flash_block_t *ptr_gc_block = NULL;
	struct flash_block_t *ptr_victim_block = NULL;
	uint8_t *temp_buff1 = NULL;
	uint8_t *temp_buff2 = NULL;
	int32_t header_data = 0;
	uint32_t mapped_ppa;
	int i;

	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_context_t *ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;
	struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;
	struct chunk_table_t *ptr_chunk_table = (struct chunk_table_t *)ptr_ftl_context->ptr_chunk_table;

	write_cnt = 0;

	// (1) get the victim block using the greedy policy
	if ((ptr_victim_block = gc_page_select_victim_greedy(ptr_ftl_context, gc_target_bus, gc_target_chip)) == NULL)
	{
		printf("blueftl_gc: victim block is not selected\n");
		return ret;
	}

	if (ptr_victim_block->nr_invalid_pages == 0)
	{
		printf("[ERROR] the victim block at least has 1 invalid page.\n");
		return ret;
	}

	// (2) get the free block reserved for the garbage collection
	ptr_gc_block = ptr_pg_mapping->ptr_gc_block;

	ret = 0;

	// (4) copy flash pages from the victim block to the gc block
	loop_page_victim = 0;
	while (loop_page_victim < ptr_ssd->nr_pages_per_block)
	{
		uint32_t physical_page_address = ftl_convert_to_physical_page_address(gc_target_bus, gc_target_chip, ptr_victim_block->no_block, loop_page_victim);
		uint32_t vnum = ptr_chunk_table->ptr_ch_table[physical_page_address / 32].valid_page_count;
		uint32_t ppnum = ptr_chunk_table->ptr_ch_table[physical_page_address / 32].nr_physical_pages;
		uint32_t comp_flag = ptr_chunk_table->ptr_ch_table[physical_page_address / 32].compress_indicator;

		if (comp_flag != COMP_INIT)
		{
			if (vnum != 0)
			{ // valid page 존재
				temp_buff1 = (uint8_t *)malloc(FLASH_PAGE_SIZE * 10);
				temp_buff2 = (uint8_t *)malloc(FLASH_PAGE_SIZE * 10);

				for (i = 0; i < ppnum; i++)
				{
					blueftl_user_vdevice_page_read(
						ptr_vdevice,
						gc_target_bus, gc_target_chip, ptr_victim_block->no_block, loop_page_victim,
						FLASH_PAGE_SIZE,
						(char *)temp_buff1 + (i * FLASH_PAGE_SIZE));
				}

				if (comp_flag)
				{
					decompress(temp_buff1, FLASH_PAGE_SIZE * ppnum, temp_buff2); //압축 해제

					for (i = 0; i < CHUNK_SIZE; i++) // header의 배열 탐색해서 맞는 page의 index 찾기 & 데이터 얻기
					{
						memcpy(&header_data, temp_buff2 + i * sizeof(int32_t), sizeof(int32_t));
						if (page_mapping_get_mapped_physical_page_address(ptr_ftl_context, header_data, &bus, &chip, &block, &page) == -1)
						{
							printf("page_mapping_get_mapped_physical_page_address ERROR\n");
						}
						mapped_ppa = ftl_convert_to_physical_page_address(bus, chip, block, page);
						if (mapped_ppa == physical_page_address)
						{ // valid data
							memcpy(&buff[FLASH_PAGE_SIZE * write_cnt], temp_buff2 + 4 * sizeof(int32_t) + FLASH_PAGE_SIZE * i, FLASH_PAGE_SIZE);
							lpa_arr[write_cnt] = header_data;
							write_cnt++;
						}
					}
				}
				else
				{
					logical_page_address = ptr_victim_block->list_pages[loop_page_victim].no_logical_page_addr;
					if (page_mapping_get_mapped_physical_page_address(ptr_ftl_context, logical_page_address, &bus, &chip, &block, &page) == -1)
					{
						printf("page_mapping_get_mapped_physical_page_address ERROR\n");
					}
					mapped_ppa = ftl_convert_to_physical_page_address(bus, chip, block, page);
					if (mapped_ppa == physical_page_address)
					{ // valid data
						memcpy(&buff[FLASH_PAGE_SIZE * write_cnt], temp_buff2 + 4 * sizeof(int32_t) + FLASH_PAGE_SIZE * i, FLASH_PAGE_SIZE);
						lpa_arr[write_cnt] = header_data;
						write_cnt++;
					}
				}
			}
			for (i = 0; i < ppnum; i++)
			{
				ptr_chunk_table->ptr_ch_table[(physical_page_address / 32) + i].valid_page_count = 0;
				ptr_chunk_table->ptr_ch_table[(physical_page_address / 32) + i].nr_physical_pages = 0;
				ptr_chunk_table->ptr_ch_table[(physical_page_address / 32) + i].compress_indicator = COMP_INIT;
			}
		}
		loop_page_victim += ppnum;
	} // block안의 valid pages 모두 buffer로 옮김

	// (5) erase the victim block
	blueftl_user_vdevice_block_erase(
		ptr_vdevice,
		ptr_victim_block->no_bus,
		ptr_victim_block->no_chip,
		ptr_victim_block->no_block);

	perf_gc_inc_blk_erasures();
	
	printf("write cnt : %d\n", write_cnt);
	blueftl_gc_write(ptr_ftl_context, ptr_gc_block);

	// reset erased block
	ptr_victim_block->nr_free_pages = ptr_ssd->nr_pages_per_block;
	ptr_victim_block->nr_valid_pages = 0;
	ptr_victim_block->nr_invalid_pages = 0;
	ptr_victim_block->nr_erase_cnt++;
	ptr_victim_block->nr_recent_erase_cnt++;
	ptr_victim_block->last_modified_time = 0;
	ptr_victim_block->is_reserved_block = 1;

	for (loop_page = 0; loop_page < ptr_ssd->nr_pages_per_block; loop_page++)
	{
		ptr_victim_block->list_pages[loop_page].no_logical_page_addr = -1;
		ptr_victim_block->list_pages[loop_page].page_status = PAGE_STATUS_FREE;
	}

	// (6) the victim block becomes the new gc block
	ptr_pg_mapping->ptr_gc_block = ptr_victim_block;

	// (7) the gc block becomes the new active block
	ptr_pg_mapping->ptr_active_block = ptr_gc_block;
	ptr_gc_block->is_reserved_block = 0;

	return ret;
}

void blueftl_gc_write(
	struct ftl_context_t *ptr_ftl_context,
	struct flash_block_t *ptr_gc_block)
{
	struct chunk_table_t *ptr_chunk_table = (struct chunk_table_t *)ptr_ftl_context->ptr_chunk_table;
	struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
	struct ftl_page_mapping_context_t *ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;
	struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;

	int i, j, k;
	UWORD comp_size;
	int32_t ppnum, ret;
	uint32_t target_ppa;
	int32_t bus, chip, block, page;
	int32_t header_data;
	int32_t page_write = 0;

	write_buff = (uint8_t *)malloc(sizeof(struct rw_buffer_t));
	compress_buff = (uint8_t *)malloc(FLASH_PAGE_SIZE * 10);

	for (i = 0; i < write_cnt; i++)
	{
		printf("0\n");
		ptr_ftl_context->gc_write_buff.lpa_arr[ptr_ftl_context->gc_counter] = lpa_arr[i];
		memcpy(&ptr_ftl_context->gc_write_buff.buff[FLASH_PAGE_SIZE * i], &(buff[FLASH_PAGE_SIZE * i]), FLASH_PAGE_SIZE);
		ptr_ftl_context->gc_counter++;
		printf("0.5 %d\n", lpa_arr[i]);
		if (ptr_ftl_context->gc_counter == CHUNK_SIZE)
		{
			printf("Enter compression in GC\n");
			gc_translate_write_buffer(ptr_ftl_context);
			comp_size = compress(write_buff, sizeof(struct rw_buffer_t), compress_buff);
			printf("1\n");
			if (comp_size != -1)
			{
				ppnum = comp_size % FLASH_PAGE_SIZE ? 1 : 0 + comp_size / FLASH_PAGE_SIZE;

				target_ppa = ftl_convert_to_physical_page_address(ptr_gc_block->no_bus, ptr_gc_block->no_chip, ptr_gc_block->no_block, page_write);

			printf("2 %d\n", ppnum);
				for (j = 0; j < ppnum; j++)
				{
					blueftl_user_vdevice_page_write(
						ptr_vdevice,
						ptr_gc_block->no_bus, ptr_gc_block->no_chip, ptr_gc_block->no_block, page_write + j,
						FLASH_PAGE_SIZE,
						(char *)compress_buff + (j * FLASH_PAGE_SIZE));
				}
			printf("3\n");

				/* mapping 정보 수정 -> loop 4번 */
				for (k = 0; k < CHUNK_SIZE; k++)
				{
					printf("3.0 %d %d\n",ptr_ftl_context->gc_write_buff.lpa_arr[k], page_write);
					page_mapping_map_logical_to_physical(ptr_ftl_context, ptr_ftl_context->gc_write_buff.lpa_arr[k], ptr_gc_block->no_bus, 
											ptr_gc_block->no_chip, ptr_gc_block->no_block, page_write);
				}
			printf("3.1\n");

				for (k = 0; k < ppnum; k++)
				{
					ptr_gc_block->nr_free_pages--;

					ptr_gc_block->list_pages[page_write + k].page_status = PAGE_STATUS_VALID;
					ptr_gc_block->list_pages[page_write + k].no_logical_page_addr = ptr_ftl_context->gc_write_buff.lpa_arr[k];
				}
			printf("3.2\n");

				/* Data chunk table entry 삽입 */
				for (k= 0; k < ppnum; k++)
				{
					ptr_chunk_table->ptr_ch_table[(target_ppa / 32) + k].compress_indicator = COMP_TRUE;
					ptr_chunk_table->ptr_ch_table[(target_ppa / 32) + k].nr_physical_pages = ppnum;
					ptr_chunk_table->ptr_ch_table[(target_ppa / 32) + k].valid_page_count = CHUNK_SIZE;
					page_write++;
				}
			printf("4\n");

			}
			else
			{
				printf("Enter Single in GC\n");
				for (k = 0; k < CHUNK_SIZE; k++)
				{
					ppnum = 1; // calculate needed page numbers

					target_ppa = ftl_convert_to_physical_page_address(ptr_gc_block->no_bus, ptr_gc_block->no_chip, ptr_gc_block->no_block, page_write);

					blueftl_user_vdevice_page_write(
						ptr_vdevice,
						ptr_gc_block->no_bus, ptr_gc_block->no_chip, ptr_gc_block->no_block, page_write,
						FLASH_PAGE_SIZE,
						(char *)&(ptr_ftl_context->gc_write_buff.buff[i*FLASH_PAGE_SIZE]));

					/* mapping 정보 수정 -> loop 4번 */
					page_mapping_map_logical_to_physical(ptr_ftl_context, ptr_ftl_context->gc_write_buff.lpa_arr[k], ptr_gc_block->no_bus, 
											ptr_gc_block->no_chip, ptr_gc_block->no_block, page_write);

					ptr_gc_block->nr_free_pages--;

					ptr_gc_block->list_pages[page_write + k].page_status = PAGE_STATUS_VALID;
					ptr_gc_block->list_pages[page_write + k].no_logical_page_addr = ptr_ftl_context->gc_write_buff.lpa_arr[k];

					/* Data chunk table entry 삽입 */
					ptr_chunk_table->ptr_ch_table[target_ppa / 32].compress_indicator = COMP_FALSE;
					ptr_chunk_table->ptr_ch_table[target_ppa / 32].nr_physical_pages = ppnum;
					ptr_chunk_table->ptr_ch_table[target_ppa / 32].valid_page_count = 1;

					page_write++;
				}
			}
			printf("5\n");

			memset(write_buff, 0, sizeof(struct rw_buffer_t));
			memset(&(ptr_ftl_context->gc_write_buff), -1, sizeof(struct rw_buffer_t));
			memset(compress_buff, 0, sizeof(struct rw_buffer_t));

			/* 정보 초기화 */
			ptr_ftl_context->gc_counter = 0; //카운터 초기화
			printf("6\n");
		} // CHUNK_SIZE 맞춰서 mapping 완료
	}
	printf("7\n");
}

void gc_translate_write_buffer(struct ftl_context_t *ptr_ftl_context)
{ // cache_write_buffer에 담아놨던 정보들 write_buffer로 옮기기
	uint8_t *tmp_ptr = write_buff;
	int i;
	
	printf("Serialize\n");
	for (i = 0; i < CHUNK_SIZE; i++)
	{
		memcpy(tmp_ptr, &(ptr_ftl_context->gc_write_buff.lpa_arr[i]), sizeof(int32_t));
		// printf("%d %d\n", cache_write_buff.lpa_arr[i],*((int32_t)tmp_ptr);
		tmp_ptr += sizeof(uint32_t);
	}

	for (i = 0; i < CHUNK_SIZE; i++)
	{
		memcpy(tmp_ptr, &(ptr_ftl_context->gc_write_buff.buff[FLASH_PAGE_SIZE * i]), FLASH_PAGE_SIZE);
		tmp_ptr += FLASH_PAGE_SIZE;
	}
}
