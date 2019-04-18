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

// /* Hot Pool Status Variable */
// dual_pool_block_info g_max_ec_in_hot_pool;
// dual_pool_block_info g_min_ec_in_hot_pool;
// dual_pool_block_info g_max_rec_in_hot_pool;
// dual_pool_block_info g_min_rec_in_hot_pool;

// /* Cold Pool Status Variable */
// dual_pool_block_info g_max_ec_in_cold_pool;
// dual_pool_block_info g_min_ec_in_cold_pool;
// dual_pool_block_info g_max_rec_in_cold_pool;
// dual_pool_block_info g_min_rec_in_cold_pool;

bool check_cold_pool_adjustment(struct ftl_context_t *ptr_ftl_context)
{
    if (ptr_ftl_context->cold_block_rec_max->nr_recent_erase_cnt - ptr_ftl_context->hot_block_rec_min->nr_recent_erase_cnt > WEAR_LEVELING_THRESHOLD)
        return true;
    else
        return false;
}

bool check_hot_pool_adjustment(struct ftl_context_t *ptr_ftl_context)
{
    if (ptr_ftl_context->hot_block_ec_max->nr_erase_cnt - ptr_ftl_context->hot_block_ec_min->nr_erase_cnt > 2 * WEAR_LEVELING_THRESHOLD)
        return true;
    else
        return false;
}

bool check_cold_data_migration(struct ftl_context_t *ptr_ftl_context)
{
    if (ptr_ftl_context->hot_block_ec_max->nr_erase_cnt - ptr_ftl_context->cold_block_ec_min->nr_erase_cnt > WEAR_LEVELING_THRESHOLD)
        return true;
    else
        return false;
}

void check_max_min_nr_erase_cnt(struct ftl_context_t *ptr_ftl_context, struct flash_block_t *ptr_erase_block)
{
    if (ptr_erase_block->hot_cold_pool == HOT_POOL)
    {
        if (ptr_ftl_context->hot_block_ec_min->nr_erase_cnt > ptr_erase_block->nr_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, EC, MIN);
        }
        else if (ptr_ftl_context->hot_block_ec_max->nr_erase_cnt < ptr_erase_block->nr_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, EC, MAX);
        }

        if (ptr_ftl_context->hot_block_rec_min->nr_recent_erase_cnt > ptr_erase_block->nr_recent_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, REC, MIN);
        }
        else if (ptr_ftl_context->hot_block_rec_max->nr_recent_erase_cnt < ptr_erase_block->nr_recent_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, HOT_POOL, REC, MAX);
        }
    }
    else // COLD_POOL
    {
        if (ptr_ftl_context->cold_block_ec_min->nr_erase_cnt > ptr_erase_block->nr_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, COLD_POOL, EC, MIN);
        }
        else if (ptr_ftl_context->cold_block_ec_max->nr_erase_cnt < ptr_erase_block->nr_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, COLD_POOL, EC, MAX);
        }

        if (ptr_ftl_context->cold_block_rec_min->nr_recent_erase_cnt > ptr_erase_block->nr_recent_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, COLD_POOL, REC, MIN);
        }
        else if (ptr_ftl_context->cold_block_rec_max->nr_recent_erase_cnt < ptr_erase_block->nr_recent_erase_cnt)
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, ptr_erase_block, COLD_POOL, REC, MAX);
        }
    }
}

void cold_pool_adjustment(struct ftl_context_t *ptr_ftl_context)
{
    struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct flash_block_t * ptr_block = ptr_ftl_context->cold_block_rec_max;

    ptr_block->hot_cold_pool = HOT_POOL;

    /* compare with ec head & tail in hot pool */
    if (ptr_block->nr_erase_cnt > ptr_ftl_context->hot_block_ec_max->nr_erase_cnt)
    {
        ptr_ftl_context->hot_block_ec_max = ptr_block;
    }
    else if (ptr_block->nr_erase_cnt < ptr_ftl_context->hot_block_ec_min->nr_erase_cnt)
    {
        ptr_ftl_context->hot_block_ec_min = ptr_block;
    }

    /* compare with rec head & tail in hot pool */
    if (ptr_block->nr_recent_erase_cnt > ptr_ftl_context->hot_block_rec_max->nr_recent_erase_cnt)
    {
        ptr_ftl_context->hot_block_rec_max = ptr_block;
    }
    else if (ptr_block->nr_recent_erase_cnt < ptr_ftl_context->hot_block_rec_min->nr_recent_erase_cnt)
    {
        ptr_ftl_context->hot_block_rec_min = ptr_block;
    }

    /* 새로운 rec max 찾아주기 */
    ptr_ftl_context->cold_block_rec_max = NULL;
    find_new_rec_max(ptr_ftl_context, COLD_POOL, &(ptr_ftl_context->cold_block_rec_max));
    
    /* 기존의 rec tail이 cold ec_head 또는 cold ec_tail이였을 경우 대체 block 찾아줌 */
    if (ptr_block == ptr_ftl_context->cold_block_ec_min) {
        find_new_ec_min(ptr_ftl_context, COLD_POOL, &(ptr_ftl_context->cold_block_ec_min));
    }
    if (ptr_block == ptr_ftl_context->cold_block_ec_max) {
        find_new_ec_max(ptr_ftl_context, COLD_POOL, &(ptr_ftl_context->cold_block_ec_max));
    }
}

void hot_pool_adjustment(struct ftl_context_t *ptr_ftl_context)
{
    struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct flash_block_t * ptr_block = ptr_ftl_context->hot_block_ec_min;

    ptr_block->hot_cold_pool = COLD_POOL;

    /* compare with ec head & tail in cold pool */
    if (ptr_block->nr_erase_cnt > ptr_ftl_context->cold_block_ec_max->nr_erase_cnt)
    {
        ptr_ftl_context->cold_block_ec_max = ptr_block;
    }
    else if (ptr_block->nr_erase_cnt < ptr_ftl_context->cold_block_ec_min->nr_erase_cnt)
    {
        ptr_ftl_context->cold_block_ec_min = ptr_block;
    }

    /* compare with rec head & tail in cold pool */
    if (ptr_block->nr_recent_erase_cnt > ptr_ftl_context->cold_block_rec_max->nr_recent_erase_cnt)
    {
        ptr_ftl_context->cold_block_rec_max = ptr_block;
    }
    else if (ptr_block->nr_recent_erase_cnt < ptr_ftl_context->cold_block_rec_min->nr_recent_erase_cnt)
    {
        ptr_ftl_context->cold_block_rec_min = ptr_block;
    }

    /* 새로운 ec min 찾아주기 */
    ptr_ftl_context->hot_block_ec_min = NULL;
    find_new_rec_max(ptr_ftl_context, COLD_POOL, &(ptr_ftl_context->hot_block_ec_min));
}

int32_t cold_data_migration(struct ftl_context_t *ptr_ftl_context)
{
    struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct ftl_page_mapping_context_t* ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;
    struct flash_block_t * ptr_reserved_block = ptr_pg_mapping->ptr_gc_block;

    struct flash_block_t * ptr_hot_head_block = ptr_ftl_context->hot_block_ec_max;
    struct flash_block_t * ptr_cold_head_block = ptr_ftl_context->cold_block_ec_min;

 //   printf("hot : %d, cold : %d, reserved : %d, active : %d\n", ptr_hot_head_block->no_block, ptr_cold_head_block->no_block, ptr_reserved_block->no_block, ptr_pg_mapping->ptr_active_block->no_block);
    /* buffer로 block의 데이터 copy */
    if (!block_to_block_transfer(ptr_ftl_context, ptr_hot_head_block, ptr_reserved_block))
    {
        printf("cold mig head blocks copy to buffer failed\n");
        return -1;
    }

    if (!block_to_block_transfer(ptr_ftl_context, ptr_cold_head_block, ptr_hot_head_block))
    {
        printf("cold mig head blocks copy to buffer failed\n");
        return -1;
    }

    if (!block_to_block_transfer(ptr_ftl_context, ptr_reserved_block, ptr_cold_head_block))
    {
        printf("cold mig head blocks copy to buffer failed\n");
        return -1;
    }

    /* active block 재지정 */
    if (ptr_hot_head_block == ptr_pg_mapping->ptr_active_block)
        ptr_pg_mapping->ptr_active_block = ptr_cold_head_block;
    else if (ptr_cold_head_block == ptr_pg_mapping->ptr_active_block)
        ptr_pg_mapping->ptr_active_block = ptr_hot_head_block;
    
    /* cold pool로 이동할 hot head block의 rec값 초기화 -> only cold pool */
    if (!rec_reset(ptr_ftl_context))
    {
        printf("cold mig rec reset failed\n"); //write data in buffer to oldest_hot_block
        exit(1);
        return -1;
    }

    /* block들의 pool 위치 변경 */
    if (!block_pool_swap(ptr_ftl_context)) // swap two blocks. to the pool tail
    {
        printf("cold mig swap failed\n");
        exit(1);
        return -1;
    }

    /* find new head for each pools */
    find_new_ec_max(ptr_ftl_context, HOT_POOL, &(ptr_ftl_context->hot_block_ec_max));
    find_new_ec_min(ptr_ftl_context, COLD_POOL, &(ptr_ftl_context->cold_block_ec_min));

    return 0;
}

void find_new_ec_max(struct ftl_context_t *ptr_ftl_context, int pool, struct flash_block_t ** ptr_block) {
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct flash_block_t *ptr_target_block = NULL;

    int32_t ec_max = -1;
    int i, j, k;
    int num_k = 0;
    
    for (i = 0; i < ptr_ssd->nr_buses; i++)
    {
        for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
        {
            for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
            {
                ptr_target_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);

                if (ptr_target_block->hot_cold_pool == pool)
                {
                    if (ptr_target_block->nr_erase_cnt > ec_max && ptr_target_block->is_reserved_block == 0)
                    {
                        ec_max = ptr_target_block->nr_erase_cnt;
                        num_k = k;
                    }
                }
            }
        }
    }

    *ptr_block = &(ptr_ssd->list_buses[0].list_chips[0].list_blocks[num_k]);
}

void find_new_ec_min(struct ftl_context_t *ptr_ftl_context, int pool, struct flash_block_t ** ptr_block) {
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct flash_block_t *ptr_target_block = NULL;

    int32_t ec_min = ~(1 << 31);
    int i, j, k;
    int num_k = 0;
    
    for (i = 0; i < ptr_ssd->nr_buses; i++)
    {
        for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
        {
            for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
            {
                ptr_target_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);

                if (ptr_target_block->hot_cold_pool == pool)
                {
                    if (ptr_target_block->nr_erase_cnt < ec_min && ptr_target_block->is_reserved_block == 0)
                    {
                        ec_min = ptr_target_block->nr_erase_cnt;
                        num_k = k;
                    }
                }
            }
        }
    }

    *ptr_block = &(ptr_ssd->list_buses[0].list_chips[0].list_blocks[num_k]);
}

void find_new_rec_max(struct ftl_context_t *ptr_ftl_context, int pool, struct flash_block_t ** ptr_block) {
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct flash_block_t *ptr_target_block = NULL;

    int32_t rec_max = -1;
    int i, j, k;
    int num_k = 0;
    
    for (i = 0; i < ptr_ssd->nr_buses; i++)
    {
        for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
        {
            for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
            {
                ptr_target_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);

                if (ptr_target_block->hot_cold_pool == pool)
                {
                    if (ptr_target_block->nr_recent_erase_cnt > rec_max && ptr_target_block->is_reserved_block == 0)
                    {
                        rec_max = ptr_target_block->nr_recent_erase_cnt;
                        num_k = k;
                    }
                }
            }
        }
    }

    *ptr_block = &(ptr_ssd->list_buses[0].list_chips[0].list_blocks[num_k]);
}

void find_new_rec_min(struct ftl_context_t *ptr_ftl_context, int pool, struct flash_block_t ** ptr_block) {
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct flash_block_t *ptr_target_block = NULL;

    int32_t rec_min = ~(1 << 31);
    int i, j, k;
    int num_k = 0;
    
    for (i = 0; i < ptr_ssd->nr_buses; i++)
    {
        for (j = 0; j < ptr_ssd->nr_chips_per_bus; j++)
        {
            for (k = 0; k < ptr_ssd->nr_blocks_per_chip; k++)
            {
                ptr_target_block = &(ptr_ssd->list_buses[i].list_chips[j].list_blocks[k]);

                if (ptr_target_block->hot_cold_pool == pool)
                {
                    if (ptr_target_block->nr_recent_erase_cnt < rec_min && ptr_target_block->is_reserved_block == 0)
                    {
                        rec_min = ptr_target_block->nr_recent_erase_cnt;
                        num_k = k;
                    }
                }
            }
        }
    }

    *ptr_block = &(ptr_ssd->list_buses[0].list_chips[0].list_blocks[num_k]);
}

bool block_to_block_transfer(struct ftl_context_t *ptr_ftl_context, struct flash_block_t *ptr_victim_block, struct flash_block_t *ptr_reserved_block)
{
    struct flash_ssd_t *ptr_ssd = ptr_ftl_context->ptr_ssd;
    struct ftl_page_mapping_context_t *ptr_pg_mapping = (struct ftl_page_mapping_context_t *)ptr_ftl_context->ptr_mapping;
    struct virtual_device_t *ptr_vdevice = ptr_ftl_context->ptr_vdevice;

    uint32_t loop_page = 0;
    uint32_t loop_page_victim = 0;
    uint32_t loop_page_gc = 0;
    uint32_t logical_page_address;
    unsigned char gc_buff[FLASH_PAGE_SIZE];

    if (ptr_victim_block == NULL || ptr_reserved_block == NULL)
    {
        printf("Invalid head blocks!!\n");
        return false;
    }

    if (ptr_reserved_block->nr_free_pages != ptr_ssd->nr_pages_per_block)
    {
        printf("[ERROR] the gc block should be empty.\n");
    }

    /*------------step 1------------*/
    /* hot block copy to reserved block */
    for (loop_page_victim = 0; loop_page_victim < ptr_ssd->nr_pages_per_block; loop_page_victim++)
    {
        if (ptr_victim_block->list_pages[loop_page_victim].page_status == PAGE_STATUS_VALID)
        {
            logical_page_address = ptr_victim_block->list_pages[loop_page_victim].no_logical_page_addr;

            blueftl_user_vdevice_page_read(
                ptr_vdevice,
                ptr_victim_block->no_bus,
                ptr_victim_block->no_chip,
                ptr_victim_block->no_block,
                loop_page_victim,
                FLASH_PAGE_SIZE,
                (char *)gc_buff);

            blueftl_user_vdevice_page_write(
                ptr_vdevice,
                ptr_reserved_block->no_bus,
                ptr_reserved_block->no_chip,
                ptr_reserved_block->no_block,
                loop_page_gc,
                FLASH_PAGE_SIZE,
                (char *)gc_buff);

            page_mapping_map_logical_to_physical(
                ptr_ftl_context,
                logical_page_address,
                ptr_reserved_block->no_bus,
                ptr_reserved_block->no_chip,
                ptr_reserved_block->no_block,
                loop_page_gc);

            // check whether the page has been moved succesfully.
            if (ptr_victim_block->list_pages[loop_page_victim].page_status != PAGE_STATUS_INVALID)
            {
                printf("[ERROR] the victim page is not invalidated\n");
            }

            if (ptr_reserved_block->list_pages[loop_page_gc].page_status != PAGE_STATUS_VALID)
            {
                printf("[ERROR] the gc page is not valid\n");
            }

            loop_page_gc++;

            perf_wl_inc_page_copies();
        }
    }

    // erase hot block & initialize
    blueftl_user_vdevice_block_erase(
        ptr_vdevice,
        ptr_victim_block->no_bus,
        ptr_victim_block->no_chip,
        ptr_victim_block->no_block);

    perf_wl_inc_blk_erasures();

    ptr_victim_block->nr_free_pages = ptr_ssd->nr_pages_per_block;
    ptr_victim_block->nr_valid_pages = 0;
    ptr_victim_block->nr_invalid_pages = 0;
    ptr_victim_block->nr_erase_cnt++;
    ptr_victim_block->nr_recent_erase_cnt++;
    ptr_victim_block->last_modified_time = 0;

    for (loop_page = 0; loop_page < ptr_ssd->nr_pages_per_block; loop_page++)
    {
        ptr_victim_block->list_pages[loop_page].no_logical_page_addr = -1;
        ptr_victim_block->list_pages[loop_page].page_status = PAGE_STATUS_FREE;
    }

    return true;
}

bool rec_reset(struct ftl_context_t *ptr_ftl_context)
{
    ptr_ftl_context->hot_block_ec_max->nr_recent_erase_cnt = 0;
    return true;
}

bool block_pool_swap(struct ftl_context_t *ptr_ftl_context)
{
    //여기서 if 인 이유: tail이 최대/최소를 갖도록 하기 위해
    if (ptr_ftl_context->hot_block_ec_max->nr_erase_cnt > ptr_ftl_context->cold_block_ec_max->nr_erase_cnt)
    {
        ptr_ftl_context->cold_block_ec_max = ptr_ftl_context->hot_block_ec_max; // hot pool의 head -> cold pool의 tail로 이동
        ptr_ftl_context->cold_block_ec_max->hot_cold_pool = COLD_POOL;
        ptr_ftl_context->hot_block_ec_max = NULL; // 기존 hot pool의 head 초기화
    }
    else
    { // tail은 못 됐지만 cold pool에는 들어감
        ptr_ftl_context->hot_block_ec_max->hot_cold_pool = COLD_POOL;
        ptr_ftl_context->hot_block_ec_max = NULL; // 기존 hot pool의 head 초기화
    }

    if (ptr_ftl_context->cold_block_ec_min->nr_erase_cnt < ptr_ftl_context->hot_block_ec_min->nr_erase_cnt)
    {
        ptr_ftl_context->hot_block_ec_min = ptr_ftl_context->cold_block_ec_min; // cold pool의 head -> hot pool의 head로 이동
        ptr_ftl_context->hot_block_ec_min->hot_cold_pool = HOT_POOL;
        ptr_ftl_context->cold_block_ec_min = NULL; // 기존 cold pool의 head 초기화
    }
    else
    {
        ptr_ftl_context->cold_block_ec_min->hot_cold_pool = HOT_POOL;
        ptr_ftl_context->cold_block_ec_min = NULL; // 기존 cold pool의 head 초기화
    }

    return true;
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
                ptr_ftl_context->hot_block_ec_min = ptr_target_block;
            }
            else // HOT EC MAX
            {
                ptr_ftl_context->hot_block_ec_max = ptr_target_block;
            }
        }
        else
        {
            if (min_max == MIN) // HOT REC MIN
            {
                ptr_ftl_context->hot_block_rec_min = ptr_target_block;
            }
            else // HOT REC MAX
            {
                ptr_ftl_context->hot_block_rec_max = ptr_target_block;
            }
        }
    }

    if (pool == COLD_POOL) // for cold pool, head is youngest (MIN)
    {
        if (type == EC)
        {
            if (min_max == MIN) // COLD EC MIN
            {
                ptr_ftl_context->cold_block_ec_min = ptr_target_block;
            }
            else // COLD EC MAX
            {
                ptr_ftl_context->cold_block_ec_max = ptr_target_block;
            }
        }
        else
        {
            if (min_max == MIN) // COLD REC MIN
            {
                ptr_ftl_context->cold_block_rec_min = ptr_target_block;
            }
            else // COLD REC MAX
            {
                ptr_ftl_context->cold_block_rec_max = ptr_target_block;
            }
        }
    }
}
