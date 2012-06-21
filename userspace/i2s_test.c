//
// output A0A0A0 on PCM_DOUT with about 6Mhz
// based on source from: http://elinux.org/RPi_Low-level_peripherals

#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define I2S_BASE                (BCM2708_PERI_BASE + 0x203000) /* GPIO controller */
#define CLOCK_BASE               (BCM2708_PERI_BASE + 0x101000) /* Clocks */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>

#include <unistd.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

int  mem_fd;
char *gpio_mem, *gpio_map;
char *i2s_mem, *i2s_map;
char *clk_mem, *clk_map;


// I/O access
volatile unsigned *gpio;
volatile unsigned *i2s;
volatile unsigned *clk;


// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

void setup_io();

int main(int argc, char **argv)
{ int g,rep;

  setup_io();

  printf("Setting GPIO regs to alt0\n");
  for (g=18; g<=21; g++)
  {
    INP_GPIO(g);
    SET_GPIO_ALT(g,0);
  }

 int i;
 printf("Memory dump\n");
 for(i=0; i<10;i++) {
    printf("GPIO memory address=0x%08x: 0x%08x\n", gpio+i, *(gpio+i));
 }

// for(i=0x26; i<=0x27;i++) {
//   printf("Clock memory address=0x%08x: 0x%08x\n", clk+i, *(clk+i));
// }

 printf("Disabling I2S clock\n");
 *(clk+0x26) = 0x5A000000;
 *(clk+0x27) = 0x5A000000;
 
 usleep(10);

 printf("Confiure I2S clock\n");
 *(clk+0x26) = 0x5A000001;
 *(clk+0x27) = 0x5A000000 | 3<<12 | 1<<9;

 usleep(10);
 printf("Enabling I2S clock\n");
 *(clk+0x26) = 0x5A000011;

  // disable I2S so we can modify the regs
  printf("Disable I2S\n");
  *(i2s+0) &= ~(1<<24);
  usleep(100);
  *(i2s+0) = 0;
  usleep(100);

  printf("Clearing FIFOs\n");
  *(i2s+0) |= 1<<3 | 1<<4 | 11<5; // clear TX FIFO
  usleep(10);

  // set register settings
  // --> enable Channel1 with 32bit width
  printf("Setting TX channel settings\n");
  *(i2s+4) = 1<<31 | 1<<30 | 8<<16;
  // --> frame width 31+1 bit
  *(i2s+2) = 31<<10;

  // --> disable STBY 
  printf("disabling standby\n");
  *(i2s+0) |= 1<<25;
  usleep(50);

  // enable I2S
  *(i2s+0) |= 0x01;

  // enable transmission
  *(i2s+0) |= 0x04;
  
  // --> ENABLE SYNC bit
  printf("setting sync bit high\n");
  *(i2s+0) |= 1<<24;

  if (*(i2s+0) & 1<<24) {
    printf("SYNC bit high, strange.\n");
  } else {
    printf("SYNC bit low, as expected.\n");
  }

  usleep(1);
  
  if (*(i2s+0) & 1<<24) {
    printf("SYNC bit high, as expected.\n");
  } else {
    printf("SYNC bit low, strange.\n");
  }

    printf("Memory dump\n");
    for(i=0; i<9;i++) {
       printf("I2S memory address=0x%08x: 0x%08x\n", i2s+i, *(i2s+i));
    }

  // fill FIFO in while loop
  int count=0;
  printf("going into loop\n");

  while (1) {

    while ((*i2s) & (1<<19)) {
      // FIFO accepts data
      *(i2s+1) = 0xA0A0A0A0;
      
      if ((++count % 1000000)==0)
         printf("Filling FIFO, count=%i\n", count);
         
    }
    
  }

  return 0;

} // main


//
// Set up a memory regions to access GPIO
//
void setup_io()
{
  printf("setup io\n");

   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit (-1);
   }

   /* mmap GPIO */

   // Allocate MAP block
   if ((gpio_mem = malloc(BLOCK_SIZE + (PAGE_SIZE-1))) == NULL) {
      printf("allocation error \n");
      exit (-1);
   }

   // Allocate MAP block
   if ((i2s_mem = malloc(BLOCK_SIZE + (PAGE_SIZE-1))) == NULL) {
      printf("allocation error \n");
      exit (-1);
   }

   // Allocate MAP block
   if ((clk_mem = malloc(BLOCK_SIZE + (PAGE_SIZE-1))) == NULL) {
      printf("allocation error \n");
      exit (-1);
   }

   // Make sure pointer is on 4K boundary
   if ((unsigned long)gpio_mem % PAGE_SIZE)
     gpio_mem += PAGE_SIZE - ((unsigned long)gpio_mem % PAGE_SIZE);
   // Make sure pointer is on 4K boundary
   if ((unsigned long)i2s_mem % PAGE_SIZE)
     i2s_mem += PAGE_SIZE - ((unsigned long)i2s_mem % PAGE_SIZE);
   // Make sure pointer is on 4K boundary
   if ((unsigned long)clk_mem % PAGE_SIZE)
     clk_mem += PAGE_SIZE - ((unsigned long)clk_mem % PAGE_SIZE);

   // Now map it
   gpio_map = (unsigned char *)mmap(
      (caddr_t)gpio_mem,
      BLOCK_SIZE,
      PROT_READ|PROT_WRITE,
      MAP_SHARED|MAP_FIXED,
      mem_fd,
      GPIO_BASE
   );

   // Now map it
   i2s_map = (unsigned char *)mmap(
      (caddr_t)i2s_mem,
      BLOCK_SIZE,
      PROT_READ|PROT_WRITE,
      MAP_SHARED|MAP_FIXED,
      mem_fd,
      I2S_BASE
   );

   clk_map = (unsigned char *)mmap(
      (caddr_t)clk_mem,
      BLOCK_SIZE,
      PROT_READ|PROT_WRITE,
      MAP_SHARED|MAP_FIXED,
      mem_fd,
      CLOCK_BASE
   );

   if ((long)gpio_map < 0) {
      printf("mmap error %d\n", (int)gpio_map);
      exit (-1);
   }
   if ((long)i2s_map < 0) {
      printf("mmap error %d\n", (int)i2s_map);
      exit (-1);
   }
   if ((long)clk_map < 0) {
      printf("mmap error %d\n", (int)clk_map);
      exit (-1);
   }

   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;
   i2s = (volatile unsigned *)i2s_map;
   clk = (volatile unsigned *)clk_map;


} // setup_io

