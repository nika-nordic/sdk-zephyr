#include <stdio.h>
#include <nrfx_rramc.h>

#if defined(NRF54L10_XXAA)
#define RRAM_BOUNDARY (1012 * 1024)
#elif defined(NRF54L15_XXAA)
#define RRAM_BOUNDARY (1524 * 1024)
#else
#error "Unsupported target"
#endif

static const nrfx_rramc_config_t rramc_config = NRFX_RRAMC_DEFAULT_CONFIG(0);

static void rram_test_write(uint32_t addr, uint32_t dataword)
{
	printf("Writing 0x%x to 0x%x\n", dataword, addr);
	nrfy_rramc_word_write(NRF_RRAMC, addr, dataword);
}

static void rram_test_read(uint32_t addr)
{
	uint32_t dataword;

	printf("Reading from 0x%x\n", addr);
	dataword = nrfy_rramc_word_read(addr);
	printf("*0x%x = 0x%x\n", addr, dataword);
}

static void rram_address_check(uint32_t addr)
{
	rram_test_read(addr);
	//rram_test_write(addr, 0xDEADBEEF);
	//rram_test_read(addr);
}

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

	nrfx_rramc_init(&rramc_config, NULL);
	printf("RRAMC initialized!\n");

	printf("NRF_FICR->INFO.PART=0x%x\n", NRF_FICR->INFO.PART);

#define NRF_MPC NRF_MPC00
	printf("MPC settings\n");
	uint32_t mpc_region_config_offset = 0;
	uint32_t mpc_region_startaddr_offset = 4;
	uint32_t mpc_region_addrmask_offset = 8;
	uint32_t mpc_region_array_offset = 16;
	uint8_t * mpc_region_base = (uint8_t *)NRF_MPC + 0x600;
	for (size_t i = 0; i < 8; i++) {
		printf("MPC->REGION[%d].CONFIG=0x%x\n", i, *(uint32_t volatile *)(mpc_region_base + mpc_region_config_offset));
		printf("MPC->REGION[%d].STARTADDR=0x%x\n", i, *(uint32_t volatile *)(mpc_region_base + mpc_region_startaddr_offset));
		printf("MPC->REGION[%d].ADDRMASK=0x%x\n", i, *(uint32_t volatile *)(mpc_region_base + mpc_region_addrmask_offset));
		mpc_region_base += mpc_region_array_offset;
	}
	for (size_t i = 0; i < 7; i++) {
		printf("MPC->OVERRIDE[%d].CONFIG=0x%x\n", i, NRF_MPC->OVERRIDE[i].CONFIG);
		printf("MPC->OVERRIDE[%d].STARTADDR=0x%x\n", i, NRF_MPC->OVERRIDE[i].STARTADDR);
		printf("MPC->OVERRIDE[%d].ENDADDR=0x%x\n", i, NRF_MPC->OVERRIDE[i].ENDADDR);
		printf("MPC->OVERRIDE[%d].PERM=0x%x\n", i, NRF_MPC->OVERRIDE[i].PERM);
		printf("MPC->OVERRIDE[%d].PERMMASK=0x%x\n", i, NRF_MPC->OVERRIDE[i].PERMMASK);
	}

#if 0
	uint32_t ok_addr = RRAM_BOUNDARY - 4;
	uint32_t nok_addr = RRAM_BOUNDARY;

	printf("--- Checking accessible address 0x%x\n", ok_addr);
	rram_address_check(ok_addr);

	printf("--- Checking *not* accessible address 0x%x\n", nok_addr);
	rram_address_check(nok_addr);
#endif

	uint32_t addr;

	addr = 64 * 1024;
	printf("--- Checking address 0x%x\n", addr);
	rram_address_check(addr);

	//---

	addr = RRAM_BOUNDARY - (20 * 1024) - 4;
	printf("--- Checking address 0x%x\n", addr);
	rram_address_check(addr);

	addr += 4;
	printf("--- Checking address 0x%x\n", addr);
	rram_address_check(addr);
	//---

	addr = RRAM_BOUNDARY - (16 * 1024) - 4;
	printf("--- Checking address 0x%x\n", addr);
	rram_address_check(addr);

	addr += 4;
	printf("--- Checking address 0x%x\n", addr);
	rram_address_check(addr);
	//---

	addr = RRAM_BOUNDARY - (12 * 1024) - 4;
	printf("--- Checking address 0x%x\n", addr);
	rram_address_check(addr);

	addr += 4;
	printf("--- Checking address 0x%x\n", addr);
	rram_address_check(addr);
	//---

	addr = RRAM_BOUNDARY - (8 * 1024) - 4;
	printf("--- Checking address 0x%x\n", addr);
	rram_address_check(addr);

	addr += 4;
	printf("--- Checking address 0x%x\n", addr);
	rram_address_check(addr);
	//---

	addr = RRAM_BOUNDARY - (4 * 1024) - 4;
	printf("--- Checking address 0x%x\n", addr);
	rram_address_check(addr);

	addr += 4;
	printf("--- Checking address 0x%x\n", addr);
	rram_address_check(addr);
	//---

	addr = RRAM_BOUNDARY - (0 * 1024) - 4;
	printf("--- Checking address 0x%x\n", addr);
	rram_address_check(addr);

	addr += 4;
	printf("--- Checking address 0x%x\n", addr);
	rram_address_check(addr);

	return 0;
}
