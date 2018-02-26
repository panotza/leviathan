obj-m += kraken.o
kraken-objs := src/kraken/main.o src/common.o

obj-m += kraken_x62.o
kraken_x62-objs := src/kraken_x62/main.o src/kraken_x62/leds.o src/kraken_x62/percent.o src/common.o src/util.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
