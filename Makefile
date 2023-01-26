PREFIX = aarch64-linux-gnu-
CC = $(PREFIX)gcc
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy

RPI = 1

CPU = cortex-a72
QCPU = cortex-a72

CFLAGS = -Wall -Og -g -MD -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=$(CPU)
CFLAGS += -I ./include/
LDFLAGS = -nostdlib #-nostartfiles

TAP_NUM = $(shell date '+%s')

MAC_H = $(shell date '+%H')
MAC_M = $(shell date '+%M')
MAC_S = $(shell date '+%S')

ifdef RPI
QEMUPREFIX = ~/project/qemu-patch-raspberry4/build/
else
QEMUPREFIX = ~/qemu/build/
endif

QEMU = $(QEMUPREFIX)qemu-system-aarch64

GIC_VERSION = 3

ifdef RPI
MACHINE = raspi4b1g
else
MACHINE = virt,gic-version=$(GIC_VERSION),virtualization=on
endif

ifndef NCPU
NCPU = 4
endif

ifndef GUEST_NCPU
GUEST_NCPU = 8
endif

ifndef GUEST_MEMORY
GUEST_MEMORY = 512
endif

QEMUOPTS = -cpu $(CPU) -machine $(MACHINE) -smp $(NCPU) -m 1G
QEMUOPTS += -global virtio-mmio.force-legacy=false
QEMUOPTS += -nographic

ifdef RPI
QEMUOPTS += -dtb ./guest/bcm2711-rpi-4-b.dtb
else
QEMUOPTS += -netdev tap,id=net0,ifname=tap$(TAP_NUM),script=no,downscript=no
QEMUOPTS += -device virtio-net-device,netdev=net0,mac=70:32:17:$(MAC_H):$(MAC_M):$(MAC_S),bus=virtio-mmio-bus.0
endif

QEMUOPTS += -kernel

# directory
B = boot
C = core
D = drivers
M = main
S = sub

BOOTOBJS = $(patsubst %.c,%.o,$(wildcard $(B)/*.c))
BOOTOBJS += $(patsubst %.S,%.o,$(wildcard $(B)/*.S))

COREOBJS = $(patsubst %.c,%.o,$(wildcard $(C)/*.c))
COREOBJS += $(patsubst %.S,%.o,$(wildcard $(C)/*.S))

DRVOBJS = $(patsubst %.c,%.o,$(wildcard $(D)/*.c))
DRVOBJS += $(patsubst %.c,%.o,$(wildcard $(D)/virtio/*.c))

MOBJS = $(patsubst %.c,%.o,$(wildcard $(M)/*.c))
SOBJS = $(patsubst %.c,%.o,$(wildcard $(S)/*.c))

MAINOBJS = $(BOOTOBJS) $(COREOBJS) $(DRVOBJS) $(MOBJS)
SUBOBJS = $(BOOTOBJS) $(COREOBJS) $(DRVOBJS) $(SOBJS)

MAINDEP = $(MAINOBJS:.o=.d)
SUBDEP = $(SUBOBJS:.o=.d)

KERNIMG = guest/linux/Image
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

guest/hello.img: guest/hello/Makefile
	make -C guest/hello

guest/vsmtest.img: guest/vsmtest/Makefile
	make -C guest/vsmtest

vmm-boot.img: poc-main
	$(OBJCOPY) -O binary $^ $@

vmm.img: poc-sub
	$(OBJCOPY) -O binary $^ $@

poc-main: $(MAINOBJS) $(M)/memory.ld dtb $(KERNIMG) guest/linux/rootfs.img
	#$(LD) -r -b binary guest/vsmtest.img -o hello-img.o
	#$(LD) -r -b binary guest/xv6/kernel.img -o xv6.o
	$(LD) -r -b binary $(KERNIMG) -o image.o
	$(LD) -r -b binary guest/linux/rootfs.img -o rootfs.img.o
	$(LD) -r -b binary virt.dtb -o virt.dtb.o
	$(LD) $(LDFLAGS) -T $(M)/memory.ld -o $@ $(MAINOBJS) virt.dtb.o rootfs.img.o image.o

poc-main-vsm: $(MAINOBJS) $(M)/memory.ld guest/vsmtest.img
	$(LD) -r -b binary guest/vsmtest.img -o hello-img.o
	$(LD) $(LDFLAGS) -T $(M)/memory.ld -o $@ $(MAINOBJS) hello-img.o

poc-sub: $(SUBOBJS) $(S)/memory.ld dtb-numa
	$(LD) -r -b binary virt.dtb -o virt.dtb.o
	$(LD) $(LDFLAGS) -T $(S)/memory.ld -o $@ $(SUBOBJS) virt.dtb.o

poc-sub-vsm: $(SUBOBJS) $(S)/memory.ld
	$(LD) -r -b binary guest/vsmtest.img -o hello-img.o
	$(LD) $(LDFLAGS) -T $(S)/memory.ld -o $@ $(SUBOBJS) virt.dtb.o rootfs.img.o image.o

dev-main: vmm-boot.img
	sudo ip link add br4poc type bridge || true
	sudo ip link set br4poc up || true
	sudo ifconfig br4poc mtu 4500 || true
	sudo ip tuntap add dev tap$(TAP_NUM) mode tap
	sudo ip link set dev tap$(TAP_NUM) master br4poc
	sudo ip link set tap$(TAP_NUM) up
	sudo ifconfig tap$(TAP_NUM) mtu 4500
	$(QEMU) $(QEMUOPTS) vmm-boot.img
	sudo ip link set tap$(TAP_NUM) down
	sudo ip tuntap del dev tap$(TAP_NUM) mode tap

dev-sub: vmm.img
	sudo ip link add br4poc type bridge || true
	sudo ip link set br4poc up || true
	sudo ifconfig br4poc mtu 4500
	sudo ip tuntap add dev tap$(TAP_NUM) mode tap
	sudo ip link set dev tap$(TAP_NUM) master br4poc
	sudo ip link set tap$(TAP_NUM) up
	sudo ifconfig tap$(TAP_NUM) mtu 4500
	$(QEMU) $(QEMUOPTS) vmm.img
	sudo ip link set tap$(TAP_NUM) down
	sudo ip tuntap del dev tap$(TAP_NUM) mode tap

dev-main-vsm: poc-main-vsm
	sudo ip link add br4poc type bridge || true
	sudo ip link set br4poc up || true
	sudo ip tuntap add dev tap$(TAP_NUM) mode tap
	sudo ip link set dev tap$(TAP_NUM) master br4poc
	sudo ip link set tap$(TAP_NUM) up
	$(QEMU) $(QEMUOPTS) poc-main-vsm -netdev tap,id=net0,ifname=tap$(TAP_NUM),script=no,downscript=no \
	  -device virtio-net-device,netdev=net0,mac=70:32:17:$(MAC_H):$(MAC_M):$(MAC_S),bus=virtio-mmio-bus.0
	sudo ip link set tap$(TAP_NUM) down
	sudo ip tuntap del dev tap$(TAP_NUM) mode tap

dev-sub-vsm: poc-sub-vsm
	sudo ip link add br4poc type bridge || true
	sudo ip link set br4poc up || true
	sudo ip tuntap add dev tap$(TAP_NUM) mode tap
	sudo ip link set dev tap$(TAP_NUM) master br4poc
	sudo ip link set tap$(TAP_NUM) up
	$(QEMU) $(QEMUOPTS) poc-sub-vsm -netdev tap,id=net0,ifname=tap$(TAP_NUM),script=no,downscript=no \
	  -device virtio-net-device,netdev=net0,mac=70:32:17:$(MAC_H):$(MAC_M):$(MAC_S),bus=virtio-mmio-bus.0
	sudo ip link set tap$(TAP_NUM) down
	sudo ip tuntap del dev tap$(TAP_NUM) mode tap

gdb-main: vmm-boot.img
	sudo ip link add br4poc type bridge || true
	sudo ip link set br4poc up || true
	sudo ifconfig br4poc mtu 4500 || true
	sudo ip tuntap add dev tap$(TAP_NUM) mode tap
	sudo ip link set dev tap$(TAP_NUM) master br4poc
	sudo ip link set tap$(TAP_NUM) up
	sudo ifconfig tap$(TAP_NUM) mtu 4500
	$(QEMU) -S -gdb tcp::1234 $(QEMUOPTS) vmm-boot.img
	sudo ip link set tap$(TAP_NUM) down
	sudo ip tuntap del dev tap$(TAP_NUM) mode tap

gdb-sub: vmm.img
	sudo ip link add br4poc type bridge || true
	sudo ip link set br4poc up || true
	sudo ifconfig br4poc mtu 4500
	sudo ip tuntap add dev tap$(TAP_NUM) mode tap
	sudo ip link set dev tap$(TAP_NUM) master br4poc
	sudo ip link set tap$(TAP_NUM) up
	sudo ifconfig tap$(TAP_NUM) mtu 4500
	$(QEMU) -S -gdb tcp::5678 $(QEMUOPTS) vmm.img
	sudo ip link set tap$(TAP_NUM) down
	sudo ip tuntap del dev tap$(TAP_NUM) mode tap

linux-gdb: $(KERNIMG)
	$(QEMU) -M virt,gic-version=3 -cpu cortex-a72 -smp $(GUEST_NCPU) -kernel $(KERNIMG) -nographic -initrd guest/linux/rootfs.img -append "console=ttyAMA0" -m 256 -S -gdb tcp::1234

linux:
	$(QEMU) -M virt,gic-version=3 -smp cores=4,sockets=2 \
	  -numa node,memdev=r0,cpus=0-3,nodeid=0 \
	  -numa node,memdev=r1,cpus=4-7,nodeid=1 \
	  -object memory-backend-ram,id=r0,size=256M \
	  -object memory-backend-ram,id=r1,size=256M \
	  -cpu cortex-a72 -kernel $(KERNIMG) \
	  -initrd guest/linux/rootfs.img \
	  -nographic -append "console=ttyAMA0 nokaslr" -m 512

linux-rpi:
	$(QEMU) -M raspi3b -smp cores=4 \
	  -kernel guest/linux/Image5.4l \
	  -initrd guest/linux/rootfs.img \
	  -nographic -append "console=ttyAMA0 nokaslr" -m 1G

dts:
	$(QEMU) -M virt,gic-version=3,dumpdtb=virt.dtb -smp $(GUEST_NCPU) \
	  -cpu $(QCPU) -kernel $(KERNIMG) -initrd guest/linux/rootfs.img \
	  -nographic -append "console=ttyAMA0 nokaslr" -m $(GUEST_MEMORY)
	dtc -I dtb -O dts -o virt.dts virt.dtb

dts-numa:
	$(QEMU) -M virt,gic-version=3,dumpdtb=virtn.dtb -smp 8 \
	  -cpu $(QCPU)	\
	  -numa node,memdev=r0,cpus=0-3,nodeid=0 \
	  -numa node,memdev=r1,cpus=4-7,nodeid=1 \
	  -object memory-backend-ram,id=r0,size=256M \
	  -object memory-backend-ram,id=r1,size=256M \
	  -kernel $(KERNIMG) -initrd guest/linux/rootfs.img \
	  -nographic -append "console=ttyAMA0 nokaslr" -m 512
	dtc -I dtb -O dts -o virtn.dts virtn.dtb

dtb:
	$(QEMU) -M virt,gic-version=3,dumpdtb=virt.dtb -smp $(GUEST_NCPU) -cpu cortex-a72 -kernel $(KERNIMG) -initrd guest/linux/rootfs.img -nographic -append "console=ttyAMA0 nokaslr" -m $(GUEST_MEMORY)

dtb-numa:
	$(QEMU) -M virt,gic-version=3,dumpdtb=virt.dtb \
	  -smp cores=4,sockets=2 -cpu $(QCPU) \
	  -numa node,memdev=r0,cpus=0-3,nodeid=0 \
	  -numa node,memdev=r1,cpus=4-7,nodeid=1 \
	  -object memory-backend-ram,id=r0,size=256M \
	  -object memory-backend-ram,id=r1,size=256M \
	  -kernel $(KERNIMG) -initrd guest/linux/rootfs.img \
	  -nographic -append "console=ttyAMA0 nokaslr" -m 512

qemu-version:
	$(QEMU) -version

clean:
	make -C guest clean
	$(RM) $(BOOTOBJS) $(COREOBJS) $(DRVOBJS) $(MOBJS) $(SOBJS) poc-main poc-sub *.img *.o */*.d *.dtb *.dts

-include: $(MAINDEP) $(SUBDEP)

.PHONY: dev-main dev-sub dev-main-vsm dev-sub-vsm clean dts dtb linux linux-gdb gdb-main gdb-sub
