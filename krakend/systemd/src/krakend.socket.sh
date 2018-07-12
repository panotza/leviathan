#!/bin/sh
set -eu

cat <<EOF
[Unit]
Description=nzxt kraken driver daemon socket

[Install]
WantedBy=sockets.target

[Socket]
ListenStream=%t/krakend/socket
SocketUser=root
SocketGroup=${socket_group}
SocketMode=0660
EOF
