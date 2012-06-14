//
//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert
//  source: http://elinux.org/RPi_Low-level_peripherals


// Access from ARM Running Linux

#define BCM2708_PERI_BASE        0x20000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define I2S_BASE                (BCM2708_PERI_BASE + 0x203000) /* I2S controller */


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
char *spi0_mem, *spi0_map;


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

 int i;


  printf("setting IO regs\n");

  // Set GPIO pins 7-11 to output
  for (g=18; g<=18; g++)
  {
    INP_GPIO(g); // must use INP_GPIO before we can use OUT_GPIO
//    OUT_GPIO(g);
    SET_GPIO_ALT(g,0);

  }

 for(i=0; i<9;i++) {
    printf("memory address=0x%x: 0x%x\n", i2s+i, *(i2s+i));
 }

  printf("restting all i2s regs to 0...\n");
  for (g=0; g<9; g++)
  {
  *(i2s+g) = 0x0;
  }

 for(i=0; i<9;i++) {
    printf("memory address=0x%x: 0x%x\n", i2s+i, *(i2s+i));
 }

  printf("disable i2s...\n");
  *(i2s) = 0x0;

  sleep(10);

  printf("disable clock...\n");
  *(i2s+2) = 0x00800000;

  printf("enable i2s...\n");
  *(i2s) = 0x5;
  
  printf("giving time...\n");
  sleep(10);

 for(i=0; i<9;i++) {
    printf("memory address=0x%x: 0x%x\n", i2s+i, *(i2s+i));
 }


  printf("disable i2s...\n");
  *(i2s) = 0x0;

//  sleep(1);

  printf("enable clock...\n");
  *(i2s+2) = 0x00000000;

  sleep(1);

  printf("enable i2s...\n");
  *(i2s) = 0x5;
  
  printf("giving time...\n");
  sleep(10);

  exit(0);
  
 for(i=0; i<9;i++) {
    printf("memory address=0x%x: 0x%x\n", i2s+i, *(i2s+i));
 }

  return 0;

} // main


//
// Set up a memory regions to access GPIO
//
void setup_io()
{

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

