obj-m += module.o

# Đường dẫn tới thư mục Kernel Source 5.10.110 đã config
KDIR ?= /content/linux-5.10.110

all:
	make -C $(KDIR) M=$(PWD) ARCH=arm64 CC=clang CROSS_COMPILE=aarch64-linux-android- modules

clean:
	make -C $(KDIR) M=$(PWD) clean
