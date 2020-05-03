obj-m := m_d.o

KDIR = /usr/src/linux-headers-4.15.0-91-generic

kv=$(shell uname -r)


all:
	make -C /lib/modules/$(kv)/build M=$(PWD) modules
	#$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	make -C /lib/modules/$(kv)/build M=$(PWD) clean
	#rm -rf *.o *.ko *.mod.* *.symvers *.order
