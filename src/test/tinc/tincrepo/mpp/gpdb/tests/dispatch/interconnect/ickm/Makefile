obj-m +=ickm.o
ickm-objs=pkt_control.o ict.o connectionTable.o log.o work_queue.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean

## ----------------------------------------------------------------------
## Provide support to copy files to home directory of all segments
## Defaults for support for releng build/test environment
## ----------------------------------------------------------------------

HOSTFILE=$${HOME}/segment_host_list
ICKM=ickm.ko

distribute:
	gpscp -v -f ${HOSTFILE} ${ICKM} =:$${HOME}/.

# should use following in cloud 9
#all:
#       make -C /usr/src/kernels/2.6.32-279.11.1.el6.x86_64/ M=$(PWD) modules
#clean:
#       make -C /usr/src/kernels/2.6.32-279.11.1.el6.x86_64/ M=$(PWD) clean
