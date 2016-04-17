#include "fih_projects.h"

#if defined(CONFIG_FIH_PROJECT_GOX)
#include "gox/gox_proj.h"	
#endif

#if defined(CONFIG_FIH_PROJECT_GO9)
#include "go9/go9_proj.h"	
#endif

int __init initialize_fih_projects(void)
{
	printk(KERN_ALERT "%s, begin\n",__func__);

#if defined(CONFIG_FIH_PROJECT_GOX)	
	GOX_init();
#endif

#if defined(CONFIG_FIH_PROJECT__GO9)	
	GO9_init();
#endif

	printk(KERN_ALERT "%s, end\n",__func__);
	
	return 0;
}
