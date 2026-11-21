# Build and run environment for the kernel.
#
#   vagrant up        create the VM and install the toolchain
#   vagrant ssh       log in; the shell starts in /vagrant (this directory)
#   make run          build the ISO and boot it in QEMU inside the terminal
#
# Leave QEMU with Esc, 2, then "quit". Remove the VM with "vagrant destroy".

Vagrant.configure("2") do |config|
  config.vm.box = "bento/ubuntu-24.04"
  config.vm.hostname = "kfs"

  # The project directory is shared with the VM as /vagrant, so the sources
  # are edited on the host and build/kfs.iso appears on the host as well.

  config.vm.provider "virtualbox" do |vb|
    vb.name = "kfs"
    vb.memory = 1024
    vb.cpus = 2
  end

  config.vm.provision "shell", inline: <<-SHELL
    set -e
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y make nasm gcc gcc-multilib \
      grub-pc-bin grub-common xorriso mtools qemu-system-x86
    grep -qx 'cd /vagrant' /home/vagrant/.bashrc ||
      echo 'cd /vagrant' >> /home/vagrant/.bashrc
  SHELL
end
