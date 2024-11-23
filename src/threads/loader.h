#ifndef THREADS_LOADER_H
#define THREADS_LOADER_H

/** Constants fixed by the PC BIOS. */
#define LOADER_BASE 0x7c00 /**< Physical address of loader's base. */
#define LOADER_END 0x7e00  /**< Physical address of end of loader. */

/** Physical address of kernel base. */
#define LOADER_KERN_BASE 0x20000 /**< 128 kB. */

/** Kernel virtual address at which all physical memory is mapped.
   Must be aligned on a 4 MB boundary. */
#define LOADER_PHYS_BASE 0xc0000000 /**< 3 GB. */

/** Sizes of loader data structures. */
#define LOADER_SIG_LEN 2
#define LOADER_PARTS_LEN 64
#define LOADER_ARGS_LEN 128  // 最多有 128Bytes 的参数
#define LOADER_ARG_CNT_LEN 4

/** Important loader physical addresses. */
#define LOADER_SIG (LOADER_END - LOADER_SIG_LEN)          /**< 0xaa55 BIOS signature. [0x7dfe, 0x7e00) */
#define LOADER_PARTS (LOADER_SIG - LOADER_PARTS_LEN)      /**< Partition table. [0x7dbe, 0x7dfe) */
#define LOADER_ARGS (LOADER_PARTS - LOADER_ARGS_LEN)      /**< Command-line args. [0x7d3e, 0x7dbe) */
#define LOADER_ARG_CNT (LOADER_ARGS - LOADER_ARG_CNT_LEN) /**< Number of args. [0x7d3a, 0x7d3e) */

/** GDT selectors defined by loader.
   More selectors are defined by userprog/gdt.h. */
#define SEL_NULL 0x00  /**< Null selector. */
#define SEL_KCSEG 0x08 /**< Kernel code selector. */
#define SEL_KDSEG 0x10 /**< Kernel data selector. */

#ifndef __ASSEMBLER__
#include <stdint.h>

/** Amount of physical memory, in 4 kB pages. */
extern uint32_t init_ram_pages;
#endif

#endif /**< threads/loader.h */
