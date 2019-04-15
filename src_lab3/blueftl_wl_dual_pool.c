#include <stdio.h>
#include <stdlib.h>
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
        if (g_min_ec_in_hot_pool.nr_erase_cnt > ptr_erase_block->nr_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, EC, MIN);
            g_min_ec_in_hot_pool.nr_erase_cnt = ptr_erase_block->nr_erase_cnt;
        }
        else if (g_max_ec_in_hot_pool.nr_erase_cnt < ptr_erase_block->nr_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, EC, MAX);
            g_max_ec_in_hot_pool.nr_erase_cnt = ptr_erase_block->nr_erase_cnt;
        }

        if (g_min_rec_in_hot_pool.nr_erase_cnt > ptr_erase_block->nr_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, REC, MIN);
            g_min_rec_in_hot_pool.nr_erase_cnt = ptr_erase_block->nr_erase_cnt;
        }
        else if (g_max_rec_in_hot_pool.nr_erase_cnt < ptr_erase_block->nr_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, REC, MAX);
            g_max_rec_in_hot_pool.nr_erase_cnt = ptr_erase_block->nr_erase_cnt;
        }
    }
    else // COLD_POOL
    {
        if (g_min_ec_in_cold_pool.nr_erase_cnt > ptr_erase_block->nr_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, COLD_POOL, EC, MIN);
            g_min_ec_in_cold_pool.nr_erase_cnt = ptr_erase_block->nr_erase_cnt;
        }
        else if (g_min_ec_in_cold_pool.nr_erase_cnt < ptr_erase_block->nr_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, COLD_POOL, EC, MAX);
            g_min_ec_in_cold_pool.nr_erase_cnt = ptr_erase_block->nr_erase_cnt;
        }

        if (g_min_rec_in_cold_pool.nr_erase_cnt > ptr_erase_block->nr_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, COLD_POOL, REC, MIN);
            g_min_rec_in_cold_pool.nr_erase_cnt = ptr_erase_block->nr_erase_cnt;
        }
        else if (g_min_rec_in_cold_pool.nr_erase_cnt < ptr_erase_block->nr_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, COLD_POOL, REC, MAX);
            g_min_rec_in_cold_pool.nr_erase_cnt = ptr_erase_block->nr_erase_cnt;
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

int32_t cold_data_migration(struct ftl_context_t *ptr_ftl_context)
{
    struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;

    // prepare buffers
    uint8_t *ptr_block_buff1 = NULL;
    uint8_t *ptr_block_buff2 = NULL;

    /* block의 데이터 보관할 buffer 선언 및 할당 */
    //   printf("step 1\n");
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

    /* buffer로 block의 데이터 copy */
    //   printf("step 2\n");
    if (!block_valid_copy(ptr_ftl_context, ptr_ftl_context->hot_block_ec_head, ptr_block_buff1)) // copying valid data of oldest hot block to buffer
    {
        printf("cold mig hot head block copy to buffer failed\n");
        exit(1);
        return -1;
    }
    if (!block_valid_copy(ptr_ftl_context, ptr_ftl_context->cold_block_ec_head, ptr_block_buff2)) // copying valid data of youngest cold block to buffer
    {
        printf("cold mig cold head block copy to buffer failed\n");
        exit(1);
        return -1;
    }

    /* head blocks (cold & hot) 삭제 */
    //   printf("step 3\n");
    if (!page_clean_in_block(ptr_ftl_context, ptr_ftl_context->hot_block_ec_head))
    { // erase hot pool oldest block
        printf("cold mig hot head block clean failed\n");
        exit(1);
        return -1;
    }
    if (!page_clean_in_block(ptr_ftl_context, ptr_ftl_context->cold_block_ec_head))
    { // erase cold pool youngest block
        printf("cold mig cold head block clean failed\n");
        exit(1);
        return -1;
    }

    /* cold block <-> hot block data swap  */
    //    printf("step 4\n");
    if (!write_buffer_to_block(ptr_ftl_context, ptr_ftl_context->hot_block_ec_head, ptr_block_buff2))
    {
        printf("cold mig hot head block copy from buffer failed\n"); //write data in buffer to oldest_hot_block
        exit(1);
        return -1;
    }
    if (!write_buffer_to_block(ptr_ftl_context, ptr_ftl_context->cold_block_ec_head, ptr_block_buff1))
    { //write data in buffer to youngest_cold_block
        printf("cold mig hot head block copy from buffer failed\n");
        exit(1);
        return -1;
    }

    /* cold pool로 이동할 hot head block의 rec값 초기화 -> only cold pool */
    //    printf("step 5\n");
    if (!rec_reset(ptr_ftl_context))
    {
        printf("cold mig rec reset failed\n"); //write data in buffer to oldest_hot_block
        exit(1);
        return -1;
    }

    /* block들의 pool 위치 변경 */
    //   printf("step 6\n");
    if (!block_pool_swap(ptr_ftl_context)) // swap two blocks. to the pool tail
    {
        printf("cold mig swap failed\n");
        exit(1);
        return -1;
    }

    /* find new head for each pools */
    //  printf("step 7\n");
    if (!find_new_heads(ptr_ftl_context)) // find new heads for two pools
    {
        printf("finding new hot pool head failed\n");
        exit(1);
        return -1;
    }

    //    printf("step 8\n");
    if (ptr_block_buff1 != NULL)
        free(ptr_block_buff1);
    if (ptr_block_buff2 != NULL)
        free(ptr_block_buff2);

    //   printf("step 9\n");
    return 0;
}

bool block_valid_copy(struct ftl_context_t *ptr_ftl_context, struct flash_block_t *ptr_target_block, uint8_t *ptr_block_buff)
{
    uint32_t loop_page = 0;
    uint8_t *ptr_page_buff = NULL;
    struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;

    if (ptr_target_block == NULL)
    {
        printf("Invalid ptr_target_block in block_valid_copy() !!\n");
        return false;
    }

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
    return true;
}

bool page_clean_in_block(struct ftl_context_t *ptr_ftl_context, struct flash_block_t *ptr_target_block)
{
    struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;

    if (ptr_target_block == NULL)
    {
        printf("Invalid ptr_target_block in page_clean_in_block() !!\n");
        return false;
    }

    blueftl_user_vdevice_block_erase(ptr_vdevice, ptr_target_block->no_bus, ptr_target_block->no_chip, ptr_target_block->no_block);
    ptr_target_block->nr_erase_cnt++;
    ptr_target_block->nr_recent_erase_cnt++; //어차피 초기화 될 예정이긴 한데..
    return true;
}

bool write_buffer_to_block(struct ftl_context_t *ptr_ftl_context, struct flash_block_t *ptr_target_block, uint8_t *ptr_block_buff)
{
    uint32_t loop_page = 0;
    uint8_t *ptr_page_buff = NULL;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;

    if (ptr_target_block == NULL)
    {
        printf("Invalid ptr_target_block in write_buffer_to_block() !!\n");
        return false;
    }

    for (loop_page = 0; loop_page < ptr_ssd->nr_pages_per_block; loop_page++)
    {
        if (ptr_target_block->list_pages[loop_page].page_status == 3)
        {
            ptr_page_buff = ptr_block_buff + (loop_page * ptr_vdevice->page_main_size);

            blueftl_user_vdevice_page_write(
                ptr_vdevice,
                ptr_target_block->no_bus, ptr_target_block->no_chip, ptr_target_block->no_block, loop_page,
                ptr_vdevice->page_main_size,
                (char *)ptr_page_buff);
        }
    }
    return true;
}

bool rec_reset(struct ftl_context_t *ptr_ftl_context)
{
    ptr_ftl_context->hot_block_ec_head->nr_recent_erase_cnt = 0;
    return true;
}

bool block_pool_swap(struct ftl_context_t *ptr_ftl_context)
{
    //여기서 if 인 이유: tail이 최대/최소를 갖도록 하기 위해
    if (ptr_ftl_context->hot_block_ec_head->nr_erase_cnt > ptr_ftl_context->cold_block_ec_tail->nr_erase_cnt)
    {
        ptr_ftl_context->cold_block_ec_tail = ptr_ftl_context->hot_block_ec_head; // hot pool의 head -> cold pool의 tail로 이동
        ptr_ftl_context->cold_block_ec_tail->hot_cold_pool = COLD_POOL;
        ptr_ftl_context->hot_block_ec_head = NULL;                                // 기존 hot pool의 head 초기화
    }
    else
    { // tail은 못 됐지만 cold pool에는 들어감
        ptr_ftl_context->hot_block_ec_head = COLD_POOL;
        ptr_ftl_context->hot_block_ec_head = NULL; // 기존 hot pool의 head 초기화
    }

    if (ptr_ftl_context->cold_block_ec_head->nr_erase_cnt < ptr_ftl_context->hot_block_ec_tail->nr_erase_cnt)
    {
        ptr_ftl_context->hot_block_ec_tail = ptr_ftl_context->cold_block_ec_head; // cold pool의 head -> hot pool의 head로 이동
        ptr_ftl_context->hot_block_ec_tail->hot_cold_pool = HOT_POOL;
        ptr_ftl_context->cold_block_ec_head = NULL;                               // 기존 cold pool의 head 초기화
    }
    else
    {
        ptr_ftl_context->cold_block_ec_head = HOT_POOL;
        ptr_ftl_context->cold_block_ec_head = NULL; // 기존 cold pool의 head 초기화
        
    }

    return true;
}

bool find_new_heads(struct ftl_context_t *ptr_ftl_context)
{
    //전체를 뒤져서 새로운 head를 찾아야 함. 전체 탐색 과정은 어떤 순서일지라도 어쩔 수 없이 포함되긴 해야 하는 듯.
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct flash_block_t *ptr_block = NULL;
    int32_t ec_max = -1;
    int32_t ec_min = ~(1 << 31);
    int i, j, k;
    int max_k = 0;
    int min_k = 0;

    for (i = 0; i < ptr_ssd->nr_buses; i++)
    {
        for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
        {
            for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
            {
                ptr_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);

                if (ptr_block->hot_cold_pool == HOT_POOL)
                {
                    if (ptr_block->nr_erase_cnt > ec_max)
                    {
                        ec_max = ptr_block->nr_erase_cnt;
                        max_k = k;
                    }
                }
                else
                { // ptr_block in cold pool
                    if (ptr_block->nr_erase_cnt < ec_min)
                    {
                        ec_min = ptr_block->nr_erase_cnt;
                        min_k = k;
                    }
                }
            }
        }
    }
    ptr_ftl_context->cold_block_ec_head = &(ptr_ssd->list_buses[0].list_chips[0].list_blocks[min_k]);
    ptr_ftl_context->hot_block_ec_head = &(ptr_ssd->list_buses[0].list_chips[0].list_blocks[max_k]);

    if (ptr_ftl_context->cold_block_ec_head->hot_cold_pool == HOT_POOL)
    {
        printf("Wrong Classification\n");
        exit(1);
    }
    if (ptr_ftl_context->hot_block_ec_head->hot_cold_pool == COLD_POOL)
    {
        printf("Wrong Classification\n");
        exit(1);
    }
    return true;
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

void update_max_min_nr_erase_cnt_in_pool(struct ftl_context_t *ptr_ftl_context, struct flash_block_t *ptr_target_block, int pool, int type, int min_max)
{
    if (ptr_target_block == NULL)
    {
        printf("update_max_min_nr_erase_cnt_in_pool NULL\n");
        exit(1);
    }
    if (pool == HOT_POOL)
    {
        if (type == EC)
        {
            if (min_max == MIN) // HOT EC MIN
            {
                ptr_ftl_context->hot_block_ec_tail = ptr_target_block;
            }
            else // HOT EC MAX
            {
                ptr_ftl_context->hot_block_ec_head = ptr_target_block;
            }
        }
        else
        {
            if (min_max == MIN) // HOT REC MIN
            {
                ptr_ftl_context->hot_block_rec_tail = ptr_target_block;
            }
            else // HOT REC MAX
            {
                ptr_ftl_context->hot_block_rec_head = ptr_target_block;
            }
        }
    }

    if (pool == COLD_POOL) // for cold pool, head is youngest (MIN)
    {
        if (type == EC)
        {
            if (min_max == MIN) // COLD EC MIN
            {
                ptr_ftl_context->cold_block_ec_head = ptr_target_block;
            }
            else // COLD EC MAX
            {
                ptr_ftl_context->cold_block_ec_tail = ptr_target_block;
            }
        }
        else
        {
            if (min_max == MIN) // COLD REC MIN
            {
                ptr_ftl_context->cold_block_rec_head = ptr_target_block;
            }
            else // COLD REC MAX
            {
                ptr_ftl_context->cold_block_rec_tail = ptr_target_block;
            }
        }
    }
}
