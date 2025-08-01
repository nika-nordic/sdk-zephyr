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
	rram_test_write(addr, 0xDEADBEEF);
	rram_test_read(addr);
}

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);

	nrfx_rramc_init(&rramc_config, NULL);
	printf("RRAMC initialized!\n");

	printf("NRF_FICR->INFO.PART=0x%x\n", NRF_FICR->INFO.PART);

	uint32_t ok_addr = RRAM_BOUNDARY - 4;
	uint32_t nok_addr = RRAM_BOUNDARY;

	printf("--- Checking accessible address 0x%x\n", ok_addr);
	rram_address_check(ok_addr);

	printf("--- Checking *not* accessible address 0x%x\n", nok_addr);
	rram_address_check(nok_addr);

	return 0;
}
