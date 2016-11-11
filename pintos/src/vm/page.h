

struct supp_page_entry{
	uint8_t *addr;
	bool writable;
	struct hash_elem he;

};

void supp_page_init();
struct supp_page_entry* supp_page_add();
//void supp_page_get();
bool supp_page_remove();
