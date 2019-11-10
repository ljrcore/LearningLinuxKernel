#include<linux/module.h>
#include<linux/init.h>
#include<linux/kernel.h>

static int __init lk_hello(void)
{
	printk("hello world!\n");
	return 0;
}
static void __exit lk_exit(void)
{
	printk("good bye!\n");

}
module_init(lk_hello);
module_exit(lk_exit);
MODULE_LICENSE("GPL");

