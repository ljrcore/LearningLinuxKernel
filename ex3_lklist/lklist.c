/**********************************************************
 * Author        : ljr
 * Email         : Liangjinrong111@163.com
 * Last modified : 2019-09-15 12:41
 * Filename      : lk_list
 * Description   : The operation of Linux Kernel List
 * *******************************************************/

#include<linux/kernel.h>  //包含了常用的内核函数
#include<linux/module.h>  //模块必备
#include<linux/init.h>  //包含了宏__init和__exit，它们允许释放内核占用的内存
#include<linux/list.h>  //包含双链表操作函数
#include<linux/slab.h>  //包含了kmalloc等内存分配函数

#define N 100  //链表节点数
struct lklist{
	int data;  //数据
	struct list_head list;  //指向双链表前后节点的指针
};
struct lklist lk; //头节点

/**
 *创建内核链表
 *指针和变量
 */
void creat_lklist(void)
{
	struct lklist *node;  //定义每次申请链表节点时所用的指针
	int i;

	INIT_LIST_HEAD(&lk.list);  //初始化头节点
	printk("\nCreating linux kernel list!\n");
	//建立N个节点，依次加到链表中
	for(i = 0; i < N; i++)
	{
		//GFP_KERNEL是linux内存分配器的标志，标识着内存分配器将要采取的行为
		node = (struct lklist *)kmalloc(sizeof(struct lklist), GFP_KERNEL);  
		node -> data = i+1;
		list_add_tail(&node -> list, &lk.list);  
		printk("Node %d has added to the lklist...\n ",i+1);
	}
}

/**
 * 遍历内核链表
 *
 */
void traversal_lklist(void)
{
	struct lklist *p;
	struct list_head *pos;
	int i = 1;
	printk("\nTraveral linux kernel list!\n");
	list_for_each(pos, &lk.list)
	{
	       	//通过结构体成员的指针来返回结构体的指针
		p = list_entry(pos, struct lklist, list); 
		printk("Node %d's data: %d\n", i, p -> data);
		i++;
	}
	printk("\n\n");
}

/**
 * 删除内核链表
 * 删除10个节点
 */
void del_lklist(void)
{
	struct list_head *n, *pos;
	struct lklist *p;
	int i=10;
	printk("\nDelete linux kernel list!\n");
	
	/*
	 * 提供一个与pos同类型的指针n
	 * 在for循环中暂存pos下一个节点的地址
	 * 避免因pos节点被释放而造成的断链
	 */
	list_for_each_safe(pos, n, &lk.list)
	{			
		p = list_entry(pos, struct lklist, list);
		list_del(pos);
		kfree(p);
		printk("\nThe node has removed from the lklist...\n");
		i--;
		if(i==0) break;
	}
}

/**
 * 内核入口函数
 *
 */
static int __init lklist(void)
{
	printk("\nLinux kernel list is starting...\n\n");
	creat_lklist();  //调用创建内核链表函数
	traversal_lklist();  //调用遍历内核链表函数
	del_lklist();  //调用删除内核链表函数
	traversal_lklist();  //调用遍历内核链表函数
	return 0;
}

/**
 * 内核出口函数
 *
 */
static void __exit lklist_exit(void)
{
	printk("\nlklist is exiting...\n");
}

module_init(lklist);  //入口点
module_exit(lklist_exit);  //出口点
MODULE_LICENSE("GPL");  //许可证
MODULE_AUTHOR("梁金荣");  //作者信息（非必要项）
MODULE_DESCRIPTION("The operation of Linux Kernel List");  //模块描述（非必要项）
