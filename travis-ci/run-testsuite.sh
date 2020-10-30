#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

# Based on SELinux userspace CI scripts from:
# https://github.com/SELinuxProject/selinux

set -ex

# CI Debug output if things go squirrely.
getenforce
id -Z
nproc
pwd

# Turn off enforcing for the setup to prevent any weirdness from breaking
# the CI.
setenforce 0

dnf install -y \
    --allowerasing \
    --skip-broken \
    make \
    perl-Test \
    perl-Test-Harness \
    perl-Test-Simple \
    perl-lib \
    selinux-policy-devel \
    gcc \
    libselinux-devel \
    net-tools \
    netlabel_tools \
    nftables \
    iptables \
    lksctp-tools-devel \
    attr \
    libbpf-devel \
    keyutils-libs-devel \
    quota \
    xfsprogs-devel \
    libuuid-devel \
    e2fsprogs \
    jfsutils \
    dosfstools \
    kernel-devel-"$(uname -r)" \
    kernel-modules-"$(uname -r)"

#
# Move to the selinux testsuite directory.
#
cd "$HOME/selinux-testsuite"

# The testsuite must be run in enforcing mode
setenforce 1

#
# Run the test suite
#
make test
