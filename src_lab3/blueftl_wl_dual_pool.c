#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "blueftl_ftl_base.h"
#include "blueftl_user_ftl_main.h"
#include "blueftl_ssdmgmt.h"
#include "blueftl_user_vdevice.h"
#include "blueftl_mapping_page.h"
#include "blueftl_util.h"
#include "blueftl_wl_dual_pool.h"

unsigned char migration_buff[FLASH_PAGE_SIZE];

/* Hot Pool Status Variable */
dual_pool_block_info g_max_ec_in_hot_pool;
dual_pool_block_info g_min_ec_in_hot_pool;
dual_pool_block_info g_max_rec_in_hot_pool;
dual_pool_block_info g_min_rec_in_hot_pool;

/* Cold Pool Status Variable */
dual_pool_block_info g_max_ec_in_cold_pool;
dual_pool_block_info g_min_ec_in_cold_pool;
dual_pool_block_info g_max_rec_in_cold_pool;
dual_pool_block_info g_min_rec_in_cold_pool;

bool check_cold_pool_adjustment()
{
    if (g_max_rec_in_cold_pool.nr_erase_cnt - g_min_rec_in_hot_pool.nr_erase_cnt > WEAR_LEVELING_THRESHOLD)
        return true;
    else
        return false;
}

bool check_hot_pool_adjustment()
{
    if (g_max_ec_in_hot_pool.nr_erase_cnt - g_min_ec_in_hot_pool.nr_erase_cnt > 2 * WEAR_LEVELING_THRESHOLD)
        return true;
    else
        return false;
}

bool check_cold_data_migration()
{
    if (g_max_ec_in_hot_pool.nr_erase_cnt - g_min_ec_in_cold_pool.nr_erase_cnt > WEAR_LEVELING_THRESHOLD)
        return true;
    else
        return false;
}

void check_max_min_nr_erase_cnt(struct ftl_context_t *ptr_ftl_context, struct flash_block_t *ptr_erase_block)
{
    if (ptr_erase_block->hot_cold_pool == HOT_POOL)
    {
        if (g_min_ec_in_hot_pool.nr_erase_cnt > ptr_erase_block->nr_erase_cnt) // max값 changed, ec TAIL in hot pool change
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, EC, MIN);
            ptr_erase_block->head_or_tail_ec = TAIL;
        }
        else if (g_max_ec_in_hot_pool.nr_erase_cnt < ptr_erase_block->nr_erase_cnt) // min값 changed, ec HEAD in hot pool change
        {
                        update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, EC, MIN);
ptr_erase_block->head_or_tail_ec = HEAD;
        }

        if (g_min_rec_in_hot_pool.nr_erase_cnt > ptr_erase_block->nr_erase_cnt) // max값 changed, rec TAIL in hot pool change
        {
                        update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, EC, MIN);
ptr_erase_block->head_or_tail_rec = TAIL;
        }
        else if (g_max_rec_in_hot_pool.nr_erase_cnt < ptr_erase_block->nr_erase_cnt) // min값 changed, rec HEAD in hot pool change
        {
                       update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, EC, MIN);
 ptr_erase_block->head_or_tail_rec = HEAD;
        }
    }
    else // COLD_POOL
    {
        if (g_min_ec_in_cold_pool.nr_erase_cnt > ptr_erase_block->nr_erase_cnt) // max값 changed, ec TAIL in hot pool change
        {
                        update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, EC, MIN);
ptr_erase_block->head_or_tail_ec = TAIL;
        }
        else if (g_min_ec_in_cold_pool.nr_erase_cnt < ptr_erase_block->nr_erase_cnt) // min값 changed, ec HEAD in hot pool change
        {
                        update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, EC, MIN);
ptr_erase_block->head_or_tail_ec = HEAD;
        }

        if (g_min_rec_in_cold_pool.nr_erase_cnt > ptr_erase_block->nr_erase_cnt) // max값 changed, rec TAIL in hot pool change
        {
                        update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, EC, MIN);
ptr_erase_block->head_or_tail_rec = TAIL;
        }
        else if (g_min_rec_in_cold_pool.nr_erase_cnt < ptr_erase_block->nr_erase_cnt) // min값 changed, rec HAID in hot pool change
        {
                        update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, EC, MIN);
ptr_erase_block->head_or_tail_rec = HEAD;
        }
    }
}

void cold_pool_adjustment(struct ftl_context_t *ptr_ftl_context)
{
}

void hot_pool_adjustment(struct ftl_context_t *ptr_ftl_context)
{
}

struct flash_block_t *get_min_max_ptr(struct ftl_context_t *ptr_ftl_context, dual_pool_block_info *pool_info)
{
}

struct flash_block_t *get_erase_blk_ptr(struct ftl_context_t *ptr_ftl_context, uint32_t target_bus, uint32_t target_chip, uint32_t target_block)
{
}

/*
bool block_copy(struct flash_block_t *ptr_src_block, struct flash_block_t *ptr_tgt_block, struct ftl_context_t *ptr_ftl_context)
{
}
*/

void cold_data_migration(struct ftl_context_t *ptr_ftl_context)
{

    //prepare buffers
    uint8_t *ptr_block_buff1 = NULL;

    uint8_t *ptr_block_buff2 = NULL;
    uint8_t *ptr_page_buff2 = NULL;

    if ((ptr_block_buff1 = (uint8_t *)malloc(ptr_ssd->nr_pages_per_block * ptr_vdevice->page_main_size)) == NULL)
    {
        printf("blueftl_cold_migration: the memory allocation for the merge buffer1 failed (%u)\n",
               ptr_ssd->nr_pages_per_block * ptr_vdevice->page_main_size);
        return -1;
    }
    memset(ptr_block_buff1, 0xFF, ptr_ssd->nr_pages_per_block * ptr_vdevice->page_main_size);

    if ((ptr_block_buff2 = (uint8_t *)malloc(ptr_ssd->nr_pages_per_block * ptr_vdevice->page_main_size)) == NULL)
    {
        printf("blueftl_cold_migration: the memory allocation for the merge buffer2 failed (%u)\n",
               ptr_ssd->nr_pages_per_block * ptr_vdevice->page_main_size);
        return -1;
    }
    memset(ptr_block_buff2, 0xFF, ptr_ssd->nr_pages_per_block * ptr_vdevice->page_main_size);

    if(block_valid_copy(ptr_ftl_context->ec_head_hot_block, ptr_block_buff1)                         //copying valid data of oldest hot block to buffer
    {
        printf("cold mig hot head block copy to buffer failed\n");
    }
    if(block_valid_copy(ptr_ftl_context->ec_head_cold_block, ptr_block_buff1)                              //copying valid data of youngest cold block to buffer
    {
        printf("cold mig cold head block copy to buffer failed\n");
    }             
    if(page_clean_in_block(ptr_ftl_context->ec_head_hot_block))                  //erase hot pool oldest block
        printf("cold mig hot head block clean failed\n");
    if(page_clean_in_block(ptr_ftl_context->ec_head_cold_block))                  //erase cold pool youngest block
        printf("cold mig cold head block clean failed\n");
    if(write_buffer_to_block(ptr_ftl_context->ec_head_hot_block, ptr_block_buff2))
        printf("cold mig hot head block copy from buffer failed\n");                //write data in buffer to oldest_hot_block
    if(write_buffer_to_block(ptr_ftl_context->ec_head_cold_block, ptr_block_buff1))             //write data in buffer to youngest_cold_block
        printf("cold mig hot head block copy from buffer failed\n");    
    if(rec_reset(ptr_ftl_context))
        printf("cold mig rec reset failed\n");                //write data in buffer to oldest_hot_block
    if(block_pool_swap(ptr_ftl_context)) // swap two blocks. to the pool tail
        printf("cold mig swap failed\n");                
    if(find_new_heads(ptr_ftl_context)) // find new heads for two pools
        printf("cold mig swap failed\n");             
}

bool block_valid_copy(struct flash_block_t *ptr_target_block, uint8_t *ptr_block_buff)
{
    uint32_t loop_page = 0;
    uint8_t *ptr_page_buff = NULL;
    struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;

    for (loop_page = 0; loop_page < ptr_ssd->nr_pages_per_block; loop_page++)
    {
        if (ptr_target_block->list_pages[loop_page].page_status == 3)
        {
            /* read the valid page data from the flash memory */
            ptr_page_buff = ptr_block_buff + (loop_page * ptr_vdevice->page_main_size);

            blueftl_user_vdevice_page_read(
                _ptr_vdevice,
                ptr_target_block->no_bus, ptr_target_block->no_chip, ptr_target_block->no_block, loop_page,
                ptr_vdevice->page_main_size,
                (char *)ptr_page_buff);
        }
    }
    return 0;
}

bool page_clean_in_block(struct flash_block_t *ptr_target_block)
{
    struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;

    blueftl_user_vdevice_block_erase(ptr_vdevice, ptr_target_block->no_bus, ptr_target_block->no_chip, ptr_target_block->no_block);
    ptr_victim_block->nr_erase_cnt++;
    ptr_victim_block->nr_recent_erase_cnt++; //어차피 초기화 될 예정이긴 한데..
    return 0;
}

bool write_buffer_to_block(struct flash_block_t *ptr_target_block, uint8_t *ptr_block_buff)
{
    uint32_t loop_page = 0;
    uint8_t *ptr_page_buff = NULL;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;
    for (loop_page = 0; loop_page < ptr_ssd->nr_pages_per_block; loop_page++)
    {
        if (ptr_victim_block->list_pages[loop_page].page_status == 3)
        { 
            ptr_page_buff = ptr_block_buff + (loop_page * ptr_vdevice->page_main_size);

            blueftl_user_vdevice_page_write(
                ptr_vdevice,
                victim_bus, victim_chip, victim_block, loop_page,
                ptr_vdevice->page_main_size,
                (char *)ptr_page_buff);
           
        }
    }
   return 0;

}



bool rec_reset(struct ftl_context_t *ptr_ftl_context)
{
    ptr_ftl_context->ec_head_hot_block->nr_recent_erase_cnt = 0;
    return 0;
}

bool block_pool_swap(struct ftl_context_t *ptr_ftl_context)
{
    //여기서 if 인 이유: tail이 최대/최소를 갖도록 하기 위해
   if(ptr_ftl_context->ec_head_hot_block->nr_erase_cnt > ptr_ftl_context->ec_tail_cold_block->nr_erase_cnt) ptr_ftl_context->ec_tail_cold_block = ptr_ftl_context->ec_head_hot_block;
   if(ptr_ftl_context->ec_head_cold_block->nr_erase_cnt < ptr_ftl_context->ec_tail_hot_block->nr_erase_cnt) ptr_ftl_context->ec_tail_hot_block = ptr_ftl_context->ec_head_cold_block;
   return 0;
}

bool find_new_heads(struct ftl_context_t *ptr_ftl_context)
{
    //전체를 뒤져서 새로운 head를 찾아야 함. 전체 탐색 과정은 어떤 순서일지라도 어쩔 수 없이 포함되긴 해야 하는 듯.
    uint32_t ec_max = 0;
    uint32_t ec_min = -1;
     
    for (i = 0; i < ptr_ssd->nr_buses; i++)
    {
        for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
        {
            for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
            {
                ptr_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);
                if (ptr_block->nr_erase_cnt > ec_max)
                {
                    ec_max = ptr_block->nr_erase_cnt;
                    ptr_ftl_context->ec_head_hot_block = ptr_block;
                }
                 if (ptr_block->nr_erase_cnt < ec_min)
                {
                    ec_min = ptr_block->nr_erase_cnt;
                    ptr_ftl_context->ec_head_cold_block = ptr_block;
                }
            }
        }
    }
   return 0;

}

void insert_pool(struct ftl_context_t *ptr_ftl_context, struct flash_block_t *ptr_erase_block)
{
}

uint32_t find_max_ec_pool_block_info(struct ftl_context_t *ptr_ftl_context, uint32_t pool)
{
}

uint32_t find_min_ec_pool_block_info(struct ftl_context_t *ptr_ftl_context, uint32_t pool)
{
}

uint32_t find_max_rec_pool_block_info(struct ftl_context_t *ptr_ftl_context, uint32_t pool)
{
}

uint32_t find_min_rec_pool_block_info(struct ftl_context_t *ptr_ftl_context, uint32_t pool)
{
}

uint32_t update_max_min_nr_erase_cnt_in_pool(struct ftl_context_t *ptr_ftl_context, struct flash_block_t *ptr_target_block, int pool, int type, int min_max)
{

    if (pool == HOT_POOL)
    {
        if (type == EC)
        {
            if (min_max == MIN) // EC MIN
            {
                ptr_ftl_context->ec_tail_hot_block = ptr_target_block;
            }
            else // EC MAX
            {
                ptr_ftl_context->ec_head_hot_block = ptr_target_block;
            }
        }
        else
        {
            if (min_max == MIN) // REC MIN
            {
                ptr_ftl_context->rec_tail_hot_block = ptr_target_block;
            }
            else //REC MAX
            {
                ptr_ftl_context->rec_head_hot_block = ptr_target_block;
            }
        }
    }

    if (pool == COLD_POOL) // for cold pool, head is youngest (MIN)
    {
        if (type == EC)
        {
            if (min_max == MIN) // EC MIN
            {
                ptr_ftl_context->ec_head_cold_block = ptr_target_block;
            }
            else // EC MAX
            {
                ptr_ftl_context->ec_tail_cold_block = ptr_target_block;
            }
        }
        else
        {
            if (min_max == MIN) // REC MIN
            {
                               ptr_ftl_context->rec_head_cold_block = ptr_target_block;

            }
            else //REC MAX
            {
                               ptr_ftl_context->ec_tail_cold_block = ptr_target_block;

            }
        }
    }
}

uint32_t update_max_min_nr_erase_cnt_in_pool(struct ftl_context_t *ptr_ftl_context, int pool, int type, int min_max, int bus, int chip, int block, uint32_t nr_erase_cnt)
{
    int i, j, k;
    struct flash_block_t *ptr_block = NULL;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;

    if (pool == HOT_POOL)
    {
        if (type == EC)
        {
            if (min_max == MIN) // EC MIN
            {
                g_min_ec_in_hot_pool.no_bus = bus;
                g_min_ec_in_hot_pool.no_chip = chip;
                g_min_ec_in_hot_pool.no_block = block;
                g_min_ec_in_hot_pool.nr_erase_cnt = nr_erase_cnt;

                if ()
                    for (i = 0; i < ptr_ssd->nr_buses; i++)
                    {
                        for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
                        {
                            for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
                            {
                                ptr_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);
                                if (IS_TAIL(ptr_block->head_or_tail_ec))
                                {
                                    DISABLE_TAIL(ptr_block->head_or_tail_ec); // 기존 EC TAIL 해제
                                }
                            }
                        }
                    }
            }
            else
            { // EC MAX
                g_max_ec_in_hot_pool.no_bus = bus;
                g_max_ec_in_hot_pool.no_chip = chip;
                g_max_ec_in_hot_pool.no_block = block;
                g_max_ec_in_hot_pool.nr_erase_cnt = nr_erase_cnt;

                for (i = 0; i < ptr_ssd->nr_buses; i++)
                {
                    for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
                    {
                        for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
                        {
                            ptr_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);
                            if (IS_HEAD(ptr_block->head_or_tail_ec))
                            {
                                DISABLE_HEAD(ptr_block->head_or_tail_ec); // 기존 EC HEAD 해제
                            }
                        }
                    }
                }
            }
        }
        else
        {                       // REC
            if (min_max == MIN) // REC MIN
            {
                g_min_rec_in_hot_pool.no_bus = bus;
                g_min_rec_in_hot_pool.no_chip = chip;
                g_min_rec_in_hot_pool.no_block = block;
                g_min_rec_in_hot_pool.nr_erase_cnt = nr_erase_cnt;

                for (i = 0; i < ptr_ssd->nr_buses; i++)
                {
                    for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
                    {
                        for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
                        {
                            ptr_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);
                            if (IS_TAIL(ptr_block->head_or_tail_rec))
                            {
                                DISABLE_TAIL(ptr_block->head_or_tail_rec); // 기존 REC TAIL 해제
                            }
                        }
                    }
                }
            }
            else
            { // REC MAX
                g_min_rec_in_hot_pool.no_bus = bus;
                g_min_rec_in_hot_pool.no_chip = chip;
                g_min_rec_in_hot_pool.no_block = block;
                g_min_rec_in_hot_pool.nr_erase_cnt = nr_erase_cnt;

                for (i = 0; i < ptr_ssd->nr_buses; i++)
                {
                    for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
                    {
                        for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
                        {
                            ptr_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);
                            if (IS_HEAD(ptr_block->head_or_tail_rec))
                            {
                                DISABLE_HEAD(ptr_block->head_or_tail_rec); // 기존 REC HEAD 해제
                            }
                        }
                    }
                }
            }
        }
    }
    else
    { // COLD_POOL
        if (type == EC)
        {
            if (min_max == MIN) // EC MIN
            {
                g_min_ec_in_cold_pool.no_bus = bus;
                g_min_ec_in_cold_pool.no_chip = chip;
                g_min_ec_in_cold_pool.no_block = block;
                g_min_ec_in_cold_pool.nr_erase_cnt = nr_erase_cnt;

                for (i = 0; i < ptr_ssd->nr_buses; i++)
                {
                    for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
                    {
                        for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
                        {
                            ptr_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);
                            if (IS_TAIL(ptr_block->head_or_tail_ec))
                            {
                                DISABLE_TAIL(ptr_block->head_or_tail_ec); // 기존 EC TAIL 해제
                            }
                        }
                    }
                }
            }
            else
            { // EC MAX
                g_min_ec_in_cold_pool.no_bus = bus;
                g_min_ec_in_cold_pool.no_chip = chip;
                g_min_ec_in_cold_pool.no_block = block;
                g_min_ec_in_cold_pool.nr_erase_cnt = nr_erase_cnt;

                for (i = 0; i < ptr_ssd->nr_buses; i++)
                {
                    for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
                    {
                        for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
                        {
                            ptr_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);
                            if (IS_HEAD(ptr_block->head_or_tail_ec))
                            {
                                DISABLE_HEAD(ptr_block->head_or_tail_ec); // 기존 EC HEAD 해제
                            }
                        }
                    }
                }
            }
        }
        else // REC
        {
            if (min_max == MIN) // REC MIN
            {
                g_min_rec_in_cold_pool.no_bus = bus;
                g_min_rec_in_cold_pool.no_chip = chip;
                g_min_rec_in_cold_pool.no_block = block;
                g_min_rec_in_cold_pool.nr_erase_cnt = nr_erase_cnt;

                for (i = 0; i < ptr_ssd->nr_buses; i++)
                {
                    for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
                    {
                        for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
                        {
                            ptr_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);
                            if (IS_TAIL(ptr_block->head_or_tail_rec))
                            {
                                DISABLE_TAIL(ptr_block->head_or_tail_rec); // 기존 REC TAIL 해제
                            }
                        }
                    }
                }
            }
            else
            { // REC MAX
                g_min_rec_in_cold_pool.no_bus = bus;
                g_min_rec_in_cold_pool.no_chip = chip;
                g_min_rec_in_cold_pool.no_block = block;
                g_min_rec_in_cold_pool.nr_erase_cnt = nr_erase_cnt;

                for (i = 0; i < ptr_ssd->nr_buses; i++)
                {
                    for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
                    {
                        for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
                        {
                            ptr_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);
                            if (IS_HEAD(ptr_block->head_or_tail_rec))
                            {
                                DISABLE_HEAD(ptr_block->head_or_tail_rec); // 기존 REC HEAD 해제
                            }
                        }
                    }
                }
            }
        }
    }
}
