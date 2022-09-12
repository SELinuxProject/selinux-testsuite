# -*- mode: ruby -*-
# vi: set ft=ruby :
# Vagrant configuration file which creates a virtual machine that can run the
# test suite.
#
# To create a new virtual machine:
#
#    FEDORA_VERSION=33 vagrant up
#
# To launch tests (for example after modifications have been made):
#
#    vagrant rsync && vagrant ssh -- sudo make -C /root/testsuite test
#
# To destroy the virtual machine (for example to start again from a clean environment):
#
#    vagrant destroy

# All Vagrant configuration is done below. The "2" in Vagrant.configure
# configures the configuration version (we support older styles for
# backwards compatibility). Please don't change it unless you know what
# you're doing.
Vagrant.configure("2") do |config|
  config.vm.box = "fedora/#{ENV['FEDORA_VERSION']}-cloud-base"
  config.vm.synced_folder ".", "/vagrant", disabled: true
  config.vm.synced_folder ".", "/root/testsuite", type: "rsync",
    # need to disable '--copy-links', which is in rsync__args by default
    rsync__args: ["-vzra", "--delete"]

  config.vm.provider "virtualbox" do |v|
    v.memory = 4096
  end
  config.vm.provider "libvirt" do |v|
    v.memory = 4096
  end

  case ENV['KERNEL_TYPE']
  when 'default'
    dnf_opts = ''
    kernel_pkgs = 'kernel-devel-"$(uname -r)" kernel-modules-"$(uname -r)"'
  when 'latest'
    dnf_opts = ''
    kernel_pkgs = 'kernel-devel kernel-modules'
  when 'secnext'
    dnf_opts = '--nogpgcheck --releasever rawhide --repofrompath kernel-secnext,https://repo.paul-moore.com/rawhide/x86_64'
    kernel_pkgs = 'kernel-devel kernel-modules'
  else
    print("Invalid KERNEL_TYPE '#{ENV['KERNEL_TYPE']}'")
    abort
  end

  extra_commands = ''
  case ENV['ROOT_DOMAIN']
  when 'unconfined_t'
  when 'sysadm_t'
    extra_commands = <<EOF
      semanage boolean --modify --on ssh_sysadm_login
      semanage login --modify -s sysadm_u root
      semanage login --add -s sysadm_u -r s0-s0:c0.c1023 vagrant
EOF
  else
    print("Invalid ROOT_DOMAIN '#{ENV['ROOT_DOMAIN']}'")
    abort
  end

  config.vm.provision :shell, inline: <<SCRIPT
    dnf install -y #{dnf_opts} \
      --allowerasing \
      --skip-broken \
      git-core \
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
      #{kernel_pkgs}

    #{extra_commands}

    # for secretmem test
    grubby --update-kernel=ALL --args=secretmem.enable=1

    reboot
SCRIPT
end
