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
QEMUPREFIX = 
QEMU = $(QEMUPREFIX)qemu-system-aarch64
GIC_VERSION = 3
MACHINE = virt,gic-version=$(GIC_VERSION),virtualization=on
ifndef NCPU
NCPU = 1
endif

C = core
D = drivers
M = main
S = sub

COREOBJS = $(patsubst %.c,%.o,$(wildcard $(C)/*.c))
COREOBJS += $(patsubst %.S,%.o,$(wildcard $(C)/*.S))
DRVOBJS = $(patsubst %.c,%.o,$(wildcard $(D)/*.c))
DRVOBJS += $(patsubst %.c,%.o,$(wildcard $(D)/virtio/*.c))

MOBJS = $(patsubst %.c,%.o,$(wildcard $(M)/*.c))
SOBJS = $(patsubst %.c,%.o,$(wildcard $(S)/*.c))

MAINOBJS = $(COREOBJS) $(DRVOBJS) $(MOBJS)
SUBOBJS = $(COREOBJS) $(DRVOBJS) $(SOBJS)

QEMUOPTS = -cpu $(QCPU) -machine $(MACHINE) -smp $(NCPU) -m 256
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -nographic -kernel

TAP_NUM = $(shell date '+%s')

MAC_H = $(shell date '+%H')
MAC_M = $(shell date '+%M')
MAC_S = $(shell date '+%S')

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

#-include: *.d

guest/hello.img: guest/hello/Makefile
	make -C guest/hello

guest/vsmtest.img: guest/vsmtest/Makefile
	make -C guest/vsmtest

poc-main: $(MAINOBJS) $(M)/memory.ld dtb guest/linux/Image guest/linux/rootfs.img
	#$(LD) -r -b binary guest/vsmtest.img -o hello-img.o
	#$(LD) -r -b binary guest/xv6/kernel.img -o xv6.o
	$(LD) -r -b binary guest/linux/Image -o image.o
	$(LD) -r -b binary guest/linux/rootfs.img -o rootfs.img.o
	$(LD) -r -b binary virt.dtb -o virt.dtb.o
	$(LD) $(LDFLAGS) -T $(M)/memory.ld -o $@ $(MAINOBJS) virt.dtb.o rootfs.img.o image.o

poc-sub: $(SUBOBJS) $(S)/memory.ld dtb guest/linux/Image guest/linux/rootfs.img
	$(LD) -r -b binary guest/linux/Image -o image.o
	$(LD) -r -b binary guest/linux/rootfs.img -o rootfs.img.o
	$(LD) -r -b binary virt.dtb -o virt.dtb.o
	$(LD) $(LDFLAGS) -T $(S)/memory.ld -o $@ $(SUBOBJS) virt.dtb.o rootfs.img.o image.o

dev-main: poc-main
	sudo ip link add br4poc type bridge || true
	sudo ip link set br4poc up || true
	sudo ip tuntap add dev tap$(TAP_NUM) mode tap
	sudo ip link set dev tap$(TAP_NUM) master br4poc
	sudo ip link set tap$(TAP_NUM) up
	$(QEMU) $(QEMUOPTS) poc-main -netdev tap,id=net0,ifname=tap$(TAP_NUM),script=no,downscript=no \
	  -device virtio-net-device,netdev=net0,mac=70:32:17:$(MAC_H):$(MAC_M):$(MAC_S),bus=virtio-mmio-bus.0
	sudo ip link set tap$(TAP_NUM) down
	sudo ip tuntap del dev tap$(TAP_NUM) mode tap

dev-sub: poc-sub
	sudo ip link add br4poc type bridge || true
	sudo ip link set br4poc up || true
	sudo ip tuntap add dev tap$(TAP_NUM) mode tap
	sudo ip link set dev tap$(TAP_NUM) master br4poc
	sudo ip link set tap$(TAP_NUM) up
	$(QEMU) $(QEMUOPTS) poc-sub -netdev tap,id=net0,ifname=tap$(TAP_NUM),script=no,downscript=no \
	  -device virtio-net-device,netdev=net0,mac=70:32:17:$(MAC_H):$(MAC_M):$(MAC_S),bus=virtio-mmio-bus.0
	sudo ip link set tap$(TAP_NUM) down
	sudo ip tuntap del dev tap$(TAP_NUM) mode tap

gdb-main: poc-main
	sudo ip link add br4poc type bridge || true
	sudo ip link set br4poc up || true
	sudo ip tuntap add dev tap$(TAP_NUM) mode tap
	sudo ip link set dev tap$(TAP_NUM) master br4poc
	sudo ip link set tap$(TAP_NUM) up
	$(QEMU) -S -gdb tcp::1234 $(QEMUOPTS) poc-main -netdev tap,id=net0,ifname=tap$(TAP_NUM),script=no,downscript=no \
	  -device virtio-net-device,netdev=net0,mac=70:32:17:$(MAC_H):$(MAC_M):$(MAC_S),bus=virtio-mmio-bus.0
	sudo ip link set tap$(TAP_NUM) down
	sudo ip tuntap del dev tap$(TAP_NUM) mode tap

dts:
	$(QEMU) -M virt,gic-version=3,dumpdtb=virt.dtb -smp $(NCPU) -cpu cortex-a72 -kernel guest/linux/Image -initrd guest/linux/rootfs.img -nographic -append "console=ttyAMA0" -m 128
	dtc -I dtb -O dts -o virt.dts virt.dtb

dtb:
	$(QEMU) -M virt,gic-version=3,dumpdtb=virt.dtb -smp $(NCPU) -cpu cortex-a72 -kernel guest/linux/Image -initrd guest/linux/rootfs.img -nographic -append "console=ttyAMA0" -m 128

clean:
	make -C guest clean
	$(RM) $(COREOBJS) $(DRVOBJS) $(MOBJS) $(SOBJS) poc-main poc-sub *.img *.o */*.d

.PHONY: dev-main dev-sub clean dts dtb
