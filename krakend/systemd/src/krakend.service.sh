#!/bin/sh
set -eu

cat <<EOF
[Unit]
Description=nzxt kraken driver daemon
ConditionUser=root
Requires=krakend.socket

[Install]
Also=krakend.socket

[Service]
Type=notify
ExecStart=${prefix}/bin/krakend --init-system=systemd
Restart=on-failure
User=root
NotifyAccess=main
EOF
