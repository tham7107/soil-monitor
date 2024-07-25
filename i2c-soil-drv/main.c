#include <linux/module.h>
#include <linux/kernel.h>

MODULE_AUTHOR("Thomas Ames");
MODULE_LICENSE("Dual BSD/GPL");

static int i2c_soil_drv_init(void)
{
    printk(KERN_INFO "hello init\n");
    return 0;
}

static void i2c_soil_drv_exit(void)
{
    printk(KERN_INFO "hello exit\n");
}

module_init(i2c_soil_drv_init)
module_exit(i2c_soil_drv_exit)
