#include <stdbool.h>
#include <list.h>

#include "threads/synch.h"

struct frame{
  void* page_addr; //base physical address
  struct list_elem elem;
  bool LRU_bit;
  bool valid;
  struct list pages; //points to all pages to share this frame
  struct lock pages_lock;
};

void frame_init(void);

void* get_frame(void);

void free_frame(void* pa);

struct frame* find_frame(void* pa);
