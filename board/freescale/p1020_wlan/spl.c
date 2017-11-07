/*
 * Copyright 2013 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <console.h>
#include <ns16550.h>
#include <malloc.h>
#include <mmc.h>
#include <nand.h>
#include <i2c.h>
#include <fsl_esdhc.h>
#include <spi_flash.h>
#include "../common/spl.h"

DECLARE_GLOBAL_DATA_PTR;

static const u32 sysclk_tbl[] = {
	66666000, 7499900, 83332500, 8999900,
	99999000, 11111000, 12499800, 13333200
};

phys_size_t get_effective_memsize(void)
{
	return CONFIG_SYS_L2_SIZE;
}

void board_init_f(ulong bootflag)
{
	u32 plat_ratio, bus_clk;
	ccsr_gur_t *gur = (void *)CONFIG_SYS_MPC85xx_GUTS_ADDR;

	volatile ccsr_gpio_t *pgpio = (void *)(CONFIG_SYS_MPC85xx_GPIO_ADDR);

	console_init_f();
	
	
	clrbits_be32(&pgpio->gpdat, 0x04000000);
	setbits_be32(&pgpio->gpdir, 0x04000000);

	#if !defined(CONFIG_SYS_RAMBOOT) && !defined(CONFIG_SPL)
	setbits_be32(&pgpio->gpdir, 0x00200000);

	setbits_be32(&pgpio->gpodr, 0x00200000);
	clrbits_be32(&pgpio->gpdat, 0x00200000);
	udelay(10*1000);
	setbits_be32(&pgpio->gpdat, 0x00200000);
	#endif

	udelay(10*1000);

	/* Set pmuxcr to allow both i2c1 and i2c2 */				 /* Comment seems as legacy on, nothing done like that here*/
	setbits_be32(&gur->pmuxcr, in_be32(&gur->pmuxcr) | 0x1000);
	setbits_be32(&gur->pmuxcr,
		     in_be32(&gur->pmuxcr) | MPC85xx_PMUXCR_SD_DATA);		/*  mmc[4:7] pins for MMC 8-bit Mode.*/

	/* Read back the register to synchronize the write. */
	in_be32(&gur->pmuxcr);

#ifdef CONFIG_SPL_SPI_BOOT
	clrbits_be32(&gur->pmuxcr, MPC85xx_PMUXCR_SD_DATA);
#endif


	/* initialize selected port with appropriate baud rate */
	plat_ratio = in_be32(&gur->porpllsr) & MPC85xx_PORPLLSR_PLAT_RATIO;	/* mask all but last 26-31 bit */
	plat_ratio >>= 1;							/*last bit(31) removal to get plat_ratio. */
	bus_clk = CONFIG_SYS_CLK_FREQ * plat_ratio;
	gd->bus_clk = bus_clk;

	NS16550_init((NS16550_t)CONFIG_SYS_NS16550_COM1,
		     bus_clk / 16 / CONFIG_BAUDRATE);
#ifdef CONFIG_SPL_MMC_BOOT
	puts("\nSD boot...\n");
#elif defined(CONFIG_SPL_SPI_BOOT)
	puts("\nSPI Flash boot...\n");
#endif

	/* copy code to RAM and jump to it - this should not return */
	/* NOTE - code has to be copied out of NAND buffer before
	 * other blocks can be read.
	 */
	relocate_code(CONFIG_SPL_RELOC_STACK, 0, CONFIG_SPL_RELOC_TEXT_BASE);	/* call to assembly function in start.S */

	/* relocate spl code to f8f8_1000 from f8f8f_0000, new stack ptr and Global data ptr also declared, meaning this is making of CRT , implies further ahead fully function C code/function will run , till now it was running in assembly context.*/
}

void board_init_r(gd_t *gd, ulong dest_addr)
{
	/* Pointer is writable since we allocated a register for it */
	gd = (gd_t *)CONFIG_SPL_GD_ADDR;
	bd_t *bd;

	memset(gd, 0, sizeof(gd_t));
	bd = (bd_t *)(CONFIG_SPL_GD_ADDR + sizeof(gd_t));
	memset(bd, 0, sizeof(bd_t));
	gd->bd = bd;
	bd->bi_memstart = CONFIG_SYS_INIT_L2_ADDR;
	bd->bi_memsize = CONFIG_SYS_L2_SIZE;

	probecpu();
	get_clocks();
	mem_malloc_init(CONFIG_SPL_RELOC_MALLOC_ADDR,
			CONFIG_SPL_RELOC_MALLOC_SIZE);
	gd->flags |= GD_FLG_FULL_MALLOC_INIT;

#ifndef CONFIG_SPL_NAND_BOOT
	env_init();
#endif
#ifdef CONFIG_SPL_MMC_BOOT
	mmc_initialize(bd);
#endif
	/* relocate environment function pointers etc. */
#ifdef CONFIG_SPL_NAND_BOOT
	nand_spl_load_image(CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE,
			    (uchar *)CONFIG_ENV_ADDR);
	gd->env_addr  = (ulong)(CONFIG_ENV_ADDR);
	gd->env_valid = 1;
#else
	env_relocate();
#endif

#ifdef CONFIG_SYS_I2C
	i2c_init_all();
#else
	i2c_init(CONFIG_SYS_I2C_SPEED, CONFIG_SYS_I2C_SLAVE);
#endif
	puts("\nSetting dram...\n");
	gd->ram_size = initdram(0);
#ifdef CONFIG_SPL_NAND_BOOT
	puts("Tertiary program loader running in sram...");
#else
	puts("Second program loader running in sram...\n");
#endif

#ifdef CONFIG_SPL_MMC_BOOT
	mmc_boot();
#elif defined(CONFIG_SPL_SPI_BOOT)
	fsl_spi_boot();
#elif defined(CONFIG_SPL_NAND_BOOT)
	nand_boot();
#endif
}
