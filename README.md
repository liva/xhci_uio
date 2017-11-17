# xhci_uio
Simple implementation of xHCI (USB3.0 Host Controller) Linux usermode driver
(and keyboard driver for demo)

## HOWTO
!! You should use SSH. !!

### setup environment
[This script](https://github.com/PFLab-OS/Raph_Kernel_devenv_box/blob/master/uio.sh) downloads linux kernel source and builds uio kernel module.
It will work on Ubuntu 14.04 or later.

```
$ wget https://raw.githubusercontent.com/PFLab-OS/Raph_Kernel_devenv_box/master/uio.sh
$ ./setup.sh
```

### Identify vendor ID and bus number

```
$ lspci | grep xHCI
00:14.0 USB controller: Intel Corporation Sunrise Point-H USB 3.0 xHCI Controller (rev 31)
```

In this case, bus id of xHCI is 00:14.0.

```
awamoto@ubuntu:~$ lspci -n
00:02.0 0380: 8086:191d (rev 06)
00:14.0 0c03: 8086:a12f (rev 31)
00:14.2 1180: 8086:a131 (rev 31)
00:16.0 0780: 8086:a13a (rev 31)
```
You now know the vendor ID is 8086:a12f. (See the entry of 00:14.0)

### Edit the Makefile.

```
load:
	sudo modprobe uio_pci_generic
	sudo sh -c "echo '8086 a12f' > /sys/bus/pci/drivers/uio_pci_generic/new_id"   <-- vendor ID
	sudo sh -c "echo -n 0000:00:14.0 > /sys/bus/pci/drivers/xhci_hcd/unbind"      <-- bus ID
	sudo sh -c "echo -n 0000:00:14.0 > /sys/bus/pci/drivers/uio_pci_generic/bind" <-- bus ID
```

### Complie and Run.

```
$ make load
$ make run
```

### Enjoy!
![image](https://user-images.githubusercontent.com/536883/32934708-11c048bc-cbb0-11e7-95a5-bca9ee4dba05.png)

## Future Work
* Bulk / Isoch Transfers
* Segmented Rings
* Multi Interrupters
