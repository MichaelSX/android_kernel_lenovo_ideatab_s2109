#ifndef __TWL_CORE_H__
#define __TWL_CORE_H__

extern int twl6030_init_irq(int irq_num, unsigned irq_base, unsigned irq_end);
extern int twl6030_exit_irq(void);
extern int twl4030_init_irq(int irq_num, unsigned irq_base, unsigned irq_end);
extern int twl4030_exit_irq(void);
extern int twl4030_init_chip_irq(const char *chip);
/* BokeeLi, 2011/12/16, Add SD detect reconfig */
extern void fih_twl6030_mmc_card_detect_config(u8 *reg_val); 
/* BokeeLi, 2011/12/16, Add SD detect reconfig */
#endif /*  __TWL_CORE_H__ */
