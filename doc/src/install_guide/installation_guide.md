# OPAE Installation Guide #

```eval_rst
.. toctree::

.. highlight:: c

.. highlight:: console
```

## System compatibility ##

The OPAE SDK has been tested on the following configurations.

* Hardware: Intel(R) FPGA Programmable Acceleration Cards: Arria(R) 10 GX, N3000.
* Operating System: Tested on Fedora 32, with Linux kernel 5.8.
* Arria&reg 10 GX FPGA FIM version: 1.0.3 (1.0 Production)

## How to download the OPAE SDK ##

OPAE SDK releases are available on [GitHub](https://github.com/OPAE/opae-sdk/releases).
Source code for the OPAE DFL device driver for Linux is also available on [GitHub](https://github.com/OPAE/linux-dfl).

## Install the Fedora 32 ##
Download the Fedora 32 (x86_64 version) installation file in [fedora](https://getfedora.org/en/workstation/download/), and install the Fedora 32 in yourserver. You can choose Fedora Workstation or Fedora server.

## Build the kernel and DFL drivers ##

For building the OPAE kernel and kernel driver, the kernel development environment is required. So before you build the kernel, you must install the required packages. Run the following commands:

```console
$ sudo yum install gcc gcc-c++ make kernel-headers kernel-devel elfutils-libelf-devel ncurses-devel openssl-devel bison flex
```

Download the OPAE upstream kernel tree from github.
```console
$ git clone https://github.com/OPAE/linux-dfl.git -b fpga-upstream-dev-5.8.0
```

Configure the kernel.
```console
$ cd linux-dfl
$ cp /boot/config-`uname -r` .config
$ cat configs/n3000_d5005_defconfig >> .config 
$ echo 'CONFIG_LOCALVERSION="-dfl"' >> .config
$ echo 'CONFIG_LOCALVERSION_AUTO=y' >> .config
$ make olddefconfig
```

Compile and install the new kernel.
```console
$ make -j
$ sudo make modules_install
$ sudo make install
```

When installed finished, reboot your system.
When the system login again, check the kernel version is correctly or not.
```console
[figo@localhost linux-dfl]$ uname -a
Linux localhost.localdomain 5.8.0-rc1-dfl-g73e16386cda0 #6 SMP Wed Aug 19 08:38:32 EDT 2020 x86_64 x86_64 x86_64 GNU/Linux
```

And also you can check the OPAE dfl drivers have auto-loaded or not.
```console
[figo@localhost linux-dfl]$ lsmod | grep fpga
ifpga_sec_mgr          20480  1 intel_m10_bmc_secure
fpga_region            20480  3 dfl_fme_region,dfl_fme,dfl
fpga_bridge            24576  4 dfl_fme_region,fpga_region,dfl_fme,dfl_fme_br
fpga_mgr               16384  4 dfl_fme_region,fpga_region,dfl_fme_mgr,dfl_fme
[figo@localhost linux-dfl]$ lsmod | grep dfl
dfl_eth_group          36864  0
dfl_fme_region         20480  0
dfl_emif               16384  0
dfl_n3000_nios         20480  0
dfl_fme_br             16384  0
dfl_fme_mgr            20480  1
dfl_fme                49152  0
dfl_afu                36864  0
dfl_pci                20480  0
dfl                    40960  7 dfl_pci,dfl_fme,dfl_fme_br,dfl_eth_group,dfl_n3000_nios,dfl_afu,dfl_emif
fpga_region            20480  3 dfl_fme_region,dfl_fme,dfl
fpga_bridge            24576  4 dfl_fme_region,fpga_region,dfl_fme,dfl_fme_br
fpga_mgr               16384  4 dfl_fme_region,fpga_region,dfl_fme_mgr,dfl_fme
```

## Build the OPAE-SDK ##
Before you build the kernel, you must install the required packages. Run the following commands:

```console
$ sudo yum install cmake libuuid-devel json-c-devel hwloc-devel tbb-devel rpm-build libedit-devel kernel-devel python3-devel python3-pip python3-virtualenv libudev-devel libcap-devel
```

Download the OPAE-SDK source code from github.
```console
$ git clone https://github.com/OPAE/opae-sdk.git
```

Compile and build the OPAE-SDK.
```console
$ cd opae-sdk
$ mkdir build
$ cmake  ..  -DCPACK_GENERATOR=RPM -DOPAE_BUILD_LEGACY=ON -DOPAE_BUILD_EXTRA_TOOLS_FPGABIST=ON
$ make -j
$ make -j package_rpm
```
After compile successful, there are 8 rpm packages generated.
```console
opae-2.0.0-1.x86_64.rpm
opae-devel-2.0.0-1.x86_64.rpm
opae-libs-2.0.0-1.x86_64.rpm
opae-opae.admin-2.0.0-1.x86_64.rpm
opae-PACSign-2.0.0-1.x86_64.rpm
opae-tests-2.0.0-1.x86_64.rpm
opae-tools-2.0.0-1.x86_64.rpm
opae-tools-extra-2.0.0-1.x86_64.rpm
```
## OPAE SDK installation with rpm packages ##
The rpm packages generated in the previous step can be installed using these commands:

```console
$ sudo yum install opae-libs-<release>.x86_64.rpm
$ sudo yum install opae-tools-<release>.x86_64.rpm
$ sudo yum install opae-tools-extra-<release>.x86_64.rpm
$ sudo yum install opae-devel-<release>.x86_64.rpm
$ sudo yum install opae-<release>.x86_64.rpm
$ sudo yum install opae-opae.admin-<release>.x86_64.rpm
$ sudo yum install opae-PACSign-<release>.x86_64.rpm
$ sudo yum install opae-tests-<release>.x86_64.rpm
```
When you installed the rpms, you can use fpgainfo command to check the FPGA FME infomation.
```console
[figo@localhost install_guide]$ fpgainfo fme
Board Management Controller, MAX10 NIOS FW version: D.2.1.24
Board Management Controller, MAX10 Build version: D.2.0.7
//****** FME ******//
Object Id                        : 0xEF00000
PCIe s:b:d.f                     : 0000:08:00.0
Device Id                        : 0x0B30
Socket Id                        : 0x00
Ports Num                        : 01
Bitstream Id                     : 0x2300011001030F
Bitstream Version                : 0.2.3
Pr Interface Id                  : f3c99413-5081-4aad-bced-07eb84a6d0bb
Boot Page                        : user
```

To uninstall the OPAE rpms, you can use this commands
```console
$ sudo yum remove opae-libs
$ sudo yum remove opae-tools
$ sudo yum remove opae-tools-extra
$ sudo yum remove opae-devel
$ sudo yum remove opae
$ sudo yum remove opae-opae.admin
$ sudo yum remove opae-PACSign
$ sudo yum remove opae-tests
```

## FPGA Device Access Permissions ##

Access to FPGA accelerators and devices is controlled using file access permissions on the
Intel&reg; FPGA device files, `/dev/dfl-fme.*` and `/dev/dfl-port.*`, as well as to the files reachable through `/sys/class/fpga_region/`.

In order to allow regular (non-root) users to access accelerators, you need to grant them read and write permissions on `/dev/dfl-port.*` (with `*` denoting the respective socket, i.e. 0 or 1). E.g.:

```console
$ sudo chmod a+rw /dev/dfl-port.0
```

## Memlock limit ##

Depending on the requirements of your application, you may also want to
increase the maximum amount of memory a user process is allowed to lock. The
exact way to do this depends on your Linux distribution.

You can check the current memlock limit using

```console
$ ulimit -l
```

A way to permanently remove the limit for locked memory for a regular user is
to add the following lines to your /etc/security/limits.conf:

```console
user1    hard   memlock           unlimited
user1    soft   memlock           unlimited
```

This removes the limit on locked memory for user `user1`. To remove it for
all users, you can replace `user1` with `*`:

```console
*    hard   memlock           unlimited
*    soft   memlock           unlimited
```

Note that settings in the /etc/security/limits.conf file don't apply to
services.  To increase the locked memory limit for a service you need to
modify the application's systemd service file and add the line:

```console
[Service]
LimitMEMLOCK=infinity
```

## Hugepage Settings ##

Users need to configure system hugepages to reserve 2MB-hugepages or
1GB-hugepages. For example, the 'hello\_fpga' sample requires several
2MB-hugepages. And the _fpgadiag_ tool requires several 1GB-hugepages.

The command below reserves 20 2M-hugepages:

```console
$ sudo sh -c 'echo 20 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages'
```

The command below reserves 4 1GB-hugepages:

```console
$ sudo sh -c 'echo 4 > /sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages'
```


For x86\_64 architecture processors, user can use following command to find out avaiable hugepage sizes:

```console
$ grep pse /proc/cpuinfo | uniq
flags : ... pse ...
```

If this commands returns a non-empty string, 2MB pages are supported.

```console
$ grep pse /proc/cpuinfo | uniq
flags : ... pdpe1gb ...
```

If this commands returns a non-empty string, 1GB pages are supported.
