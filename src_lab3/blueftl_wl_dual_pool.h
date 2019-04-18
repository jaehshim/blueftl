#ifndef BLUEFTL_WL_DUAL_POOL
#define BLUEFTL_WL_DUAL_POOL

#ifndef TRUE
#define TRUE 	1
#endif
#ifndef FALSE
#define FALSE 	0
#endif  

#define	WEAR_LEVELING_THRESHOLD 5

#define ERASURE_COUNT  		 0
#define RECENT_ERASURE_COUNT 1

#define MAX 			     0
#define MIN  				 1 

#define EC					0
#define REC					1

#define HOT_POOL 			 0
#define COLD_POOL 			 1
#define HOT_REC_POOL 		 2
#define COLD_REC_POOL 		 3


#define NONE 				 0x00
#define HEAD 				 0x0F
#define TAIL 				 0xF0

#define IS_HEAD(a) (a&HEAD)
#define DISABLE_HEAD(a) (a=(a&0xF0))
#define IS_TAIL(a) (a&TAIL)
#define DISABLE_TAIL(a) (a=(a&0x0F))
#define 	INIT_CHAR_VAL 			0xFF
#define NUM_OF_POOL 		4

typedef struct _block_info_node{
	uint32_t 			no_block;
	uint32_t 			no_chip;
	uint32_t 			no_bus;
	uint32_t    		nr_erase_cnt;
} dual_pool_block_info;

/*
struct ftl_wl_t {
	void (* cold_data_migration) (struct ftl_context_t* ptr_ftl_context_t);
	int (* check_cold_data_migration) (void);
	uint32_t (* update_max_min_nr_erase_cnt_in_pool) ( int pool, int type, int min_max, int bus, int chip, int block, uint32_t erasure_count);
	int (* check_cold_pool_adjustment) (void);
	int (* check_hot_pool_adjustment)(void);
	void (* cold_pool_adjustment) (struct ftl_context_t *ptr_ftl_context_t);
	void (* hot_pool_adjustment) (struct ftl_context_t *ptr_ftl_context_t);
	void (* insert_pool)(struct ftl_context_t* ptr_ftl_context_t, struct flash_block_t* ptr_erase_block); 
	void (* init_global_wear_leveling_metadata) (void); 
	struct flash_block_t * (* get_min_max_ptr)(struct ftl_context_t *ptr_ftl_context, dual_pool_block_info *pool_info);
	struct flash_block_t * (* get_erase_blk_ptr)(struct ftl_context_t *ptr_ftl_context, uint32_t target_bus, uint32_t target_chip, uint32_t target_block);
	int (* block_copy) (struct flash_block_t *ptr_src_block, struct flash_block_t *ptr_tgt_block, struct ftl_context_t *ptr_ftl_context);
};
*/
void check_max_min_nr_erase_cnt(struct ftl_context_t *ptr_ftl_context, struct flash_block_t* ptr_erase_block);
int check_cold_data_migration(struct ftl_context_t *ptr_ftl_context);
int32_t cold_data_migration(struct ftl_context_t* ptr_ftl_context_t);
void update_max_min_nr_erase_cnt_in_pool(struct ftl_context_t *ptr_ftl_context, struct flash_block_t *ptr_target_block, int pool, int type, int min_max);
int check_cold_pool_adjustment(struct ftl_context_t *ptr_ftl_context);
int check_hot_pool_adjustment(struct ftl_context_t *ptr_ftl_context);
void cold_pool_adjustment(struct ftl_context_t *ptr_ftl_context_t);
void hot_pool_adjustment(struct ftl_context_t *ptr_ftl_context_t);
struct flash_block_t *get_min_max_ptr(struct ftl_context_t *ptr_ftl_context, dual_pool_block_info *pool_info);
struct flash_block_t *get_erase_blk_ptr(struct ftl_context_t *ptr_ftl_context, uint32_t target_bus, uint32_t target_chip, uint32_t target_block);


//added functions
//int block_data_swap(struct ftl_context_t *ptr_ftl_context);
int block_to_block_transfer(struct ftl_context_t *ptr_ftl_context, struct flash_block_t * ptr_victim_block, struct flash_block_t * ptr_reserved_block);
int rec_reset(struct ftl_context_t *ptr_ftl_context);
int block_pool_swap(struct ftl_context_t *ptr_ftl_context);
void find_new_ec_max(struct ftl_context_t *ptr_ftl_context, int pool, struct flash_block_t ** ptr_block);
void find_new_ec_min(struct ftl_context_t *ptr_ftl_context, int pool, struct flash_block_t ** ptr_block);
void find_new_rec_max(struct ftl_context_t *ptr_ftl_context, int pool, struct flash_block_t ** ptr_block);
void find_new_rec_min(struct ftl_context_t *ptr_ftl_context, int pool, struct flash_block_t ** ptr_block);

#endif
