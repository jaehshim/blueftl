#define chunk_size 4

#define COMP_FALSE 0
#define COMP_TRUE 1


struct comp_header_t
{
    uint32_t comp_lba_array[chunk_size];

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

