

struct frame{
  void* page_addr; //base physical address
  struct list_elem elem;
  bool LRU_bit;
  struct list pages; //points to all pages to share this frame
  struct lock pages_lock;
};

void frame_init(void);

void* get_frame(enum palloc_flags);

struct frame* find_frame(void* pa);
