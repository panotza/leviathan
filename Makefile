obj-m += kraken.o
kraken-objs := kraken_main.o common.o

obj-m += kraken_x62.o
kraken_x62-objs := kraken_x62_main.o common.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
