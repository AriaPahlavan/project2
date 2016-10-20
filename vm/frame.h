#include "../lib/stdbool.h"

// #include <debug.h>
// #include <inttypes.h>
// #include <round.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <hash.h>
//
// #include "userprog/gdt.h"
// #include "userprog/pagedir.h"
// #include "userprog/tss.h"
// #include "userprog/process.h"
//
// #include "filesys/directory.h"
// #include "filesys/file.h"
// #include "filesys/filesys.h"
//
// #include "threads/flags.h"
// #include "threads/init.h"
// #include "threads/interrupt.h"
// #include "threads/malloc.h"
// #include "threads/palloc.h"
// #include "threads/thread.h"
// #include "threads/vaddr.h"
// #include "threads/synch.h"

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
