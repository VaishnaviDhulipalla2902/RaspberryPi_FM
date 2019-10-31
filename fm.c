// Access from ARM Running Linux

#define BCM2708_PERI_BASE        0x3F000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */


#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <dirent.h>
#include <math.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

int  shutdown;
int  mem_fd;
void *gpio_map;



volatile unsigned *gpio;
volatile unsigned *mem_map;


#define ACCESS(base) *(volatile int*)((int)mem_map+base-0x7e000000)
#define SETBIT(base, bit) ACCESS(base) |= 1<<bit
#define CLRBIT(base, bit) ACCESS(base) &= ~(1<<bit)

void setup_io();

#define CM_GP0CTL (0x7e101070)
#define GPFSEL0 (0x7E200000)
#define CM_GP0DIV (0x7e101074)

struct GPCTL {
    char SRC         : 4;
    char ENAB        : 1;
    char KILL        : 1;
    char             : 1;
    char BUSY        : 1;
    char FLIP        : 1;
    char MASH        : 2;
    unsigned int     : 13;
    char PASSWD      : 8;
};

void setup_fm(int state)
{
    mem_map = (unsigned *)mmap(
                  NULL,
                  0x01000000,  //len
                  PROT_READ|PROT_WRITE,
                  MAP_SHARED,
                  mem_fd,
                  0x3F000000  //base
              );

    if ((int)mem_map==-1) exit(-1);
    
    // GPIO 4 selecting alt func 0 -> 1-0-0 bits to pin 14-13-12 : FSEL4 -> clock
    SETBIT(GPFSEL0 , 14);
    CLRBIT(GPFSEL0 , 13);
    CLRBIT(GPFSEL0 , 12);

    
    
    // 6 -> PLLD clock 500MHz , ENABLE
    struct GPCTL setupword = {6, state, 0, 0, 0, state,0x5a};
    ACCESS(CM_GP0CTL) = *((int*)&setupword); //setting up clock
}

void shutdown_fm()
{
    if(!shutdown){
        shutdown=1;
        printf("\nShutting Down\n");
        setup_fm(0);// to shutdown the fm
        exit(0);
    }
}


void modulate(int m,int mod)
{
    ACCESS(CM_GP0DIV) = (0x5a << 24) + mod + m;

}

void playWav(char* filename,int mod,float bw)
{
    int fp;int intval;
    fp = open(filename, 'r');
    lseek(fp, 22, SEEK_SET); //Skip header .wav file 44 bytes
    short* data = (short*)malloc(1024);
    
   
    while (read(fp, data, 1024))  { // reading data from .wav file
	    for (int j=0; j<1024/2; j++){
            	float dval = (float)(data[j])/65536.0*bw;
            	intval = (int)(floor(dval));
            	for (int i=0; i<300; i++) {
              		modulate(intval,mod);
            	}
	    }
    }
}



int main(int argc, char **argv)
{
    
    	// Set up gpi pointer for direct register access
    	signal(SIGTERM, &shutdown_fm);
    	signal(SIGINT, &shutdown_fm);
   	atexit(&shutdown_fm);

    	setup_io();

    	setup_fm(1);

	// argv[1] - filename, argv[2]-carrier frequency,argv[3]-bandwidth
    	float freq = atof(argv[2]);   
    	float bw;
    	int mod = (500/freq)*4096;
    	modulate(0,mod);
    	if (argc==3){
      		bw = 8.0;
      		playWav(argv[1],mod,bw);// if bandwidth is not given by default bw=8
    	}else if (argc==4){
      		bw = atof(argv[3]);
      		playWav(argv[1],mod,bw);
    	}else{
		printf("Arguments not given");
	}
}

void setup_io()
{
    /* open /dev/mem */
    if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
        printf("can't open /dev/mem \n");
        exit (-1);
    }
    /* mmap GPIO */
    // Allocate MAP block
   
     /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );
   
    // Always use volatile pointer!
    gpio = (volatile unsigned *)gpio_map;
} // setup_io
