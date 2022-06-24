PREFIX = aarch64-linux-gnu-
CC = $(PREFIX)gcc
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy

CPU = cortex-a72
QCPU = cortex-a72

CFLAGS = -Wall -Og -g -MD -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=$(CPU)
CFLAGS += -I ./include/
LDFLAGS = -nostdlib -nostartfiles

#QEMUPREFIX = ~/qemu/build/
QEMU = $(QEMUPREFIX)qemu-system-aarch64
GIC_VERSION = 3
MACHINE = virt,gic-version=$(GIC_VERSION),virtualization=on
ifndef NCPU
NCPU = 1
endif

OBJS = src/boot.o src/init.o src/uart.o src/lib.o src/kalloc.o src/printf.o src/vcpu.o \
			 src/vm.o src/mm.o src/vector.o src/guest.o src/trap.o src/pcpu.o src/vgic.o \
			 src/gic.o src/mmio.o src/vtimer.o src/pci.o src/virtio-pci.o src/vpsci.o \
			 src/virtio-mmio-dev.o src/spinlock.o src/vsysreg.o

QEMUOPTS = -cpu $(QCPU) -machine $(MACHINE) -smp $(NCPU) -m 256
#QEMUOPTS += -device ioh3420,id=pcie.1,bus=pcie.0,chassis=1,slot=1
#QEMUOPTS += -device pcie-root-port,port=0x10,chassis=1,id=pcie.1,bus=pcie.0,multifunction=on,addr=0x2
#QEMUOPTS += -device pcie-root-port,port=0x11,chassis=2,id=pcie.2,bus=pcie.0,addr=0x2.0x1
#QEMUOPTS += -device virtio-net-pci,bus=pcie.0,disable-legacy=on,disable-modern=off
#QEMUOPTS += -device virtio-rng-pci,bus=pcie.0,disable-legacy=on,disable-modern=off
#QEMUOPTS += -device e1000,bus=pcie.0
QEMUOPTS += -nographic -kernel mvmm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

guest/hello/hello.img: guest/Makefile
	make -C guest/hello

guest/xv6/kernel.img: guest/xv6/Makefile guest/xv6/kernel
	make -C guest/xv6

-include: *.d

mvmm: $(OBJS) src/memory.ld dtb guest/linux/Image guest/linux/rootfs.img guest/xv6/kernel.img #guest/hello/hello.img
	$(LD) -r -b binary guest/xv6/kernel.img -o xv6.o
	$(LD) -r -b binary guest/linux/Image -o image.o
	$(LD) -r -b binary guest/linux/rootfs.img -o rootfs.img.o
	$(LD) -r -b binary virt.dtb -o virt.dtb.o
	$(LD) $(LDFLAGS) -T src/memory.ld -o $@ $(OBJS) xv6.o virt.dtb.o rootfs.img.o image.o

qemu: mvmm guest/xv6/fs.img
	$(QEMU) --version
	$(QEMU) $(QEMUOPTS) -drive file=guest/xv6/fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

qemu-linux: mvmm
	$(QEMU) --version
	$(QEMU) $(QEMUOPTS) -initrd guest/linux/rootfs.img -append "console=ttyAMA0"

qemu-gdb: mvmm
	$(QEMU) --version
	$(QEMU) $(QEMUOPTS) -initrd guest/linux/rootfs.img -append "console=ttyAMA0" -S -gdb tcp::1234 

linux: guest/linux/Image
	$(QEMU) -M virt,gic-version=3 -cpu cortex-a72 -smp $(NCPU) -kernel guest/linux/Image -initrd guest/linux/rootfs.img -nographic -append "console=ttyAMA0" -m 128

linux-gdb: guest/linux/Image
	$(QEMU) -M virt,gic-version=3 -cpu cortex-a72 -smp $(NCPU) -kernel guest/linux/Image -initrd guest/linux/rootfs.img -nographic -append "console=ttyAMA0" -m 128 -S -gdb tcp::1234

dts:
	$(QEMU) -M virt,gic-version=3,dumpdtb=virt.dtb -cpu cortex-a72 -kernel guest/linux/Image -initrd guest/linux/rootfs.img -nographic -append "console=ttyAMA0" -m 128
	dtc -I dtb -O dts -o virt.dts virt.dtb

dtb:
	$(QEMU) -M virt,gic-version=3,dumpdtb=virt.dtb -smp $(NCPU) -cpu cortex-a72 -kernel guest/linux/Image -initrd guest/linux/rootfs.img -nographic -append "console=ttyAMA0" -m 128

clean:
	make -C guest clean
	$(RM) $(OBJS) mvmm *.img *.o */*.d

.PHONY: qemu qemu-linux gdb clean dts dtb
