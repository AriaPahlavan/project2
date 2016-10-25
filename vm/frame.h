#include <stdbool.h>
#include <list.h>

#include "threads/synch.h"
#include "vm/page.h"

#ifndef FRAME_H
#define FRAME_H
struct frame{
  void* page_addr; //base physical address
  struct list_elem elem;
  bool valid;
  struct list pages; //points to all pages to share this frame
  struct lock pages_lock;
};

void frame_init(void);

void* get_frame(spte *spte_cur);

void free_frame(spte *spte_cur);

struct frame* find_frame(void* pa);

void notify_frame_in_mem(void *pa);

#endif /*vm/frame.h*/
