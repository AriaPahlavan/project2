#include <stdint.h>
#include <hash.h>

typedef enum enum_page_loc_t {
  PAGE_IN_MEM,
  PAGE_IN_SWAP,
  PAGE_IN_DSK
} page_loc_t;

typedef struct struct_spte {
  struct hash_elem elem;

  void *vaddr;
  page_loc_t page_loc; /*location of the page data*/
  bool isPinned;
} spte;

struct hash *spt_new(void);
void spt_delete(struct hash *spt);

spte *spt_addSpte(struct hash *spt, const void *vaddr);
spte *spt_getSpte(struct hash *spt, const void *vaddr);
void spt_deleteSpte(struct hash *spt, const void *vaddr);
