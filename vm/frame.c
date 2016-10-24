#include <list.h>
#include <string.h>
#include "lib/debug.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

struct lock frame_lock;

static struct list frame_list;
static struct list_elem *e;
static struct frame *clk_frame_ptr;

static void *alloc_frame(struct frame *fp, spte *spte_cur);
static struct frame *evict_frame(void);

void frame_init(void){
  struct frame* frame_ptr;
  void* page_addr;

  list_init(&frame_list);
  lock_init(&frame_lock);

  while((page_addr = palloc_get_page(PAL_USER))){ /*acquire all available frames*/
    frame_ptr = (struct frame*) malloc(sizeof(struct frame));
    if (frame_ptr == NULL) return;
    frame_ptr->page_addr = page_addr;
    frame_ptr->valid = 0;
    list_init(&frame_ptr->pages);
    lock_init(&frame_ptr->pages_lock);
    lock_acquire(&frame_lock);
    list_push_back(&frame_list,&frame_ptr->elem);
    lock_release(&frame_lock);
  }

  clk_frame_ptr = NULL;
}

void* get_frame(spte* spte_cur){
  void* ret = NULL;

  lock_acquire(&frame_lock);
    
  for(e = list_begin(&frame_list); e!= list_end(&frame_list); e = list_next(e)){
    struct frame* fp = list_entry(e, struct frame, elem);
    if (!fp->valid){ /* current frame not inuse*/
      fp->valid = true;
      ret = alloc_frame(fp, spte_cur);
      lock_release(&frame_lock);
    }
  }
    
  if(!ret) {
    struct frame *ret_fp = evict_frame();
    ret_fp->valid = true;
    ret = alloc_frame(ret_fp, spte_cur);
    lock_release(&frame_lock);
  }

  return ret;
}

static void *alloc_frame(struct frame *fp, spte *spte_cur) {
  void *ret = fp->page_addr;
  fp->valid = true;
  lock_acquire(&fp->pages_lock);
  list_push_back(&fp->pages, &spte_cur->list_elem);
  lock_release(&fp->pages_lock);

  return ret;
}

void free_frame(void* pa) {
  lock_acquire(&frame_lock);
  struct frame* fp = find_frame(pa);
  if (fp == NULL) return;
  fp->valid = false;
  struct list_elem *pgs;
  lock_acquire(&fp->pages_lock);
  for(pgs = list_begin(&fp->pages); pgs != list_end(&fp->pages); pgs = list_next(pgs)) {
    spte *spte_cur = list_entry(pgs, spte, list_elem);
    if(spte_cur) {
      spte_cur->page_loc = PAGE_NOWHERE;
      list_remove(&spte_cur->list_elem);
    }
  }

  lock_release(&fp->pages_lock);
  lock_release(&frame_lock);
}

struct frame* find_frame(void* pa){
  for(e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)) {
    struct frame * fp = list_entry(e, struct frame, elem);
    if ((fp->page_addr == pa) && fp->valid) {
      return fp;
    }
  }
  return NULL;
}

void notify_frame_in_mem(void *pa) {
  lock_acquire(&frame_lock);
  struct frame *fp = find_frame(pa);
  struct list_elem *pgs;

  lock_acquire(&fp->pages_lock);
  for(pgs = list_begin(&fp->pages); pgs != list_end(&fp->pages); pgs = list_next(pgs)) {
    spte *spte_cur = list_entry(pgs, spte, list_elem);
    if(spte_cur) {
      spte_cur->page_loc = PAGE_IN_MEM;
    }
  }
  lock_release(&fp->pages_lock);
  lock_release(&frame_lock);
}

//TODO: following function is not complete. The LRU algorithm to find the frame to be evicted is implemented; not yet implement how to actually evict the frame(just remove from list and call palloc?). Feel free to rewrite or change the function accordingly

static struct frame *evict_frame(void){
  //set current frame LRU bit to 0
  if (clk_frame_ptr == NULL) clk_frame_ptr = list_entry(list_begin(&frame_list), struct frame, elem);

  struct thread *t = thread_current();
  uint32_t *pd = t->pagedir;
  struct hash *spt = t->spt;
  spte *spte_cur = spt_getSpte(spt, clk_frame_ptr->page_addr);
  ASSERT(spte_cur != NULL);

  struct frame *ret = NULL;
  
  /*clk_frame_ptr -> end -> beginning -> clk_frame_ptr - 1*/
  for(e = &clk_frame_ptr->elem; 
      list_next(e) != &clk_frame_ptr->elem; 
      e = (e == list_end(&frame_list))? list_begin(&frame_list): list_next(e)) {
    struct frame *fp = list_entry(e, struct frame, elem);
    
    if(!pagedir_is_accessed(pd, (const void*) fp->page_addr)) {
      if(!spte_cur->isPinned) { /*found a frame to evict. Throw current frame into swap*/ 
	/*this step might actually require a semaphore. What if every page has been pinned? 
	  I would reach the end of this loop with ret still being NULL*/
	struct list_elem *pgs;

	if(!pagedir_is_dirty(pd, fp->page_addr)) {
	  size_t swp_ind = swap_frame(fp->page_addr);

	  for(pgs = list_begin(&fp->pages); pgs != list_end(&fp->pages); pgs = list_next(pgs)) {
	    spte *spte_f = list_entry(pgs, spte, list_elem);

	    spte_f->swap_i = swp_ind;
	    spte_f->page_loc = PAGE_IN_SWAP;
	  }
	} else {
	  for(pgs = list_begin(&fp->pages); pgs != list_end(&fp->pages); pgs = list_next(pgs)) {
	    spte *spte_f = list_entry(pgs, spte, list_elem);
	    spte_f->page_loc = PAGE_IN_DSK;
	  }
	}

	ret = fp;
        struct list_elem *next = list_next(e);
	clk_frame_ptr = list_entry(next, struct frame, elem);
      }
    }
    
    pagedir_set_accessed(pd, spte_cur->vaddr, true);
    if(ret != NULL) {
      fp->valid = true;
      break;
    }
  }

  return ret;
}
