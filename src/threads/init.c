#include "threads/init.h"

#include <console.h>
#include <debug.h>
#include <inttypes.h>
#include <limits.h>
#include <random.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "devices/input.h"
#include "devices/kbd.h"
#include "devices/rtc.h"
#include "devices/serial.h"
#include "devices/shutdown.h"
#include "devices/timer.h"
#include "devices/vga.h"
#include "threads/interrupt.h"
#include "threads/io.h"
#include "threads/loader.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/pte.h"
#include "threads/thread.h"
#ifdef USERPROG
#include "userprog/exception.h"
#include "userprog/gdt.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "userprog/tss.h"
#else
#include "tests/threads/tests.h"
#endif
#ifdef FILESYS
#include "devices/block.h"
#include "devices/ide.h"
#include "filesys/filesys.h"
#include "filesys/fsutil.h"
#endif

/** Page directory with kernel mappings only. */
uint32_t *init_page_dir;

#ifdef FILESYS
/** -f: Format the file system? */
static bool format_filesys;

/** -filesys, -scratch, -swap: Names of block devices to use,
   overriding the defaults. */
static const char *filesys_bdev_name;
static const char *scratch_bdev_name;
#ifdef VM
static const char *swap_bdev_name;
#endif
#endif /**< FILESYS */

/** -ul: Maximum number of pages to put into palloc's user pool. */
static size_t user_page_limit = SIZE_MAX;

static void bss_init(void);
static void paging_init(void);

static char **read_command_line(int argc, char *p);
static char **parse_options(char **argv);
static void run_actions(char **argv);
static void run_interactive(char **argv) UNUSED;
static void interactive(char *argv[]);
static void usage(void);

#ifdef FILESYS
static void locate_block_devices(void);
static void locate_block_device(enum block_type, const char *name);
#endif

int pintos_init(void) NO_RETURN;

/** Pintos main entry point. */
int pintos_init(void) {
  /* Clear BSS. */
  bss_init();

  /* Break command line into arguments and parse options. */
  int argc = *(uint32_t *)ptov(LOADER_ARG_CNT);
  char *p = ptov(LOADER_ARGS);
  char **argv = read_command_line(argc, p);
  argv = parse_options(argv);

  /* Initialize ourselves as a thread so we can use locks,
     then enable console locking. */
  thread_init();
  console_init();

  /* Greet user. */
  // kernel(printf) -> vprintf -> __vprintf -> vprintf_helper -> putchar_have_lock -> serial_putc -> putc_poll
  printf("Pintos booting with %'" PRIu32 " kB RAM...\n", init_ram_pages * PGSIZE / 1024);

  /* Initialize memory system. */
  palloc_init(user_page_limit);  // init the page allocator
  malloc_init();
  paging_init();

  /* Segmentation. */
#ifdef USERPROG
  tss_init();
  gdt_init();
#endif

  /* Initialize interrupt handlers. */
  intr_init();
  timer_init();
  kbd_init();
  input_init();
#ifdef USERPROG
  exception_init();
  syscall_init();
#endif

  /* Start thread scheduler and enable interrupts. */
  thread_start();
  serial_init_queue();
  timer_calibrate();

#ifdef FILESYS
  /* Initialize file system. */
  ide_init();
  locate_block_devices();
  filesys_init(format_filesys);
#endif

  printf("Boot complete.\n");

  if (*argv != NULL) {
    /* Run actions specified on kernel command line. */
    run_actions(argv);
  } else {
    // TODO: no command line passed to kernel. Run interactively
    interactive(argv);
  }

  /* Finish up. */
  shutdown();
  thread_exit();  // would call schedule, which would never return, the same as xv6
}

static int readline(char *buf, int size) {
  int len = 0;
  char ch;
  while ((ch = input_getc()) != '\r' && len < size) {
    if (ch == '\177') {
      if (len > 0) {
        buf[--len] = '\0';
        putchar('\b');
        putchar(' ');
        putchar('\b');
      }
    } else {
      putchar(ch);
      buf[len++] = ch;
    }
  }
  putchar('\n');
  buf[len] = '\0';
  return len;
}

static int tokenize(char *buf, char **argv) {
  char *token, *save_ptr;
  int argc = 0;
  for (token = strtok_r(buf, " \t", &save_ptr); token != NULL; token = strtok_r(NULL, " \t", &save_ptr)) {
    argv[argc++] = token;
  }
  return argc;
}

/** the argv 只是一个复用 */
static void interactive(char *argv[]) {
  while (true) {
    printf("pkuos> ");
    char buf[128] = {};
    int len = readline(buf, sizeof buf);
    if (len == 0) continue;
    int argc = tokenize(buf, argv);
    if (argv[0] != NULL) run_interactive(argv);
    for (int i = 0; i < argc; i++) {  // clear argv
      argv[i] = NULL;
    }
  }
}

/** Clear the "BSS", a segment that should be initialized to
   zeros.  It isn't actually stored on disk or zeroed by the
   kernel loader, so we have to zero it ourselves.

   The start and end of the BSS segment is recorded by the
   linker as _start_bss and _end_bss.  See kernel.lds. */
static void bss_init(void) {
  extern char _start_bss, _end_bss;
  memset(&_start_bss, 0, &_end_bss - &_start_bss);
}

/** Populates the base page directory and page table with the
   kernel virtual mapping, and then sets up the CPU to use the
   new page directory.  Points init_page_dir to the page
   directory it creates. */
static void paging_init(void) {
  extern char _start, _end_kernel_text;  // defined in kernel.lds.S

  uint32_t *pd = init_page_dir /*global*/ = palloc_get_page(PAL_ASSERT | PAL_ZERO);
  uint32_t *pt = NULL;
  for (size_t page = 0; page < init_ram_pages; page++) {  // 物理内存挂在页表下
    uintptr_t paddr = page * PGSIZE;
    char *vaddr = ptov(paddr);
    size_t pde_idx = pd_no(vaddr);
    size_t pte_idx = pt_no(vaddr);
    bool in_kernel_text = &_start <= vaddr && vaddr < &_end_kernel_text;  // if it is in .text

    if (pd[pde_idx] == 0) {
      pt = palloc_get_page(PAL_ASSERT | PAL_ZERO);
      pd[pde_idx] = pde_create(pt);
    }

    pt[pte_idx] = pte_create_kernel(vaddr, !in_kernel_text);
  }

  /* Store the physical address of the page directory into CR3
     aka PDBR (page directory base register).  This activates our
     new page tables immediately.  See [IA32-v2a] "MOV--Move
     to/from Control Registers" and [IA32-v3a] 3.7.5 "Base Address
     of the Page Directory". */
  asm volatile("movl %0, %%cr3" : : "r"(vtop(init_page_dir)));
}

/* ---------- ---------- ---------- ---------- command line ---------- ---------- ---------- ---------- */

/** If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
static bool thread_mlfqs_;

bool thread_mlfqs(void) { return thread_mlfqs_; }

/** Breaks the kernel command line into words and returns them as
   an argv-like array. */
static char **read_command_line(int argc, char *p) {
  static char *argv[LOADER_ARGS_LEN / 2 + 1];  // 最多的参数个数, 假设每个参数都是单个字母, 那么 a b c ..., 一个字母+一个空格, 2个字节

  char *end = p + LOADER_ARGS_LEN;
  for (int i = 0; i < argc; i++) {
    if (p >= end) PANIC("command line arguments overflow");

    argv[i] = p;
    p += strnlen(p, end - p) + 1;
  }
  argv[argc] = NULL;  // 参数数组, 以 NULL 结尾

  /* Print kernel command line. */
  printf("Kernel command line:");
  for (int i = 0; i < argc; i++)
    if (strchr(argv[i], ' ') == NULL)
      printf(" %s", argv[i]);
    else
      printf(" '%s'", argv[i]);
  printf("\n");

  return argv;
}

/** Parses options in ARGV[]
   and returns the first non-option argument. */
static char **parse_options(char **argv) {
  for (; *argv != NULL && **argv == '-'; argv++) {
    char *save_ptr;
    char *name = strtok_r(*argv, "=", &save_ptr);
    char *value = strtok_r(NULL, "", &save_ptr);

    if (!strcmp(name, "-h")) {
      usage();
    } else if (!strcmp(name, "-q")) {
      shutdown_configure(SHUTDOWN_POWER_OFF);
    } else if (!strcmp(name, "-r")) {
      shutdown_configure(SHUTDOWN_REBOOT);
    }
#ifdef FILESYS
    else if (!strcmp(name, "-f")) {
      format_filesys = true;
    } else if (!strcmp(name, "-filesys")) {
      filesys_bdev_name = value;
    } else if (!strcmp(name, "-scratch")) {
      scratch_bdev_name = value;
    }
#ifdef VM
    else if (!strcmp(name, "-swap")) {
      swap_bdev_name = value;
    }
#endif
#endif
    else if (!strcmp(name, "-rs")) {
      random_init(atoi(value));
    } else if (!strcmp(name, "-mlfqs")) {
      thread_mlfqs_ = true;
    }
#ifdef USERPROG
    else if (!strcmp(name, "-ul")) {
      user_page_limit = atoi(value);
    }
#endif
    else {
      PANIC("unknown option `%s' (use -h for help)", name);
    }
  }

  /* Initialize the random number generator based on the system
     time.  This has no effect if an "-rs" option was specified.

     When running under Bochs, this is not enough by itself to
     get a good seed value, because the pintos script sets the
     initial time to a predictable value, not to the local time,
     for reproducibility.  To fix this, give the "-r" option to
     the pintos script to request real-time execution. */
  random_init(rtc_get_time());

  return argv;
}

/* ---------- ---------- ---------- ---------- action ---------- ---------- ---------- ---------- */

#define ERT_NOT_FOUND 1  // error run test

/** Runs the task specified in ARGV[1]. */
static int run_task(char **argv) {
  const char *task = argv[1];

  printf("Executing '%s':\n", task);
#ifdef USERPROG
  process_wait(process_execute(task));
#else
  int ret = run_test(task);
  if (ret == -ERT_NOT_FOUND) {
    printf("not found test '%s'.\n", task);
    return ret;
  }
#endif
  printf("Execution of '%s' complete.\n", task);
  return 0;
}

static int act_whoami(char **argv UNUSED) {
  printf("miao miao miao\n");
  return 0;
}

NO_RETURN static int act_exit(char **argv UNUSED) { shutdown_power_off(); }

/* An action. */
struct action {
  const char *name;             /**< Action name. */
  int argc;                     /**< # of args, including action name. */
  int (*function)(char **argv); /**< Function to execute action. */
};

/* Table of supported actions. */
static const struct action actions[] = {
    {"run", 2, run_task},      /*  */
    {"whoami", 1, act_whoami}, /*  */
    {"exit", 1, act_exit},     /*  */
#ifdef FILESYS
    {"ls", 1, fsutil_ls},           /*  */
    {"cat", 2, fsutil_cat},         /*  */
    {"rm", 2, fsutil_rm},           /*  */
    {"extract", 1, fsutil_extract}, /*  */
    {"append", 2, fsutil_append},   /*  */
#endif
    {NULL, 0, NULL},
};

static void run_interactive(char **argv) {
  const struct action *act;

  /* Find action name. */
  for (act = actions; act->name /*dead loop*/; act++) {
    if (0 == strcmp(*argv, act->name)) break;  // 找到 action
  }

  if (act->name == NULL) {  // check found action
    printf("unknown action `%s' (use -h for help)\n", *argv);
    return;
  }

  int argc = 0;
  while (argv[argc] != NULL) argc++;  // count args
  // printf("argc: %d\n", argc);

  if (argc != act->argc) {
    printf("action `%s' requires %d / %d\n", *argv, argc, act->argc);
    return;
  }

  /* Invoke action and advance. */
  int ret = act->function(argv);
  if (ret == -ERT_NOT_FOUND) {
    // do nothing
  }
  argv += act->argc;
}

/** Executes all of the actions specified in ARGV[]
   up to the null pointer sentinel. */
static void run_actions(char **argv) {
  while (*argv != NULL) {
    const struct action *a;

    /* Find action name. */
    for (a = actions; /*dead loop*/; a++) {
      if (a->name == NULL) {
        PANIC("unknown action `%s' (use -h for help)", *argv);
      } else if (!strcmp(*argv, a->name)) {
        break;
      }
    }

    /* Check for required arguments. */
    for (int i = 1; i < a->argc; i++) {
      if (argv[i] == NULL) {
        PANIC("action `%s' requires %d argument(s)", *argv, a->argc - 1);
      }
    }

    /* Invoke action and advance. */
    int ret = a->function(argv);
    if (ret == -ERT_NOT_FOUND) {
      PANIC("no test named \"%s\"", argv[1]);
    }

    argv += a->argc;
  }
}

/** Prints a kernel command line help message and powers off the
   machine. */
static void usage(void) {
  printf(
      "\nCommand line syntax: [OPTION...] [ACTION...]\n"
      "Options must precede actions.\n"
      "Actions are executed in the order specified.\n"
      "\nAvailable actions:\n"
#ifdef USERPROG
      "  run 'PROG [ARG...]' Run PROG and wait for it to complete.\n"
#else
      "  run TEST           Run TEST.\n"
#endif
#ifdef FILESYS
      "  ls                 List files in the root directory.\n"
      "  cat FILE           Print FILE to the console.\n"
      "  rm FILE            Delete FILE.\n"
      "Use these actions indirectly via `pintos' -g and -p options:\n"
      "  extract            Untar from scratch device into file system.\n"
      "  append FILE        Append FILE to tar file on scratch device.\n"
#endif
      "\nOptions:\n"
      "  -h                 Print this help message and power off.\n"
      "  -q                 Power off VM after actions or on panic.\n"
      "  -r                 Reboot after actions.\n"
#ifdef FILESYS
      "  -f                 Format file system device during startup.\n"
      "  -filesys=BDEV      Use BDEV for file system instead of default.\n"
      "  -scratch=BDEV      Use BDEV for scratch instead of default.\n"
#ifdef VM
      "  -swap=BDEV         Use BDEV for swap instead of default.\n"
#endif
#endif
      "  -rs=SEED           Set random number seed to SEED.\n"
      "  -mlfqs             Use multi-level feedback queue scheduler.\n"
#ifdef USERPROG
      "  -ul=COUNT          Limit user memory to COUNT pages.\n"
#endif
  );
  shutdown_power_off();
}

#ifdef FILESYS
/** Figure out what block devices to cast in the various Pintos roles. */
static void locate_block_devices(void) {
  locate_block_device(BLOCK_FILESYS, filesys_bdev_name);
  locate_block_device(BLOCK_SCRATCH, scratch_bdev_name);
#ifdef VM
  locate_block_device(BLOCK_SWAP, swap_bdev_name);
#endif
}

/** Figures out what block device to use for the given ROLE: the
   block device with the given NAME, if NAME is non-null,
   otherwise the first block device in probe order of type
   ROLE. */
static void locate_block_device(enum block_type role, const char *name) {
  struct block *block = NULL;

  if (name != NULL) {
    block = block_get_by_name(name);
    if (block == NULL) PANIC("No such block device \"%s\"", name);
  } else {
    for (block = block_first(); block != NULL; block = block_next(block))
      if (block_type(block) == role) break;
  }

  if (block != NULL) {
    printf("%s: using %s\n", block_type_name(role), block_name(block));
    block_set_role(role, block);
  }
}
#endif
