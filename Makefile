obj-m += module.o

KDIR ?= /content/linux-5.10.110

all:
	make -C $(KDIR) M=$(PWD) ARCH=arm64 LLVM=1 LLVM_IAS=1 CC=clang CROSS_COMPILE=aarch64-linux-android- modules

clean:
	make -C $(KDIR) M=$(PWD) clean
