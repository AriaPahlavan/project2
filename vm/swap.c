#include "bitmap.h"
#include <debug.h>
#include <limits.h>
#include <stdio.h>
#include "devices/block.h"
#include "userprog/syscall.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "vm/swap.h"

struct lock swap_lock;
static uint32_t block_per_page;


static struct block *swap_block;
static block_sector_t swap_size;
static struct bitmap* swap_bm;

void swap_init(void){
  lock_init(&swap_lock);
  swap_block = block_get_role(BLOCK_SWAP);  
  block_per_page = PGSIZE / BLOCK_SECTOR_SIZE;
  swap_size = block_size(swap_block) / block_per_page;
  /*bitmap to keep track of inuse swap slots; 0: not used*/
  swap_bm = bitmap_create(swap_size); 
  
}

size_t swap_frame(void* page_ptr){
  lock_acquire(&swap_lock);
  /*search for empty slots*/ 
  size_t swap_index = bitmap_scan_and_flip(swap_bm, 0, 1, false);


  /*write page into swap*/
  uint32_t i;
  for (i = 0; i < block_per_page; i++) {
    block_write(swap_block, swap_index+i, page_ptr + BLOCK_SECTOR_SIZE * i);
  }

  lock_release(&swap_lock);
  return swap_index;
}


void restore_frame(void* page_ptr, size_t swap_index){
  lock_acquire(&swap_lock);

  /*read swap into page*/
  uint32_t i;
  for (i = 0; i < block_per_page; i++) {
    block_read(swap_block, swap_index*block_per_page+i, page_ptr + BLOCK_SECTOR_SIZE * i);
  }

  /*reclaim free swap slot*/
  bitmap_set(swap_bm,swap_index,false);
  lock_release(&swap_lock);
}

void free_swap(size_t swap_index){
  lock_acquire(&swap_lock);
  if (swap_index >= swap_size) exit(-1);
  /*reclaim free swap slot*/
  bitmap_set(swap_bm,swap_index,false);
  lock_release(&swap_lock);
}

