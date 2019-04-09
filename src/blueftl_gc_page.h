#ifndef _BLUESSD_GC_BLOCK
#define _BLUESSD_GC_BLOCK

#include <linux/types.h>
#include "blueftl_ftl_base.h"

int32_t gc_page_trigger_gc_lab (
	struct ftl_context_t* ptr_ftl_context,
	uint32_t victim_bus, 
	uint32_t victim_chip);
	
void select_victim_block (
	struct flash_ssd_t * ptr_ssd,
	uint32_t victim_bus,
	uint32_t victim_chip,
	uint32_t * victim_block,
	int select_case);
#endif
