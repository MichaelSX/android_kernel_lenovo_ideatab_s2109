#ifndef FIH_SETUP_H
#define FIH_SETUP_H

/* SW5, PinyCHWu, 20111005 { */
#ifdef CONFIG_FIH_HW_INFO
#define ATAG_FIH_HWID	0x48574944
struct tag_fih_hwid {
	u32 product_id;
	u32 phase_id;
	u32 band_id;
};
#else
#define tag_fih_hwid	NULL
#endif
/* }, SW5, PinyCHWu, 20111005 */

#endif /* FIH_SETUP_H */
