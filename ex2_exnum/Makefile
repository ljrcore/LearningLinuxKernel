#产生目标文件
obj-m:=exnum.o
#定义当前路径
CURRENT_PATH:=$(shell pwd)
#定义内核版本号
LINUX_KERNEL:=$(shell uname -r)
#定义内核源码绝对路径
LINUX_KERNEL_PATH:=/usr/src/linux-headers-$(LINUX_KERNEL)
#编译模块
all:
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules
#清理模块
clean:
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) clean


