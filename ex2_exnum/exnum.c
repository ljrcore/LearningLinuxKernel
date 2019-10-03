/**********************************************************
 * Author        : 梁金荣
 * Email         : Liangjinrong111@163.com
 * Last modified : 2019-09-14 13:00
 * Filename      : exnum.c
 * Description   : 模块初始化函数中求最大数（使用内核中的代码）
 * *******************************************************/

//必要的头文件
#include<linux/kernel.h>
#include<linux/module.h>
#include<linux/init.h>
//模块的初始化函数，模块的入口函数，加载模块，需超级用户权限
static int __init lk_exnum(void)
{
	int x = 1, y = 2;

		printk("The module is starting here！\n");
		printk("x = %d, y = %d\n", x, y);
        printk("max=%d\n", max(x++, y++));
        printk("x = %d, y = %d\n", x, y);
        printk("min=%d\n", min(x++, y++));
        	
	return 0;
}
//出口函数，卸载模块，需超级用户权限
static void __exit lk_exit(void)
{
	printk("The maxnum moudle has exited!\n");
}

module_init(lk_exnum); //内核入口点，调用初始化函数，包含在module.h中
module_exit(lk_exit); //出口点
MODULE_LICENSE("GPL"); //许可证
MODULE_AUTHOR("ljr"); //作者（非必须）
MODULE_DESCRIPTION("max and min number"); //模块描述（非必须）

