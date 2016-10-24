#include <stdint.h>
#include <list.h>
#include <hash.h>
#include "filesys/file.h"

#ifndef PAGE_H
#define PAGE_H

typedef enum enum_page_loc_t {
  PAGE_NOWHERE, /*default location when a spte is first created*/
  PAGE_IN_MEM,
  PAGE_IN_SWAP,
  PAGE_IN_DSK
} page_loc_t;

typedef struct struct_spte {
  struct list_elem list_elem;
  struct hash_elem hash_elem;
  size_t swap_i;
  void *vaddr;
  page_loc_t page_loc;
  bool isPinned; /*keeps the frame from being evicted in case a syscall needs a
		  guarantee that the frame will not be invalidated*/
  uint32_t read_bytes, zero_bytes;
  off_t ofs;
  bool isWritable;
} spte;

struct hash *spt_new(void);
void spt_delete(struct hash *spt);

spte *spt_addSpte(struct hash *spt, const void *vaddr);
spte *spt_getSpte(struct hash *spt, const void *vaddr);
void spt_deleteSpte(struct hash *spt, const void *vaddr);

#endif /*vm/page.h*/
