/*
 * Copyright 2010-2011, 2013 Freescale Semiconductor, Inc.
 *    2017-07-26
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <hwconfig.h>
#include <pci.h>
#include <i2c.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/cache.h>
#include <asm/immap_85xx.h>
#include <asm/fsl_pci.h>
#include <fsl_ddr_sdram.h>
#include <asm/io.h>
#include <asm/fsl_law.h>
#include <asm/fsl_lbc.h>
#include <asm/mp.h>
#include <miiphy.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <fsl_mdio.h>
#include <tsec.h>
#include <vsc7385.h>
#include <ioports.h>
#include <asm/fsl_serdes.h>
#include <netdev.h>


#ifdef CONFIG_SYS_CPLD_BASE

typedef struct cpld_data_cont{
	u8 cpld_rev;
	u8 pcba_rev;
	u8 wd_cfg;
	u8 rst_bps_sw;
	u8 load_default_n;
	u8 rst_bps_wd;
	u8 bypass_enable;
	u8 bps_led;
	u8 status_led; 			/* offset: 0x8 */
	u8 rev3;
	u8 vcore_voltage_mgn; 	/* offset: 0xa */
	u8 rev4[2];
	u8 system_rst;			/* offset: 0xd */
	u8 bps_out;
}cpld_data_t;

#define CPLD_WD_CFG		(0x03)
#define CPLD_RST_BSW	(0x00)
#define CPLD_RST_BWD	(0x00)
#define CPLD_BYPASS_EN	(0x03)
#define CPLD_STATUS_LED	(0x01)
#define CPLD_VCORE_VOLT	(0x00)
#define CPLD_SYS_RST	(0x00)

#endif




#ifdef CONFIG_SYS_CPLD_BASE
void board_cpld_init(void)
{
	volatile cpld_data_t *cpld_data = (void *)(CONFIG_SYS_CPLD_BASE);
puts("\nBOARD: CPLD init\n");
#ifdef _SETTINGS_TESTED_
	out_8(&cpld_data->wd_cfg, CPLD_WD_CFG);
	out_8(&cpld_data->rst_bps_sw, CPLD_RST_BSW);
	out_8(&cpld_data->rst_bps_wd, CPLD_RST_BWD);
	out_8(&cpld_data->bypass_enable, CPLD_BYPASS_EN);
	out_8(&cpld_data->status_led, CPLD_STATUS_LED);
	out_8(&cpld_data->vcore_voltage_mgn, CPLD_VCORE_VOLT);
	out_8(&cpld_data->system_rst, CPLD_SYS_RST);
#else
	cpld_data->wd_cfg  			= CPLD_WD_CFG;
	cpld_data->rst_bps_sw 			= CPLD_RST_BSW;
	cpld_data->rst_bps_wd 			= CPLD_RST_BWD;
	cpld_data->bypass_enable 		= CPLD_BYPASS_EN;
	cpld_data->status_led  			= CPLD_STATUS_LED;
	cpld_data->vcore_voltage_mgn 		= CPLD_VCORE_VOLT;
	cpld_data->system_rst 			= CPLD_SYS_RST;
#endif

}
#endif



void delay_B(unsigned long us)
{
/*
unsigned long j;
j= 10000 * n;

while(j--);
*/

while(us--)
{
udelay(40);
udelay(40);
}

}

////////////////////////////////////  board_gpio_init() ///////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////
void board_gpio_init(void)
{

	ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);


	/*clrbits_be32(&pgpio->gpodr, 0x04000000);*/
	setbits_be32(&pgpio->gpdir, 0x04100000);


	setbits_be32(&pgpio->gpdat, 0x04100000);

	/*setbits_be32(&pgpio->gpdat, 0x04100000);
	udelay(1000000);*/

while(1){
	clrbits_be32(&pgpio->gpdat, 0x04100000);
	
	delay_B(1);
	setbits_be32(&pgpio->gpdat, 0x04100000);
	delay_B(20);
}


	/*
	  * GPIO06 RGMII PHY Reset
	  * GPIO10 DDR Reset, open drain
	  * GPIO14 SGMII PHY Reset

	 may do 
	 * GPIO15 PCIE Reset 
	  * GPIO7  LOAD_DEFAULT_N          Input
	  * GPIO11  WDI (watchdog input)   
	  * GPIO12  Ethernet Switch Reset   
	  */

	#if !defined(CONFIG_SYS_RAMBOOT) && !defined(CONFIG_SPL)
	/* init DDR3 reset signal */
	setbits_be32(&pgpio->gpdir, 0x023f0000);
	setbits_be32(&pgpio->gpodr, 0x00200000);
	clrbits_be32(&pgpio->gpdat, 0x00200000);
	udelay(1000);
	setbits_be32(&pgpio->gpdat, 0x00200000);
	udelay(1000);
	clrbits_be32(&pgpio->gpdir, 0x00200000);
	#endif

       /* puts("\niN EARLY_INIT_F console print will not work\n"); */

	/* reset sgmii/rgmii phy & PCIe */
	setbits_be32(&pgpio->gpdir, 0x02010000);
	setbits_be32(&pgpio->gpdat, 0x021f0000);
/*
	udelay(1000);
	clrbits_be32(&pgpio->gpdat, 0x02000000);	*/
}



int board_early_init_f(void)
{
	ccsr_gur_t *gur = (void *)(CONFIG_SYS_MPC85xx_GUTS_ADDR);

	#ifdef CONFIG_MMC
	setbits_be32(&gur->pmuxcr,
			(MPC85xx_PMUXCR_SDHC_CD | MPC85xx_PMUXCR_SDHC_WP));

	/*clrbits_be32(&gur->sdhcdcr, SDHCDCR_CD_INV); */
	
	setbits_be32(&gur->pordevsr2, MPC85xx_PORDEVSR2_SDHC_CD_POL_SEL);

	volatile unsigned int *sdhcdcr = (void *)(0xFFEE0F64);
	clrbits_be32(sdhcdcr, 0x80000000);
	#endif


	clrbits_be32(&gur->pmuxcr, MPC85xx_PMUXCR_SD_DATA);

	#ifdef CONFIG_TDM
	setbits_be32(&gur->pmuxcr, MPC85xx_PMUXCR_TDM_ENA);
	#endif


	board_gpio_init();

	#ifdef CONFIG_SYS_CPLD_BASE
	board_cpld_init();
	#endif

	return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

int checkboard (void)
{
	volatile ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);
	volatile cpld_data_t *cpld_data = (void *)(CONFIG_SYS_CPLD_BASE);

	puts("\nBoard P1020wlan:checkboard rev.00abcd\n");	

#ifdef PORTED_P1020WLAN
	struct cpu_type *cpu = gd->cpu;;

	printf("Board: %sP1020WLAN, fw rev:SKU01 ", cpu->name);

	#ifdef CONFIG_PHYS_64BIT
	printf ("(36-bit addrmap) ");
	#endif
	printf("\n");

	printf("CPLD:  V%d.0\n", cpld_data->cpld_rev & 0x0F);
	printf("PCBA:  V%d.0\n", cpld_data->pcba_rev & 0x0F);

	udelay(10 * 1000);


	/* reset DDR3 */
	setbits_be32(&pgpio->gpdat, 0x20000000);
	udelay(10 * 1000);
	
	/* refuse any ops to ddr enable signal */
	clrbits_be32(&pgpio->gpdir, 0x20000000);

//	creat_squa_wave();
	
#endif
	return 0;
}


#ifdef CONFIG_PCI
void pci_init_board(void)
{
	puts("Board init: PCI\n");
	fsl_pcie_init_board(0);
}
#endif





int board_early_init_r(void)
{
	

#ifdef TARGET_P1020RDB_PD


	const unsigned int flashbase = CONFIG_SYS_FLASH_BASE;
	int flash_esel = find_tlb_idx((void *)flashbase, 1);    //should be =2

	/*
	 * Remap Boot flash region to caching-inhibited
	 * so that flash can be erased properly.
	 */

	/* Flush d-cache and invalidate i-cache of any FLASH data */
	flush_dcache();
	invalidate_icache();

	if (flash_esel == -1) {
		/* very unlikely unless something is messed up */
		puts("Error: Could not find TLB for FLASH BASE\n");
		flash_esel = 2;	/* give our best effort to continue */
	} else {
		/* invalidate existing TLB entry for flash */
		disable_tlb(flash_esel);
	}

	set_tlb(1, flashbase, CONFIG_SYS_FLASH_BASE_PHYS, /* tlb, epn, rpn */
		MAS3_SX|MAS3_SW|MAS3_SR, MAS2_I|MAS2_G,/* perms, wimge */
		0, flash_esel, BOOKE_PAGESZ_64M, 1);/* ts, esel, tsize, iprot */

	return 0;
#endif

	const unsigned int flashbase = CONFIG_SYS_FLASH_BASE;
	const u8 flash_esel = 2;

	/*
	 * Remap Boot flash region to caching-inhibited
	 * so that flash can be erased properly.
	 */

	/* Flush d-cache and invalidate i-cache of any FLASH data */
	flush_dcache();
	invalidate_icache();

	/* invalidate existing TLB entry for flash */
	disable_tlb(flash_esel);

	set_tlb(1, flashbase, CONFIG_SYS_FLASH_BASE_PHYS,	/* tlb, epn, rpn */
			MAS3_SX|MAS3_SW|MAS3_SR, MAS2_I|MAS2_G,	/* perms, wimge */
			0, flash_esel, BOOKE_PAGESZ_64M, 1);	/* ts, esel, tsize, iprot */
	return 0;
}





#ifdef CONFIG_TSEC_ENET
int board_eth_init(bd_t *bis)
{
	struct fsl_pq_mdio_info mdio_info;
	struct tsec_info_struct tsec_info[4];
	ccsr_gur_t *gur __attribute__((unused)) =
		(void *)(CONFIG_SYS_MPC85xx_GUTS_ADDR);
	int num = 0;

#ifdef CONFIG_VSC7385_ENET
	char *tmp;
	unsigned int vscfw_addr;
#endif

#ifdef CONFIG_TSEC1
	SET_STD_TSEC_INFO(tsec_info[num], 1);
	num++;
#endif
#ifdef CONFIG_TSEC2
	SET_STD_TSEC_INFO(tsec_info[num], 2);
	if (is_serdes_configured(SGMII_TSEC2)) {
		printf("eTSEC2 is in sgmii mode.\n");
		tsec_info[num].flags |= TSEC_SGMII;
	}
	num++;
#endif
#ifdef CONFIG_TSEC3
	SET_STD_TSEC_INFO(tsec_info[num], 3);
	/*if (!(gur->pordevsr & MPC85xx_PORDEVSR_SGMII3_DIS))*/
		tsec_info[num].flags |= TSEC_SGMII;
	num++;
#endif

	if (!num) {
		printf("No TSECs initialized\n");
		return 0;
	}


	printf("2T SECs initialized\n");

	mdio_info.regs = TSEC_GET_MDIO_REGS_BASE(1);
	mdio_info.name = DEFAULT_MII_NAME;

	fsl_pq_mdio_init(bis, &mdio_info);

	tsec_eth_init(bis, tsec_info, num);

	return 0;




#if defined(CONFIG_UEC_ETH)
	/*  QE0 and QE3 need to be exposed for UCC1 and UCC5 Eth mode */
	setbits_be32(&gur->pmuxcr, MPC85xx_PMUXCR_QE0);
	setbits_be32(&gur->pmuxcr, MPC85xx_PMUXCR_QE3);

	uec_standard_init(bis);
#endif

	/*return pci_eth_init(bis);*/
}
#endif





#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, bd_t *bd)
{
	phys_addr_t base;
	phys_size_t size;

#if defined(CONFIG_SDCARD) || defined(CONFIG_SPIFLASH)
	int err;
#endif

	ft_cpu_setup(blob, bd);

	base = getenv_bootm_low();
	size = getenv_bootm_size();

	fdt_fixup_memory(blob, (u64)base, (u64)size);

#ifdef CONFIG_PCIE
	FT_FSL_PCI_SETUP;
#endif


return 0;
}
#endif
