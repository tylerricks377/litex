#include <generated/csr.h>
#ifdef DFII_BASE

#include <stdio.h>
#include <stdlib.h>

#include <generated/sdram_phy.h>
#include <generated/mem.h>
#include <hw/flags.h>

#include "sdram.h"

static void cdelay(int i)
{
	while(i > 0) {
#if defined (__lm32__)
		__asm__ volatile("nop");
#elif defined (__or1k__)
		__asm__ volatile("l.nop");
#else
#error Unsupported architecture
#endif
		i--;
	}
}

void sdrsw(void)
{
	dfii_control_write(DFII_CONTROL_CKE|DFII_CONTROL_ODT|DFII_CONTROL_RESET_N);
	printf("SDRAM now under software control\n");
}

void sdrhw(void)
{
	dfii_control_write(DFII_CONTROL_SEL);
	printf("SDRAM now under hardware control\n");
}

void sdrrow(char *_row)
{
	char *c;
	unsigned int row;
	
	if(*_row == 0) {
		dfii_pi0_address_write(0x0000);
		dfii_pi0_baddress_write(0);
		command_p0(DFII_COMMAND_RAS|DFII_COMMAND_WE|DFII_COMMAND_CS);
		cdelay(15);
		printf("Precharged\n");
	} else {
		row = strtoul(_row, &c, 0);
		if(*c != 0) {
			printf("incorrect row\n");
			return;
		}
		dfii_pi0_address_write(row);
		dfii_pi0_baddress_write(0);
		command_p0(DFII_COMMAND_RAS|DFII_COMMAND_CS);
		cdelay(15);
		printf("Activated row %d\n", row);
	}
}

void sdrrdbuf(int dq)
{
	int i, p;
	int first_byte, step;

	if(dq < 0) {
		first_byte = 0;
		step = 1;
	} else {
		first_byte = DFII_PIX_RDDATA_SIZE/2 - 1 - dq;
		step = DFII_PIX_RDDATA_SIZE/2;
	}

	for(p=0;p<DFII_NPHASES;p++)
		for(i=first_byte;i<DFII_PIX_RDDATA_SIZE;i+=step)
			printf("%02x", MMPTR(dfii_pix_rddata_addr[p]+4*i));
	printf("\n");
}

void sdrrd(char *startaddr, char *dq)
{
	char *c;
	unsigned int addr;
	int _dq;

	if(*startaddr == 0) {
		printf("sdrrd <address>\n");
		return;
	}
	addr = strtoul(startaddr, &c, 0);
	if(*c != 0) {
		printf("incorrect address\n");
		return;
	}
	if(*dq == 0)
		_dq = -1;
	else {
		_dq = strtoul(dq, &c, 0);
		if(*c != 0) {
			printf("incorrect DQ\n");
			return;
		}
	}
	
	dfii_pird_address_write(addr);
	dfii_pird_baddress_write(0);
	command_prd(DFII_COMMAND_CAS|DFII_COMMAND_CS|DFII_COMMAND_RDDATA);
	cdelay(15);
	sdrrdbuf(_dq);
}

void sdrrderr(char *count)
{
	char *c;
	int _count;
	int i, j, p;
	unsigned char prev_data[DFII_NPHASES*DFII_PIX_RDDATA_SIZE];
	unsigned char errs[DFII_PIX_RDDATA_SIZE/2];

	if(*count == 0) {
		printf("sdrrderr <count>\n");
		return;
	}
	_count = strtoul(count, &c, 0);
	if(*c != 0) {
		printf("incorrect count\n");
		return;
	}

	dfii_pird_address_write(0);
	dfii_pird_baddress_write(0);
	command_prd(DFII_COMMAND_CAS|DFII_COMMAND_CS|DFII_COMMAND_RDDATA);
	cdelay(15);
	for(p=0;p<DFII_NPHASES;p++)
		for(i=0;i<DFII_PIX_RDDATA_SIZE;i++)
			prev_data[p*DFII_PIX_RDDATA_SIZE+i] = MMPTR(dfii_pix_rddata_addr[p]+4*i);
	for(i=0;i<DFII_PIX_RDDATA_SIZE/2;i++)
		errs[i] = 0;

	for(j=0;j<_count;j++) {
		command_prd(DFII_COMMAND_CAS|DFII_COMMAND_CS|DFII_COMMAND_RDDATA);
		cdelay(15);
		for(p=0;p<DFII_NPHASES;p++)
			for(i=0;i<DFII_PIX_RDDATA_SIZE;i++) {
				unsigned char new_data;

				new_data = MMPTR(dfii_pix_rddata_addr[p]+4*i);
				errs[i%(DFII_PIX_RDDATA_SIZE/2)] |= prev_data[p*DFII_PIX_RDDATA_SIZE+i] ^ new_data;
				prev_data[p*DFII_PIX_RDDATA_SIZE+i] = new_data;
			}
	}
	for(i=0;i<DFII_PIX_RDDATA_SIZE/2;i++)
		printf("%02x ", errs[i]);
	printf("\n");
}

void sdrwr(char *startaddr)
{
	char *c;
	unsigned int addr;
	int i;
	int p;

	if(*startaddr == 0) {
		printf("sdrrd <address>\n");
		return;
	}
	addr = strtoul(startaddr, &c, 0);
	if(*c != 0) {
		printf("incorrect address\n");
		return;
	}

	for(p=0;p<DFII_NPHASES;p++)
		for(i=0;i<DFII_PIX_WRDATA_SIZE;i++)
			MMPTR(dfii_pix_wrdata_addr[p]+4*i) = 0x10*p + i;
	
	dfii_piwr_address_write(addr);
	dfii_piwr_baddress_write(0);
	command_pwr(DFII_COMMAND_CAS|DFII_COMMAND_WE|DFII_COMMAND_CS|DFII_COMMAND_WRDATA);
}

#define TEST_SIZE (4*1024*1024)

int memtest_silent(void)
{
	volatile unsigned int *array = (unsigned int *)SDRAM_BASE;
	int i;
	unsigned int prv;
	unsigned int error_cnt;
	
	prv = 0;
	for(i=0;i<TEST_SIZE/4;i++) {
		prv = 1664525*prv + 1013904223;
		array[i] = prv;
	}
	
	prv = 0;
	error_cnt = 0;
	for(i=0;i<TEST_SIZE/4;i++) {
		prv = 1664525*prv + 1013904223;
		if(array[i] != prv)
			error_cnt++;
	}
	return error_cnt;
}

int memtest(void)
{
	unsigned int e;

	e = memtest_silent();
	if(e != 0) {
		printf("Memtest failed: %d/%d words incorrect\n", e, TEST_SIZE/4);
		return 0;
	} else {
		printf("Memtest OK\n");
		return 1;
	}
}

int sdrinit(void)
{
	printf("Initializing SDRAM...\n");
	
	init_sequence();
	dfii_control_write(DFII_CONTROL_SEL);
	if(!memtest())
		return 0;
	
	return 1;
}

#endif
