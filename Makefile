obj-m += kraken.o
kraken-objs := src/kraken/main.o src/common.o

obj-m += kraken_x62.o
kraken_x62-objs := src/kraken_x62/main.o src/common.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
