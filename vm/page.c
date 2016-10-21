/*supplementary page table*/

#include <stdbool.h>
#include <stdint.h>
#include <debug.h>

#include <hash.h>
#include <bitmap.h>

#include <thread.h>
#include <malloc.h>
#include <vaddr.h>

#include <pagedir.h>

#include "page.h"

static spte *spte_new(const void *vaddr);
static void spte_delete(spte *s);

static spte *spte_new(const void *vaddr) {
  if(!(vaddr && is_user_vaddr(vaddr))) {
    return NULL;
  }

  spte *s;
  if(!(s = (spte*) malloc(sizeof(spte)))) {
    debug_panic("page.c", 22, "spte_new", "supplementary page table entry memory allocation failed!");
  }

  s->vaddr = (uint32_t*) vaddr;

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
  spte *s = hash_entry(e, spte, elem);
  return hash_int((int) s->vaddr);
}

static bool hash_spte_less(const struct hash_elem *a,
                            const struct hash_elem *b,
			    void *aux UNUSED) {
  spte *ca = hash_entry(a, spte, elem);
  spte *cb = hash_entry(b, spte, elem);

  return ca->vaddr < cb->vaddr;
}

static void hash_spte_delete(struct hash_elem *e, void *aux UNUSED) {
  spte *s = hash_entry(e, spte, elem);
  spte_delete(s);
}

void spt_new() {
  struct hash *spt_cur = thread_current()->spt = (struct hash*) malloc(sizeof(struct hash));
  hash_init(spt_cur, hash_spte_hash, hash_spte_less, NULL);
}

void spt_delete() {
  struct hash *spt_cur = thread_current()->spt = (struct hash*) malloc(sizeof(struct hash));
  hash_destroy(spt_cur, hash_spte_delete);
}

spte *spt_getSpte (const void *vaddr) {
  spte p;
  struct hash_elem *e;

  struct hash *spt = thread_current()->spt;

  p.vaddr = (void*) vaddr;
  e = hash_find (spt, &p.elem);
  return e? hash_entry (e, spte, elem): NULL;
}

void spt_deleteSpte(const void *vaddr) {
  spte *s = spt_getSpte(vaddr);
  if(s) {
    hash_delete (thread_current()->spt, &s->elem);
    spte_delete(s);
  }
}
