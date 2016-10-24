/*supplementary page table*/

#include <stdbool.h>
#include <stdint.h>
#include <debug.h>

#include <hash.h>
#include <bitmap.h>

#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"

#include "userprog/pagedir.h"

#include "vm/page.h"

static spte *spte_new(const void *vaddr);
static void spte_delete(spte *s);

static spte *spte_new(const void *vaddr) {
  if(!(vaddr && is_user_vaddr(vaddr))) {
    return NULL;
  }

  spte *s = (spte*) malloc(sizeof(spte));
  if(!s) {
    debug_panic("page.c", 22, "spte_new", "supplementary page table entry memory allocation failed!");
  }

  s->vaddr = (void*) vaddr;
  s->page_loc = PAGE_NOWHERE;
  s->isPinned = false;
  s->swap_i = -1;
  s->read_bytes = -1;
  s->zero_bytes = -1;
  s->ofs = -1;
  s->isWritable = false;
  return s;
}

static void spte_delete(spte *s) {
  if(!s) {
    return;
  }

  free(s);
}



static unsigned hash_spte_hash(const struct hash_elem *e, void *aux);
static bool hash_spte_less(const struct hash_elem *a,
                            const struct hash_elem *b,
                            void *aux);
static void hash_spte_delete(struct hash_elem *e, void *aux);


static unsigned hash_spte_hash(const struct hash_elem *e, void *aux UNUSED) {
  spte *s = hash_entry(e, spte, hash_elem);
  return hash_int((int) s->vaddr);
}

static bool hash_spte_less(const struct hash_elem *a,
                            const struct hash_elem *b,
			    void *aux UNUSED) {
  spte *ca = hash_entry(a, spte, hash_elem);
  spte *cb = hash_entry(b, spte, hash_elem);

  return ca->vaddr < cb->vaddr;
}

static void hash_spte_delete(struct hash_elem *e, void *aux UNUSED) {
  spte *s = hash_entry(e, spte, hash_elem);
  spte_delete(s);
}

/*spt is lazily loaded. sptes should be added manually*/
struct hash *spt_new(void) {
  struct hash *spt = (struct hash*) malloc(sizeof(struct hash));
  hash_init(spt, hash_spte_hash, hash_spte_less, NULL);

  return spt;
}

void spt_delete(struct hash *spt) {
  if(spt) {
    hash_destroy(spt, hash_spte_delete);
  }
}

spte *spt_addSpte(struct hash *spt, const void *vaddr) {
  spte *s = spte_new(vaddr);
  
  struct hash_elem *e = hash_insert(spt, &s->hash_elem);
  if(e) {
    spte_delete(s);
  }

  return e? hash_entry(e, spte, hash_elem): NULL;
}

spte *spt_getSpte(struct hash *spt, const void *vaddr) {
  spte p;
  struct hash_elem *e;

  p.vaddr = (void*) vaddr;
  e = hash_find (spt, &p.hash_elem);
  return e? hash_entry (e, spte, hash_elem): NULL;
}

void spt_deleteSpte(struct hash *spt, const void *vaddr) {
  spte *s = spt_getSpte(spt, vaddr);
  if(s) {
    hash_delete (spt, &s->hash_elem);
    spte_delete(s);
  }
}
