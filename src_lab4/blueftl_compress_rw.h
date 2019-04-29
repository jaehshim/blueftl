#define CHUNK_SIZE 4

#define COMP_FALSE 0
#define COMP_TRUE 1


struct comp_header_t
{
    uint32_t comp_lba_array[CHUNK_SIZE];

};

struct chunk_entry_t
{
    uint32_t valid_page_count;
    uint32_t nr_physical_pages;
    uint32_t comp_indi;
}


struct chunk_table_t
{
	uint32_t nr_chunk_table_entries;	/* the number of pages that belong to the page mapping table */
	struct chunk_entry_t* ptr_ch_table; /* for the page mapping */
};

uint32_t init_chunk_table(struct ftl_context_t *ptr_ftl_context);
void blueftl_compressed_page_read(
    struct ftl_context_t *ptr_ftl_context,
    struct virtual_device_t *ptr_vdevice,
    int32_t requested_lpa,
    int32_t bus,
    int32_t chip,
    int32_t block,
    int32_t page,
    int32_t page_length,
    char *ptr_page_data);