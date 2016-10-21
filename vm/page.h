#include <stdint.h>
#include <hash.h>

typedef struct struct_spte {
  struct hash_elem elem;

  uint32_t *vaddr;

  bool evictable;
  bool canEvict;
  uint8_t status; /*replacement policy value*/
} spte;

void spt_new();
void spt_delete();
spte *spt_getSpte (const void *vaddr);
void spt_deleteSpte(const void *vaddr);
void spt_deleteSpte(const void *vaddr);
