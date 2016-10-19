#include "frame.h"

struct lock frame_lock;

static struct list frame_list;
static list_elem e;
static frame *cur_frame_ptr;


void frame_init(void){
  struct frame* frame_ptr;
  void* page_addr;

  list_init(&frame_list);
  lock_init(&frame_lock);

  while((page_addr = palloc_get_page(PAL_USER))){ /*acquire all available frames*/
    frame_ptr = (struct frame*) malloc(sizeof(struct frame));
    if (frame_ptr == NULL) return;
    frame_ptr->page_addr = page_addr;
    frame_ptr->LRU_bit = 0;
    frame_ptr->valid = 0;
    lock_init(frame_ptr->pages_lock);
    lock_acquire(frame_lock);
    list_push_back(&frame_list,&frame_ptr->list_elem);
    lock_release(frame_lock);
  }


}

void* get_frame(void){

  lock_acquire(frame_lock);

  for(e = list_begin(&frame_list); e!= list_end(&frame_ist); e = list_next(e)){
    struct frame* fp = list_entry(e, struct frame, list_elem);
    if (!fp->valid){ /* current frame not inuse*/
      fp->valid = true;
      lock_release(frame_lock);
      return fp->page_addr;
    }
  }
    /*
    lock_release(frame_lock);
    evict_frame();
    return get_frame(flag);*/

}

void free_frame(void* pa) {
  lock_acquire(frame_lock);
  struct frame* fp = find_frame(pa);
  if (fp == NULL) return;
  fp->valid = false;
  lock_release(frame_lock);
}

struct frame* find_frame(void* pa){
  for(e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)) {
    struct frame * fp = list_entry(e, struct frame, list_elem);
    if ((fp->page_addr == pa) && fp->valid) {
      return fp;
    }
  }
  return NULL;
}


//TODO: following function is not complete. The LRU algorithm to find the frame to be evicted is implemented; not yet implement how to actually evict the frame(just remove from list and call palloc?). Feel free to rewrite or change the function accordingly

static void evict_frame(void){
  //set current frame LRU bit to 0 
  if (cur_frame_ptr == NULL) cur_frame_ptr = &list_begin(&frame_list);
  cur_frame_ptr->LRU_bit = 0;

  lock_acquire(frame_lock);
  //start with next frame and circulate the frame list
  for (e = list_next(*cur_frame_ptr); e != list_end(&frame_list); e = list_next(e)) {
    struct frame *fp = list_entry(e, struct frame, list_elem);
    if (fp->LRU_bit == 0){
      fp->LRU_bit = 1;
    } else {
      //TODO: evict this frame



      lock_release(frame_lock);
      return;
    }
  }

  //wrap the list back to beginning(full circular check)
  for (e = list_begin(&frame_list); e != *cur_frame_ptr; e = list_next(e)){
    struct frame *fp = list_entry(e, struct frame, list_elem);
    if (fp->LRU_bit == 0){
      fp->LRU = 1;
    } else {
      //TODO: evict this frame


      lock_release(frame_lock);
      return;
    }
  }
}

