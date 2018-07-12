.PHONY : build
build :
	cargo build --release

.PHONY : clean
clean :
	cargo clean
	$(RM) -r systemd/target/

export prefix := /usr/local

.PHONY : install
install :
	install -s target/release/krakend $(prefix)/bin/

export socket_group := krakend

.PHONY : install-systemd
install-systemd : install
	./systemd/build.sh
	install -C systemd/target/* /etc/systemd/system/
	groupadd -fr $(socket_group)
	systemctl enable krakend
