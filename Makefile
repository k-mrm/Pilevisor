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

C = common
M = main
S = sub

COMMONOBJS = $(C)/boot.o $(C)/init.o $(C)/uart.o $(C)/lib.o $(C)/pmalloc.o $(C)/printf.o $(C)/vcpu.o \
			 $(C)/mm.o $(C)/vector.o $(C)/guest.o $(C)/trap.o $(C)/pcpu.o $(C)/vgic.o $(C)/node.o \
			 $(C)/gic.o $(C)/mmio.o $(C)/vtimer.o $(C)/vpsci.o $(C)/vsm.o $(C)/msg.o $(C)/virtio.o $(C)/virtio_net.o

MOBJS = $(M)/main-node.o
SOBJS = $(S)/sub-node.o

MAINOBJS = $(COMMONOBJS) $(MOBJS)
SUBOBJS = $(COMMONOBJS) $(SOBJS)

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

-include: *.d

guest/hello.img: guest/hello/Makefile
	make -C guest/hello

guest/vsmtest.img: guest/vsmtest/Makefile
	make -C guest/vsmtest

poc-main: $(MAINOBJS) $(M)/memory.ld guest/vsmtest.img
	$(LD) -r -b binary guest/vsmtest.img -o hello-img.o
	$(LD) $(LDFLAGS) -T $(M)/memory.ld -o $@ $(MAINOBJS) hello-img.o

poc-sub: $(SUBOBJS) $(S)/memory.ld guest/vsmtest.img
	$(LD) -r -b binary guest/vsmtest.img -o hello-img.o
	$(LD) $(LDFLAGS) -T $(S)/memory.ld -o $@ $(SUBOBJS) hello-img.o

dev-main: poc-main
	sudo ip link add br4poc type bridge || true
	sudo ip link set br4poc up || true
	sudo ip tuntap add dev tap$(TAP_NUM) mode tap
	sudo ip link set dev tap$(TAP_NUM) master br4poc
	sudo ip link set tap$(TAP_NUM) up
	$(QEMU) $(QEMUOPTS) poc-main -netdev tap,id=net0,ifname=tap$(TAP_NUM),script=no,downscript=no -device virtio-net-device,netdev=net0,mac=70:32:17:$(MAC_H):$(MAC_M):$(MAC_S),bus=virtio-mmio-bus.0
	sudo ip link set tap$(TAP_NUM) down
	sudo ip tuntap del dev tap$(TAP_NUM) mode tap

dev-sub: poc-sub
	sudo ip link add br4poc type bridge || true
	sudo ip link set br4poc up || true
	sudo ip tuntap add dev tap$(TAP_NUM) mode tap
	sudo ip link set dev tap$(TAP_NUM) master br4poc
	sudo ip link set tap$(TAP_NUM) up
	$(QEMU) $(QEMUOPTS) poc-sub -netdev tap,id=net0,ifname=tap$(TAP_NUM),script=no,downscript=no -device virtio-net-device,netdev=net0,mac=70:32:17:$(MAC_H):$(MAC_M):$(MAC_S),bus=virtio-mmio-bus.0
	sudo ip link set tap$(TAP_NUM) down
	sudo ip tuntap del dev tap$(TAP_NUM) mode tap

gdb: poc-main
	$(QEMU) -S -gdb tcp::1234 $(QEMUOPTS) poc-main

clean:
	make -C guest clean
	$(RM) $(COMMONOBJS) $(MOBJS) $(SOBJS) poc-main poc-sub *.img *.o */*.d

.PHONY: qemu gdb clean
