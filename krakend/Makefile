.PHONY : build
build :
	cargo build --release

.PHONY : clean
clean :
	cargo clean

prefix := /usr/local

.PHONY : install
install :
	install -Cs target/release/krakend $(prefix)/bin/

install-systemd : install
	groupadd -fr krakend
	install -C systemd/* /etc/systemd/system/
	systemctl enable krakend
