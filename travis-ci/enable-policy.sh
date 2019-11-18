#!/bin/bash

set -e

# create a dummy /etc/selinux/config
sudo mkdir -p /etc/selinux
sudo tee /etc/selinux/config >/dev/null <<EOF
SELINUX=disabled
SELINUXTYPE=$1
EOF
