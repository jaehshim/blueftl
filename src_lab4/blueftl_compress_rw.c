#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "blueftl_ftl_base.h"
#include "blueftl_ssdmgmt.h"
#include "blueftl_user_vdevice.h"
#include "blueftl_mapping_page.h"
#include "blueftl_gc_page.h"
#include "blueftl_util.h"
#include "blueftl_compress_rw.h"
#include "lzrw3.h"

struct rw_buffer_t cache_write_buff;

int write_counter = 0; //write 요청 들어온 lpa 개수 count

uint8_t *write_buff;
uint8_t *compress_buff;
uint32_t is_update[CHUNK_SIZE];
uint32_t is_update_flag;
int cnt = 0;

uint32_t init_chunk_table(struct ftl_context_t *ptr_ftl_context)
{
    uint32_t init_loop = 0;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct chunk_table_t *ptr_chunk_table;

    /* calculate the number of entries for the chunk table */
    // chunk table 크기는 mapping table과 같게 잡음. ppa에 해당하는 index 로 바로 access 할수 있게
    ptr_ftl_context->ptr_chunk_table = (struct chunk_table_t *)malloc(sizeof(struct chunk_table_t));

    ptr_chunk_table = (struct chunk_table_t *)ptr_ftl_context->ptr_chunk_table;
    ptr_chunk_table->nr_chunk_table_entries =
        ptr_ssd->nr_buses * ptr_ssd->nr_chips_per_bus * ptr_ssd->nr_blocks_per_chip * ptr_ssd->nr_pages_per_block;

    /* allocate the memory for the chunk table */
    if ((ptr_chunk_table->ptr_ch_table = (struct chunk_entry_t *)malloc(ptr_chunk_table->nr_chunk_table_entries * sizeof(struct chunk_entry_t))) == NULL)
    {
        printf("blueftl_compress_rw: failed to allocate the memory for ptr_chunk_table\n");
        exit(1);
    }

    /* init the chunk table  */
    for (init_loop = 0; init_loop < ptr_chunk_table->nr_chunk_table_entries; init_loop++)
    {
        ptr_chunk_table->ptr_ch_table[init_loop].valid_page_count = 0;
        ptr_chunk_table->ptr_ch_table[init_loop].nr_physical_pages = 0;
        ptr_chunk_table->ptr_ch_table[init_loop].compress_indicator = COMP_INIT;
    }

    write_buff = (uint8_t *)malloc(sizeof(struct rw_buffer_t));
    compress_buff = (uint8_t *)malloc(FLASH_PAGE_SIZE * 10);

    printf("init chunk table\n");

    return 0;
}

void blueftl_compressed_page_read(
    struct ftl_context_t *ptr_ftl_context,
    struct virtual_device_t *ptr_vdevice,
    int32_t requested_lpa,
    int32_t bus,
    int32_t chip,
    int32_t block,
    int32_t page,
    int32_t page_length,
    char *page_data)
{
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct chunk_table_t *ptr_chunk_table = (struct chunk_table_t *)ptr_ftl_context->ptr_chunk_table;
    int32_t requested_ppa;
    int32_t ppnum, header_data = 0;
    int32_t compress_flag;
    int i;
    uint8_t *temp_buff1 = NULL;
    uint8_t *temp_buff2 = NULL;
    UWORD comp_size;
    int flag = 0;

    for (i = 0; i < write_counter; i++)
    {
        if (requested_lpa == cache_write_buff.lpa_arr[i])
        {
            memcpy(page_data, &(cache_write_buff.buff[FLASH_PAGE_SIZE * i]), FLASH_PAGE_SIZE);
            printf("data exists in the cache write buffer %d\n", i);
            return;
        }
    }

    requested_ppa = ftl_convert_to_physical_page_address(bus, chip, block, page); // 요청된 bus chip block page에 대응되는 ppa 변환
    ppnum = ptr_chunk_table->ptr_ch_table[requested_ppa/32].nr_physical_pages;       //chunk table에서 num of phy pages 읽음
    compress_flag = ptr_chunk_table->ptr_ch_table[requested_ppa/32].compress_indicator;

    temp_buff1 = (uint8_t *)malloc(FLASH_PAGE_SIZE * 10);
    temp_buff2 = (uint8_t *)malloc(FLASH_PAGE_SIZE * 10);

    blueftl_user_vdevice_page_read( //vdevice read를 통해 해당 ppa에서 해당 chunk의 데이터 읽음
        ptr_vdevice,
        bus, chip, block, page,
        FLASH_PAGE_SIZE * ppnum,
        (char *)temp_buff1);

    if (compress_flag) // compressed data 처리
    {
        decompress(temp_buff1, FLASH_PAGE_SIZE * ppnum, temp_buff2); //압축 해제

        for (i = 0; i < CHUNK_SIZE; i++) // header의 배열 탐색해서 맞는 page의 index 찾기 & 데이터 얻기
        {
            memcpy(&header_data, temp_buff2 + i * sizeof(int32_t), sizeof(int32_t));
            if (header_data == requested_lpa)
            {
                memcpy(page_data, temp_buff2 + 4 * sizeof(int32_t) + FLASH_PAGE_SIZE * i, FLASH_PAGE_SIZE);
                flag = 1;
                break;
            }
        }
    }
    else
    { // 일반 page mapping data
        memcpy(page_data, temp_buff1, FLASH_PAGE_SIZE);
    }

    if (flag == 0)
    {
        printf("Invalid LPA Request!!\n");
        exit(1);
    }

    free(temp_buff1);
    free(temp_buff2);

    return;
}

void blueftl_compressed_page_write(
    struct ftl_context_t *ptr_ftl_context,
    struct virtual_device_t *ptr_vdevice,
    int32_t requested_lpa,
    int32_t page_length,
    char *page_data)
{
    struct chunk_table_t *ptr_chunk_table = (struct chunk_table_t *)ptr_ftl_context->ptr_chunk_table;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct ftl_page_mapping_context_t *ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;
    int i, j;
    UWORD comp_size;
    int32_t ppnum, ret;
    uint32_t target_ppa;
    int32_t bus, chip, block, page;
    struct flash_block_t *ptr_block;
    int32_t update_bus, update_chip, update_block, update_page;
    int32_t header_data;

    /* 기존 flash에 존재하던 데이터의 업데이트는 배열에 표시해줌 */
    if (page_mapping_get_mapped_physical_page_address(ptr_ftl_context, requested_lpa, &update_bus, &update_chip, &update_block, &update_page) != -1)
    {
        printf("update data\n");
        is_update[write_counter] = UPDATE;
    }

    /* 버퍼링된 데이터의 업데이트인 경우 버퍼만 바꿔 주고 즉시 return */
    for (i = 0; i < write_counter; i++)
    {
        if (requested_lpa == cache_write_buff.lpa_arr[i])
        {
            printf("buffering data update\n");
            memcpy(&cache_write_buff.buff[FLASH_PAGE_SIZE * i], page_data, FLASH_PAGE_SIZE); // store write data, buffer index not sure
            return;                                                                          // 버퍼링된 데이터의 업데이트인 경우 버퍼만 바꿔 주고 즉시 return
        }
    }

    //counter increase
    cache_write_buff.lpa_arr[write_counter] = requested_lpa;                                     // store lpa
    memcpy(&cache_write_buff.buff[FLASH_PAGE_SIZE * write_counter], page_data, FLASH_PAGE_SIZE); // store write data, buffer index not sure
    write_counter++;

    if (write_counter == CHUNK_SIZE) // cache_write_buff is full
    {
        translate_write_buffer();
        comp_size = compress(write_buff, sizeof(struct rw_buffer_t), compress_buff);
        if (comp_size != -1)
        {
            ppnum = comp_size % FLASH_PAGE_SIZE ? 1 : 0 + comp_size / FLASH_PAGE_SIZE; // calculate needed page numbers

            /* ppnum개 만큼의 연속된 page 요청 */
            if ((target_ppa = get_free_physical_pages(ptr_ftl_context, ppnum)) == -1)
            {
                printf("call gc\n");
                gc_page_trigger_gc_lab(ptr_ftl_context, ptr_pg_mapping->ru_bus, ptr_pg_mapping->ptr_ru_chips[ptr_pg_mapping->ru_bus]);
                if ((target_ppa = get_free_physical_pages(ptr_ftl_context, ppnum)) == -1)
                    printf("something wrong in gc\n");
            } // target_ppa에 연속된 free physical_page의 시작 주소 가져옴

            /* ppnum개의 연속된 page vdevice_write */
            ftl_convert_to_ssd_layout(target_ppa, &bus, &chip, &block, &page); // ppa를 bus,chip,block,page로 바꿔줌

            blueftl_user_vdevice_page_write(
                ptr_vdevice,
                bus, chip, block, page,
                FLASH_PAGE_SIZE * ppnum,
                (char *)compress_buff);

            /* is_update_flag 확인해서 is_update 배열 확인해서 처리 -> data chunk table에서 값 수정*/
            for (i = 0; i < CHUNK_SIZE; i++)
            {
                if (is_update[i] == UPDATE)
                {
                    printf("CHUNK is update %d\n", cache_write_buff.lpa_arr[i]);
                    page_mapping_get_mapped_physical_page_address(ptr_ftl_context, cache_write_buff.lpa_arr[i], &update_bus, &update_chip, &update_block, &update_page);
                    uint32_t physical_page_address = ftl_convert_to_physical_page_address(update_bus, update_chip, update_block, update_page);
                    uint32_t temp_ppnum = ptr_chunk_table->ptr_ch_table[physical_page_address/32].nr_physical_pages;
                    for (j = 0; j < temp_ppnum; j++) // 해당 chunk 전체에 대해 valid page count 1씩 감소
                    {
                        if (--ptr_chunk_table->ptr_ch_table[(physical_page_address + j)/32].valid_page_count < 0)
                            printf("valid page count negative\n");
                        // else if (--ptr_chunk_table->ptr_ch_table[physical_page_address + j].valid_page_count == 0)
                        // {
                        //     ptr_block = &(ptr_ssd->list_buses[update_bus].list_chips[update_chip].list_blocks[update_block]);
                        //     ptr_block->list_pages[page + i].page_status = PAGE_STATUS_INVALID;
                        // }
                        // valid page count 1감소, 0보다작으면 오류메시지
                    }
                }
            }
            /* mapping 정보 수정 -> loop 4번 */
            for (i = 0; i < CHUNK_SIZE; i++)
            {
                page_mapping_map_logical_to_physical(ptr_ftl_context, cache_write_buff.lpa_arr[i], bus, chip, block, page);
            }

            ptr_block = &(ptr_ssd->list_buses[bus].list_chips[chip].list_blocks[block]);
            for (i = 0; i < ppnum; i++)
            {
                //    ptr_block->nr_valid_pages++;
                ptr_block->nr_free_pages--;

                ptr_block->list_pages[page + i].page_status = PAGE_STATUS_VALID;
            }

            /* Data chunk table entry 삽입 */
            for (i = 0; i < ppnum; i++)
            {
                ptr_chunk_table->ptr_ch_table[(target_ppa + i)/32].compress_indicator = COMP_TRUE;
                ptr_chunk_table->ptr_ch_table[(target_ppa + i)/32].nr_physical_pages = ppnum;
                ptr_chunk_table->ptr_ch_table[(target_ppa + i)/32].valid_page_count = CHUNK_SIZE;
            }
        }
        else
        { // 압축 불가, overruned
            for (i = 0; i < CHUNK_SIZE; i++)
            {
                ppnum = 1; // calculate needed page numbers

                /* ppnum개 만큼의 연속된 page 요청 */
                if ((target_ppa = get_free_physical_pages(ptr_ftl_context, ppnum)) == -1)
                {
                    printf("call gc\n");
                    gc_page_trigger_gc_lab(ptr_ftl_context, ptr_pg_mapping->ru_bus, ptr_pg_mapping->ptr_ru_chips[ptr_pg_mapping->ru_bus]);
                    if ((target_ppa = get_free_physical_pages(ptr_ftl_context, ppnum)) == -1)
                        printf("something wrong in gc\n");
                } // target_ppa에 연속된 free physical_page의 시작 주소 가져옴

                /* ppnum개의 연속된 page vdevice_write */
                ftl_convert_to_ssd_layout(target_ppa, &bus, &chip, &block, &page); // ppa를 bus,chip,block,page로 바꿔줌

                blueftl_user_vdevice_page_write(
                    ptr_vdevice,
                    bus, chip, block, page,
                    FLASH_PAGE_SIZE * ppnum,
                    (char *)compress_buff);

                if (is_update[i] == UPDATE)
                {
                    printf("SINGLE is update %d\n", cache_write_buff.lpa_arr[i]);
                    page_mapping_get_mapped_physical_page_address(ptr_ftl_context, cache_write_buff.lpa_arr[i], &update_bus, &update_chip, &update_block, &update_page);
                    uint32_t physical_page_address = ftl_convert_to_physical_page_address(update_bus, update_chip, update_block, update_page);
                    uint32_t temp_ppnum = ptr_chunk_table->ptr_ch_table[physical_page_address/32].nr_physical_pages;

                    for (j = 0; j < temp_ppnum; j++) // 해당 chunk 전체에 대해 valid page count 1씩 감소
                    {
                        if (--ptr_chunk_table->ptr_ch_table[(physical_page_address + j)/32].valid_page_count < 0)
                            printf("valid page count negative\n");
                        // valid page count 1감소, 0보다작으면 오류메시지
                    }
                }

                /* mapping 정보 수정 -> loop 4번 */
                page_mapping_map_logical_to_physical(ptr_ftl_context, cache_write_buff.lpa_arr[i], bus, chip, block, page);

                ptr_block = &(ptr_ssd->list_buses[bus].list_chips[chip].list_blocks[block]);

                //ptr_block->nr_valid_pages++;
                ptr_block->nr_free_pages--;

                ptr_block->list_pages[page + i].page_status = PAGE_STATUS_VALID;

                /* Data chunk table entry 삽입 */
                ptr_chunk_table->ptr_ch_table[target_ppa/32].compress_indicator = COMP_FALSE;
                ptr_chunk_table->ptr_ch_table[target_ppa/32].nr_physical_pages = ppnum;
                ptr_chunk_table->ptr_ch_table[target_ppa/32].valid_page_count = 1;

            }
        }
        /* cache 초기화 */
        memset(write_buff, -1, sizeof(struct rw_buffer_t));
        memset(&cache_write_buff, -1, sizeof(struct rw_buffer_t));
        memset(compress_buff, -1, sizeof(struct rw_buffer_t));
        memset(is_update, -1, sizeof(uint32_t) * CHUNK_SIZE);

        /* 정보 초기화 */
        write_counter = 0; //카운터 초기화
        is_update_flag = 0;
    }
}

void translate_write_buffer()
{ // cache_write_buffer에 담아놨던 정보들 write_buffer로 옮기기
    uint8_t *tmp_ptr = write_buff;
    int i;
    //    printf("Serialize\n");
    for (i = 0; i < CHUNK_SIZE; i++)
    {
        memcpy(tmp_ptr, &(cache_write_buff.lpa_arr[i]), sizeof(int32_t));
        // printf("%d %d\n", cache_write_buff.lpa_arr[i],*((int32_t)tmp_ptr);
        tmp_ptr += sizeof(uint32_t);
    }

    for (i = 0; i < CHUNK_SIZE; i++)
    {
        memcpy(tmp_ptr, &(cache_write_buff.buff[FLASH_PAGE_SIZE * i]), FLASH_PAGE_SIZE);
        tmp_ptr += FLASH_PAGE_SIZE;
    }
}

uint32_t get_free_physical_pages(struct ftl_context_t *ptr_ftl_context, int ppnum)
{

    uint32_t curr_bus, curr_chip;
    uint32_t curr_block, curr_page;
    uint32_t curr_physical_page_addr;

    struct flash_block_t *ptr_active_block = NULL;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct ftl_page_mapping_context_t *ptr_pg_mapping =
        (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;

    int no_bus, no_chip, no_block, no_page;

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
    if (ptr_active_block->nr_free_pages < ppnum) // 압축된 데이터 크기만큼의 공간이 비어 있는지 확인
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