/** 
* Debug Configuration Tool
********************************************************/
#include "fih_dbg.h"
#include <mach/alog_ram_console.h>

#ifdef CONFIG_FIH_LAST_ALOG 

/* Unused memory allocation for Tesla : 0xA0000000 - 0xA001FFFF 
     FIH reserve this memory to ram console*/
#define RAM_CONSOLE_BASE 0xA0000000
#define RAM_CONSOLE_SIZE 0x20000

static struct resource ram_console_resources[] = {
    {
	.start = RAM_CONSOLE_BASE,
	.end = RAM_CONSOLE_BASE + RAM_CONSOLE_SIZE -1,
	.flags = IORESOURCE_MEM,
    }
};

static struct platform_device ram_console_device = {
	.name = "ram_console",
	.id = -1,
	.num_resources = ARRAY_SIZE(ram_console_resources),
	.resource = ram_console_resources,
};

#define ALOG_RAM_CONSOLE_PHYS_MAIN (RAM_CONSOLE_BASE + RAM_CONSOLE_SIZE)

#define ALOG_RAM_CONSOLE_SIZE_MAIN 0x00020000 //128KB
#define ALOG_RAM_CONSOLE_PHYS_RADIO (ALOG_RAM_CONSOLE_PHYS_MAIN +  ALOG_RAM_CONSOLE_SIZE_MAIN)
#define ALOG_RAM_CONSOLE_SIZE_RADIO 0x00020000 //128KB
#define ALOG_RAM_CONSOLE_PHYS_EVENTS (ALOG_RAM_CONSOLE_PHYS_RADIO + ALOG_RAM_CONSOLE_SIZE_RADIO)
#define ALOG_RAM_CONSOLE_SIZE_EVENTS 0x00020000 //128KB
#define ALOG_RAM_CONSOLE_PHYS_SYSTEM (ALOG_RAM_CONSOLE_PHYS_EVENTS + ALOG_RAM_CONSOLE_SIZE_EVENTS)
#define ALOG_RAM_CONSOLE_SIZE_SYSTEM 0x00020000 //128KB

static struct resource alog_ram_console_resources[4] = {
        [0] = {
        .name = "alog_main_buffer",
                .start  = ALOG_RAM_CONSOLE_PHYS_MAIN,
                .end    = ALOG_RAM_CONSOLE_PHYS_MAIN + ALOG_RAM_CONSOLE_SIZE_MAIN - 1,
                .flags  = IORESOURCE_MEM,
        },
        [1] = {
            .name = "alog_radio_buffer",
                .start  = ALOG_RAM_CONSOLE_PHYS_RADIO,
                .end    = ALOG_RAM_CONSOLE_PHYS_RADIO + ALOG_RAM_CONSOLE_SIZE_RADIO - 1,
                .flags  = IORESOURCE_MEM,
        },
        [2] = {
        .name = "alog_events_buffer",
                .start  = ALOG_RAM_CONSOLE_PHYS_EVENTS,
                .end    = ALOG_RAM_CONSOLE_PHYS_EVENTS + ALOG_RAM_CONSOLE_SIZE_EVENTS - 1,
                .flags  = IORESOURCE_MEM,
        },
        [3] = {
		.name = "alog_system_buffer",
                .start  = ALOG_RAM_CONSOLE_PHYS_SYSTEM,
                .end    = ALOG_RAM_CONSOLE_PHYS_SYSTEM + ALOG_RAM_CONSOLE_SIZE_SYSTEM - 1,
                .flags  = IORESOURCE_MEM,
        },
};

static struct platform_device alog_ram_console_device = {
        .name   = "alog_ram_console",
        .id     = -1,
        .num_resources  = ARRAY_SIZE(alog_ram_console_resources),
        .resource       = alog_ram_console_resources,
};

int __init ram_console_init(void)
{
	printk(KERN_ALERT "%s()\n",__func__);
#ifdef CONFIG_FIH_LAST_ALOG
        omap_save_reboot_reason();
#endif
        platform_device_register(&ram_console_device);
        platform_device_register(&alog_ram_console_device);
	
	return 0;	
}

int __init last_alog_init(void)
{
	printk(KERN_ALERT "%s()\n",__func__);
	return 0;	
}
#else	
int __init ram_console_init(void)
{
	return 0;	
}

int __init last_alog_init(void)
{
	return 0;	
}
#endif	
