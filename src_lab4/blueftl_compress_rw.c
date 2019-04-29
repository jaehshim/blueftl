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

int write_counter = -1; //write 요청 들어온 lpa 개수 count

uint8_t * write_buff;
uint8_t * compress_buff;
uint32_t is_update[CHUNK_SIZE];
uint32_t is_update_flag;

uint32_t init_chunk_table(struct ftl_context_t *ptr_ftl_context)
{
    uint32_t init_loop = 0;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct chunk_table_t *ptr_chunk_table = (struct chunk_table_t *)ptr_ftl_context->ptr_chunk_table;

    /* calculate the number of entries for the chunk table */
    // chunk table 크기는 mapping table과 같게 잡음. ppa에 해당하는 index 로 바로 access 할수 있게
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
    char *ptr_page_data)
{
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct chunk_table_t *ptr_chunk_table = (struct chunk_table_t *)ptr_ftl_context->ptr_chunk_table;
    int32_t requested_ppa;
    int32_t ppnum;
    int32_t compress_flag;
    int i;
    uint8_t *temp_buff1 = NULL;
    uint8_t *temp_buff2 = NULL;
    UWORD comp_size;

    for (i = 0; i<write_counter; i++) {
        if (requested_lpa == cache_write_buff.lpa_arr[i]) {
            ptr_page_data = &cache_write_buff.buff[FLASH_PAGE_SIZE * i];
            printf("data exists in the cache write buffer\n");
        }
    }

    requested_ppa = ftl_convert_to_physical_page_address(bus, chip, block, page); // 요청된 bus chip block page에 대응되는 ppa 변환
    ppnum = ptr_chunk_table->ptr_ch_table[requested_ppa].nr_physical_pages; //chunk table에서 num of phy pages 읽음
    compress_flag = ptr_chunk_table->ptr_ch_table[requested_ppa].compress_indicator;

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
            if (*(temp_buff2 + i * sizeof(uint32_t)) == requested_lpa)
            {
                ptr_page_data = temp_buff2 + 4 * sizeof(uint32_t) + FLASH_PAGE_SIZE * i; //해당 lpa의 데이터 위치로 포인터 이동
                break;
            }
        }
    }
    else { // 일반 page mapping data
        ptr_page_data = temp_buff1;
    }
}

void blueftl_compressed_page_write(
    struct ftl_context_t *ptr_ftl_context,
    struct virtual_device_t *ptr_vdevice,
    int32_t requested_lpa,
    int32_t bus,
    int32_t chip,
    int32_t block,
    int32_t page,
    int32_t page_length,
    char *ptr_page_data)
{
    UWORD comp_size;
    int32_t ppnum;

    write_counter++; //counter increase
    cache_write_buff.lpa_arr[write_counter] = requested_lpa; // store lpa
    memcpy(&cache_write_buff.buff[FLASH_PAGE_SIZE * write_counter], ptr_page_data, FLASH_PAGE_SIZE); // store write data, buffer index not sure

    if (write_counter == CHUNK_SIZE-1) // cache_write_buff is full
    {
        write_counter = -1; //카운터 초기화

        translate_write_buffer();
        comp_size = compress(write_buff, sizeof(struct rw_buffer_t), compress_buff);
        if (comp_size != -1)
        {
            ppnum = comp_size % FLASH_PAGE_SIZE ? 1 : 0 + comp_size / FLASH_PAGE_SIZE; // calculate needed page numbers

            /* ppnum개 만큼의 연속된 page 요청 */


            /* ppnum개의 연속된 page vdevice_write */


            /* mapping 정보 수정 -> loop 4번 */


            /* is_update_flag 확인해서 is_update 배열 확인해서 처리 */


            /* Data chunk table 기록 & update */


        }

    }
}

void translate_write_buffer() { // cache_write_buffer에 담아놨던 정보들 write_buffer로 옮기기
    uint8_t * tmp_ptr = write_buff;
    int i;

    for (i = 0; i < CHUNK_SIZE; i++)
    {
        memcpy(&cache_write_buff.lpa_arr[i], tmp_ptr, sizeof(uint32_t));
        tmp_ptr += sizeof(uint32_t);
    }

       for (i = 0; i < CHUNK_SIZE; i++)
    {
        memcpy(&cache_write_buff.buff[FLASH_PAGE_SIZE * i], tmp_ptr, FLASH_PAGE_SIZE);
        tmp_ptr += FLASH_PAGE_SIZE;
    }
}
