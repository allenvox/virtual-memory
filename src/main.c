#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define attribute(value) __attribute__((value))
#define MMU_PTE_COUNT 1024
#define MMU_PDE_COUNT 1024
#define malloc_a(size, align) kmalloc_a(size, align)

// page directory entry
struct PDE {
  uint32_t P : 1; // present (is page now in memory)
  uint32_t RW : 1; // read-write
  uint32_t US : 1; // user/supervisor
  uint32_t PWT : 1; // write through
  uint32_t PCD : 1; // cache off flag
  uint32_t A : 1; // accessed (is page in use)
  uint32_t D : 1; // dirty (is page edited)
  uint32_t PS : 1; // page size
  uint32_t G : 1; // global flag (ignored)
  uint32_t reserved : 3; // reserved bits
  uint32_t address : 20; // page table address
} attribute(packed);

// page table entry
struct PTE {
  uint32_t P : 1; // present
  uint32_t RW : 1; // read-write
  uint32_t US : 1; // user/supervisor
  uint32_t PWT : 1; // write through
  uint32_t PCD : 1; // cache off flag
  uint32_t A : 1; // accessed
  uint32_t D : 1; // dirty
  uint32_t PAT : 1; // page attributes table
  uint32_t G : 1; // global (ignored)
  uint32_t reserved : 3; // reserved bits
  uint32_t ADDRESS : 20; // physical address
} attribute(packed);

struct page {
  uint32_t frames[1024];
};

// aligned structures
// page directory
struct PDE page_dir[MMU_PDE_COUNT] attribute(aligned(4096));
// page table
struct PTE page_tab[MMU_PDE_COUNT][MMU_PTE_COUNT] attribute(aligned(4096));
// page
struct page page_0;
uint32_t cr3; // control register 3

uint32_t translate_from_logic(uint32_t logical_addr) {
  // get physical address from logical
  uint32_t dir_addr = (cr3 >> 12) << 12;
  uint32_t cat_id = logical_addr >> 22;
  uint32_t tab_id = (logical_addr << 10) >> 22;
  uint32_t addr_on_page = (logical_addr << 20) >> 20;
  struct PDE *directory_addr = (struct PDE *)(uintptr_t)dir_addr;
  struct PTE *table_addr = (struct PTE *)(uintptr_t)((directory_addr[cat_id]).address << 12);
  struct page *page_addr = (struct page *)(uintptr_t)((table_addr[tab_id]).ADDRESS << 12);
  uint32_t phys_addr = ((uintptr_t)page_addr << 12) + addr_on_page;
  return phys_addr;
}

void mmu_print(uint32_t page_tab_count, uint32_t pages_count) {
  printf("present\tRW\tuser_supervisor\twritethru\tcache\taccessed\tdirty\tpgsize\tignore\tpgtableaddr\n");
  for (uint32_t i = 0; i < page_tab_count; i++) {
    // pages directory
    printf("%d\t\t", (page_dir[i]).P);
    printf("%d\t", (page_dir[i]).RW);
    printf("%d\t\t\t\t", (page_dir[i]).US);
    printf("%d\t\t\t", (page_dir[i]).PWT);
    printf("%d\t\t", (page_dir[i]).PCD);
    printf("%d\t\t\t", (page_dir[i]).A);
    printf("%d\t\t", (page_dir[i]).D);
    printf("%d\t\t", (page_dir[i]).PS);
    printf("%d\t\t", (page_dir[i]).G);
    printf("0x%x\n", (uint32_t)((page_dir[i]).address));
    // pages table
    printf("present\tRW\tuser_supervisor\twritethru\tcache\taccessed\tdirty\tPAT\tignore\tphysaddr\tvirtaddr\n");
    for (uint32_t j = 0; j < MMU_PTE_COUNT; j++) {
      printf("%d\t\t", (page_tab[i][j]).P);
      printf("%d\t", (page_tab[i][j]).RW);
      printf("%d\t\t\t\t", (page_tab[i][j]).US);
      printf("%d\t\t\t", (page_tab[i][j]).PWT);
      printf("%d\t\t", (page_tab[i][j]).PCD);
      printf("%d\t\t\t", (page_tab[i][j]).A);
      printf("%d\t\t", (page_tab[i][j]).D);
      printf("%d\t", (page_tab[i][j]).PAT);
      printf("%d\t\t", (page_tab[i][j]).G);
      printf("%p\t\t", (void *)(uintptr_t)((page_tab[i][j]).ADDRESS));
      printf("0x%x\n", (uint32_t)((i << 22) + ((j << 22) >> 10)); // virtual address
    }
  }
}

int main(int argc, char **argv) {
  cr3 = (uintptr_t)page_dir >> 12 << 12; // control register 3 - no cache, PAE off

  uint32_t bin_size; // size of memory in bytes
  if (argc > 1) { // if cli arg[1] exists
    uint8_t i = 0;
    while (argv[1][i] <= '9' && argv[1][i] >= '0') {
      bin_size = bin_size * 10 + (uint32_t)(argv[1][i] - '0');
      i++;
    }
    if (argv[1][i] == 'K' || argv[1][i] == 'k') {
      bin_size *= 1024;
    } else if (argv[1][i] == 'M' || argv[1][i] == 'm') {
      bin_size *= (1024 * 1024);
    }
  } else { // default - 4 KB * 16
    bin_size = 4096 * 16;
  }
  
  uint32_t pages_count = bin_size / 4096;
  if ((bin_size % 4096) > 0)
    pages_count++;
  
  uint32_t page_tab_count = pages_count / 1024;
  if ((pages_count % 1024) > 0)
    page_tab_count++;
  
  for (uint16_t i = 0; i < page_tab_count; i++) {
    page_dir[i].P = 1;
    page_dir[i].RW = 1;
    page_dir[i].US = 1;
    page_dir[i].PWT = 1;
    page_dir[i].PCD = 1;
    page_dir[i].A = 0;
    page_dir[i].D = 1;
    page_dir[i].PS = 1; // 4 KB
    page_dir[i].address = (uintptr_t)(&page_tab[i]) >> 12;
    for (uint32_t j = 0; j < MMU_PTE_COUNT; j++) {
      page_tab[i][j].D = 0;
      page_tab[i][j].A = 0;
      page_tab[i][j].PAT = 0;
      page_tab[i][j].PCD = 0;
      page_tab[i][j].G = 1;
      page_tab[i][j].P = 1;
      page_tab[i][j].RW = 1;
      page_tab[i][j].US = 1;
      page_tab[i][j].PWT = 1;
      page_tab[i][j].ADDRESS = ((uintptr_t)(&page_0) + j * 4096) >> 12; // assume 4 KB pages
    }
  }
  printf("memsize = %d, ", bin_size);
  printf("pgdiraddr = %p, ", (void *)page_dir);
  printf("pgtabcnt = %d, ", page_tab_count);
  printf("pgcnt = %d\n\n", pages_count);
  printf("MMU table\n");
  mmu_print(page_tab_count, pages_count);
  return 0;
}
