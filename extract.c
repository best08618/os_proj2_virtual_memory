#include<stdio.h>
#include<stdlib.h>
#include<time.h>

int main()
{
	int bit_space;
	unsigned int addr;
	unsigned int offset;	
	unsigned int pageIndex;
	
	srand(time(NULL));
	printf("0x");

	addr = rand() %0xff;
	addr |= (rand()&0xff)<<8;
	printf("%x\n", addr);
	
	offset = addr & 0xfff;
	printf("Offset: 0x%04x\n", offset);

	pageIndex = addr & 0xf000;
	printf("Page Index: 0x%4x\n", pageIndex);

	return 0;
}


