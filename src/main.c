#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define attribute(value) __attribute__((value))
#define MMU_PTE_COUNT 1024
#define MMU_PDE_COUNT 1024
#define malloc_a(size, align) kmalloc_a(size, align)

struct PDE {
  uint32_t P : 1;
  uint32_t RW : 1;
  uint32_t US : 1;
  uint32_t PWT : 1;
  uint32_t PCD : 1;
  uint32_t A : 1;
  uint32_t D : 1;
  uint32_t PS : 1;
  uint32_t G : 1;
  uint32_t reserved : 3;
  uint32_t address : 20;
} attribute(packed);

struct PTE {
  uint32_t P : 1;
  uint32_t RW : 1;
  uint32_t US : 1;
  uint32_t PWT : 1;
  uint32_t PCD : 1;
  uint32_t A : 1;
  uint32_t D : 1;
  uint32_t PAT : 1;
  uint32_t G : 1;
  uint32_t reserved : 3;
  uint32_t ADDRESS : 20;
} attribute(packed);

struct page {
  uint32_t frames[1024];
};

//Экземпляры структур с выравниванием
struct PDE page_dir[MMU_PDE_COUNT] attribute(aligned(4096));
struct PTE page_tab[MMU_PDE_COUNT][MMU_PTE_COUNT] attribute(aligned(4096));
struct page page_0;
uint32_t cr3;

uint32_t translate_from_logic(uint32_t logical_addr) {
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
  uint32_t virt_addr = 0;
  for (uint32_t i = 0; i < page_tab_count; i++) { //каталог страниц
    printf("present: %d ", (page_dir[i]).P);
    printf("read_write: %d ", (page_dir[i]).RW);
    printf("user_supervisor: %d ", (page_dir[i]).US);
    printf("write_through: %d ", (page_dir[i]).PWT);
    printf("cache_disabled: %d ", (page_dir[i]).PCD);
    printf("accessed: %d ", (page_dir[i]).A);
    printf("dirty: %d ", (page_dir[i]).D);
    printf("page_size:%d ", (page_dir[i]).PS);
    printf("ignored: %d ", (page_dir[i]).G);
    printf("page_table_addr: 0x%x ", (uint32_t)((page_dir[i]).address));
    printf("\n");
    // Таблица страниц
    for (uint32_t j = 0; j < MMU_PTE_COUNT; j++) {
      virt_addr = (i << 22) + ((j << 22) >> 10);
      printf("present: %d ", (page_tab[i][j]).P);
      printf("read_write: %d ", (page_tab[i][j]).RW);
      printf("user_supervisor: %d ", (page_tab[i][j]).US);
      printf("write_through: %d ", (page_tab[i][j]).PWT);
      printf("cache_disabled: %d ", (page_tab[i][j]).PCD);
      printf("accessed: %d ", (page_tab[i][j]).A);
      printf("dirty: %d ", (page_tab[i][j]).D);
      printf("PAT: %d ", (page_tab[i][j]).PAT);
      printf("ignored: %d ", (page_tab[i][j]).G);
      printf("page_phys_addr: %p\t", (void *)(uintptr_t)((page_tab[i][j]).ADDRESS));
      printf("page_virt_addr: 0x%x\t", virt_addr);
      printf("\n");
    }
  }
}

int main(int argc, char **argv) {
  uint32_t bin_size;
  cr3 = (uintptr_t)page_dir >> 12 << 12; //кэш не предусмотрен, PAE отключено
  if (argc > 1) {
    if (argv[1][0] == '-') {
      uint8_t i = 1;
      while (argv[1][i] <= '9' && argv[1][i] >= '0') {
        bin_size = bin_size * 10 + (uint32_t)(argv[1][i] - '0');
        i++;
      }
      if (argv[1][i] == 'K' || argv[1][i] == 'k') {
        bin_size *= 1024;
      } else if (argv[1][i] == 'M' || argv[1][i] == 'm') {
        bin_size *= (1024 * 1024);
      }
    }
  } else {
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
    page_dir[i].PS = 1; /* 4KB */
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
      page_tab[i][j].ADDRESS = ((uintptr_t)(&page_0) + j * 4096) >> 12; /* assume 4Kb pages */
    }
  }
  printf("Memory size = %d\n", bin_size);
  printf("Page directory address = %p\n", (void *)page_dir);
  printf("Page table count = %d:\n", page_tab_count);
  printf("Pages count = %d\n", pages_count);
  printf("MMU Table:\n");
  mmu_print(page_tab_count, pages_count);
  return 0;
}
