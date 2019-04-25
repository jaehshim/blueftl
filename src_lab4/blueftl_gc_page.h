#include "blueftl_ftl_base.h"
#include "blueftl_ssdmgmt.h"
#include "blueftl_user_vdevice.h"

struct flash_block_t* gc_page_select_victim_random (
	struct ftl_context_t* ptr_ftl_context,
	int32_t gc_target_bus, 
	int32_t gc_target_chip);

struct flash_block_t* gc_page_select_victim_greedy (
	struct ftl_context_t* ptr_ftl_context,
	int32_t gc_target_bus, 
	int32_t gc_target_chip);

struct flash_block_t* gc_page_select_victim_cost_benefit (
	struct ftl_context_t* ptr_ftl_context,
	int32_t gc_target_bus, 
	int32_t gc_target_chip);

int32_t gc_page_trigger_gc_lab (
	struct ftl_context_t* ptr_ftl_context,
	int32_t gc_target_bus,
	int32_t gc_target_chip);


