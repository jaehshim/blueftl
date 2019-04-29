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

unsigned char read_buff[FLASH_PAGE_SIZE * (CHUNK_SIZE + 1)]; //header 공간을 위해 CHUNK_SIZE 보다 크게 줌. 압축이 안 될 경우 대비
unsigned char write_buff[FLASH_PAGE_SIZE * (CHUNK_SIZE + 1)];
int write_counter = -1; //write 요청 들어온 lpa 개수 count
struct comp_header_t *comp_header;
uint8_t write_buff[FLASH_PAGE_SIZE * CHUNK_SIZE];

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
    struct chunk_table_t *ptr_chunk_table = (struct chunk_table_t *)ptr_ftl_context->ptr_chunk_table;
    int32_t requested_ppa;
    int32_t ppnum;
    int i;
    uint8_t *temp_buff1 = NULL;
    uint8_t *temp_buff2 = NULL;
    UWORD comp_size;

    requested_ppa = ftl_convert_to_physical_page_address(bus, chip, block, page); // 요청된 bus chip block page에 대응되는 ppa 변환
    ppnum = ptr_chunk_table->ptr_ch_table[requested_ppa].nr_physical_pages; //chunk table에서 num of phy pages 읽음

    blueftl_user_vdevice_page_read( //vdevice read를 통해 해당 ppa에서 해당 chunk의 데이터 읽음
        ptr_vdevice,
        bus, chip, block, page,
        ptr_vdevice->page_main_size * ppnum,
        (char *)temp_buff1);

    decompress(temp_buff1, ptr_vdevice->page_main_size * ppnum, temp_buff2); //압축 해제
    
    for (i = 0; i < CHUNK_SIZE; i++) // header의 배열 탐색해서 맞는 page의 index 찾기 & 데이터 얻기
    {
        if (*(temp_buff2 + i * sizeof(uint32_t)) == requested_lpa) {
            ptr_page_data = temp_buff2 + sizeof(struct comp_header_t) + ptr_vdevice->page_main_size * i; //해당 lpa의 데이터 위치로 포인터 이동
            break;
        }
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


    write_counter++; //counter increase
    comp_header->comp_lba_array[write_counter] = requested_lpa; // lpa store
    memcpy(write_buff + write_counter * FLASH_PAGE_SIZE, ptr_page_data, FLASH_PAGE_SIZE); //write data store

    if (write_counter == CHUNK_SIZE - 1)
    {
        uint8_t *comp_buff1 = calloc(FLASH_PAGE_SIZE * (CHUNK_SIZE+1));
        uint8_t *comp_buff2 = calloc(FLASH_PAGE_SIZE * (CHUNK_SIZE+1));

        write_counter = -1; //카운터 초기화
        
        memcpy(comp_buff1, comp_header, sizeof(struct comp_header_t)); // comp_buff1 에 header 기록
        memcpy(comp_buff1 + sizeof(struct comp_header_t), write_buff, FLASH_PAGE_SIZE * CHUNK_SIZE); // comp_buff1 에 data 기록
        compress(comp_buff1, sizeof(struct comp_header_t) + FLASH_PAGE_SIZE * CHUNK_SIZE, comp_buff2); // 헤더와 데이터 합쳐진 comp_buff1 압축
        blueftl_user_vdevice_page_write (
						ptr_vdevice, 
						bus, chip, block, page, 
						ptr_vdevice->page_main_size * (CHUNK_SIZE+1), 
						(char*)ptr_lba_buff);

    }
    

}
