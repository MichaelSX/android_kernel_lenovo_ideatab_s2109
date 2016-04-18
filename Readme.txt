*OS Kernel: Linux@3.0 

*Tool Chain for building Kernel and Drivers
The Kernel and Driver sources are built using Sourcery G++ Lite 2010q1-202 for ARM GNU/Linux version.
This tool chain can be obtained from below
www.codesourcery.com/sgpp/lite/arm/portal/package6488/public/arm-none-linux-gnueabi/arm-2010q1-202-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2


*Build script,
 - Setting up build environment 
    export PATH=$PATH:<toolchain_parent_dir>/arm-2010q1/bin
 
 - Building Kernel   
    cd Kernel_0320
    make -f kernel/FIHKernel.mk kernelonly FIH_PROJECT_ROOT_DIR=`pwd`/kernel