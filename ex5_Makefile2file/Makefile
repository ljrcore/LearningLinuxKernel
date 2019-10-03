
hello-objs+=hello1.o hello2.o 

obj-m:=hello.o
CU_PATH:=$(shell pwd)
LK:=$(shell uname -r)
LK_PATH:=/usr/src/linux-headers-$(LK)
all:
	make -C $(LK_PATH) M=$(CU_PATH) modules
clean:
	make -C $(LK_PATH) M=$(CU_PATH) clean
