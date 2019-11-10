#include<linux/module.h>
#include<linux/init.h>
#include<linux/kernel.h>
extern int lk_hello1(void);
extern void lk_exit1(void);
static int __init lk_hello2(void)
{
	lk_hello1();
	printk("hello,i an the second module!\n");
	return 0;
}
static void __exit lk_exit2(void)
{
	lk_exit1();
	printk("good bye,the second module!\n");

}
module_init(lk_hello2);
module_exit(lk_exit2);
MODULE_LICENSE("GPL");

