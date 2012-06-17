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

void sendFrame(int subframe, char preamble) {

    static int lastBit;

    // encode subframe into 2x32bit BMC
    uint64_t w;
    
    if (preamble == 'X' && lastBit)
       w = 0b00011101;
    else if (preamble == 'X' && !lastBit)
       w = 0b11100010;
    else if (preamble == 'Y' && lastBit)
       w = 0b00011011;
    else if (preamble == 'Y' && !lastBit)
       w = 0b11100100;
    else if (preamble == 'Z' && lastBit)
       w = 0b00010111;
    else if (preamble == 'Z' && !lastBit)
       w = 0b11101000;


    // encode bmc
    int t;
    for (t=4; t<32; t++) {
      w |= (lastBit?0:1)<<(t*2);

      if (subframe & (1<<t))
        w |= (lastBit?1:0)<<(t*2+1);
      else
        w |= (lastBit?0:1)<<(t*2+1);

      lastBit = w & (1<<(t*2+1));
    }

    int part1 = (int)(w & 0x00000000FFFFFFFF);
    int part2 = (int)((w>>32) & 0x00000000FFFFFFFF);

    while (!((*i2s) & (1<<19)));
    *(i2s+1) = part1;

    while (!((*i2s) & (1<<19)));
    *(i2s+1) = part2;


}

int nextChannelStatusBit() {
 static position = 0;
 position = position % (32*24);
 int c = 0;
 c |= 1<<2; // copy permit
 
 if (position++ >= 30)
   return 0;
 else
   return (c & 1<<position)?1:0;
}

int evenParity(int word) {
 int t;
 int count=0;
 for(t=4;t<31;t++)
   if (word & 1<<t)
     count++;
 
 return (count%2==0)?0:1;
}

int makeSubFrame() {

  uint16_t pcm = 0xAAAA;
  
  int ret = pcm<<8;
  ret |= 1<<28; // validity bit
  ret |= nextChannelStatusBit()<<30;
  ret |= evenParity(ret)<<31;

  return ret;
}

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

 for(i=0x26; i<=0x27;i++) {
   printf("Clock memory address=0x%08x: 0x%08x\n", clk+i, *(clk+i));
 }
 
 printf("Confiure I2S clock\n");
 *(clk+0x27) = 0x5A000000 | 0<<12;

 printf("Enabling I2S clock\n");
 *(clk+0x26) = 0x5A000011;

 for(i=0x26; i<=0x27;i++) {
   printf("Clock memory address=0x%08x: 0x%08x\n", clk+i, *(clk+i));
 }

  // disable I2S so we can modify the regs
  printf("Disable I2S\n");
  *(i2s+0) &= ~(1<<24);
  usleep(100);
  *(i2s+0) = 0;
  usleep(100);

  // XXX: seems not to be working. FIFO still full, after this.
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
  int e=0;
  printf("going into loop\n");

  while (1) {
    // send one audio block

    int f;
    for (f=0; f<192; f++) {
      // send one frame
      
        // channels are identical
        int sf = makeSubFrame();
        
        // send channel A
        if (f==0)
          sendFrame(sf,'Z');
        else
          sendFrame(sf,'X');
        
        // send channel B
        sendFrame(sf,'Y');
    }
  
    while ((*i2s) & (1<<19)) {
      // FIFO accepts data
      *(i2s+1) = 0xAAAAAAAA;
      
      if ((++count % 1000000)==0)
         printf("Filling FIFO, empty=%i count=%i\n", e, count);
         
    }
    
    e++;
    
    // Toogle SYNC Bit
    //( XXX: do I have to deactivate I2S to do that? datasheet is unclear)
/*    *(i2s+0) &= ~(0x01);
    *(i2s+0) ^= 1<<24;
    *(i2s+0) |= 0x01;
*/
//    sleep(1);

/*    
    printf("Memory dump\n");
    for(i=0; i<9;i++) {
       printf("I2S memory address=0x%08x: 0x%08x\n", i2s+i, *(i2s+i));
    }
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

