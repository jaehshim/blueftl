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
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, HOT_POOL, EC, MIN, ptr_erase_block->no_bus, ptr_erase_block->no_chip, ptr_erase_block->no_block, ptr_erase_block->nr_erase_cnt);
            ptr_erase_block->head_or_tail_ec = TAIL;
        }
        else if (g_max_ec_in_hot_pool.nr_erase_cnt < ptr_erase_block->nr_erase_cnt) // min값 changed, ec HEAD in hot pool change
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, HOT_POOL, EC, MAX, ptr_erase_block->no_bus, ptr_erase_block->no_chip, ptr_erase_block->no_block, ptr_erase_block->nr_erase_cnt);
            ptr_erase_block->head_or_tail_ec = HEAD;
        }

        if (g_min_rec_in_hot_pool.nr_erase_cnt > ptr_erase_block->nr_erase_cnt) // max값 changed, rec TAIL in hot pool change
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, HOT_POOL, REC, MIN, ptr_erase_block->no_bus, ptr_erase_block->no_chip, ptr_erase_block->no_block, ptr_erase_block->nr_erase_cnt);
            ptr_erase_block->head_or_tail_rec = TAIL;
        }
        else if (g_max_rec_in_hot_pool.nr_erase_cnt < ptr_erase_block->nr_erase_cnt) // min값 changed, rec HEAD in hot pool change
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, HOT_POOL, REC, MAX, ptr_erase_block->no_bus, ptr_erase_block->no_chip, ptr_erase_block->no_block, ptr_erase_block->nr_erase_cnt);
            ptr_erase_block->head_or_tail_rec = HEAD;
        }
    }
    else // COLD_POOL
    {
         if (g_min_ec_in_cold_pool.nr_erase_cnt > ptr_erase_block->nr_erase_cnt) // max값 changed, ec TAIL in hot pool change
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, COLD_POOL, EC, MIN, ptr_erase_block->no_bus, ptr_erase_block->no_chip, ptr_erase_block->no_block, ptr_erase_block->nr_erase_cnt);
            ptr_erase_block->head_or_tail_ec = TAIL;
        }
        else if (g_min_ec_in_cold_pool.nr_erase_cnt < ptr_erase_block->nr_erase_cnt) // min값 changed, ec HEAD in hot pool change
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, COLD_POOL, EC, MAX, ptr_erase_block->no_bus, ptr_erase_block->no_chip, ptr_erase_block->no_block, ptr_erase_block->nr_erase_cnt);
            ptr_erase_block->head_or_tail_ec = HEAD;
        }

        if (g_min_rec_in_cold_pool.nr_erase_cnt > ptr_erase_block->nr_erase_cnt) // max값 changed, rec TAIL in hot pool change
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, COLD_POOL, REC, MIN, ptr_erase_block->no_bus, ptr_erase_block->no_chip, ptr_erase_block->no_block, ptr_erase_block->nr_erase_cnt);
            ptr_erase_block->head_or_tail_rec = TAIL;
        }
        else if (g_min_rec_in_cold_pool.nr_erase_cnt < ptr_erase_block->nr_erase_cnt) // min값 changed, rec HAID in hot pool change
        {
            update_max_min_nr_erase_cnt_in_pool(ptr_ftl_context, COLD_POOL, REC, MAX, ptr_erase_block->no_bus, ptr_erase_block->no_chip, ptr_erase_block->no_block, ptr_erase_block->nr_erase_cnt);
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

bool block_copy(struct flash_block_t *ptr_src_block, struct flash_block_t *ptr_tgt_block, struct ftl_context_t *ptr_ftl_context)
{
}

bool page_clean_in_block(struct flash_block_t *ptr_block, struct ftl_context_t *ptr_ftl_context)
{
}

void cold_data_migration(struct ftl_context_t *ptr_ftl_context)
{
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

uint32_t update_max_min_nr_erase_cnt_in_pool(struct ftl_context_t *ptr_ftl_context, int pool, int type, int min_max, int bus, int chip, int block, uint32_t nr_erase_cnt)
{
    int i, j, k;
    struct flash_block_t * ptr_block = NULL;
    struct flash_ssd_t * ptr_ssd = ptr_ftl_context->ptr_ssd;

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
