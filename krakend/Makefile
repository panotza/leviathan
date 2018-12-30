CARGO = cargo
CARGO_FLAGS =
GROUPADD = groupadd
INSTALL = install
SUDO = sudo
SYSTEMCTL = systemctl

BUILD_FLAGS = --release
CLEAN_FLAGS =


.PHONY : build
build :
	$(CARGO) $(CARGO_FLAGS) build $(BUILD_FLAGS)

.PHONY : clean
clean :
	$(CARGO) $(CARGO_FLAGS) clean $(CLEAN_FLAGS)
	$(RM) -r systemd/target/


export prefix = /usr/local

.PHONY : install
install : build
	$(SUDO) $(INSTALL) -s target/release/krakend $(prefix)/bin/

export socket_group = krakend

.PHONY : install-systemd
install-systemd : install
	./systemd/build.sh
	$(SUDO) $(INSTALL) -C systemd/target/* /etc/systemd/system/
	$(SUDO) $(GROUPADD) -fr $(socket_group)
	$(SUDO) $(SYSTEMCTL) enable krakend
