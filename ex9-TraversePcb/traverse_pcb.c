# include <linux/kernel.h>
# include <linux/module.h>
# include <uapi/linux/sched.h>
# include <linux/init_task.h>
# include <linux/init.h>
# include <linux/fdtable.h>
# include <linux/fs_struct.h>
# include <linux/mm_types.h>

//内核模块初始化函数
static int __init traverse_pcb(void)
{
	struct task_struct *task, *p;//定义指向task_struct类型的指针
	struct list_head *pos;//定义双向链表指针
	int count=0;//定义统计系统进程个数的变量
	printk("Printf process'message begin:\n");//提示模块开始运行
	task = &init_task;//指向0号进程的PCB
	
	list_for_each(pos,&task->tasks)//使用list_for_each宏来遍历进程链表
	{
		p = list_entry(pos,struct task_struct,tasks);//指向当前进程的task_struct结构
		count++;//统计系统进程个数
		printk("\n\n");//方便查看后续打印信息
		/*
		打印task_struct中的字段，其中pid：进程的pid号；state：进程的状态；
		prio:动态优先级；static_prio:静态优先级; parent'pid:父进程的pid号；
		count:文件系统信息，文件被使用的次数; umask:进程权限位的默认设置;
		使用atomic_read原子操作是为了(p->files)->count字段计数不被打断
		*/
		printk("pid:%d; state:%lx; prio:%d; static_prio:%d; parent'pid:%d; count:%d; umask:%d;",	\
			p->pid,p->state,p->prio,p->static_prio,(p->parent)->pid,								\
			atomic_read((&(p->files)->count)),(p->fs)->umask);
		//打印进程地址空间的信息
		if((p->mm)!=NULL)
			printk("total_vm:%ld;",(p->mm)->total_vm);//total_vm：线性区总的页数
	}
	printk("进程的个数：%d\n",count);//打印进程个数
	return 0;
}

//内核模块退出函数
static void __exit end_pcb(void)
{
	printk("traverse pcb is end.");
}
module_init(traverse_pcb);//入口
module_exit(end_pcb);//出口
MODULE_LICENSE("GPL");//许可证