obj-m = setest_module_load.o setest_module_request.o

TARGETS = finit_load init_load
LDLIBS += -lselinux
KDIR = /lib/modules/$(shell uname -r)/build

all: $(TARGETS)
	$(MAKE) -C $(KDIR) M=$(PWD)

clean:
	rm -f $(TARGETS)
	rm -f *.a *.o *.ko *.cmd *.mod *.mod.c .*.cmd Module.symvers modules.order
