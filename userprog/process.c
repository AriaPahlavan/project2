#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hash.h>

#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/process.h"

#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

#include "vm/frame.h"

typedef struct struct_child {
  struct hash_elem hash_elem;
  char *prog;
  char *fname;
  struct semaphore *sema;
  int argc;
  char **argv;
  tid_t tid;
  bool success_load;
  bool waiting;
  int32_t exit_status;
} child;

child* child_new(const char *prog);
void child_delete(child *c);

child *child_new(const char *prog) {
  child *c = (child*) malloc(sizeof(child));
  c->prog = (char*) get_frame(0);
  c->sema = (struct semaphore*) malloc(sizeof(struct semaphore));
  int argvMAX = 40;
  c->argv = (char**) malloc(sizeof(char*) * 40); /* "40" needs to be changed*/

  /* return null, if error with memory allocation */
  if(!(c && c->prog && c->sema && c->argv)) {
    child_delete(c);
    return NULL;
  }

  strlcpy(c->prog, prog, PGSIZE);

  sema_init(c->sema, 0);

  c->tid = TID_ERROR;

  c->success_load = c->waiting = false;

  c->exit_status = 0; /*default success for exit status*/

  char *prog_tok;
  char *rest_ptr;

  /* Extract and save file name */
  c->argv[0] = c->fname = strtok_r(c->prog, " ", &rest_ptr);

  /*Extract and save args one by one, increment argc*/
  c->argc = 1;
  while((prog_tok = strtok_r(rest_ptr, " ", &rest_ptr))) {
    if(c->argc == argvMAX) {
      c->argv = (char**) realloc(c->argv, (argvMAX *= 2) * sizeof(char*));
    }

    c->argv[c->argc] = prog_tok;
    ++(c->argc);
  }

  return c;
}

void child_delete(child *c) {
  if(!c) {
    return;
  }

  palloc_free_page (c->prog);

  free(c->sema);
  free(c->argv);
  free(c);
}


struct hash *hash_children;

static unsigned hash_child_hash(const struct hash_elem *e, void *aux);
static bool hash_child_less(const struct hash_elem *a,
                            const struct hash_elem *b,
                            void *aux);
static void hash_child_delete(struct hash_elem *e, void *aux);
static child *hash_children_getChild(const tid_t tid);
static void hash_children_deleteChild(const tid_t tid);

static unsigned hash_child_hash(const struct hash_elem *e, void *aux UNUSED) {
  child *c = hash_entry(e, child, hash_elem);
  return hash_int((int) c->tid);
}

static bool hash_child_less(const struct hash_elem *a,
                            const struct hash_elem *b,
			    void *aux UNUSED) {
  child *ca = hash_entry(a, child, hash_elem);
  child *cb = hash_entry(b, child, hash_elem);

  return ca->tid < cb->tid;
}

static void hash_child_delete(struct hash_elem *e, void *aux UNUSED) {
  child * c = hash_entry(e, child, hash_elem);
  child_delete(c);
}

static child *hash_children_getChild (const tid_t tid) {
  child p;
  struct hash_elem *e;

  p.tid = tid;
  e = hash_find (hash_children, &p.hash_elem);
  return e? hash_entry (e, child, hash_elem): NULL;
}

static void hash_children_deleteChild(const tid_t tid) {
  child *c = hash_children_getChild(tid);
  if(c) {
    hash_delete (hash_children, &c->hash_elem);
    child_delete(c);
  }
}

static thread_func start_process NO_RETURN;
static bool load (const child *childProcess, void (**eip) (void), void **esp);

void process_init(void) {
  hash_children = (struct hash*) malloc(sizeof(struct hash));
  hash_init(hash_children, hash_child_hash, hash_child_less, NULL);
}

void process_done(void) {
  hash_destroy(hash_children, &hash_child_delete);
}

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */

tid_t
process_execute (const char *prog)
{
  child *cp;
  tid_t tid;

  if(!prog) {
    return TID_ERROR;
  }

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  cp = child_new(prog); /* @Niko: this is ours we can make it to point to the child structure*/
  if (!cp)
    return TID_ERROR;

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (cp->prog, PRI_DEFAULT, start_process, cp); /*this may need to change...*/

  /* not successful */
  if (tid == TID_ERROR){
    child_delete(cp);
    return TID_ERROR;
  } else {
      cp->tid = tid;
      hash_insert(hash_children, &cp->hash_elem);

      sema_down(cp->sema); /*wait for child process to complete loading*/
      if(!(cp->success_load)) {
	hash_children_deleteChild(cp->tid);
	return TID_ERROR;
      }
  }

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *cp) /*@Nico: void *file_name_ need to be changed to a child struct argument
									which will be passed to here by thead_creat func*/
{
  child *childProcess = (child*) cp;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (childProcess, &if_.eip, &if_.esp);
  childProcess->success_load = success;
  							/* @Nico: you have two options: 1) Only send the file name to load...
							no args should be send to load.
							In other words, make sure that file_name only contains the file name and no arguments
							(example: if command line is "ls -l", only pass "ls" to load) The reason is that load
							also loads the file executable from disk onto the memory. If you choose this option,
							then you need to do the setup_stack call here (after "if(!success)"), instead of doing
							it inside the load function 2) send the whole command and make sure in load function,
							you only pass file name to filesys_open without any arguments. */

  /* If load failed, quit. */
  if (!success)
    thread_exit ();

	/* @Nico: before asm, make sure you sema_up or release the parent process using the argumet passed to this function */
  sema_up(childProcess->sema); /*wait for program to be loaded first*/


  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid)
{
  int ret;
  child *c;
  if((c = hash_children_getChild(child_tid))) {
    if(c->waiting) { /*catch calling wait twice*/
      ret = -1;
    } else {
      c->waiting = true;
      sema_down(c->sema);
      c->waiting = false;
      ret = c->exit_status;
      printf ("%s: exit(%d)\n", c->fname, ret); /*exit feedback*/
      hash_children_deleteChild(child_tid);
    }
  } else {
    ret = -1;
  }

  return ret; /*return this if child_tid was valid*/
}

/* Free the current process's resources. */
void
process_exit ()
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* now I'm gonna close my exe file :) */
  struct file *file = cur->executable;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }

  /*unblock calling process*/
  child *c = hash_children_getChild(thread_tid());
  c->exit_status = cur->exit_status;

  sema_up(c->sema);
  file_close(file);

}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, const child *cp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const child *childProcess, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;
  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (childProcess->fname);
  if (file == NULL)
    {
      printf ("load: %s: open failed\n", childProcess->fname);
      goto done;
    }


    t->executable = file;
    file_deny_write(file);

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
    {
      printf ("load: %s: error loading executable\n", childProcess->fname);
      goto done;
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type)
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file))
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp, childProcess))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* Let's not close the execuatble here
     and instead close it when we're about to exit?! */
  /*file_close (file);*/
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;


  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0)
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = get_frame (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false;
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable))
        {
          palloc_free_page (kpage);
          return false;
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
union mem_ptr {
  char *c;
  int *i;
} mp;

static bool
setup_stack (void **esp, const child *cp)
{
  uint8_t *kpage;
  bool success = false;

  kpage = get_frame (PAL_USER | PAL_ZERO);
  if (kpage != NULL)
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }

  mp.c = (char*) PHYS_BASE - 1;
  /*@Nico: all of these variables created here can be part of child struct*/
  int i, j;
  char *arg;
  int len;

  /* inserting args on stack, including the file name (argv[0])*/
  for(i = 0; i < cp->argc; ++i) {
    arg = cp->argv[cp->argc - i - 1];
    len = strlen(arg);
    for(j = 0; j <= len; ++j) {
      *(mp.c) = arg[len - j];
      --(mp.c);
    }
  }

  while(((int) (mp.c + 1)) % sizeof(int)) { /*offset padding*/
    *(mp.c) = 0x00;
    --(mp.c);
  }

  mp.c -= 3; /*move to the lsb of this int*/

  /*delimit end of argv array*/
  *(mp.i) = 0x0000;
  --(mp.i);

  /*copy argument addresses to the stack*/
  int start = (int) PHYS_BASE;
  int total = 0;
  for(i = 0; i < cp->argc; ++i) {
    total += strlen(cp->argv[cp->argc - 1 - i]) + 1;
    *(mp.i) = start - total;
    --(mp.i);
  }

  /*ptr to argv[0]*/
  *(mp.i) = (int) (mp.i + 1);
  --(mp.i);

  /*argc*/
  *(mp.i) = cp->argc;
  --(mp.i);

  /*ret addr*/
  *(mp.i) = 0x0000; /*for now...*/

  *esp = mp.i;

  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
