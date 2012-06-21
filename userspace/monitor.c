//
//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert
//  source: http://elinux.org/RPi_Low-level_peripherals

// output white noise on I2S

// Access from ARM Running Linux

#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define I2S_BASE                (BCM2708_PERI_BASE + 0x203000) /* GPIO controller */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

int  mem_fd;
char *gpio_mem, *gpio_map;
char *i2s_mem, *i2s_map;


// I/O access
volatile unsigned *gpio;
volatile unsigned *i2s;


// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))

#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0

void setup_io();

int main(int argc, char **argv)
{ int g,rep;

  // Set up gpi pointer for direct register access
  setup_io();

  // Switch GPIO 7..11 to output mode

 /************************************************************************\
  * You are about to change the GPIO settings of your computer.          *
  * Mess this up and it will stop working!                               *
  * It might be a good idea to 'sync' before running this program        *
  * so at least you still have your code changes written to the SD-card! *
 \************************************************************************/

  // fill FIFO in while loop
  int count=0;
  printf("going into loop\n");
  int oldReg[8];
  while (1) {
  
    int i,j;
    for (j=0; j<7; j++) {
      int newReg = *(i2s+j);
      if (oldReg[j] != newReg) {
        printf("Field changed. addr=%i, old=0x%08x, new=0x%08x, xor=0x%08x\n",j, oldReg[j], newReg, oldReg[j]^newReg);
        oldReg[j] = newReg;
      
        for(i=0; i<10;i++) {
          printf("memory address=0x%08x: 0x%08x\n", i2s+i, *(i2s+i));
       }
    }

    }
    
/*
   if (*i2s & 1<<13)
     printf("TX FIFO in sync\n");
   else
     printf("TX FIFO OUT OF sync\n");

   if (*i2s & 1<<15)
     printf("TX FIFO ERROR\n");

   if (*i2s & 1<<15)
     printf("TX FIFO ERROR\n");

   if (*i2s & 1<<16) {
     printf("RX FIFO ERROR, clearing\n");
     *i2s &= ~(1<<16);
   }

   if (*i2s & 1<<17)
     printf("TX FIFO needs writing\n");

   if (*i2s & 1<<19)
     printf("TX FIFO can accept data\n");

   if (*i2s & 1<<21)
     printf("TX FIFO EMPTY\n");

   if (*i2s & 1<<24)
     printf("SYNC Bit set\n");

//9    printf("Read shit:%i\n",*(i2s+1));
*/
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

   // Make sure pointer is on 4K boundary
   if ((unsigned long)gpio_mem % PAGE_SIZE)
     gpio_mem += PAGE_SIZE - ((unsigned long)gpio_mem % PAGE_SIZE);
   // Make sure pointer is on 4K boundary
   if ((unsigned long)i2s_mem % PAGE_SIZE)
     i2s_mem += PAGE_SIZE - ((unsigned long)i2s_mem % PAGE_SIZE);

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

   if ((long)gpio_map < 0) {
      printf("mmap error %d\n", (int)gpio_map);
      exit (-1);
   }
   if ((long)i2s_map < 0) {
      printf("mmap error %d\n", (int)i2s_map);
      exit (-1);
   }

   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;
   i2s = (volatile unsigned *)i2s_map;


} // setup_io

