#ifndef _FIH_HW_INFO_H
#define _FIH_HW_INFO_H
#include <linux/platform_device.h>

typedef enum 
{
  Product_Unknow = 0,

  // GO series ++
  Product_GOX = 1,
  Product_GOX_MAX = 20,

  Product_ID_MAX = 255,
  Product_ID_EXTEND_TO_DWORD = 0xFFFFFFFF,
}fih_virtual_project_id_type;

typedef enum 
{
  Product_PR_Unknow = 0,

  Product_EVB   = 1,
  Product_PR1   = 10,
  Product_PR1p1 = 11,
  Product_PR2   = 20,
  Product_PR3   = 30,
  Product_PR4   = 40,
  Product_PR5   = 50,
  Product_PCR   = 100,

  Product_PHASE_MAX   = 255,
  Product_PHASE_EXTEND_TO_DWORD = 0xFFFFFFFF,
}fih_virtual_phase_id_type;

typedef enum 
{
  FIH_NO_BAND = 0,

  // GSM Quad-band + WCDMA
  FIH_BAND_W1245 = 1,
  FIH_BAND_W1248,
  FIH_BAND_W125,
  FIH_BAND_W128,
  FIH_BAND_W25,
  FIH_BAND_W18,
  FIH_BAND_W15,

  // CDMA (and EVDO) only
  FIH_BAND_C01 = 21,
  FIH_BAND_C0,
  FIH_BAND_C1,
  FIH_BAND_C01_AWS,

  // Multimode + Quad-band
  FIH_BAND_W1245_C01 = 41,
  
  // GSM Tri-band + WCDMA
  FIH_BAND_W1245_G_850_1800_1900 = 61,
  FIH_BAND_W1248_G_900_1800_1900,
  FIH_BAND_W125_G_850_1800_1900,
  FIH_BAND_W128_G_900_1800_1900,
  FIH_BAND_W25_G_850_1800_1900,
  FIH_BAND_W18_G_900_1800_1900,
  FIH_BAND_W15_G_850_1800_1900,
  FIH_BAND_W1_XI_G_900_1800_1900,

  FIH_BAND_MAX = 255,
  FIH_BAND_EXTEND_TO_DWORD = 0xFFFFFFFF,
}fih_virtual_band_id_type;

typedef struct {
	int	id;
	char	str[64];
}fih_id_to_str;

typedef struct {
  fih_virtual_project_id_type   virtual_project_id;
  fih_virtual_phase_id_type     virtual_phase_id;
  fih_virtual_band_id_type      virtual_band_id;
}fih_hw_info_type;

typedef struct {
  unsigned int hwid1_index;
  unsigned int hwid2_index;
  unsigned int hwid3_index;
  fih_virtual_project_id_type virtual_project_id_value;
  fih_virtual_phase_id_type virtual_phase_id_value;
  fih_virtual_band_id_type virtual_band_id_value;
  char* name_string;
}fih_hwid_entry;

//only for setup.c to set id value from u-boot tags
extern void fih_set_product_id(unsigned int product_id);
extern void fih_set_product_phase(unsigned int phase_id);
extern void fih_set_band_id(unsigned int band_id);

//for general used
#ifdef CONFIG_FIH_HW_INFO
extern unsigned int fih_get_product_id(void);
extern unsigned int fih_get_product_phase(void);
extern unsigned int fih_get_band_id(void);
#else
#define fih_get_product_id	NULL
#define fih_get_product_phase	NULL
#define fih_get_band_id		NULL
#endif

//called by adc driver once for calculate hwid, but not use now.
extern void get_hwid(void);

//called for setup process
#ifdef CONFIG_FIH_HW_INFO
int __init parse_tag_fih_hwid(const struct tag *tag);
#else
#define parse_tag_fih_hwid	NULL
#endif

#endif /* end of _FIH_HW_INFO_H */
