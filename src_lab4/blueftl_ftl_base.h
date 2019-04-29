#ifndef _BLUESSD_MAPPING_BASE
#define _BLUESSD_MAPPING_BASE

#include "blueftl_ssdmgmt.h"
#include "blueftl_user_vdevice.h"
#include "blueftl_user.h"

#define MAPPING_POLICY_PAGE 1
#define MAPPING_POLICY_BLOCK 2

#define GC_POLICY_MERGE 1
#define GC_POLICY_RAMDOM 2
#define GC_POLICY_GREEDY 3
#define GC_POLICY_COST_BENEFIT 4

#define WL_POLICY_NONE 1
#define WL_DUAL_POOL 2

struct ftl_context_t
{
	// 이전에 쓰고 있던 block 추적용
	uint32_t recent_bus;
	uint32_t recent_chip;
	uint32_t recent_block;
	uint32_t recent_page;

	// hot/cold 의 head/tail 추적용
	struct flash_block_t *hot_block_ec_max;
	struct flash_block_t *hot_block_ec_min;
	struct flash_block_t *cold_block_ec_max;
	struct flash_block_t *cold_block_ec_min;

	struct flash_block_t *hot_block_rec_max;
	struct flash_block_t *hot_block_rec_min;
	struct flash_block_t *cold_block_rec_max;
	struct flash_block_t *cold_block_rec_min;

	/* all about SSD */
	struct flash_ssd_t *ptr_ssd;

	/* private data for a mapping scheme */
	void *ptr_mapping;
	
	/* private data for a chunk table */
	void *ptr_chunk;

	/* virtual device */
	struct virtual_device_t *ptr_vdevice;
};

struct ftl_base_t
{
	/* the ftl context */
	struct ftl_context_t *ptr_ftl_context;

	/* the functions the FTL must provide */
	struct ftl_context_t *(*ftl_create_ftl_context)(struct virtual_device_t *ptr_vdevice);
	void (*ftl_destroy_ftl_context)(struct ftl_context_t *ptr_ftl_context);

	int32_t (*ftl_get_mapped_physical_page_address)(
		struct ftl_context_t *ptr_ftl_context,
		uint32_t logical_page_address,
		uint32_t *ptr_bus,
		uint32_t *ptr_chip,
		uint32_t *ptr_block,
		uint32_t *ptr_page);

	int32_t (*ftl_get_free_physical_page_address)(
		struct ftl_context_t *ptr_ftl_context,
		uint32_t logical_page_address,
		uint32_t *ptr_bus,
		uint32_t *ptr_chip,
		uint32_t *ptr_block,
		uint32_t *ptr_page);

	int32_t (*ftl_map_logical_to_physical)(
		struct ftl_context_t *ptr_ftl_context,
		uint32_t logical_page_address,
		uint32_t bus,
		uint32_t chip,
		uint32_t block,
		uint32_t page);

	int32_t (*ftl_trigger_gc)( // gc가 수행을 요청함, gc가 수행된 버스와 칩 번호 알려줌
		struct ftl_context_t *ptr_ftl_context,
		int32_t gc_target_bus,
		int32_t gc_target_chip);

	int32_t (*ftl_trigger_merge)( // 대상이 되는 버스와 칩, 블록번호를 알려줌
		struct ftl_context_t *ptr_ftl_context,
		uint32_t logical_page_address,
		uint8_t *ptr_lba_buffer,
		uint32_t merge_bus,
		uint32_t merge_chip,
		uint32_t merge_block);

	void (*ftl_trigger_wear_leveler)(void);
};

void ftl_convert_to_ssd_layout( // 물리주소를 flash의 물리적 위치로 변환
	uint32_t physical_page_address,
	uint32_t *ptr_bus,
	uint32_t *ptr_chip,
	uint32_t *ptr_block,
	uint32_t *ptr_page);

uint32_t ftl_convert_to_physical_page_address( // flash의 울리적 위치를 물리 주소로 변환
	uint32_t bus,
	uint32_t chip,
	uint32_t block,
	uint32_t page);

#endif
