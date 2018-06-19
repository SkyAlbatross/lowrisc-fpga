// See LICENSE for license details.

#include <stdio.h>
#include "encoding.h"
#include "bits.h"
#include "diskio.h"
#include "ff.h"
#include "hid.h"
#include "elf.h"
#include "mini-printf.h"
#include "lowrisc_memory_map.h"

FATFS FatFs;   // Work area (file system object) for logical drive

// max size of file image is 16M
#define MAX_FILE_SIZE 0x1000000

// 4K size read burst
#define SD_READ_SIZE 4096

char md5buf[SD_READ_SIZE];

int main (void)
{
  FIL fil;                // File object
  FRESULT fr;             // FatFs return code
  uint8_t *boot_file_buf = (uint8_t *)(get_ddr_base()) + ((uint64_t)get_ddr_size()) - MAX_FILE_SIZE; // at the end of DDR space
  uint8_t *memory_base = (uint8_t *)(get_ddr_base());

  printf("lowRISC boot program\n=====================================\n");

  // Register work area to the default drive
  if(f_mount(&FatFs, "", 1)) {
    printf("Fail to mount SD driver!\n");
    return 1;
  }

  // Open a file
  printf("Load boot0000.bin into memory\n");
  fr = f_open(&fil, "boot0000.bin", FA_READ);
  if (fr) {
    printf("Failed to open boot!\n");
    return (int)fr;
  }

  // Read file into memory
  uint8_t *buf = boot_file_buf;
  uint32_t fsize = 0;           // file size count
  uint32_t br;                  // Read count
  do {
    char *sum;
    fr = f_read(&fil, boot_file_buf+fsize, SD_READ_SIZE, &br);  // Read a chunk of source file
    if (!fr)
      {
	hid_send('\b');
	hid_send("|/-\\"[(fsize/SD_READ_SIZE)&3]);
	fsize += br;
      }
  } while(!(fr || br == 0));

  printf("Load %d bytes to memory address %x from boot.bin of %d bytes.\n", fsize, boot_file_buf, fil.fsize);

  // read elf
  printf("load elf to DDR memory\n");
  if(br = load_elf(boot_file_buf, fil.fsize))
    printf("elf read failed with code %d", br);

  // Close the file
  if(f_close(&fil)) {
    printf("fail to close file!");
    return 1;
  }
  if(f_mount(NULL, "", 1)) {         // unmount it
    printf("fail to umount disk!");
    return 1;
  }

  printf("Boot the loaded program...\n");

  uintptr_t mstatus = read_csr(mstatus);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPP, PRV_M);
  mstatus = INSERT_FIELD(mstatus, MSTATUS_MPIE, 1);
  write_csr(mstatus, mstatus);
  write_csr(mepc, memory_base);
  asm volatile ("mret");
}

void external_interrupt(void)
{
  int i, claim, handled = 0;
#ifdef VERBOSE
  printf("Hello external interrupt! "__TIMESTAMP__"\n");
#endif  
}
