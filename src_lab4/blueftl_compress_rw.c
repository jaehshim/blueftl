#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "lzrw3.h"
#include "blueftl_ftl_base.h"
#include "blueftl_ssdmgmt.h"
#include "blueftl_user_vdevice.h"
#include "blueftl_mapping_page.h"
#include "blueftl_gc_page.h"
#include "blueftl_util.h"
#include "blueftl_compress_rw.h"

unsigned char read_buff[FLASH_PAGE_SIZE * (chunk_size + 1)]; //header 공간을 위해 chunk_size 보다 크게 줌. 압축이 안 될 경우 대비
unsigned char write_buff[FLASH_PAGE_SIZE * (chunk_size + 1)];

uint32_t init_chunk_table(struct ftl_context_t *ptr_ftl_context)
{
    uint32_t init_loop = 0;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct chunk_table_t *ptr_chunk_table =
        ((struct chunk_table_t *)ptr_ftl_context->ptr_chunk)->ptr_ch_table;

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
        ptr_chunk_table->ptr_ch_table[init_loop].comp_indi = COMP_FALSE;
    }

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
    struct chunk_table_t *ptr_chunk_table =
        ((struct chunk_table_t *)ptr_ftl_context->ptr_chunk)->ptr_ch_table;
    int32_t requested_ppa;
    int32_t ppnum;
    int i;
    uint32_t *temp_buff1;
    uint32_t *temp_buff2;
    uint32_t *header_buff;

    requested_ppa = ftl_convert_to_physical_page_address(bus, chip, block, page); // 요청된 bus chip block page에 대응되는 ppa
    ppnum = ptr_chunk_table->ptr_ch_table[requested_ppa].nr_physical_pages; //chunk table에서 num of phy pages 읽음
    blueftl_user_vdevice_page_read( //vdevice read를 통해 해당 ppa에서 해당 chunk의 데이터 읽음
        ptr_vdevice,
        bus, chip, block, page,
        ptr_vdevice->page_main_size * ppnum,
        (char *)temp_buff1);
    decompress(temp_buff1, ptr_vdevice->page_main_size * ppnum, temp_buff2); //압축 해제
    header_buff = temp_buff2;
    for (i=0; i<chunk_size; i++)
    {
        if (header_buff[i] == requested_lpa)
        {
            ptr_page_data = temp_buff2 + ptr_vdevice->page_main_size * i; //해당 lpa의 데이터 위치로 포인터 이동
        }
    }
}

blueftl_compressed_page_write()
{
}