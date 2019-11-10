#include<linux/module.h>
#include<linux/init.h>
#include<linux/kernel.h>

int __init lk_hello1(void)
{
	printk("hello,i am the first module!\n");
	return 0;
}
void __exit lk_exit1(void)
{
	printk("good bye,the first module !\n");

}
//module_init(lk_hello1);
//module_exit(lk_exit1);
MODULE_LICENSE("GPL");

