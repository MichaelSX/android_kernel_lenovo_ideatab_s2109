#ifndef TARGET_H
#define TARGET_H

/* There are 8 bits for FIHV. (b7-b0)
   bit 7: 1: FIHTDC
   bit 5-6: Project name. (00: BEZ; 01: OCT; 02: AD1; 03: M92; 04: FP400; 05: MUL; 06:TSN)
   bit 4: Flag (1: HW ID flag)
   bit 0-3: Flag value. 
*/

#define FIHV_GOX       10800000
#define FIHV_GOX_B0    10810000
#define FIHV_GOX_B1    10810001
#define FIHV_GOX_B2    10810002
#define FIHV_GOX_B3    10810003
#define FIHV_GOX_B4    10810004
#define FIHV_GOX_B5    10810005
#define FIHV_GOX_B6    10810006
#define FIHV_GOX_B7    10810007
#define FIHV_GOX_B8    10810008
#define FIHV_GOX_B9    10810009

#endif /* TARGET_H */
