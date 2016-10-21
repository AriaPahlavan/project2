#include "bitmap.h"
#include <debug.h>
#include <limits.h>
#include <stdio.h>
#include "swap.h"
#include "../threads/vaddr.h"

struct struct lock swap_lock;
static uint32_t block_per_page;


static struct block *swap_block;
static struct block_sector_t swap_size;
static struct bitmap* swap_bm;

void swap_init(void){
  lock_init(&swap_lock);
  swap_block = block_get_role(BLOCK_SWAP);  
  block_per_page = PGSIZE/swap_block->size;
  swap_size = block_size(swap_block)/block_per_page;
  /*bitmap to keep track of inuse swap slots; 0: not used*/
  swap_bm = bitmap_create(swap_size); 
}

size_t swap_frame(void* page_ptr){
  lock_acquire(&swap_lock);
  /*search for empty slots*/ 
  size_t swap_index = bitmap_scan_and_flip(swap_block,0,1,true);


  /*write page into swap*/
  for (uint32_t i = 0; i < block_per_page; i++) {
    block_write(swap_block, swap_index*block_per_page+i,(char*)page_ptr+swap_block->size);
  }

  lock_release(&swap_lock);
  return swap_index;
}


void restore_frame(void* page_ptr, size_t swap_index){
  lock_acquire(&swap_lock);

  /*read swap into page*/
  for (uint32_t i = 0; i < block_per_page; i++) {
    block_read(swap_block, swap_index*block_per_page+i,(char*)page_ptr+swap_block->size);
  }

  /*reclaim free swap slot*/
  bitmap_set(swap_block,swap_index,false);
  lock_release(&swap_lock);
}

void free_frame(size_t swap_index){
  lock_acquire(&swap_lock);
  if (swap_index > swap_size) exit(-1);
  /*reclaim free swap slot*/
  bitmap_set(swap_block,swap_index,false);
  lock_release(&swap_lock);
}

