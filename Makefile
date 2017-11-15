OBJS= main.o keyboard.o xhci.o usb.o hub.o
DEPS= $(filter %.d, $(subst .o,.d, $(OBJS)))

CXXFLAGS += -g -std=c++11 -MMD -MP

.PHONY: load_uio run

default: a.out

-include $(DEPS)

# need to be edited by yourself
load:
	sudo modprobe uio_pci_generic
	sudo sh -c "echo 'xxxx xxxx' > /sys/bus/pci/drivers/uio_pci_generic/new_id"
	sudo sh -c "echo -n 0000:xx:xx.0 > /sys/bus/pci/drivers/xhci_hcd/unbind"
	sudo sh -c "echo -n 0000:xx:xx.0 > /sys/bus/pci/drivers/uio_pci_generic/bind"

load_vagrant:
	sudo modprobe uio_pci_generic
	sudo sh -c "echo '8086 1e31' > /sys/bus/pci/drivers/uio_pci_generic/new_id"
	sudo sh -c "echo -n 0000:00:0c.0 > /sys/bus/pci/drivers/xhci_hcd/unbind"
	sudo sh -c "echo -n 0000:00:0c.0 > /sys/bus/pci/drivers/uio_pci_generic/bind"

load_nuc:
	sudo modprobe uio_pci_generic
	sudo sh -c "echo '8086 9d2f' > /sys/bus/pci/drivers/uio_pci_generic/new_id"
	sudo sh -c "echo -n 0000:00:14.0 > /sys/bus/pci/drivers/xhci_hcd/unbind"
	sudo sh -c "echo -n 0000:00:14.0 > /sys/bus/pci/drivers/uio_pci_generic/bind"

load_qemu:
	sudo modprobe uio_pci_generic
	sudo sh -c "echo '1033 0194' > /sys/bus/pci/drivers/uio_pci_generic/new_id"
	sudo sh -c "echo -n 0000:01:03.0 > /sys/bus/pci/drivers/xhci_hcd/unbind"
	sudo sh -c "echo -n 0000:01:03.0 > /sys/bus/pci/drivers/uio_pci_generic/bind"

load_skylake:
	sudo modprobe uio_pci_generic
	sudo sh -c "echo '8086 a12f' > /sys/bus/pci/drivers/uio_pci_generic/new_id"
	sudo sh -c "echo -n 0000:00:14.0 > /sys/bus/pci/drivers/xhci_hcd/unbind"
	sudo sh -c "echo -n 0000:00:14.0 > /sys/bus/pci/drivers/uio_pci_generic/bind"

run: a.out
	sudo sh -c "echo 120 > /proc/sys/vm/nr_hugepages"
	sudo ./a.out

a.out: $(OBJS)
	g++ -g -std=c++11 -pthread $^

clean:
	-rm a.out $(DEPS) $(OBJS)
