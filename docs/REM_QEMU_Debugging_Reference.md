# QEMU System and Debugging Reference Extract

Extracted from the QEMU 8.1.5 documentation PDF. It covers system emulation, invocation, the monitor, GDB usage, and tracing. MyEmulator32-specific behavior remains repository-specific.

---

```text
QEMU Documentation, Release 8.1.5


mips r4k platform (removed in 5.2)

This machine type was very old and unmaintained. Users should use the malta machine type instead.


mips fulong2e machine alias (removed in 6.0)

This machine has been renamed fuloong2e.


pc-0.10 up to pc-1.3 (removed in 4.0 up to 6.0)

These machine types were very old and likely could not be used for live migration from old QEMU versions anymore.
Use a newer machine type instead.


Raspberry Pi raspi2 and raspi3 machines (removed in 6.2)

The Raspberry Pi machines come in various models (A, A+, B, B+). To be able to distinguish which model QEMU is
implementing, the raspi2 and raspi3 machines have been renamed raspi2b and raspi3b.


Aspeed swift-bmc machine (removed in 7.0)

This machine was removed because it was unused.           Alternative AST2500 based OpenPOWER machines are
witherspoon-bmc and romulus-bmc.


ppc taihu machine (removed in 7.2)

This machine was removed because it was partially emulated and 405 machines are very similar. Use the ref405ep
machine instead.


1.4.9 linux-user mode CPUs

tilegx CPUs (removed in 6.0)

The tilegx guest CPU support has been removed without replacement. It was only implemented in linux-user mode,
but support for this CPU was removed from the upstream Linux kernel in 2018, and it has also been dropped from
glibc, so there is no new Linux development taking place with this architecture. For running the old binaries, you can
use older versions of QEMU.


ppc64abi32 CPUs (removed in 7.0)

The ppc64abi32 architecture has a number of issues which regularly tripped up the CI testing and was suspected to
be quite broken. For that reason the maintainers strongly suspected no one actually used it.




26                                                                                      Chapter 1. About QEMU
                                                                          QEMU Documentation, Release 8.1.5


1.4.10 TCG introspection features

TCG trace-events (since 6.2)

The ability to add new TCG trace points had bit rotted and as the feature can be replicated with TCG plugins it was
removed. If any user is currently using this feature and needs help with converting to using TCG plugins they should
contact the qemu-devel mailing list.


1.4.11 System emulator devices

spapr-pci-vfio-host-bridge (removed in 2.12)

The spapr-pci-vfio-host-bridge device type has been replaced by the spapr-pci-host-bridge device type.


ivshmem (removed in 4.0)

Replaced by either the ivshmem-plain or ivshmem-doorbell.


ide-drive (removed in 6.0)

The ‘ide-drive’ device has been removed. Users should use ‘ide-hd’ or ‘ide-cd’ as appropriate to get an IDE hard disk
or CD-ROM as needed.


scsi-disk (removed in 6.0)

The ‘scsi-disk’ device has been removed. Users should use ‘scsi-hd’ or ‘scsi-cd’ as appropriate to get a SCSI hard disk
or CD-ROM as needed.


sga (removed in 8.0)

The sga device loaded an option ROM for x86 targets which enabled SeaBIOS to send messages to the serial console.
SeaBIOS 1.11.0 onwards contains native support for this feature and thus use of the option ROM approach was obsolete.
The native SeaBIOS support can be activated by using -machine graphics=off.


1.4.12 Related binaries

qemu-nbd --partition (removed in 5.0)

The qemu-nbd --partition $digit code (also spelled -P) could only handle MBR partitions, and never correctly
handled logical partitions beyond partition 5. Exporting a partition can still be done by utilizing the --image-opts
option with a raw blockdev using the offset and size parameters layered on top of any other existing blockdev. For
example, if partition 1 is 100MiB long starting at 1MiB, the old command:

qemu-nbd -t -P 1 -f qcow2 file.qcow2

can be rewritten as:

qemu-nbd -t --image-opts driver=raw,offset=1M,size=100M,file.driver=qcow2,file.file.
 ˓→driver=file,file.file.filename=file.qcow2




1.4. Removed features                                                                                               27
QEMU Documentation, Release 8.1.5


qemu-img convert -n -o (removed in 5.1)

All options specified in -o are image creation options, so they are now rejected when used with -n to skip image
creation.


qemu-img create -b bad file $size (removed in 5.1)

When creating an image with a backing file that could not be opened, qemu-img create used to issue a warning about
the failure but proceed with the image creation if an explicit size was provided. However, as the -u option exists for
this purpose, it is safer to enforce that any failure to open the backing image (including if the backing file is missing or
an incorrect format was specified) is an error when -u is not used.


qemu-img amend to adjust backing file (removed in 6.1)

The use of qemu-img amend to modify the name or format of a qcow2 backing image was never fully documented
or tested, and interferes with other amend operations that need access to the original backing image (such as deciding
whether a v3 zero cluster may be left unallocated when converting to a v2 image). Any changes to the backing chain
should be performed with qemu-img rebase -u either before or after the remaining changes being performed by
amend, as appropriate.


qemu-img backing file without format (removed in 6.1)

The use of qemu-img create, qemu-img rebase, or qemu-img convert to create or modify an image that depends
on a backing file now requires that an explicit backing format be provided. This is for safety: if QEMU probes a different
format than what you thought, the data presented to the guest will be corrupt; similarly, presenting a raw image to a
guest allows a potential security exploit if a future probe sees a non-raw image based on guest writes.
To avoid creating unsafe backing chains, you must pass -o backing_fmt= (or the shorthand -F during create) to
specify the intended backing format. You may use qemu-img rebase -u to retroactively add a backing format to an
existing image. However, be aware that there are already potential security risks to blindly using qemu-img info to
probe the format of an untrusted backing image, when deciding what format to add into an existing image.


1.4.13 Block devices

VXHS backend (removed in 5.1)

The VXHS code did not compile since v2.12.0. It was removed in 5.1.


sheepdog driver (removed in 6.0)

The corresponding upstream server project is no longer maintained. Users are recommended to switch to an alternative
distributed block device driver such as RBD.




28                                                                                           Chapter 1. About QEMU
                                                                       QEMU Documentation, Release 8.1.5


1.4.14 Tools

virtiofsd (removed in 8.0)

There is a newer Rust implementation of virtiofsd at https://gitlab.com/virtio-fs/virtiofsd; this has
been stable for some time and is now widely used. The command line and feature set is very close to the removed C
implementation.


1.5 License

QEMU is a trademark of Fabrice Bellard.
QEMU is released under the GNU General Public License, version 2. Parts of QEMU have specific licenses, see file
LICENSE.




1.5. License                                                                                                  29
QEMU Documentation, Release 8.1.5




30                                  Chapter 1. About QEMU
                                                                                                          CHAPTER

                                                                                                              TWO


                                                                                 SYSTEM EMULATION



This section of the manual is the overall guide for users using QEMU for full system emulation (as opposed to user-mode
emulation). This includes working with hypervisors such as KVM, Xen, Hax or Hypervisor.Framework.


2.1 Introduction

2.1.1 Virtualisation Accelerators

QEMU’s system emulation provides a virtual model of a machine (CPU, memory and emulated devices) to run a guest
OS. It supports a number of hypervisors (known as accelerators) as well as a JIT known as the Tiny Code Generator
(TCG) capable of emulating many CPUs.

                                          Table 1: Supported Accelerators
 Accelerator                           Host OS                         Host Architectures
 KVM                                   Linux                           Arm (64 bit only), MIPS, PPC, RISC-V,
                                                                       s390x, x86
 Xen                                   Linux (as dom0)                 Arm, x86
 Intel HAXM (hax)                      Linux, Windows                  x86
 Hypervisor Framework (hvf)            MacOS                           x86 (64 bit only), Arm (64 bit only)
 Windows Hypervisor Platform           Windows                         x86
 (whpx)
 NetBSD Virtual Machine Monitor        NetBSD                             x86
 (nvmm)
 Tiny Code Generator (tcg)             Linux, other POSIX, Windows,       Arm, x86, Loongarch64, MIPS, PPC,
                                       MacOS                              s390x, Sparc64


2.1.2 Feature Overview

System emulation provides a wide range of device models to emulate various hardware components you may want to
add to your machine. This includes a wide number of VirtIO devices which are specifically tuned for efficient operation
under virtualisation. Some of the device emulation can be offloaded from the main QEMU process using either vhost-
user (for VirtIO) or Multi-process QEMU. If the platform supports it QEMU also supports directly passing devices
through to guest VMs to eliminate the device emulation overhead. See Device Emulation for more details.
There is a full featured block layer which allows for construction of complex storage topology which can be stacked
across multiple layers supporting redirection, networking, snapshots and migration support.
The flexible chardev system allows for handling IO from character like devices using stdio, files, unix sockets and
TCP networking.


                                                                                                                    31
QEMU Documentation, Release 8.1.5


QEMU provides a number of management interfaces including a line based Human Monitor Protocol (HMP) that
allows you to dynamically add and remove devices as well as introspect the system state. The QEMU Monitor Protocol
(QMP) is a well defined, versioned, machine usable API that presents a rich interface to other tools to create, control
and manage Virtual Machines. This is the interface used by higher level tools interfaces such as Virt Manager using
the libvirt framework.
For the common accelerators QEMU, supported debugging with its gdbstub which allows users to connect GDB and
debug system software images.


2.1.3 Running

QEMU provides a rich and complex API which can be overwhelming to understand. While some architectures can
boot something with just a disk image, those examples elide a lot of details with defaults that may not be optimal for
modern systems.
For a non-x86 system where we emulate a broad range of machine types, the command lines are generally more explicit
in defining the machine and boot behaviour. You will find often find example command lines in the QEMU System
Emulator Targets section of the manual.
While the project doesn’t want to discourage users from using the command line to launch VMs, we do want to highlight
that there are a number of projects dedicated to providing a more user friendly experience. Those built around the
libvirt framework can make use of feature probing to build modern VM images tailored to run on the hardware you
have.
That said, the general form of a QEMU command line can be expressed as:
$ qemu-system-x86_64 [machine opts] \
                [cpu opts] \
                [accelerator opts] \
                [device opts] \
                [backend opts] \
                [interface opts] \
                [boot opts]
Most options will generate some help information. So for example:
$ qemu-system-x86_64 -M help
will list the machine types supported by that QEMU binary. help can also be passed as an argument to another option.
For example:
$ qemu-system-x86_64 -device scsi-hd,help
will list the arguments and their default values of additional options that can control the behaviour of the scsi-hd
device.




32                                                                                 Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5



                                             Table 2: Options Overview
 Options
 Ma-         Define the machine type, amount of memory etc
 chine
 CPU         Type and number/topology of vCPUs. Most accelerators offer a host cpu option which simply passes
             through your host CPU configuration without filtering out any features.
 Acceler-    This will depend on the hypervisor you run. Note that the default is TCG, which is purely emulated, so
 ator        you must specify an accelerator type to take advantage of hardware virtualization.
 Devices     Additional devices that are not defined by default with the machine type.
 Back-       Backends are how QEMU deals with the guest’s data, for example how a block device is stored, how
 ends        network devices see the network or how a serial device is directed to the outside world.
 Inter-      How the system is displayed, how it is managed and controlled or debugged.
 faces
 Boot        How the system boots, via firmware or direct kernel boot.

In the following example we first define a virt machine which is a general purpose platform for running Aarch64
guests. We enable virtualisation so we can use KVM inside the emulated guest. As the virt machine comes with
some built in pflash devices we give them names so we can override the defaults later.

$ qemu-system-aarch64 \
   -machine type=virt,virtualization=on,pflash0=rom,pflash1=efivars \
   -m 4096 \

We then define the 4 vCPUs using the max option which gives us all the Arm features QEMU is capable of emulating.
We enable a more emulation friendly implementation of Arm’s pointer authentication algorithm. We explicitly specify
TCG acceleration even though QEMU would default to it anyway.

-cpu max,pauth-impdef=on \
-smp 4 \
-accel tcg \

As the virt platform doesn’t have any default network or storage devices we need to define them. We give them ids
so we can link them with the backend later on.

-device virtio-net-pci,netdev=unet \
-device virtio-scsi-pci \
-device scsi-hd,drive=hd \

We connect the user-mode networking to our network device. As user-mode networking isn’t directly accessible from
the outside world we forward localhost port 2222 to the ssh port on the guest.

-netdev user,id=unet,hostfwd=tcp::2222-:22 \

We connect the guest visible block device to an LVM partition we have set aside for our guest.

-blockdev driver=raw,node-name=hd,file.driver=host_device,file.filename=/dev/lvm-disk/
 ˓→debian-bullseye-arm64 \




We then tell QEMU to multiplex the QEMU Monitor with the serial port output (we can switch between the two using
Keys in the character backend multiplexer). As there is no default graphical device we disable the display as we can
work entirely in the terminal.

-serial mon:stdio \
-display none \


2.1. Introduction                                                                                                33
QEMU Documentation, Release 8.1.5


Finally we override the default firmware to ensure we have some storage for EFI to persist its configuration. That
firmware is responsible for finding the disk, booting grub and eventually running our system.

-blockdev node-name=rom,driver=file,filename=(pwd)/pc-bios/edk2-aarch64-code.fd,read-
 ˓→only=true \

-blockdev node-name=efivars,driver=file,filename=$HOME/images/qemu-arm64-efivars



2.2 Invocation

qemu-system-x86_64 [options] [disk_image]
disk_image is a raw hard disk image for IDE hard disk 0. Some targets do not need a disk image.


2.2.1 Standard options

-h
      Display help and exit
-version
     Display version information and exit
-machine [type=]name[,prop=value[,...]]
     Select the emulated machine by name. Use -machine help to list available machines.
      For architectures which aim to support live migration compatibility across releases, each release will introduce
      a new versioned machine type. For example, the 2.8.0 release introduced machine types “pc-i440fx-2.8” and
      “pc-q35-2.8” for the x86_64/i686 architectures.
      To allow live migration of guests from QEMU version 2.8.0, to QEMU version 2.9.0, the 2.9.0 version must
      support the “pc-i440fx-2.8” and “pc-q35-2.8” machines too. To allow users live migrating VMs to skip multiple
      intermediate releases when upgrading, new releases of QEMU will support machine types from many previous
      versions.
      Supported machine properties are:
      accel=accels1[:accels2[:...]]
          This is used to enable an accelerator. Depending on the target architecture, kvm, xen, hax, hvf, nvmm,
          whpx or tcg can be available. By default, tcg is used. If there is more than one accelerator specified, the
          next one is used if the previous one fails to initialize.
      vmport=on|off|auto
          Enables emulation of VMWare IO port, for vmmouse etc. auto says to select the value based on accel. For
          accel=xen the default is off otherwise the default is on.
      dump-guest-core=on|off
          Include guest memory in a core dump. The default is on.
      mem-merge=on|off
          Enables or disables memory merge support. This feature, when supported by the host, de-duplicates iden-
          tical memory pages among VMs instances (enabled by default).
      aes-key-wrap=on|off
          Enables or disables AES key wrapping support on s390-ccw hosts. This feature controls whether AES
          wrapping keys will be created to allow execution of AES cryptographic functions. The default is on.




34                                                                                Chapter 2. System Emulation
                                                                       QEMU Documentation, Release 8.1.5


     dea-key-wrap=on|off
         Enables or disables DEA key wrapping support on s390-ccw hosts. This feature controls whether DEA
         wrapping keys will be created to allow execution of DEA cryptographic functions. The default is on.
     nvdimm=on|off
         Enables or disables NVDIMM support. The default is off.
     memory-encryption=
         Memory encryption object to use. The default is none.
     hmat=on|off
         Enables or disables ACPI Heterogeneous Memory Attribute Table (HMAT) support. The default is off.
     memory-backend='id'
         An alternative to legacy -mem-path and mem-prealloc options. Allows to use a memory backend as
         main RAM.
          For example:

          -object memory-backend-file,id=pc.ram,size=512M,mem-path=/hugetlbfs,prealloc=on,
           ˓→share=on

          -machine memory-backend=pc.ram
          -m 512M

          Migration compatibility note:
            • as backend id one shall use value of ‘default-ram-id’, advertised by machine type (available via
              query-machines QMP command), if migration to/from old QEMU (<5.0) is expected.
            • for machine types 4.0 and older, user shall use x-use-canonical-path-for-ramblock-id=off
              backend option if migration to/from old QEMU (<5.0) is expected.
          For example:

          -object memory-backend-ram,id=pc.ram,size=512M,x-use-canonical-path-for-
           ˓→ramblock-id=off

          -machine memory-backend=pc.ram
          -m 512M

     cxl-fmw.0.targets.0=firsttarget,cxl-fmw.0.targets.1=secondtarget,cxl-fmw.0.
     size=size[,cxl-fmw.0.interleave-granularity=granularity]
         Define a CXL Fixed Memory Window (CFMW).
          Described in the CXL 2.0 ECN: CEDT CFMWS & QTG _DSM.
          They are regions of Host Physical Addresses (HPA) on a system which may be interleaved across one
          or more CXL host bridges. The system software will assign particular devices into these windows and
          configure the downstream Host-managed Device Memory (HDM) decoders in root ports, switch ports and
          devices appropriately to meet the interleave requirements before enabling the memory devices.
          targets.X=target provides the mapping to CXL host bridges which may be identified by the id provided
          in the -device entry. Multiple entries are needed to specify all the targets when the fixed memory window
          represents interleaved memory. X is the target index from 0.
          size=size sets the size of the CFMW. This must be a multiple of 256MiB. The region will be aligned to
          256MiB but the location is platform and configuration dependent.
          interleave-granularity=granularity sets the granularity of interleave. Default 256KiB. Only
          256KiB, 512KiB, 1024KiB, 2048KiB 4096KiB, 8192KiB and 16384KiB granularities supported.
          Example:



2.2. Invocation                                                                                                 35
QEMU Documentation, Release 8.1.5



            -machine cxl-fmw.0.targets.0=cxl.0,cxl-fmw.0.targets.1=cxl.1,cxl-fmw.0.
             ˓→size=128G,cxl-fmw.0.interleave-granularity=512k




sgx-epc.0.memdev=@var{memid},sgx-epc.0.node=@var{numaid}
     Define an SGX EPC section.
-cpu model
     Select CPU model (-cpu help for list and additional feature selection)
-accel name[,prop=value[,...]]
     This is used to enable an accelerator. Depending on the target architecture, kvm, xen, hax, hvf, nvmm, whpx or
     tcg can be available. By default, tcg is used. If there is more than one accelerator specified, the next one is used
     if the previous one fails to initialize.
      igd-passthru=on|off
          When Xen is in use, this option controls whether Intel integrated graphics devices can be passed through
          to the guest (default=off)
      kernel-irqchip=on|off|split
          Controls KVM in-kernel irqchip support. The default is full acceleration of the interrupt controllers. On
          x86, split irqchip reduces the kernel attack surface, at a performance cost for non-MSI interrupts. Disabling
          the in-kernel irqchip completely is not recommended except for debugging purposes.
      kvm-shadow-mem=size
          Defines the size of the KVM shadow MMU.
      one-insn-per-tb=on|off
          Makes the TCG accelerator put only one guest instruction into each translation block. This slows down
          emulation a lot, but can be useful in some situations, such as when trying to analyse the logs produced by
          the -d option.
      split-wx=on|off
          Controls the use of split w^x mapping for the TCG code generation buffer. Some operating systems require
          this to be enabled, and in such a case this will default on. On other operating systems, this will default off,
          but one may enable this for testing or debugging.
      tb-size=n
          Controls the size (in MiB) of the TCG translation block cache.
      thread=single|multi
          Controls number of TCG threads. When the TCG is multi-threaded there will be one thread per vCPU there-
          fore taking advantage of additional host cores. The default is to enable multi-threading where both the back-
          end and front-ends support it and no incompatible TCG features have been enabled (e.g. icount/replay).
      dirty-ring-size=n
          When the KVM accelerator is used, it controls the size of the per-vCPU dirty page ring buffer (number
          of entries for each vCPU). It should be a value that is power of two, and it should be 1024 or bigger (but
          still less than the maximum value that the kernel supports). 4096 could be a good initial value if you have
          no idea which is the best. Set this value to 0 to disable the feature. By default, this feature is disabled
          (dirty-ring-size=0). When enabled, KVM will instead record dirty pages in a bitmap.
      notify-vmexit=run|internal-error|disable,notify-window=n
          Enables or disables notify VM exit support on x86 host and specify the corresponding notify window to
          trigger the VM exit if enabled. run option enables the feature. It does nothing and continue if the exit
          happens. internal-error option enables the feature. It raises a internal error. disable option doesn’t
          enable the feature. This feature can mitigate the CPU stuck issue due to event windows don’t open up for a
          specified of time (i.e. notify-window). Default: notify-vmexit=run,notify-window=0.




36                                                                                  Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


-smp [[cpus=]n][,maxcpus=maxcpus][,sockets=sockets][,dies=dies][,clusters=clusters][,
cores=cores][,threads=threads]
     Simulate a SMP system with ‘n‘ CPUs initially present on the machine type board. On boards supporting CPU
     hotplug, the optional ‘maxcpus‘ parameter can be set to enable further CPUs to be added at runtime. When both
     parameters are omitted, the maximum number of CPUs will be calculated from the provided topology members
     and the initial CPU count will match the maximum number. When only one of them is given then the omitted
     one will be set to its counterpart’s value. Both parameters may be specified, but the maximum number of CPUs
     must be equal to or greater than the initial CPU count. Product of the CPU topology hierarchy must be equal to
     the maximum number of CPUs. Both parameters are subject to an upper limit that is determined by the specific
     machine type chosen.
      To control reporting of CPU topology information, values of the topology parameters can be specified. Machines
      may only support a subset of the parameters and different machines may have different subsets supported which
      vary depending on capacity of the corresponding CPU targets. So for a particular machine type board, an ex-
      pected topology hierarchy can be defined through the supported sub-option. Unsupported parameters can also
      be provided in addition to the sub-option, but their values must be set as 1 in the purpose of correct parsing.
      Either the initial CPU count, or at least one of the topology parameters must be specified. The specified pa-
      rameters must be greater than zero, explicit configuration like “cpus=0” is not allowed. Values for any omitted
      parameters will be computed from those which are given.
      For example, the following sub-option defines a CPU topology hierarchy (2 sockets totally on the machine, 2
      cores per socket, 2 threads per core) for a machine that only supports sockets/cores/threads. Some members of
      the option can be omitted but their values will be automatically computed:

      -smp 8,sockets=2,cores=2,threads=2,maxcpus=8

      The following sub-option defines a CPU topology hierarchy (2 sockets totally on the machine, 2 dies per socket,
      2 cores per die, 2 threads per core) for PC machines which support sockets/dies/cores/threads. Some members
      of the option can be omitted but their values will be automatically computed:

      -smp 16,sockets=2,dies=2,cores=2,threads=2,maxcpus=16

      The following sub-option defines a CPU topology hierarchy (2 sockets totally on the machine, 2 clusters
      per socket, 2 cores per cluster, 2 threads per core) for ARM virt machines which support sockets/clusters
      /cores/threads. Some members of the option can be omitted but their values will be automatically computed:

      -smp 16,sockets=2,clusters=2,cores=2,threads=2,maxcpus=16

      Historically preference was given to the coarsest topology parameters when computing missing values (ie sockets
      preferred over cores, which were preferred over threads), however, this behaviour is considered liable to change.
      Prior to 6.2 the preference was sockets over cores over threads. Since 6.2 the preference is cores over sockets
      over threads.
      For example, the following option defines a machine board with 2 sockets of 1 core before 6.2 and 1 socket of 2
      cores after 6.2:

      -smp 2

      Note: The cluster topology will only be generated in ACPI and exposed to guest if it’s explicitly specified in
      -smp.
-numa node[,mem=size][,cpus=firstcpu[-lastcpu]][,nodeid=node][,initiator=initiator]
-numa node[,memdev=id][,cpus=firstcpu[-lastcpu]][,nodeid=node][,initiator=initiator]
-numa dist,src=source,dst=destination,val=distance
-numa cpu,node-id=node[,socket-id=x][,core-id=y][,thread-id=z]


2.2. Invocation                                                                                                     37
QEMU Documentation, Release 8.1.5


-numa hmat-lb,initiator=node,target=node,hierarchy=hierarchy,data-type=type[,
latency=lat][,bandwidth=bw]
-numa hmat-cache,node-id=node,size=size,level=level[,associativity=str][,policy=str][,
line=size]
     Define a NUMA node and assign RAM and VCPUs to it. Set the NUMA distance from a source node to a
     destination node. Set the ACPI Heterogeneous Memory Attributes for the given nodes.
     Legacy VCPU assignment uses ‘cpus‘ option where firstcpu and lastcpu are CPU indexes. Each ‘cpus‘ option
     represent a contiguous range of CPU indexes (or a single VCPU if lastcpu is omitted). A non-contiguous set of
     VCPUs can be represented by providing multiple ‘cpus‘ options. If ‘cpus‘ is omitted on all nodes, VCPUs are
     automatically split between them.
     For example, the following option assigns VCPUs 0, 1, 2 and 5 to a NUMA node:

     -numa node,cpus=0-2,cpus=5

     ‘cpu‘ option is a new alternative to ‘cpus‘ option which uses ‘socket-id|core-id|thread-id‘ properties to
     assign CPU objects to a node using topology layout properties of CPU. The set of properties is machine specific,
     and depends on used machine type/’smp‘ options. It could be queried with ‘hotpluggable-cpus‘ monitor
     command. ‘node-id‘ property specifies node to which CPU object will be assigned, it’s required for node to be
     declared with ‘node‘ option before it’s used with ‘cpu‘ option.
     For example:

     -M pc \
     -smp 1,sockets=2,maxcpus=2 \
     -numa node,nodeid=0 -numa node,nodeid=1 \
     -numa cpu,node-id=0,socket-id=0 -numa cpu,node-id=1,socket-id=1

     ‘memdev‘ option assigns RAM from a given memory backend device to a node. It is recommended to use
     ‘memdev‘ option over legacy ‘mem‘ option. This is because ‘memdev‘ option provides better performance and
     more control over the backend’s RAM (e.g. ‘prealloc‘ parameter of ‘-memory-backend-ram‘ allows memory
     preallocation).
     For compatibility reasons, legacy ‘mem‘ option is supported in 5.0 and older machine types. Note that ‘mem‘ and
     ‘memdev‘ are mutually exclusive. If one node uses ‘memdev‘, the rest nodes have to use ‘memdev‘ option, and
     vice versa.
     Users must specify memory for all NUMA nodes by ‘memdev‘ (or legacy ‘mem‘ if available). In QEMU 5.2, the
     support for ‘-numa node‘ without memory specified was removed.
     ‘initiator‘ is an additional option that points to an initiator NUMA node that has best performance (the lowest
     latency or largest bandwidth) to this NUMA node. Note that this option can be set only when the machine property
     ‘hmat’ is set to ‘on’.
     Following example creates a machine with 2 NUMA nodes, node 0 has CPU. node 1 has only memory, and its
     initiator is node 0. Note that because node 0 has CPU, by default the initiator of node 0 is itself and must be
     itself.

     -machine hmat=on \
     -m 2G,slots=2,maxmem=4G \
     -object memory-backend-ram,size=1G,id=m0 \
     -object memory-backend-ram,size=1G,id=m1 \
     -numa node,nodeid=0,memdev=m0 \
     -numa node,nodeid=1,memdev=m1,initiator=0 \
     -smp 2,sockets=2,maxcpus=2 \
                                                                                                 (continues on next page)




38                                                                               Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


                                                                                               (continued from previous page)
     -numa cpu,node-id=0,socket-id=0 \
     -numa cpu,node-id=0,socket-id=1

     source and destination are NUMA node IDs. distance is the NUMA distance from source to destination. The
     distance from a node to itself is always 10. If any pair of nodes is given a distance, then all pairs must be given
     distances. Although, when distances are only given in one direction for each pair of nodes, then the distances in
     the opposite directions are assumed to be the same. If, however, an asymmetrical pair of distances is given for
     even one node pair, then all node pairs must be provided distance values for both directions, even when they are
     symmetrical. When a node is unreachable from another node, set the pair’s distance to 255.
     Note that the -numa option doesn’t allocate any of the specified resources, it just assigns existing resources to
     NUMA nodes. This means that one still has to use the -m, -smp options to allocate RAM and VCPUs respectively.
     Use ‘hmat-lb‘ to set System Locality Latency and Bandwidth Information between initiator and target NUMA
     nodes in ACPI Heterogeneous Attribute Memory Table (HMAT). Initiator NUMA node can create memory
     requests, usually it has one or more processors. Target NUMA node contains addressable memory.
     In ‘hmat-lb‘ option, node are NUMA node IDs. hierarchy is the memory hierarchy of the target NUMA node:
     if hierarchy is ‘memory’, the structure represents the memory performance; if hierarchy is ‘first-level|second-
     level|third-level’, this structure represents aggregated performance of memory side caches for each domain.
     type of ‘data-type’ is type of data represented by this structure instance: if ‘hierarchy’ is ‘memory’, ‘data-
     type’ is ‘access|read|write’ latency or ‘access|read|write’ bandwidth of the target memory; if ‘hierarchy’ is ‘first-
     level|second-level|third-level’, ‘data-type’ is ‘access|read|write’ hit latency or ‘access|read|write’ hit bandwidth
     of the target memory side cache.
     lat is latency value in nanoseconds. bw is bandwidth value, the possible value and units are NUM[M|G|T], mean
     that the bandwidth value are NUM byte per second (or MB/s, GB/s or TB/s depending on used suffix). Note that
     if latency or bandwidth value is 0, means the corresponding latency or bandwidth information is not provided.
     In ‘hmat-cache‘ option, node-id is the NUMA-id of the memory belongs. size is the size of memory side
     cache in bytes. level is the cache level described in this structure, note that the cache level 0 should not be used
     with ‘hmat-cache‘ option. associativity is the cache associativity, the possible value is ‘none/direct(direct-
     mapped)/complex(complex cache indexing)’. policy is the write policy. line is the cache Line size in bytes.
     For example, the following options describe 2 NUMA nodes. Node 0 has 2 cpus and a ram, node 1 has only a ram.
     The processors in node 0 access memory in node 0 with access-latency 5 nanoseconds, access-bandwidth is 200
     MB/s; The processors in NUMA node 0 access memory in NUMA node 1 with access-latency 10 nanoseconds,
     access-bandwidth is 100 MB/s. And for memory side cache information, NUMA node 0 and 1 both have 1 level
     memory cache, size is 10KB, policy is write-back, the cache Line size is 8 bytes:

     -machine hmat=on \
     -m 2G \
     -object memory-backend-ram,size=1G,id=m0 \
     -object memory-backend-ram,size=1G,id=m1 \
     -smp 2,sockets=2,maxcpus=2 \
     -numa node,nodeid=0,memdev=m0 \
     -numa node,nodeid=1,memdev=m1,initiator=0 \
     -numa cpu,node-id=0,socket-id=0 \
     -numa cpu,node-id=0,socket-id=1 \
     -numa hmat-lb,initiator=0,target=0,hierarchy=memory,data-type=access-latency,
      ˓→latency=5 \

     -numa hmat-lb,initiator=0,target=0,hierarchy=memory,data-type=access-bandwidth,
      ˓→bandwidth=200M \

     -numa hmat-lb,initiator=0,target=1,hierarchy=memory,data-type=access-latency,
      ˓→latency=10 \

                                                                                                     (continues on next page)



2.2. Invocation                                                                                                          39
QEMU Documentation, Release 8.1.5


                                                                                               (continued from previous page)
      -numa hmat-lb,initiator=0,target=1,hierarchy=memory,data-type=access-bandwidth,
       ˓→bandwidth=100M \

      -numa hmat-cache,node-id=0,size=10K,level=1,associativity=direct,policy=write-back,
       ˓→line=8 \

      -numa hmat-cache,node-id=1,size=10K,level=1,associativity=direct,policy=write-back,
       ˓→line=8




-add-fd fd=fd,set=set[,opaque=opaque]
     Add a file descriptor to an fd set. Valid options are:
      fd=fd
          This option defines the file descriptor of which a duplicate is added to fd set. The file descriptor cannot be
          stdin, stdout, or stderr.
      set=set
          This option defines the ID of the fd set to add the file descriptor to.
      opaque=opaque
          This option defines a free-form string that can be used to describe fd.
      You can open an image using pre-opened file descriptors from an fd set:
      qemu-system-x86_64 \
       -add-fd fd=3,set=2,opaque="rdwr:/path/to/file" \
       -add-fd fd=4,set=2,opaque="rdonly:/path/to/file" \
       -drive file=/dev/fdset/2,index=0,media=disk
-set group.id.arg=value
     Set parameter arg for item id of type group
-global driver.prop=value
-global driver=driver,property=property,value=value
     Set default value of driver’s property prop to value, e.g.:
      qemu-system-x86_64 -global ide-hd.physical_block_size=4096 disk-image.img
      In particular, you can use this to set driver properties for devices which are created automatically by the machine
      model. To create a device which is not created automatically and set properties on it, use -device.
      -global driver.prop=value is shorthand for -global driver=driver,property=prop,value=value. The longhand syn-
      tax works even when driver contains a dot.
-boot [order=drives][,once=drives][,menu=on|off][,splash=sp_name][,splash-time=sp_time][,
reboot-timeout=rb_timeout][,strict=on|off]
     Specify boot order drives as a string of drive letters. Valid drive letters depend on the target architecture. The
     x86 PC uses: a, b (floppy 1 and 2), c (first hard disk), d (first CD-ROM), n-p (Etherboot from network adapter
     1-4), hard disk boot is the default. To apply a particular boot order only on the first startup, specify it via once.
     Note that the order or once parameter should not be used together with the bootindex property of devices,
     since the firmware implementations normally do not support both at the same time.
      Interactive boot menus/prompts can be enabled via menu=on as far as firmware/BIOS supports them. The default
      is non-interactive boot.
      A splash picture could be passed to bios, enabling user to show it as logo, when option splash=sp_name is given
      and menu=on, If firmware/BIOS supports them. Currently Seabios for X86 system support it. limitation: The
      splash file could be a jpeg file or a BMP file in 24 BPP format(true color). The resolution should be supported
      by the SVGA mode, so the recommended is 320x240, 640x480, 800x640.




40                                                                                   Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5


      A timeout could be passed to bios, guest will pause for rb_timeout ms when boot failed, then reboot. If rb_timeout
      is ‘-1’, guest will not reboot, qemu passes ‘-1’ to bios by default. Currently Seabios for X86 system support it.
      Do strict boot via strict=on as far as firmware/BIOS supports it. This only effects when boot priority is changed
      by bootindex options. The default is non-strict boot.
      # try to boot from network first, then from hard disk
      qemu-system-x86_64 -boot order=nc
      # boot from CD-ROM first, switch back to default order after reboot
      qemu-system-x86_64 -boot once=d
      # boot with a splash picture for 5 seconds.
      qemu-system-x86_64 -boot menu=on,splash=/root/boot.bmp,splash-time=5000
      Note: The legacy format ‘-boot drives’ is still supported but its use is discouraged as it may be removed from
      future versions.
-m [size=]megs[,slots=n,maxmem=size]
     Sets guest startup RAM size to megs megabytes. Default is 128 MiB. Optionally, a suffix of “M” or “G” can be
     used to signify a value in megabytes or gigabytes respectively. Optional pair slots, maxmem could be used to set
     amount of hotpluggable memory slots and maximum amount of memory. Note that maxmem must be aligned
     to the page size.
      For example, the following command-line sets the guest startup RAM size to 1GB, creates 3 slots to hotplug
      additional memory and sets the maximum memory the guest can reach to 4GB:
      qemu-system-x86_64 -m 1G,slots=3,maxmem=4G
      If slots and maxmem are not specified, memory hotplug won’t be enabled and the guest startup RAM will never
      increase.
-mem-path path
     Allocate guest RAM from a temporarily created file in path.
-mem-prealloc
     Preallocate memory when using -mem-path.
-k language
     Use keyboard layout language (for example fr for French). This option is only needed where it is not easy to get
     raw PC keycodes (e.g. on Macs, with some X11 servers or with a VNC or curses display). You don’t normally
     need to use it on PC/Linux or PC/Windows hosts.
      The available layouts are:

      ar   de-ch    es    fo       fr-ca     hu   ja   mk        no   pt-br     sv
      da   en-gb    et    fr       fr-ch     is   lt   nl        pl   ru        th
      de   en-us    fi    fr-be    hr        it   lv   nl-be     pt   sl        tr

      The default is en-us.
-audio-help
     Will show the -audiodev equivalent of the currently specified (deprecated) environment variables.
-audio [driver=]driver,model=value[,prop[=value][,...]]
     This option is a shortcut for configuring both the guest audio hardware and the host audio backend in one go.
     The driver option is the same as with the corresponding -audiodev option below. The guest hardware model
     can be set with model=modelname.
      Use driver=help to list the available drivers, and model=help to list the available device types.
      The following two example do exactly the same, to show how -audio can be used to shorten the command line
      length:



2.2. Invocation                                                                                                      41
QEMU Documentation, Release 8.1.5


      qemu-system-x86_64 -audiodev pa,id=pa -device sb16,audiodev=pa
      qemu-system-x86_64 -audio pa,model=sb16
-audiodev [driver=]driver,id=id[,prop[=value][,...]]
     Adds a new audio backend driver identified by id. There are global and driver specific properties. Some values
     can be set differently for input and output, they’re marked with in|out.. You can set the input’s property with
     in.prop and the output’s property with out.prop. For example:

      -audiodev alsa,id=example,in.frequency=44110,out.frequency=8000
      -audiodev alsa,id=example,out.channels=1 # leaves in.channels unspecified

      NOTE: parameter validation is known to be incomplete, in many cases specifying an invalid option causes QEMU
      to print an error message and continue emulation without sound.
      Valid global options are:
      id=identifier
          Identifies the audio backend.
      timer-period=period
          Sets the timer period used by the audio subsystem in microseconds. Default is 10000 (10 ms).
      in|out.mixing-engine=on|off
          Use QEMU’s mixing engine to mix all streams inside QEMU and convert audio formats when not supported
          by the backend. When off, fixed-settings must be off too. Note that disabling this option means that the
          selected backend must support multiple streams and the audio formats used by the virtual cards, otherwise
          you’ll get no sound. It’s not recommended to disable this option unless you want to use 5.1 or 7.1 audio, as
          mixing engine only supports mono and stereo audio. Default is on.
      in|out.fixed-settings=on|off
          Use fixed settings for host audio. When off, it will change based on how the guest opens the sound card. In
          this case you must not specify frequency, channels or format. Default is on.
      in|out.frequency=frequency
          Specify the frequency to use when using fixed-settings. Default is 44100Hz.
      in|out.channels=channels
          Specify the number of channels to use when using fixed-settings. Default is 2 (stereo).
      in|out.format=format
          Specify the sample format to use when using fixed-settings. Valid values are: s8, s16, s32, u8, u16, u32,
          f32. Default is s16.
      in|out.voices=voices
          Specify the number of voices to use. Default is 1.
      in|out.buffer-length=usecs
          Sets the size of the buffer in microseconds.
-audiodev none,id=id[,prop[=value][,...]]
     Creates a dummy backend that discards all outputs. This backend has no backend specific properties.
-audiodev alsa,id=id[,prop[=value][,...]]
     Creates backend using the ALSA. This backend is only available on Linux.
      ALSA specific options are:
      in|out.dev=device
          Specify the ALSA device to use for input and/or output. Default is default.
      in|out.period-length=usecs
          Sets the period length in microseconds.


42                                                                                Chapter 2. System Emulation
                                                                        QEMU Documentation, Release 8.1.5


      in|out.try-poll=on|off
          Attempt to use poll mode with the device. Default is on.
      threshold=threshold
          Threshold (in microseconds) when playback starts. Default is 0.
-audiodev coreaudio,id=id[,prop[=value][,...]]
     Creates a backend using Apple’s Core Audio. This backend is only available on Mac OS and only supports
     playback.
      Core Audio specific options are:
      in|out.buffer-count=count
          Sets the count of the buffers.
-audiodev dsound,id=id[,prop[=value][,...]]
     Creates a backend using Microsoft’s DirectSound. This backend is only available on Windows and only supports
     playback.
      DirectSound specific options are:
      latency=usecs
          Add extra usecs microseconds latency to playback. Default is 10000 (10 ms).
-audiodev oss,id=id[,prop[=value][,...]]
     Creates a backend using OSS. This backend is available on most Unix-like systems.
      OSS specific options are:
      in|out.dev=device
          Specify the file name of the OSS device to use. Default is /dev/dsp.
      in|out.buffer-count=count
          Sets the count of the buffers.
      in|out.try-poll=on|of
          Attempt to use poll mode with the device. Default is on.
      try-mmap=on|off
          Try using memory mapped device access. Default is off.
      exclusive=on|off
          Open the device in exclusive mode (vmix won’t work in this case). Default is off.
      dsp-policy=policy
          Sets the timing policy (between 0 and 10, where smaller number means smaller latency but higher CPU
          usage). Use -1 to use buffer sizes specified by buffer and buffer-count. This option is ignored if you
          do not have OSS 4. Default is 5.
-audiodev pa,id=id[,prop[=value][,...]]
     Creates a backend using PulseAudio. This backend is available on most systems.
      PulseAudio specific options are:
      server=server
          Sets the PulseAudio server to connect to.
      in|out.name=sink
          Use the specified source/sink for recording/playback.
      in|out.latency=usecs
          Desired latency in microseconds. The PulseAudio server will try to honor this value but actual latencies
          may be lower or higher.



2.2. Invocation                                                                                                43
QEMU Documentation, Release 8.1.5


-audiodev pipewire,id=id[,prop[=value][,...]]
     Creates a backend using PipeWire. This backend is available on most systems.
      PipeWire specific options are:
      in|out.latency=usecs
          Desired latency in microseconds.
      in|out.name=sink
          Use the specified source/sink for recording/playback.
      in|out.stream-name
          Specify the name of pipewire stream.
-audiodev sdl,id=id[,prop[=value][,...]]
     Creates a backend using SDL. This backend is available on most systems, but you should use your platform’s
     native backend if possible.
      SDL specific options are:
      in|out.buffer-count=count
          Sets the count of the buffers.
-audiodev sndio,id=id[,prop[=value][,...]]
     Creates a backend using SNDIO. This backend is available on OpenBSD and most other Unix-like systems.
      Sndio specific options are:
      in|out.dev=device
          Specify the sndio device to use for input and/or output. Default is default.
      in|out.latency=usecs
          Sets the desired period length in microseconds.
-audiodev spice,id=id[,prop[=value][,...]]
     Creates a backend that sends audio through SPICE. This backend requires -spice and automatically selected in
     that case, so usually you can ignore this option. This backend has no backend specific properties.
-audiodev wav,id=id[,prop[=value][,...]]
     Creates a backend that writes audio to a WAV file.
      Backend specific options are:
      path=path
          Write recorded audio into the specified file. Default is qemu.wav.
-device driver[,prop[=value][,...]]
     Add device driver. prop=value sets driver properties. Valid properties depend on the driver. To get help on
     possible drivers and properties, use -device help and -device driver,help.
      Some drivers are:
-device ipmi-bmc-sim,id=id[,prop[=value][,...]]
     Add an IPMI BMC. This is a simulation of a hardware management interface processor that normally sits on a
     system. It provides a watchdog and the ability to reset and power control the system. You need to connect this
     to an IPMI interface to make it useful
      The IPMI slave address to use for the BMC. The default is 0x20. This address is the BMC’s address on the I2C
      network of management controllers. If you don’t know what this means, it is safe to ignore it.
      id=id
          The BMC id for interfaces to use this device.




44                                                                              Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


      slave_addr=val
          Define slave address to use for the BMC. The default is 0x20.
      sdrfile=file
          file containing raw Sensor Data Records (SDR) data. The default is none.
      fruareasize=val
          size of a Field Replaceable Unit (FRU) area. The default is 1024.
      frudatafile=file
          file containing raw Field Replaceable Unit (FRU) inventory data. The default is none.
      guid=uuid
          value for the GUID for the BMC, in standard UUID format. If this is set, get “Get GUID” command to the
          BMC will return it. Otherwise “Get GUID” will return an error.
-device ipmi-bmc-extern,id=id,chardev=id[,slave_addr=val]
     Add a connection to an external IPMI BMC simulator. Instead of locally emulating the BMC like the above item,
     instead connect to an external entity that provides the IPMI services.
      A connection is made to an external BMC simulator. If you do this, it is strongly recommended that you use the
      “reconnect=” chardev option to reconnect to the simulator if the connection is lost. Note that if this is not used
      carefully, it can be a security issue, as the interface has the ability to send resets, NMIs, and power off the VM.
      It’s best if QEMU makes a connection to an external simulator running on a secure port on localhost, so neither
      the simulator nor QEMU is exposed to any outside network.
      See the “lanserv/README.vm” file in the OpenIPMI library for more details on the external interface.
-device isa-ipmi-kcs,bmc=id[,ioport=val][,irq=val]
     Add a KCS IPMI interface on the ISA bus. This also adds a corresponding ACPI and SMBIOS entries, if
     appropriate.
      bmc=id
          The BMC to connect to, one of ipmi-bmc-sim or ipmi-bmc-extern above.
      ioport=val
          Define the I/O address of the interface. The default is 0xca0 for KCS.
      irq=val
          Define the interrupt to use. The default is 5. To disable interrupts, set this to 0.
-device isa-ipmi-bt,bmc=id[,ioport=val][,irq=val]
     Like the KCS interface, but defines a BT interface. The default port is 0xe4 and the default interrupt is 5.
-device pci-ipmi-kcs,bmc=id
     Add a KCS IPMI interface on the PCI bus.
      bmc=id
          The BMC to connect to, one of ipmi-bmc-sim or ipmi-bmc-extern above.
-device pci-ipmi-bt,bmc=id
     Like the KCS interface, but defines a BT interface on the PCI bus.
-device intel-iommu[,option=...]
     This is only supported by -machine q35, which will enable Intel VT-d emulation within the guest. It supports
     below options:
      intremap=on|off (default: auto)
          This enables interrupt remapping feature. It’s required to enable complete x2apic. Currently it only supports
          kvm kernel-irqchip modes off or split, while full kernel-irqchip is not yet supported. The default value
          is “auto”, which will be decided by the mode of kernel-irqchip.




2.2. Invocation                                                                                                       45
QEMU Documentation, Release 8.1.5


      caching-mode=on|off (default: off)
          This enables caching mode for the VT-d emulated device. When caching-mode is enabled, each guest DMA
          buffer mapping will generate an IOTLB invalidation from the guest IOMMU driver to the vIOMMU device
          in a synchronous way. It is required for -device vfio-pci to work with the VT-d device, because host
          assigned devices requires to setup the DMA mapping on the host before guest DMA starts.
      device-iotlb=on|off (default: off)
          This enables device-iotlb capability for the emulated VT-d device. So far virtio/vhost should be the only
          real user for this parameter, paired with ats=on configured for the device.
      aw-bits=39|48 (default: 39)
          This decides the address width of IOVA address space. The address space has 39 bits width for 3-level
          IOMMU page tables, and 48 bits for 4-level IOMMU page tables.
      Please also refer to the wiki page for general scenarios of VT-d emulation in QEMU: https://wiki.qemu.org/
      Features/VT-d.
-name name
     Sets the name of the guest. This name will be displayed in the SDL window caption. The name will also be used
     for the VNC server. Also optionally set the top visible process name in Linux. Naming of individual threads can
     also be enabled on Linux to aid debugging.
-uuid uuid
     Set system UUID.


2.2.2 Block device options

The QEMU block device handling options have a long history and have gone through several iterations as the feature
set and complexity of the block layer have grown. Many online guides to QEMU often reference older and deprecated
options, which can lead to confusion.
The most explicit way to describe disks is to use a combination of -device to specify the hardware device and
-blockdev to describe the backend. The device defines what the guest sees and the backend describes how QEMU
handles the data. It is the only guaranteed stable interface for describing block devices and as such is recommended for
management tools and scripting.
The -drive option combines the device and backend into a single command line option which is a more human friendly.
There is however no interface stability guarantee although some older board models still need updating to work with
the modern blockdev forms.
Older options like -hda are essentially macros which expand into -drive options for various drive interfaces. The
original forms bake in a lot of assumptions from the days when QEMU was emulating a legacy PC, they are not
recommended for modern configurations.
-fda file
-fdb file
     Use file as floppy disk 0/1 image (see the Disk Images chapter in the System Emulation Users Guide).
-hda file
-hdb file
-hdc file
-hdd file
     Use file as hard disk 0, 1, 2 or 3 image on the default bus of the emulated machine (this is for example the IDE
     bus on most x86 machines, but it can also be SCSI, virtio or something else on other target architectures). See
     also the Disk Images chapter in the System Emulation Users Guide.



46                                                                                 Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


-cdrom file
     Use file as CD-ROM image on the default bus of the emulated machine (which is IDE1 master on x86, so you
     cannot use -hdc and -cdrom at the same time there). On systems that support it, you can use the host CD-ROM
     by using /dev/cdrom as filename.
-blockdev option[,option[,option[,...]]]
     Define a new block driver node. Some of the options apply to all block drivers, other options are only accepted
     for a specific block driver. See below for a list of generic options and options for the most common block drivers.
      Options that expect a reference to another node (e.g. file) can be given in two ways. Either you specify the
      node name of an already existing node (file=node-name), or you define a new node inline, adding options for the
      referenced node after a dot (file.filename=path,file.aio=native).
      A block driver node created with -blockdev can be used for a guest device by specifying its node name for the
      drive property in a -device argument that defines a block device.
      Valid options for any block driver node:
            driver
                Specifies the block driver to use for the given node.
            node-name
                This defines the name of the block driver node by which it will be referenced later. The name must be
                unique, i.e. it must not match the name of a different block driver node, or (if you use -drive as well)
                the ID of a drive.
                If no node name is specified, it is automatically generated. The generated node name is not intended
                to be predictable and changes between QEMU invocations. For the top level, an explicit node name
                must be specified.
            read-only
                Open the node read-only. Guest write attempts will fail.
                Note that some block drivers support only read-only access, either generally or in certain configura-
                tions. In this case, the default value read-only=off does not work and the option must be specified
                explicitly.
            auto-read-only
                If auto-read-only=on is set, QEMU may fall back to read-only usage even when read-only=off
                is requested, or even switch between modes as needed, e.g. depending on whether the image file is
                writable or whether a writing user is attached to the node.
            force-share
                Override the image locking system of QEMU by forcing the node to utilize weaker shared access
                for permissions where it would normally request exclusive access. When there is the potential for
                multiple instances to have the same file open (whether this invocation of QEMU is the first or the second
                instance), both instances must permit shared access for the second instance to succeed at opening the
                file.
                Enabling force-share=on requires read-only=on.
            cache.direct
                The host page cache can be avoided with cache.direct=on. This will attempt to do disk IO directly
                to the guest’s memory. QEMU may still perform an internal copy of the data.
            cache.no-flush
                In case you don’t care about data integrity over host failures, you can use cache.no-flush=on. This
                option tells QEMU that it never needs to write any data to the disk but can instead keep things in cache.
                If anything goes wrong, like your host losing power, the disk storage getting disconnected accidentally,
                etc. your image will most probably be rendered unusable.



2.2. Invocation                                                                                                       47
QEMU Documentation, Release 8.1.5


          discard=discard
              discard is one of “ignore” (or “off”) or “unmap” (or “on”) and controls whether discard (also known
              as trim or unmap) requests are ignored or passed to the filesystem. Some machine types may not
              support discard requests.
          detect-zeroes=detect-zeroes
              detect-zeroes is “off”, “on” or “unmap” and enables the automatic conversion of plain zero writes by
              the OS to driver specific optimized zero write commands. You may even choose “unmap” if discard
              is set to “unmap” to allow a zero write to be converted to an unmap operation.
     Driver-specific options for file
         This is the protocol-level block driver for accessing regular files.
          filename
              The path to the image file in the local filesystem
          aio
                Specifies the AIO backend (threads/native/io_uring, default: threads)
          locking
              Specifies whether the image file is protected with Linux OFD / POSIX locks. The default is to use the
              Linux Open File Descriptor API if available, otherwise no lock is applied. (auto/on/off, default: auto)
          Example:

          -blockdev driver=file,node-name=disk,filename=disk.img

     Driver-specific options for raw
         This is the image format block driver for raw images. It is usually stacked on top of a protocol level block
         driver such as file.
          file
              Reference to or definition of the data source block driver node (e.g. a file driver node)
          Example 1:

          -blockdev driver=file,node-name=disk_file,filename=disk.img
          -blockdev driver=raw,node-name=disk,file=disk_file

          Example 2:

          -blockdev driver=raw,node-name=disk,file.driver=file,file.filename=disk.img

     Driver-specific options for qcow2
         This is the image format block driver for qcow2 images. It is usually stacked on top of a protocol level
         block driver such as file.
          file
              Reference to or definition of the data source block driver node (e.g. a file driver node)
          backing
              Reference to or definition of the backing file block device (default is taken from the image file). It is
              allowed to pass null here in order to disable the default backing file.
          lazy-refcounts
              Whether to enable the lazy refcounts feature (on/off; default is taken from the image file)
          cache-size
              The maximum total size of the L2 table and refcount block caches in bytes (default: the sum of l2-
              cache-size and refcount-cache-size)



48                                                                                Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


           l2-cache-size
               The maximum size of the L2 table cache in bytes (default: if cache-size is not specified - 32M on Linux
               platforms, and 8M on non-Linux platforms; otherwise, as large as possible within the cache-size, while
               permitting the requested or the minimal refcount cache size)
           refcount-cache-size
               The maximum size of the refcount block cache in bytes (default: 4 times the cluster size; or if cache-
               size is specified, the part of it which is not used for the L2 cache)
           cache-clean-interval
               Clean unused entries in the L2 and refcount caches. The interval is in seconds. The default value is
               600 on supporting platforms, and 0 on other platforms. Setting it to 0 disables this feature.
           pass-discard-request
               Whether discard requests to the qcow2 device should be forwarded to the data source (on/off; default:
               on if discard=unmap is specified, off otherwise)
           pass-discard-snapshot
               Whether discard requests for the data source should be issued when a snapshot operation (e.g. deleting
               a snapshot) frees clusters in the qcow2 file (on/off; default: on)
           pass-discard-other
               Whether discard requests for the data source should be issued on other occasions where a cluster gets
               freed (on/off; default: off)
           discard-no-unref
               When enabled, data clusters will remain preallocated when they are no longer used, e.g. because they
               are discarded or converted to zero clusters. As usual, whether the old data is discarded or kept on the
               protocol level (i.e. in the image file) depends on the setting of the pass-discard-request option. Keeping
               the clusters preallocated prevents qcow2 fragmentation that would otherwise be caused by freeing and
               re-allocating them later. Besides potential performance degradation, such fragmentation can lead to
               increased allocation of clusters past the end of the image file, resulting in image files whose file length
               can grow much larger than their guest disk size would suggest. If image file length is of concern (e.g.
               when storing qcow2 images directly on block devices), you should consider enabling this option.
           overlap-check
               Which overlap checks to perform for writes to the image (none/constant/cached/all; default: cached).
               For details or finer granularity control refer to the QAPI documentation of blockdev-add.
           Example 1:

           -blockdev driver=file,node-name=my_file,filename=/tmp/disk.qcow2
           -blockdev driver=qcow2,node-name=hda,file=my_file,overlap-check=none,cache-
            ˓→size=16777216




           Example 2:

           -blockdev driver=qcow2,node-name=disk,file.driver=http,file.filename=http://
            ˓→example.com/image.qcow2




      Driver-specific options for other drivers
          Please refer to the QAPI documentation of the blockdev-add QMP command.
-drive option[,option[,option[,...]]]
     Define a new drive. This includes creating a block driver node (the backend) as well as a guest device, and is
     mostly a shortcut for defining the corresponding -blockdev and -device options.
      -drive accepts all options that are accepted by -blockdev. In addition, it knows the following options:




2.2. Invocation                                                                                                        49
QEMU Documentation, Release 8.1.5


     file=file
         This option defines which disk image (see the Disk Images chapter in the System Emulation Users Guide)
         to use with this drive. If the filename contains comma, you must double it (for instance, “file=my„file” to
         use file “my,file”).
          Special files such as iSCSI devices can be specified using protocol specific URLs. See the section for
          “Device URL Syntax” for more information.
     if=interface
         This option defines on which type on interface the drive is connected. Available types are: ide, scsi, sd,
         mtd, floppy, pflash, virtio, none.
     bus=bus,unit=unit
         These options define where is connected the drive by defining the bus number and the unit id.
     index=index
         This option defines where the drive is connected by using an index in the list of available connectors of a
         given interface type.
     media=media
         This option defines the type of the media: disk or cdrom.
     snapshot=snapshot
         snapshot is “on” or “off” and controls snapshot mode for the given drive (see -snapshot).
     cache=cache
         cache is “none”, “writeback”, “unsafe”, “directsync” or “writethrough” and controls how the host cache is
         used to access block data. This is a shortcut that sets the cache.direct and cache.no-flush options
         (as in -blockdev), and additionally cache.writeback, which provides a default for the write-cache
         option of block guest devices (as in -device). The modes correspond to the following settings:

                                            cache.writeback      cache.direct     cache.no-flush

                            writeback       on                   off              off
                            none            on                   on               off
                            writethrough    off                  off              off
                            directsync      off                  on               off
                            unsafe          on                   off              on

          The default mode is cache=writeback.
     aio=aio
         aio is “threads”, “native”, or “io_uring” and selects between pthread based disk I/O, native Linux AIO, or
         Linux io_uring API.
     format=format
         Specify which disk format will be used rather than detecting the format. Can be used to specify format=raw
         to avoid interpreting an untrusted format header.
     werror=action,rerror=action
         Specify which action to take on write and read errors. Valid actions are: “ignore” (ignore the error and try
         to continue), “stop” (pause QEMU), “report” (report the error to the guest), “enospc” (pause QEMU only
         if the host disk is full; report the error to the guest otherwise). The default setting is werror=enospc and
         rerror=report.
     copy-on-read=copy-on-read
         copy-on-read is “on” or “off” and enables whether to copy read backing file sectors into the image file.




50                                                                               Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5


     bps=b,bps_rd=r,bps_wr=w
         Specify bandwidth throttling limits in bytes per second, either for all request types or for reads or writes
         only. Small values can lead to timeouts or hangs inside the guest. A safe minimum for disks is 2 MB/s.
     bps_max=bm,bps_rd_max=rm,bps_wr_max=wm
         Specify bursts in bytes per second, either for all request types or for reads or writes only. Bursts allow the
         guest I/O to spike above the limit temporarily.
     iops=i,iops_rd=r,iops_wr=w
         Specify request rate limits in requests per second, either for all request types or for reads or writes only.
     iops_max=bm,iops_rd_max=rm,iops_wr_max=wm
         Specify bursts in requests per second, either for all request types or for reads or writes only. Bursts allow
         the guest I/O to spike above the limit temporarily.
     iops_size=is
         Let every is bytes of a request count as a new request for iops throttling purposes. Use this option to prevent
         guests from circumventing iops limits by sending fewer but larger requests.
     group=g
         Join a throttling quota group with given name g. All drives that are members of the same group are ac-
         counted for together. Use this option to prevent guests from circumventing throttling limits by using many
         small disks instead of a single larger disk.
     By default, the cache.writeback=on mode is used. It will report data writes as completed as soon as the data
     is present in the host page cache. This is safe as long as your guest OS makes sure to correctly flush disk caches
     where needed. If your guest OS does not handle volatile disk write caches correctly and your host crashes or
     loses power, then the guest may experience data corruption.
     For such guests, you should consider using cache.writeback=off. This means that the host page cache will
     be used to read and write data, but write notification will be sent to the guest only after QEMU has made sure to
     flush each write to the disk. Be aware that this has a major impact on performance.
     When using the -snapshot option, unsafe caching is always used.
     Copy-on-read avoids accessing the same backing file sectors repeatedly and is useful when the backing file is
     over a slow network. By default copy-on-read is off.
     Instead of -cdrom you can use:
     qemu-system-x86_64 -drive file=file,index=2,media=cdrom
     Instead of -hda, -hdb, -hdc, -hdd, you can use:
     qemu-system-x86_64 -drive file=file,index=0,media=disk
     qemu-system-x86_64 -drive file=file,index=1,media=disk
     qemu-system-x86_64 -drive file=file,index=2,media=disk
     qemu-system-x86_64 -drive file=file,index=3,media=disk
     You can open an image using pre-opened file descriptors from an fd set:
     qemu-system-x86_64 \
      -add-fd fd=3,set=2,opaque="rdwr:/path/to/file" \
      -add-fd fd=4,set=2,opaque="rdonly:/path/to/file" \
      -drive file=/dev/fdset/2,index=0,media=disk
     You can connect a CDROM to the slave of ide0:
     qemu-system-x86_64 -drive file=file,if=ide,index=1,media=cdrom
     If you don’t specify the “file=” argument, you define an empty drive:
     qemu-system-x86_64 -drive if=ide,index=1,media=cdrom



2.2. Invocation                                                                                                      51
QEMU Documentation, Release 8.1.5


      Instead of -fda, -fdb, you can use:
      qemu-system-x86_64 -drive file=file,index=0,if=floppy
      qemu-system-x86_64 -drive file=file,index=1,if=floppy
      By default, interface is “ide” and index is automatically incremented:
      qemu-system-x86_64 -drive file=a -drive file=b
      is interpreted like:
      qemu-system-x86_64 -hda a -hdb b
-mtdblock file
     Use file as on-board Flash memory image.
-sd file
     Use file as SecureDigital card image.
-snapshot
     Write to temporary files instead of disk image files. In this case, the raw disk image you use is not written back.
     You can however force the write back by pressing C-a s (see the Disk Images chapter in the System Emulation
     Users Guide).


        Warning: snapshot is incompatible with -blockdev (instead use qemu-img to manually create snapshot
        images to attach to your blockdev). If you have mixed -blockdev and -drive declarations you can use the
        ‘snapshot’ property on your drive declarations instead of this global option.


-fsdev local,id=id,path=path,security_model=security_model
[,writeout=writeout][,readonly=on][,fmode=fmode][,dmode=dmode]
[,throttling.option=value[,throttling.option=value[,...]]]
-fsdev proxy,id=id,socket=socket[,writeout=writeout][,readonly=on]
-fsdev proxy,id=id,sock_fd=sock_fd[,writeout=writeout][,readonly=on]
-fsdev synth,id=id[,readonly=on]
     Define a new file system device. Valid options are:
      local
          Accesses to the filesystem are done by QEMU.
      proxy
          Accesses to the filesystem are done by virtfs-proxy-helper(1). This option is deprecated (since QEMU 8.1)
          and will be removed in a future version of QEMU. Use local instead.
      synth
          Synthetic filesystem, only used by QTests.
      id=id
          Specifies identifier for this device.
      path=path
          Specifies the export path for the file system device. Files under this path will be available to the 9p client
          on the guest.
      security_model=security_model
          Specifies the security model to be used for this export path. Supported security models are “passthrough”,
          “mapped-xattr”, “mapped-file” and “none”. In “passthrough” security model, files are stored using the
          same credentials as they are created on the guest. This requires QEMU to run as root. In “mapped-xattr”
          security model, some of the file attributes like uid, gid, mode bits and link target are stored as file attributes.


52                                                                                     Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5


            For “mapped-file” these attributes are stored in the hidden .virtfs_metadata directory. Directories exported
            by this security model cannot interact with other unix tools. “none” security model is same as passthrough
            except the sever won’t report failures if it fails to set file attributes like ownership. Security model is
            mandatory only for local fsdriver. Other fsdrivers (like proxy) don’t take security model as a parameter.
     writeout=writeout
         This is an optional argument. The only supported value is “immediate”. This means that host page cache
         will be used to read and write data but write notification will be sent to the guest only when the data has
         been reported as written by the storage subsystem.
     readonly=on
         Enables exporting 9p share as a readonly mount for guests. By default read-write access is given.
     socket=socket
         Enables proxy filesystem driver to use passed socket file for communicating with virtfs-proxy-helper(1).
     sock_fd=sock_fd
         Enables proxy filesystem driver to use passed socket descriptor for communicating with virtfs-proxy-
         helper(1). Usually a helper like libvirt will create socketpair and pass one of the fds as sock_fd.
     fmode=fmode
         Specifies the default mode for newly created files on the host. Works only with security models “mapped-
         xattr” and “mapped-file”.
     dmode=dmode
         Specifies the default mode for newly created directories on the host. Works only with security models
         “mapped-xattr” and “mapped-file”.
     throttling.bps-total=b,throttling.bps-read=r,throttling.bps-write=w
         Specify bandwidth throttling limits in bytes per second, either for all request types or for reads or writes
         only.
     throttling.bps-total-max=bm,bps-read-max=rm,bps-write-max=wm
         Specify bursts in bytes per second, either for all request types or for reads or writes only. Bursts allow the
         guest I/O to spike above the limit temporarily.
     throttling.iops-total=i,throttling.iops-read=r, throttling.iops-write=w
         Specify request rate limits in requests per second, either for all request types or for reads or writes only.
     throttling.iops-total-max=im,throttling.iops-read-max=irm,
     throttling.iops-write-max=iwm
         Specify bursts in requests per second, either for all request types or for reads or writes only. Bursts allow
         the guest I/O to spike above the limit temporarily.
     throttling.iops-size=is
         Let every is bytes of a request count as a new request for iops throttling purposes.
     -fsdev option is used along with -device driver “virtio-9p-. . . ”.
-device virtio-9p-type,fsdev=id,mount_tag=mount_tag
     Options for virtio-9p-. . . driver are:
     type
            Specifies the variant to be used. Supported values are “pci”, “ccw” or “device”, depending on the machine
            type.
     fsdev=id
         Specifies the id value specified along with -fsdev option.
     mount_tag=mount_tag
         Specifies the tag name to be used by the guest to mount this export point.



2.2. Invocation                                                                                                      53
QEMU Documentation, Release 8.1.5


-virtfs local,path=path,mount_tag=mount_tag
,security_model=security_model[,writeout=writeout][,readonly=on]
[,fmode=fmode][,dmode=dmode][,multidevs=multidevs]
-virtfs proxy,socket=socket,mount_tag=mount_tag [,writeout=writeout][,readonly=on]
-virtfs proxy,sock_fd=sock_fd,mount_tag=mount_tag [,writeout=writeout][,readonly=on]
-virtfs synth,mount_tag=mount_tag
     Define a new virtual filesystem device and expose it to the guest using a virtio-9p-device (a.k.a. 9pfs), which
     essentially means that a certain directory on host is made directly accessible by guest as a pass-through file system
     by using the 9P network protocol for communication between host and guests, if desired even accessible, shared
     by several guests simultaneously.
      Note that -virtfs is actually just a convenience shortcut for its generalized form -fsdev -device
      virtio-9p-pci.
      The general form of pass-through file system options are:
      local
          Accesses to the filesystem are done by QEMU.
      proxy
          Accesses to the filesystem are done by virtfs-proxy-helper(1). This option is deprecated (since QEMU 8.1)
          and will be removed in a future version of QEMU. Use local instead.
      synth
          Synthetic filesystem, only used by QTests.
      id=id
          Specifies identifier for the filesystem device
      path=path
          Specifies the export path for the file system device. Files under this path will be available to the 9p client
          on the guest.
      security_model=security_model
          Specifies the security model to be used for this export path. Supported security models are “passthrough”,
          “mapped-xattr”, “mapped-file” and “none”. In “passthrough” security model, files are stored using the
          same credentials as they are created on the guest. This requires QEMU to run as root. In “mapped-xattr”
          security model, some of the file attributes like uid, gid, mode bits and link target are stored as file attributes.
          For “mapped-file” these attributes are stored in the hidden .virtfs_metadata directory. Directories exported
          by this security model cannot interact with other unix tools. “none” security model is same as passthrough
          except the sever won’t report failures if it fails to set file attributes like ownership. Security model is
          mandatory only for local fsdriver. Other fsdrivers (like proxy) don’t take security model as a parameter.
      writeout=writeout
          This is an optional argument. The only supported value is “immediate”. This means that host page cache
          will be used to read and write data but write notification will be sent to the guest only when the data has
          been reported as written by the storage subsystem.
      readonly=on
          Enables exporting 9p share as a readonly mount for guests. By default read-write access is given.
      socket=socket
          Enables proxy filesystem driver to use passed socket file for communicating with virtfs-proxy-helper(1).
          Usually a helper like libvirt will create socketpair and pass one of the fds as sock_fd.
      sock_fd
          Enables proxy filesystem driver to use passed ‘sock_fd’ as the socket descriptor for interfacing with virtfs-
          proxy-helper(1).



54                                                                                     Chapter 2. System Emulation
                                                                              QEMU Documentation, Release 8.1.5


       fmode=fmode
           Specifies the default mode for newly created files on the host. Works only with security models “mapped-
           xattr” and “mapped-file”.
       dmode=dmode
           Specifies the default mode for newly created directories on the host. Works only with security models
           “mapped-xattr” and “mapped-file”.
       mount_tag=mount_tag
           Specifies the tag name to be used by the guest to mount this export point.
       multidevs=multidevs
           Specifies how to deal with multiple devices being shared with a 9p export. Supported behaviours are
           either “remap”, “forbid” or “warn”. The latter is the default behaviour on which virtfs 9p expects only one
           device to be shared with the same export, and if more than one device is shared and accessed via the same
           9p export then only a warning message is logged (once) by qemu on host side. In order to avoid file ID
           collisions on guest you should either create a separate virtfs export for each device to be shared with guests
           (recommended way) or you might use “remap” instead which allows you to share multiple devices with only
           one export instead, which is achieved by remapping the original inode numbers from host to guest in a way
           that would prevent such collisions. Remapping inodes in such use cases is required because the original
           device IDs from host are never passed and exposed on guest. Instead all files of an export shared with
           virtfs always share the same device id on guest. So two files with identical inode numbers but from actually
           different devices on host would otherwise cause a file ID collision and hence potential misbehaviours on
           guest. “forbid” on the other hand assumes like “warn” that only one device is shared by the same export,
           however it will not only log a warning message but also deny access to additional devices on guest. Note
           though that “forbid” does currently not block all possible file access operations (e.g. readdir() would still
           return entries from other devices).
-iscsi
     Configure iSCSI session parameters.


2.2.3 USB convenience options

-usb
       Enable USB emulation on machine types with an on-board USB host controller (if not enabled by default). Note
       that on-board USB host controllers may not support USB 3.0. In this case -device qemu-xhci can be used
       instead on machines with PCI.
-usbdevice devname
     Add the USB device devname, and enable an on-board USB controller if possible and necessary (just like it
     can be done via -machine usb=on). Note that this option is mainly intended for the user’s convenience only.
     More fine-grained control can be achieved by selecting a USB host controller (if necessary) and the desired USB
     device via the -device option instead. For example, instead of using -usbdevice mouse it is possible to use
     -device qemu-xhci -device usb-mouse to connect the USB mouse to a USB 3.0 controller instead (at least
     on machines that support PCI and do not have an USB controller enabled by default yet). For more details, see
     the chapter about Connecting USB devices in the System Emulation Users Guide. Possible devices for devname
     are:
       braille
           Braille device. This will use BrlAPI to display the braille output on a real or fake device (i.e. it also creates
           a corresponding braille chardev automatically beside the usb-braille USB device).
       keyboard
           Standard USB keyboard. Will override the PS/2 keyboard (if present).
       mouse
           Virtual Mouse. This will override the PS/2 mouse emulation when activated.


2.2. Invocation                                                                                                          55
QEMU Documentation, Release 8.1.5


      tablet
          Pointer device that uses absolute coordinates (like a touchscreen). This means QEMU is able to report
          the mouse position without having to grab the mouse. Also overrides the PS/2 mouse emulation when
          activated.
      wacom-tablet
          Wacom PenPartner USB tablet.


2.2.4 Display options

-display type
     Select type of display to use. Use -display help to list the available display types. Valid values for type are
      spice-app[,gl=on|off]
          Start QEMU as a Spice server and launch the default Spice client application. The Spice server will redirect
          the serial consoles and QEMU monitors. (Since 4.0)
      dbus
             Export the display over D-Bus interfaces. (Since 7.0)
             The connection is registered with the “org.qemu” name (and queued when already owned).
             addr=<dbusaddr> : D-Bus bus address to connect to.
             p2p=yes|no : Use peer-to-peer connection, accepted via QMP add_client.
             gl=on|off|core|es : Use OpenGL for rendering (the D-Bus interface will share framebuffers with
             DMABUF file descriptors).
      sdl
             Display video output via SDL (usually in a separate graphics window; see the SDL documentation for other
             possibilities). Valid parameters are:
             grab-mod=<mods> : Used to select the modifier keys for toggling the mouse grabbing in conjunction with
             the “g” key. <mods> can be either lshift-lctrl-lalt or rctrl.
             gl=on|off|core|es : Use OpenGL for displaying
             show-cursor=on|off : Force showing the mouse cursor
             window-close=on|off : Allow to quit qemu with window close button
      gtk
             Display video output in a GTK window. This interface provides drop-down menus and other UI elements
             to configure and control the VM during runtime. Valid parameters are:
             full-screen=on|off : Start in fullscreen mode
             gl=on|off : Use OpenGL for displaying
             grab-on-hover=on|off : Grab keyboard input on mouse hover
             show-tabs=on|off
                 [Display the tab bar for switching between the] various graphical interfaces (e.g. VGA and virtual
                 console character devices) by default.
             show-cursor=on|off : Force showing the mouse cursor
             window-close=on|off : Allow to quit qemu with window close button
             show-menubar=on|off : Display the main window menubar, defaults to “on”




56                                                                                Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5


             zoom-to-fit=on|off
                 [Expand video output to the window size,] defaults to “off”
      curses[,charset=<encoding>]
          Display video output via curses. For graphics device models which support a text mode, QEMU can display
          this output using a curses/ncurses interface. Nothing is displayed when the graphics device is in graphical
          mode or if the graphics device does not support a text mode. Generally only the VGA device models support
          text mode. The font charset used by the guest can be specified with the charset option, for example
          charset=CP850 for IBM CP850 encoding. The default is CP437.
      cocoa
          Display video output in a Cocoa window. Mac only. This interface provides drop-down menus and other
          UI elements to configure and control the VM during runtime. Valid parameters are:
             show-cursor=on|off : Force showing the mouse cursor
             left-command-key=on|off : Disable forwarding left command key to host
      egl-headless[,rendernode=<file>]
          Offload all OpenGL operations to a local DRI device. For any graphical display, this display needs to be
          paired with either VNC or SPICE displays.
      vnc=<display>
          Start a VNC server on display <display>
      none
             Do not display video output. The guest will still see an emulated graphics card, but its output will not be
             displayed to the QEMU user. This option differs from the -nographic option in that it only affects what is
             done with video output; -nographic also changes the destination of the serial and parallel port data.
-nographic
     Normally, if QEMU is compiled with graphical window support, it displays output such as guest graphics, guest
     console, and the QEMU monitor in a window. With this option, you can totally disable graphical output so that
     QEMU is a simple command line application. The emulated serial port is redirected on the console and muxed
     with the monitor (unless redirected elsewhere explicitly). Therefore, you can still use QEMU to debug a Linux
     kernel with a serial console. Use C-a h for help on switching between the console and monitor.
-spice option[,option[,...]]
     Enable the spice remote desktop protocol. Valid options are
      port=<nr>
          Set the TCP port spice is listening on for plaintext channels.
      addr=<addr>
          Set the IP address spice is listening on. Default is any address.
      ipv4=on|off; ipv6=on|off; unix=on|off
          Force using the specified IP version.
      password-secret=<secret-id>
          Set the ID of the secret object containing the password you need to authenticate.
      sasl=on|off
          Require that the client use SASL to authenticate with the spice. The exact choice of authentication method
          used is controlled from the system / user’s SASL configuration file for the ‘qemu’ service. This is typi-
          cally found in /etc/sasl2/qemu.conf. If running QEMU as an unprivileged user, an environment variable
          SASL_CONF_PATH can be used to make it search alternate locations for the service config. While some
          SASL auth methods can also provide data encryption (eg GSSAPI), it is recommended that SASL always
          be combined with the ‘tls’ and ‘x509’ settings to enable use of SSL and server certificates. This ensures a
          data encryption preventing compromise of authentication credentials.



2.2. Invocation                                                                                                      57
QEMU Documentation, Release 8.1.5


      disable-ticketing=on|off
          Allow client connects without authentication.
      disable-copy-paste=on|off
          Disable copy paste between the client and the guest.
      disable-agent-file-xfer=on|off
          Disable spice-vdagent based file-xfer between the client and the guest.
      tls-port=<nr>
          Set the TCP port spice is listening on for encrypted channels.
      x509-dir=<dir>
          Set the x509 file directory. Expects same filenames as -vnc $display,x509=$dir
      x509-key-file=<file>; x509-key-password=<file>; x509-cert-file=<file>;
      x509-cacert-file=<file>; x509-dh-key-file=<file>
          The x509 file names can also be configured individually.
      tls-ciphers=<list>
          Specify which ciphers to use.
      tls-channel=[main|display|cursor|inputs|record|playback];
      plaintext-channel=[main|display|cursor|inputs|record|playback]
          Force specific channel to be used with or without TLS encryption. The options can be specified multiple
          times to configure multiple channels. The special name “default” can be used to set the default mode. For
          channels which are not explicitly forced into one mode the spice client is allowed to pick tls/plaintext as he
          pleases.
      image-compression=[auto_glz|auto_lz|quic|glz|lz|off]
          Configure image compression (lossless). Default is auto_glz.
      jpeg-wan-compression=[auto|never|always];
      zlib-glz-wan-compression=[auto|never|always]
          Configure wan image compression (lossy for slow links). Default is auto.
      streaming-video=[off|all|filter]
          Configure video stream detection. Default is off.
      agent-mouse=[on|off]
          Enable/disable passing mouse events via vdagent. Default is on.
      playback-compression=[on|off]
          Enable/disable audio stream compression (using celt 0.5.1). Default is on.
      seamless-migration=[on|off]
          Enable/disable spice seamless migration. Default is off.
      gl=[on|off]
          Enable/disable OpenGL context. Default is off.
      rendernode=<file>
          DRM render node for OpenGL rendering. If not specified, it will pick the first available. (Since 2.9)
-portrait
     Rotate graphical output 90 deg left (only PXA LCD).
-rotate deg
     Rotate graphical output some deg left (only PXA LCD).
-vga type
     Select type of VGA card to emulate. Valid values for type are




58                                                                                  Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5


      cirrus
          Cirrus Logic GD5446 Video card. All Windows versions starting from Windows 95 should recognize and
          use this graphic card. For optimal performances, use 16 bit color depth in the guest and the host OS. (This
          card was the default before QEMU 2.2)
      std
             Standard VGA card with Bochs VBE extensions. If your guest OS supports the VESA 2.0 VBE extensions
             (e.g. Windows XP) and if you want to use high resolution modes (>= 1280x1024x16) then you should use
             this option. (This card is the default since QEMU 2.2)
      vmware
          VMWare SVGA-II compatible adapter. Use it if you have sufficiently recent XFree86/XOrg server or Win-
          dows guest with a driver for this card.
      qxl
             QXL paravirtual graphic card. It is VGA compatible (including VESA 2.0 VBE support). Works best with
             qxl guest drivers installed though. Recommended choice when using the spice protocol.
      tcx
             (sun4m only) Sun TCX framebuffer. This is the default framebuffer for sun4m machines and offers both
             8-bit and 24-bit colour depths at a fixed resolution of 1024x768.
      cg3
             (sun4m only) Sun cgthree framebuffer. This is a simple 8-bit framebuffer for sun4m machines available in
             both 1024x768 (OpenBIOS) and 1152x900 (OBP) resolutions aimed at people wishing to run older Solaris
             versions.
      virtio
          Virtio VGA card.
      none
             Disable VGA card.
-full-screen
     Start in full screen.
-g widthxheight[xdepth]
      Set the initial graphical resolution and depth (PPC, SPARC only).
      For PPC the default is 800x600x32.
      For SPARC with the TCX graphics device, the default is 1024x768x8 with the option of 1024x768x24. For
      cgthree, the default is 1024x768x8 with the option of 1152x900x8 for people who wish to use OBP.
-vnc display[,option[,option[,...]]]
     Normally, if QEMU is compiled with graphical window support, it displays output such as guest graphics, guest
     console, and the QEMU monitor in a window. With this option, you can have QEMU listen on VNC display
     display and redirect the VGA display over the VNC session. It is very useful to enable the usb tablet device when
     using this option (option -device usb-tablet). When using the VNC display, you must use the -k parameter
     to set the keyboard layout if you are not using en-us. Valid syntax for the display is
      to=L
             With this option, QEMU will try next available VNC displays, until the number L, if the origianlly defined
             “-vnc display” is not available, e.g. port 5900+display is already used by another application. By default,
             to=0.
      host:d
          TCP connections will only be allowed from host on display d. By convention the TCP port is 5900+d.
          Optionally, host can be omitted in which case the server will accept connections from any host.




2.2. Invocation                                                                                                      59
QEMU Documentation, Release 8.1.5


     unix:path
         Connections will be allowed over UNIX domain sockets where path is the location of a unix socket to listen
         for connections on.
     none
            VNC is initialized but not started. The monitor change command can be used to later start the VNC server.
     Following the display value there may be one or more option flags separated by commas. Valid options are
     reverse=on|off
         Connect to a listening VNC client via a “reverse” connection. The client is specified by the display. For
         reverse network connections (host:d,``reverse``), the d argument is a TCP port number, not a display num-
         ber.
     websocket=on|off
         Opens an additional TCP listening port dedicated to VNC Websocket connections. If a bare websocket
         option is given, the Websocket port is 5700+display. An alternative port can be specified with the syntax
         websocket=port.
            If host is specified connections will only be allowed from this host. It is possible to control the websocket
            listen address independently, using the syntax websocket=host:port.
            If no TLS credentials are provided, the websocket connection runs in unencrypted mode. If TLS credentials
            are provided, the websocket connection requires encrypted client connections.
     password=on|off
         Require that password based authentication is used for client connections.
            The password must be set separately using the set_password command in the QEMU Monitor. The
            syntax to change your password is: set_password <protocol> <password> where <protocol> could
            be either “vnc” or “spice”.
            If you would like to change <protocol> password expiration, you should use expire_password
            <protocol> <expiration-time> where expiration time could be one of the following options: now,
            never, +seconds or UNIX time of expiration, e.g. +60 to make password expire in 60 seconds, or
            1335196800 to make password expire on “Mon Apr 23 12:00:00 EDT 2012” (UNIX time for this date
            and time).
            You can also use keywords “now” or “never” for the expiration time to allow <protocol> password to expire
            immediately or never expire.
     password-secret=<secret-id>
         Require that password based authentication is used for client connections, using the password provided by
         the secret object identified by secret-id.
     tls-creds=ID
         Provides the ID of a set of TLS credentials to use to secure the VNC server. They will apply to both the
         normal VNC server socket and the websocket socket (if enabled). Setting TLS credentials will cause the
         VNC server socket to enable the VeNCrypt auth mechanism. The credentials should have been previously
         created using the -object tls-creds argument.
     tls-authz=ID
         Provides the ID of the QAuthZ authorization object against which the client’s x509 distinguished name will
         validated. This object is only resolved at time of use, so can be deleted and recreated on the fly while the
         VNC server is active. If missing, it will default to denying access.
     sasl=on|off
         Require that the client use SASL to authenticate with the VNC server. The exact choice of authentication
         method used is controlled from the system / user’s SASL configuration file for the ‘qemu’ service. This is
         typically found in /etc/sasl2/qemu.conf. If running QEMU as an unprivileged user, an environment variable
         SASL_CONF_PATH can be used to make it search alternate locations for the service config. While some


60                                                                                  Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


          SASL auth methods can also provide data encryption (eg GSSAPI), it is recommended that SASL always
          be combined with the ‘tls’ and ‘x509’ settings to enable use of SSL and server certificates. This ensures a
          data encryption preventing compromise of authentication credentials. See the VNC security section in the
          System Emulation Users Guide for details on using SASL authentication.
     sasl-authz=ID
         Provides the ID of the QAuthZ authorization object against which the client’s SASL username will vali-
         dated. This object is only resolved at time of use, so can be deleted and recreated on the fly while the VNC
         server is active. If missing, it will default to denying access.
     acl=on|off
         Legacy method for enabling authorization of clients against the x509 distinguished name and SASL
         username. It results in the creation of two authz-list objects with IDs of vnc.username and vnc.
         x509dname. The rules for these objects must be configured with the HMP ACL commands.
          This option is deprecated and should no longer be used. The new sasl-authz and tls-authz options
          are a replacement.
     lossy=on|off
         Enable lossy compression methods (gradient, JPEG, . . . ). If this option is set, VNC client may receive lossy
         framebuffer updates depending on its encoding settings. Enabling this option can save a lot of bandwidth
         at the expense of quality.
     non-adaptive=on|off
         Disable adaptive encodings. Adaptive encodings are enabled by default. An adaptive encoding will try to
         detect frequently updated screen regions, and send updates in these regions using a lossy encoding (like
         JPEG). This can be really helpful to save bandwidth when playing videos. Disabling adaptive encodings
         restores the original static behavior of encodings like Tight.
     share=[allow-exclusive|force-shared|ignore]
         Set display sharing policy. ‘allow-exclusive’ allows clients to ask for exclusive access. As suggested by
         the rfb spec this is implemented by dropping other connections. Connecting multiple clients in parallel
         requires all clients asking for a shared session (vncviewer: -shared switch). This is the default. ‘force-
         shared’ disables exclusive client access. Useful for shared desktop sessions, where you don’t want someone
         forgetting specify -shared disconnect everybody else. ‘ignore’ completely ignores the shared flag and allows
         everybody connect unconditionally. Doesn’t conform to the rfb spec but is traditional QEMU behavior.
     key-delay-ms
         Set keyboard delay, for key down and key up events, in milliseconds. Default is 10. Keyboards are low-
         bandwidth devices, so this slowdown can help the device and guest to keep up and not lose events in case
         events are arriving in bulk. Possible causes for the latter are flaky network connections, or scripts for
         automated testing.
     audiodev=audiodev
         Use the specified audiodev when the VNC client requests audio transmission. When not using an -audiodev
         argument, this option must be omitted, otherwise is must be present and specify a valid audiodev.
     power-control=on|off
         Permit the remote client to issue shutdown, reboot or reset power control requests.




2.2. Invocation                                                                                                     61
QEMU Documentation, Release 8.1.5


2.2.5 i386 target only

-win2k-hack
     Use it when installing Windows 2000 to avoid a disk full bug. After Windows 2000 is installed, you no longer
     need this option (this option slows down the IDE transfers).
-no-fd-bootchk
     Disable boot signature checking for floppy disks in BIOS. May be needed to boot from old floppy disks.
-no-acpi
     Disable ACPI (Advanced Configuration and Power Interface) support. Use it if your guest OS complains about
     ACPI problems (PC target machine only).
-no-hpet
     Disable HPET support. Deprecated, use ‘-machine hpet=off’ instead.
-acpitable [sig=str][,rev=n][,oem_id=str][,oem_table_id=str][,oem_rev=n]
[,asl_compiler_id=str][,asl_compiler_rev=n][,data=file1[:file2]...]
     Add ACPI table with specified header fields and context from specified files. For file=, take whole ACPI table
     from the specified files, including all ACPI headers (possible overridden by other options). For data=, only data
     portion of the table is used, all header information is specified in the command line. If a SLIC table is supplied
     to QEMU, then the SLIC’s oem_id and oem_table_id fields will override the same in the RSDT and the FADT
     (a.k.a. FACP), in order to ensure the field matches required by the Microsoft SLIC spec and the ACPI spec.
-smbios file=binary
     Load SMBIOS entry from binary file.
-smbios type=0[,vendor=str][,version=str][,date=str][,release=%d.%d][,uefi=on|off]
     Specify SMBIOS type 0 fields
-smbios type=1[,manufacturer=str][,product=str][,version=str][,serial=str][,uuid=uuid][,
sku=str][,family=str]
     Specify SMBIOS type 1 fields
-smbios type=2[,manufacturer=str][,product=str][,version=str][,serial=str][,asset=str][,
location=str]
     Specify SMBIOS type 2 fields
-smbios type=3[,manufacturer=str][,version=str][,serial=str][,asset=str][,sku=str]
     Specify SMBIOS type 3 fields
-smbios type=4[,sock_pfx=str][,manufacturer=str][,version=str][,serial=str][,asset=str][,
part=str][,processor-id=%d]
     Specify SMBIOS type 4 fields
-smbios type=11[,value=str][,path=filename]
     Specify SMBIOS type 11 fields
      This argument can be repeated multiple times, and values are added in the order they are parsed. Applications
      intending to use OEM strings data are encouraged to use their application name as a prefix for the value string.
      This facilitates passing information for multiple applications concurrently.
      The value=str syntax provides the string data inline, while the path=filename syntax loads data from a file
      on disk. Note that the file is not permitted to contain any NUL bytes.
      Both the value and path options can be repeated multiple times and will be added to the SMBIOS table in the
      order in which they appear.
      Note that on the x86 architecture, the total size of all SMBIOS tables is limited to 65535 bytes. Thus the OEM
      strings data is not suitable for passing large amounts of data into the guest. Instead it should be used as a indicator
      to inform the guest where to locate the real data set, for example, by specifying the serial ID of a block device.
      An example passing three strings is


62                                                                                     Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


      -smbios type=11,value=cloud-init:ds=nocloud-net;s=http://10.10.0.1:8000/,\
                       value=anaconda:method=http://dl.fedoraproject.org/pub/fedora/linux/
       ˓→releases/25/x86_64/os,\

                       path=/some/file/with/oemstringsdata.txt
      In the guest OS this is visible with the dmidecode command

            $ dmidecode -t 11
            Handle 0x0E00, DMI type 11, 5 bytes
            OEM Strings
                  String 1: cloud-init:ds=nocloud-net;s=http://10.10.0.1:8000/
                  String 2: anaconda:method=http://dl.fedoraproject.org/pub/fedora/linux/
             ˓→releases/25/x86_64/os

                  String 3: myapp:some extra data

-smbios type=17[,loc_pfx=str][,bank=str][,manufacturer=str][,serial=str][,asset=str][,
part=str][,speed=%d]
     Specify SMBIOS type 17 fields
-smbios type=41[,designation=str][,kind=str][,instance=%d][,pcidev=str]
     Specify SMBIOS type 41 fields
      This argument can be repeated multiple times. Its main use is to allow network interfaces be created as enoX on
      Linux, with X being the instance number, instead of the name depending on the interface position on the PCI
      bus.
      Here is an example of use:
      -netdev user,id=internet \
      -device virtio-net-pci,mac=50:54:00:00:00:42,netdev=internet,id=internet-dev \
      -smbios type=41,designation='Onboard LAN',instance=1,kind=ethernet,
       ˓→pcidev=internet-dev


      In the guest OS, the device should then appear as eno1:
      ..parsed-literal:

      $ ip -brief l
      lo                    UNKNOWN              00:00:00:00:00:00 <LOOPBACK,UP,LOWER_UP>
      eno1                  UP                   50:54:00:00:00:42 <BROADCAST,MULTICAST,UP,LOWER_UP>

      Currently, the PCI device has to be attached to the root bus.


2.2.6 Network options

-nic [tap|bridge|user|l2tpv3|vde|netmap|vhost-user|socket][,...][,mac=macaddr][,model=mn]
     This option is a shortcut for configuring both the on-board (default) guest NIC hardware and the host network
     backend in one go. The host backend options are the same as with the corresponding -netdev options below.
     The guest NIC model can be set with model=modelname. Use model=help to list the available device types.
     The hardware MAC address can be set with mac=macaddr.
      The following two example do exactly the same, to show how -nic can be used to shorten the command line
      length:
      qemu-system-x86_64 -netdev user,id=n1,ipv6=off -device e1000,netdev=n1,
       ˓→mac=52:54:98:76:54:32

      qemu-system-x86_64 -nic user,ipv6=off,model=e1000,mac=52:54:98:76:54:32



2.2. Invocation                                                                                                   63
QEMU Documentation, Release 8.1.5


-nic none
     Indicate that no network devices should be configured. It is used to override the default configuration (default
     NIC with “user” host network backend) which is activated if no other networking options are provided.
-netdev user,id=id[,option][,option][,...]
     Configure user mode host network backend which requires no administrator privilege to run. Valid options are:
      id=id
          Assign symbolic name for use in monitor commands.
      ipv4=on|off and ipv6=on|off
          Specify that either IPv4 or IPv6 must be enabled. If neither is specified both protocols are enabled.
      net=addr[/mask]
          Set IP network address the guest will see. Optionally specify the netmask, either in the form a.b.c.d or as
          number of valid top-most bits. Default is 10.0.2.0/24.
      host=addr
          Specify the guest-visible address of the host. Default is the 2nd IP in the guest network, i.e. x.x.x.2.
      ipv6-net=addr[/int]
          Set IPv6 network address the guest will see (default is fec0::/64). The network prefix is given in the usual
          hexadecimal IPv6 address notation. The prefix size is optional, and is given as the number of valid top-most
          bits (default is 64).
      ipv6-host=addr
          Specify the guest-visible IPv6 address of the host. Default is the 2nd IPv6 in the guest network, i.e. xxxx::2.
      restrict=on|off
          If this option is enabled, the guest will be isolated, i.e. it will not be able to contact the host and no guest IP
          packets will be routed over the host to the outside. This option does not affect any explicitly set forwarding
          rules.
      hostname=name
          Specifies the client hostname reported by the built-in DHCP server.
      dhcpstart=addr
          Specify the first of the 16 IPs the built-in DHCP server can assign. Default is the 15th to 31st IP in the
          guest network, i.e. x.x.x.15 to x.x.x.31.
      dns=addr
          Specify the guest-visible address of the virtual nameserver. The address must be different from the host
          address. Default is the 3rd IP in the guest network, i.e. x.x.x.3.
      ipv6-dns=addr
          Specify the guest-visible address of the IPv6 virtual nameserver. The address must be different from the
          host address. Default is the 3rd IP in the guest network, i.e. xxxx::3.
      dnssearch=domain
          Provides an entry for the domain-search list sent by the built-in DHCP server. More than one domain
          suffix can be transmitted by specifying this option multiple times. If supported, this will cause the guest to
          automatically try to append the given domain suffix(es) in case a domain name can not be resolved.
           Example:
           qemu-system-x86_64 -nic user,dnssearch=mgmt.example.org,dnssearch=example.org
      domainname=domain
          Specifies the client domain name reported by the built-in DHCP server.
      tftp=dir
          When using the user mode network stack, activate a built-in TFTP server. The files in dir will be exposed



64                                                                                     Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


          as the root of a TFTP server. The TFTP client on the guest must be configured in binary mode (use the
          command bin of the Unix TFTP client).
     tftp-server-name=name
         In BOOTP reply, broadcast name as the “TFTP server name” (RFC2132 option 66). This can be used to
         advise the guest to load boot files or configurations from a different server than the host address.
     bootfile=file
         When using the user mode network stack, broadcast file as the BOOTP filename. In conjunction with tftp,
         this can be used to network boot a guest from a local directory.
          Example (using pxelinux):
          qemu-system-x86_64 -hda linux.img -boot n -device e1000,netdev=n1 \
              -netdev user,id=n1,tftp=/path/to/tftp/files,bootfile=/pxelinux.0
     smb=dir[,smbserver=addr]
         When using the user mode network stack, activate a built-in SMB server so that Windows OSes can access
         to the host files in dir transparently. The IP address of the SMB server can be set to addr. By default the
         4th IP in the guest network is used, i.e. x.x.x.4.
          In the guest Windows OS, the line:

          10.0.2.4 smbserver

          must be added in the file C:\WINDOWS\LMHOSTS (for windows 9x/Me) or C:\WINNT\SYSTEM32\
          DRIVERS\ETC\LMHOSTS (Windows NT/2000).
          Then dir can be accessed in \\smbserver\qemu.
          Note that a SAMBA server must be installed on the host OS.
     hostfwd=[tcp|udp]:[hostaddr]:hostport-[guestaddr]:guestport
         Redirect incoming TCP or UDP connections to the host port hostport to the guest IP address guestaddr on
         guest port guestport. If guestaddr is not specified, its value is x.x.x.15 (default first address given by the
         built-in DHCP server). By specifying hostaddr, the rule can be bound to a specific host interface. If no
         connection type is set, TCP is used. This option can be given multiple times.
          For example, to redirect host X11 connection from screen 1 to guest screen 0, use the following:
          # on the host
          qemu-system-x86_64 -nic user,hostfwd=tcp:127.0.0.1:6001-:6000
          # this host xterm should open in the guest X11 server
          xterm -display :1
          To redirect telnet connections from host port 5555 to telnet port on the guest, use the following:
          # on the host
          qemu-system-x86_64 -nic user,hostfwd=tcp::5555-:23
          telnet localhost 5555
          Then when you use on the host telnet localhost 5555, you connect to the guest telnet server.
     guestfwd=[tcp]:server:port-dev; guestfwd=[tcp]:server:port-cmd:command
         Forward guest TCP connections to the IP address server on port port to the character device dev or to a
         program executed by cmd:command which gets spawned for each connection. This option can be given
         multiple times.
          You can either use a chardev directly and have that one used throughout QEMU’s lifetime, like in the
          following example:
          # open 10.10.1.1:4321 on bootup, connect 10.0.2.100:1234 to it whenever
          # the guest accesses it


2.2. Invocation                                                                                                     65
QEMU Documentation, Release 8.1.5


           qemu-system-x86_64 -nic user,guestfwd=tcp:10.0.2.100:1234-tcp:10.10.1.1:4321
           Or you can execute a command on every TCP connection established by the guest, so that QEMU behaves
           similar to an inetd process for that virtual server:
           # call "netcat 10.10.1.1 4321" on every TCP connection to 10.0.2.100:1234
           # and connect the TCP stream to its stdin/stdout
           qemu-system-x86_64 -nic 'user,id=n1,guestfwd=tcp:10.0.2.100:1234-cmd:netcat 10.
            ˓→10.1.1 4321'


-netdev tap,id=id[,fd=h][,ifname=name][,script=file][,downscript=dfile][,br=bridge][,
helper=helper]
     Configure a host TAP network backend with ID id.
      Use the network script file to configure it and the network script dfile to deconfigure it. If name is not provided,
      the OS automatically provides one. The default network configure script is /etc/qemu-ifup and the default
      network deconfigure script is /etc/qemu-ifdown. Use script=no or downscript=no to disable script exe-
      cution.
      If running QEMU as an unprivileged user, use the network helper to configure the TAP interface and attach it to
      the bridge. The default network helper executable is /path/to/qemu-bridge-helper and the default bridge
      device is br0.
      fd=h can be used to specify the handle of an already opened host TAP interface.
      Examples:
      #launch a QEMU instance with the default network script
      qemu-system-x86_64 linux.img -nic tap
      #launch a QEMU instance with two NICs, each one connected
      #to a TAP device
      qemu-system-x86_64 linux.img \
              -netdev tap,id=nd0,ifname=tap0 -device e1000,netdev=nd0 \
              -netdev tap,id=nd1,ifname=tap1 -device rtl8139,netdev=nd1
      #launch a QEMU instance with the default network helper to
      #connect a TAP device to bridge br0
      qemu-system-x86_64 linux.img -device virtio-net-pci,netdev=n1 \
              -netdev tap,id=n1,"helper=/path/to/qemu-bridge-helper"
-netdev bridge,id=id[,br=bridge][,helper=helper]
     Connect a host TAP network interface to a host bridge device.
      Use the network helper helper to configure the TAP interface and attach it to the bridge. The default network
      helper executable is /path/to/qemu-bridge-helper and the default bridge device is br0.
      Examples:
      #launch a QEMU instance with the default network helper to
      #connect a TAP device to bridge br0
      qemu-system-x86_64 linux.img -netdev bridge,id=n1 -device virtio-net,netdev=n1
      #launch a QEMU instance with the default network helper to
      #connect a TAP device to bridge qemubr0
      qemu-system-x86_64 linux.img -netdev bridge,br=qemubr0,id=n1 -device virtio-net,
       ˓→netdev=n1


-netdev socket,id=id[,fd=h][,listen=[host]:port][,connect=host:port]
     This host network backend can be used to connect the guest’s network to another QEMU virtual machine using a
     TCP socket connection. If listen is specified, QEMU waits for incoming connections on port (host is optional).



66                                                                                   Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


      connect is used to connect to another QEMU instance using the listen option. fd=h specifies an already
      opened TCP socket.
      Example:
      # launch a first QEMU instance
      qemu-system-x86_64 linux.img \
                       -device e1000,netdev=n1,mac=52:54:00:12:34:56 \
                       -netdev socket,id=n1,listen=:1234
      # connect the network of this instance to the network of the first instance
      qemu-system-x86_64 linux.img \
                       -device e1000,netdev=n2,mac=52:54:00:12:34:57 \
                       -netdev socket,id=n2,connect=127.0.0.1:1234
-netdev socket,id=id[,fd=h][,mcast=maddr:port[,localaddr=addr]]
     Configure a socket host network backend to share the guest’s network traffic with another QEMU virtual machines
     using a UDP multicast socket, effectively making a bus for every QEMU with same multicast address maddr and
     port. NOTES:
        1. Several QEMU can be running on different hosts and share same bus (assuming correct multicast setup for
           these hosts).
        2. mcast support is compatible with User Mode Linux (argument ethN=mcast), see http://user-mode-linux.
           sf.net.
        3. Use fd=h to specify an already opened UDP multicast socket.
      Example:
      # launch one QEMU instance
      qemu-system-x86_64 linux.img \
                       -device e1000,netdev=n1,mac=52:54:00:12:34:56 \
                       -netdev socket,id=n1,mcast=230.0.0.1:1234
      # launch another QEMU instance on same "bus"
      qemu-system-x86_64 linux.img \
                       -device e1000,netdev=n2,mac=52:54:00:12:34:57 \
                       -netdev socket,id=n2,mcast=230.0.0.1:1234
      # launch yet another QEMU instance on same "bus"
      qemu-system-x86_64 linux.img \
                       -device e1000,netdev=n3,mac=52:54:00:12:34:58 \
                       -netdev socket,id=n3,mcast=230.0.0.1:1234
      Example (User Mode Linux compat.):
      # launch QEMU instance (note mcast address selected is UML's default)
      qemu-system-x86_64 linux.img \
                       -device e1000,netdev=n1,mac=52:54:00:12:34:56 \
                       -netdev socket,id=n1,mcast=239.192.168.1:1102
      # launch UML
      /path/to/linux ubd0=/path/to/root_fs eth0=mcast
      Example (send packets from host’s 1.2.3.4):
      qemu-system-x86_64 linux.img \
                       -device e1000,netdev=n1,mac=52:54:00:12:34:56 \
                       -netdev socket,id=n1,mcast=239.192.168.1:1102,localaddr=1.2.3.4
-netdev l2tpv3,id=id,src=srcaddr,dst=dstaddr[,srcport=srcport][,dstport=dstport],
txsession=txsession[,rxsession=rxsession][,ipv6=on|off][,udp=on|off][,cookie64][,
counter][,pincounter][,txcookie=txcookie][,rxcookie=rxcookie][,offset=offset]
     Configure a L2TPv3 pseudowire host network backend. L2TPv3 (RFC3931) is a popular protocol to transport


2.2. Invocation                                                                                                  67
QEMU Documentation, Release 8.1.5


     Ethernet (and other Layer 2) data frames between two systems. It is present in routers, firewalls and the Linux
     kernel (from version 3.3 onwards).
     This transport allows a VM to communicate to another VM, router or firewall directly.
     src=srcaddr
         source address (mandatory)
     dst=dstaddr
         destination address (mandatory)
     udp
            select udp encapsulation (default is ip).
     srcport=srcport
         source udp port.
     dstport=dstport
         destination udp port.
     ipv6
            force v6, otherwise defaults to v4.
     rxcookie=rxcookie; txcookie=txcookie
         Cookies are a weak form of security in the l2tpv3 specification. Their function is mostly to prevent mis-
         configuration. By default they are 32 bit.
     cookie64
         Set cookie size to 64 bit instead of the default 32
     counter=off
         Force a ‘cut-down’ L2TPv3 with no counter as in draft-mkonstan-l2tpext-keyed-ipv6-tunnel-00
     pincounter=on
         Work around broken counter handling in peer. This may also help on networks which have packet reorder.
     offset=offset
         Add an extra offset between header and data
     For example, to attach a VM running on host 4.3.2.1 via L2TPv3 to the bridge br-lan on the remote Linux host
     1.2.3.4:
     # Setup tunnel on linux host using raw ip as encapsulation
     # on 1.2.3.4
     ip l2tp add tunnel remote 4.3.2.1 local 1.2.3.4 tunnel_id 1 peer_tunnel_id 1 \
         encap udp udp_sport 16384 udp_dport 16384
     ip l2tp add session tunnel_id 1 name vmtunnel0 session_id \
         0xFFFFFFFF peer_session_id 0xFFFFFFFF
     ifconfig vmtunnel0 mtu 1500
     ifconfig vmtunnel0 up
     brctl addif br-lan vmtunnel0


     # on 4.3.2.1
     # launch QEMU instance - if your network has reorder or is very lossy add ,
      ˓→pincounter



     qemu-system-x86_64 linux.img -device e1000,netdev=n1 \
          -netdev l2tpv3,id=n1,src=4.2.3.1,dst=1.2.3.4,udp,srcport=16384,dstport=16384,
      ˓→rxsession=0xffffffff,txsession=0xffffffff,counter




68                                                                              Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


-netdev vde,id=id[,sock=socketpath][,port=n][,group=groupname][,mode=octalmode]
     Configure VDE backend to connect to PORT n of a vde switch running on host and listening for incoming
     connections on socketpath. Use GROUP groupname and MODE octalmode to change default ownership and
     permissions for communication port. This option is only available if QEMU has been compiled with vde support
     enabled.
      Example:
      # launch vde switch
      vde_switch -F -sock /tmp/myswitch
      # launch QEMU instance
      qemu-system-x86_64 linux.img -nic vde,sock=/tmp/myswitch
-netdev vhost-user,chardev=id[,vhostforce=on|off][,queues=n]
     Establish a vhost-user netdev, backed by a chardev id. The chardev should be a unix domain socket backed one.
     The vhost-user uses a specifically defined protocol to pass vhost ioctl replacement messages to an application on
     the other end of the socket. On non-MSIX guests, the feature can be forced with vhostforce. Use ‘queues=n’ to
     specify the number of queues to be created for multiqueue vhost-user.
      Example:

      qemu -m 512 -object memory-backend-file,id=mem,size=512M,mem-path=/hugetlbfs,
       ˓→share=on \

            -numa node,memdev=mem \
            -chardev socket,id=chr0,path=/path/to/socket \
            -netdev type=vhost-user,id=net0,chardev=chr0 \
            -device virtio-net-pci,netdev=net0

-netdev vhost-vdpa[,vhostdev=/path/to/dev][,vhostfd=h]
     Establish a vhost-vdpa netdev.
      vDPA device is a device that uses a datapath which complies with the virtio specifications with a vendor specific
      control path. vDPA devices can be both physically located on the hardware or emulated by software.
-netdev hubport,id=id,hubid=hubid[,netdev=nd]
     Create a hub port on the emulated hub with ID hubid.
      The hubport netdev lets you connect a NIC to a QEMU emulated hub instead of a single netdev. Alternatively,
      you can also connect the hubport to another netdev with ID nd by using the netdev=nd option.
-net nic[,netdev=nd][,macaddr=mac][,model=type] [,name=name][,addr=addr][,vectors=v]
     Legacy option to configure or create an on-board (or machine default) Network Interface Card(NIC) and connect
     it either to the emulated hub with ID 0 (i.e. the default hub), or to the netdev nd. If model is omitted, then the
     default NIC model associated with the machine type is used. Note that the default NIC model may change in
     future QEMU releases, so it is highly recommended to always specify a model. Optionally, the MAC address can
     be changed to mac, the device address set to addr (PCI cards only), and a name can be assigned for use in monitor
     commands. Optionally, for PCI cards, you can specify the number v of MSI-X vectors that the card should have;
     this option currently only affects virtio cards; set v = 0 to disable MSI-X. If no -net option is specified, a single
     NIC is created. QEMU can emulate several different models of network card. Use -net nic,model=help for
     a list of available devices for your target.
-net user|tap|bridge|socket|l2tpv3|vde[,...][,name=name]
     Configure a host network backend (with the options corresponding to the same -netdev option) and connect it
     to the emulated hub 0 (the default hub). Use name to specify the name of the hub port.




2.2. Invocation                                                                                                        69
QEMU Documentation, Release 8.1.5


2.2.7 Character device options

The general form of a character device option is:
-chardev backend,id=id[,mux=on|off][,options]
     Backend is one of: null, socket, udp, msmouse, vc, ringbuf, file, pipe, console, serial, pty, stdio,
     braille, parallel, spicevmc, spiceport. The specific backend will determine the applicable options.
      Use -chardev help to print all available chardev backend types.
      All devices must have an id, which can be any string up to 127 characters long. It is used to uniquely identify
      this device in other command line directives.
      A character device may be used in multiplexing mode by multiple front-ends. Specify mux=on to enable this
      mode. A multiplexer is a “1:N” device, and here the “1” end is your specified chardev backend, and the “N”
      end is the various parts of QEMU that can talk to a chardev. If you create a chardev with id=myid and mux=on,
      QEMU will create a multiplexer with your specified ID, and you can then configure multiple front ends to use
      that chardev ID for their input/output. Up to four different front ends can be connected to a single multiplexed
      chardev. (Without multiplexing enabled, a chardev can only be used by a single front end.) For instance you
      could use this to allow a single stdio chardev to be used by two serial ports and the QEMU monitor:

      -chardev stdio,mux=on,id=char0 \
      -mon chardev=char0,mode=readline \
      -serial chardev:char0 \
      -serial chardev:char0

      You can have more than one multiplexer in a system configuration; for instance you could have a TCP port
      multiplexed between UART 0 and UART 1, and stdio multiplexed between the QEMU monitor and a parallel
      port:

      -chardev stdio,mux=on,id=char0 \
      -mon chardev=char0,mode=readline \
      -parallel chardev:char0 \
      -chardev tcp,...,mux=on,id=char1 \
      -serial chardev:char1 \
      -serial chardev:char1

      When you’re using a multiplexed character device, some escape sequences are interpreted in the input. See the
      chapter about Keys in the character backend multiplexer in the System Emulation Users Guide for more details.
      Note that some other command line options may implicitly create multiplexed character backends; for instance
      -serial mon:stdio creates a multiplexed stdio backend connected to the serial port and the QEMU monitor,
      and -nographic also multiplexes the console and the monitor to stdio.
      There is currently no support for multiplexing in the other direction (where a single QEMU front end takes input
      and output from multiple chardevs).
      Every backend supports the logfile option, which supplies the path to a file to record all data transmitted via
      the backend. The logappend option controls whether the log file will be truncated or appended to when opened.
The available backends are:
-chardev null,id=id
     A void device. This device will not emit any data, and will drop any data it receives. The null backend does not
     take any options.
-chardev socket,id=id[,TCP options or unix options][,server=on|off][,wait=on|off][,
telnet=on|off][,websocket=on|off][,reconnect=seconds][,tls-creds=id][,tls-authz=id]
     Create a two-way stream socket, which can be either a TCP or a unix socket. A unix socket will be created if
     path is specified. Behaviour is undefined if TCP options are specified for a unix socket.


70                                                                                Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


     server=on|off specifies that the socket shall be a listening socket.
     wait=on|off specifies that QEMU should not block waiting for a client to connect to a listening socket.
     telnet=on|off specifies that traffic on the socket should interpret telnet escape sequences.
     websocket=on|off specifies that the socket uses WebSocket protocol for communication.
     reconnect sets the timeout for reconnecting on non-server sockets when the remote end goes away. qemu will
     delay this many seconds and then attempt to reconnect. Zero disables reconnecting, and is the default.
     tls-creds requests enablement of the TLS protocol for encryption, and specifies the id of the TLS credentials
     to use for the handshake. The credentials must be previously created with the -object tls-creds argument.
     tls-auth provides the ID of the QAuthZ authorization object against which the client’s x509 distinguished
     name will be validated. This object is only resolved at time of use, so can be deleted and recreated on the fly
     while the chardev server is active. If missing, it will default to denying access.
     TCP and unix socket options are given below:
     TCP options:
     port=port[,host=host][,to=to][,ipv4=on|off][,ipv6=on|off][,nodelay=on|off]
         host for a listening socket specifies the local address to be bound. For a connecting socket species the
         remote host to connect to. host is optional for listening sockets. If not specified it defaults to 0.0.0.0.
          port for a listening socket specifies the local port to be bound. For a connecting socket specifies the port
          on the remote host to connect to. port can be given as either a port number or a service name. port is
          required.
          to is only relevant to listening sockets. If it is specified, and port cannot be bound, QEMU will attempt to
          bind to subsequent ports up to and including to until it succeeds. to must be specified as a port number.
          ipv4=on|off and ipv6=on|off specify that either IPv4 or IPv6 must be used. If neither is specified the
          socket may use either protocol.
          nodelay=on|off disables the Nagle algorithm.
     unix options: path=path[,abstract=on|off][,tight=on|off]
         path specifies the local path of the unix socket. path is required. abstract=on|off specifies the use
         of the abstract socket namespace, rather than the filesystem. Optional, defaults to false. tight=on|off
         sets the socket length of abstract sockets to their minimum, rather than the full sun_path length. Optional,
         defaults to true.
-chardev udp,id=id[,host=host],port=port[,localaddr=localaddr][,localport=localport][,
ipv4=on|off][,ipv6=on|off]
     Sends all traffic from the guest to a remote host over UDP.
     host specifies the remote host to connect to. If not specified it defaults to localhost.
     port specifies the port on the remote host to connect to. port is required.
     localaddr specifies the local address to bind to. If not specified it defaults to 0.0.0.0.
     localport specifies the local port to bind to. If not specified any available local port will be used.
     ipv4=on|off and ipv6=on|off specify that either IPv4 or IPv6 must be used. If neither is specified the device
     may use either protocol.
-chardev msmouse,id=id
     Forward QEMU’s emulated msmouse events to the guest. msmouse does not take any options.
-chardev vc,id=id[[,width=width][,height=height]][[,cols=cols][,rows=rows]]
     Connect to a QEMU text console. vc may optionally be given a specific size.
     width and height specify the width and height respectively of the console, in pixels.


2.2. Invocation                                                                                                    71
QEMU Documentation, Release 8.1.5


      cols and rows specify that the console be sized to fit a text console with the given dimensions.
-chardev ringbuf,id=id[,size=size]
     Create a ring buffer with fixed size size. size must be a power of two and defaults to 64K.
-chardev file,id=id,path=path[,input-path=input-path]
     Log all traffic received from the guest to a file.
      path specifies the path of the file to be opened. This file will be created if it does not already exist, and overwritten
      if it does. path is required.
      If input-path is specified, this is the path of a second file which will be used for input. If input-path is not
      specified, no input will be available from the chardev.
      Note that input-path is not supported on Windows hosts.
-chardev pipe,id=id,path=path
     Create a two-way connection to the guest. The behaviour differs slightly between Windows hosts and other hosts:
      On Windows, a single duplex pipe will be created at \\.pipe\path.
      On other hosts, 2 pipes will be created called path.in and path.out. Data written to path.in will be received
      by the guest. Data written by the guest can be read from path.out. QEMU will not create these fifos, and
      requires them to be present.
      path forms part of the pipe path as described above. path is required.
-chardev console,id=id
     Send traffic from the guest to QEMU’s standard output. console does not take any options.
      console is only available on Windows hosts.
-chardev serial,id=id,path=path
     Send traffic from the guest to a serial device on the host.
      On Unix hosts serial will actually accept any tty device, not only serial lines.
      path specifies the name of the serial device to open.
-chardev pty,id=id
     Create a new pseudo-terminal on the host and connect to it. pty does not take any options.
      pty is not available on Windows hosts.
-chardev stdio,id=id[,signal=on|off]
     Connect to standard input and standard output of the QEMU process.
      signal controls if signals are enabled on the terminal, that includes exiting QEMU with the key sequence
      Control-c. This option is enabled by default, use signal=off to disable it.
-chardev braille,id=id
     Connect to a local BrlAPI server. braille does not take any options.
-chardev parallel,id=id,path=path


            parallel is only available on Linux, FreeBSD and DragonFlyBSD hosts.
            Connect to a local parallel port.
            path specifies the path to the parallel port device. path is required.
-chardev spicevmc,id=id,debug=debug,name=name
     spicevmc is only available when spice support is built in.
      debug debug level for spicevmc



72                                                                                      Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


      name name of spice channel to connect to
      Connect to a spice virtual machine channel, such as vdiport.
-chardev spiceport,id=id,debug=debug,name=name
     spiceport is only available when spice support is built in.
      debug debug level for spicevmc
      name name of spice port to connect to
      Connect to a spice port, allowing a Spice client to handle the traffic identified by a name (preferably a fqdn).


2.2.8 TPM device options

The general form of a TPM device option is:
-tpmdev backend,id=id[,options]
     The specific backend type will determine the applicable options. The -tpmdev option creates the TPM backend
     and requires a -device option that specifies the TPM frontend interface model.
      Use -tpmdev help to print all available TPM backend types.
The available backends are:
-tpmdev passthrough,id=id,path=path,cancel-path=cancel-path
     (Linux-host only) Enable access to the host’s TPM using the passthrough driver.
      path specifies the path to the host’s TPM device, i.e., on a Linux host this would be /dev/tpm0. path is optional
      and by default /dev/tpm0 is used.
      cancel-path specifies the path to the host TPM device’s sysfs entry allowing for cancellation of an ongoing
      TPM command. cancel-path is optional and by default QEMU will search for the sysfs entry to use.
      Some notes about using the host’s TPM with the passthrough driver:
      The TPM device accessed by the passthrough driver must not be used by any other application on the host.
      Since the host’s firmware (BIOS/UEFI) has already initialized the TPM, the VM’s firmware (BIOS/UEFI) will
      not be able to initialize the TPM again and may therefore not show a TPM-specific menu that would otherwise
      allow the user to configure the TPM, e.g., allow the user to enable/disable or activate/deactivate the TPM. Further,
      if TPM ownership is released from within a VM then the host’s TPM will get disabled and deactivated. To enable
      and activate the TPM again afterwards, the host has to be rebooted and the user is required to enter the firmware’s
      menu to enable and activate the TPM. If the TPM is left disabled and/or deactivated most TPM commands will
      fail.
      To create a passthrough TPM use the following two options:

      -tpmdev passthrough,id=tpm0 -device tpm-tis,tpmdev=tpm0

      Note that the -tpmdev id is tpm0 and is referenced by tpmdev=tpm0 in the device option.
-tpmdev emulator,id=id,chardev=dev
     (Linux-host only) Enable access to a TPM emulator using Unix domain socket based chardev backend.
      chardev specifies the unique ID of a character device backend that provides connection to the software TPM
      server.
      To create a TPM emulator backend device with chardev socket backend:

      -chardev socket,id=chrtpm,path=/tmp/swtpm-sock -tpmdev emulator,id=tpm0,
       ˓→chardev=chrtpm -device tpm-tis,tpmdev=tpm0




2.2. Invocation                                                                                                          73
QEMU Documentation, Release 8.1.5


2.2.9 Boot Image or Kernel specific

There are broadly 4 ways you can boot a system with QEMU.
     • specify a firmware and let it control finding a kernel
     • specify a firmware and pass a hint to the kernel to boot
     • direct kernel image boot
     • manually load files into the guest’s address space
The third method is useful for quickly testing kernels but as there is no firmware to pass configuration information to the
kernel the hardware must either be probeable, the kernel built for the exact configuration or passed some configuration
data (e.g. a DTB blob) which tells the kernel what drivers it needs. This exact details are often hardware specific.
The final method is the most generic way of loading images into the guest address space and used mostly for bare
metal type development where the reset vectors of the processor are taken into account.
For x86 machines and some other architectures -bios will generally do the right thing with whatever it is given. For
other machines the more strict -pflash option needs an image that is sized for the flash device for the given machine
type.
Please see the QEMU System Emulator Targets section of the manual for more detailed documentation.
-bios file
     Set the filename for the BIOS.
-pflash file
     Use file as a parallel flash image.
The kernel options were designed to work with Linux kernels although other things (like hypervisors) can be packaged
up as a kernel executable image. The exact format of a executable image is usually architecture specific.
The way in which the kernel is started (what address it is loaded at, what if any information is passed to it via CPU
registers, the state of the hardware when it is started, and so on) is also architecture specific. Typically it follows the
specification laid down by the Linux kernel for how kernels for that architecture must be started.
-kernel bzImage
     Use bzImage as kernel image. The kernel can be either a Linux kernel or in multiboot format.
-append cmdline
     Use cmdline as kernel command line
-initrd file
     Use file as initial ram disk.
-initrd "file1 arg=foo,file2"
     This syntax is only available with multiboot.
       Use file1 and file2 as modules and pass arg=foo as parameter to the first module.
-dtb file
     Use file as a device tree binary (dtb) image and pass it to the kernel on boot.
Finally you can also manually load images directly into the address space of the guest. This is most useful for developers
who already know the layout of their guest and take care to ensure something sane will happen when the reset vector
executes.
The generic loader can be invoked by using the loader device:
-device loader,addr=<addr>,data=<data>,data-len=<data-len>[,data-be=<data-be>][,
cpu-num=<cpu-num>]




74                                                                                   Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


there is also the guest loader which operates in a similar way but tweaks the DTB so a hypervisor loaded via -kernel
can find where the guest image is:
-device guest-loader,addr=<addr>[,kernel=<path>,[bootargs=<arguments>]][,initrd=<path>]


2.2.10 Debug/Expert options

-compat [deprecated-input=@var{input-policy}][,deprecated-output=@var{output-policy}]
     Set policy for handling deprecated management interfaces (experimental):
      deprecated-input=accept (default)
          Accept deprecated commands and arguments
      deprecated-input=reject
          Reject deprecated commands and arguments
      deprecated-input=crash
          Crash on deprecated commands and arguments
      deprecated-output=accept (default)
          Emit deprecated command results and events
      deprecated-output=hide
          Suppress deprecated command results and events
      Limitation: covers only syntactic aspects of QMP.
-compat [unstable-input=@var{input-policy}][,unstable-output=@var{output-policy}]
     Set policy for handling unstable management interfaces (experimental):
      unstable-input=accept (default)
          Accept unstable commands and arguments
      unstable-input=reject
          Reject unstable commands and arguments
      unstable-input=crash
          Crash on unstable commands and arguments
      unstable-output=accept (default)
          Emit unstable command results and events
      unstable-output=hide
          Suppress unstable command results and events
      Limitation: covers only syntactic aspects of QMP.
-fw_cfg [name=]name,file=file
     Add named fw_cfg entry with contents from file file.
-fw_cfg [name=]name,string=str
     Add named fw_cfg entry with contents from string str.
      The terminating NUL character of the contents of str will not be included as part of the fw_cfg item data. To
      insert contents with embedded NUL characters, you have to use the file parameter.
      The fw_cfg entries are passed by QEMU through to the guest.
      Example:

      -fw_cfg name=opt/com.mycompany/blob,file=./my_blob.bin

      creates an fw_cfg entry named opt/com.mycompany/blob with contents from ./my_blob.bin.


2.2. Invocation                                                                                                  75
QEMU Documentation, Release 8.1.5


-serial dev
     Redirect the virtual serial port to host character device dev. The default device is vc in graphical mode and stdio
     in non graphical mode.
      This option can be used several times to simulate up to 4 serial ports.
      Use -serial none to disable all serial ports.
      Available character devices are:
      vc[:WxH]
          Virtual console. Optionally, a width and height can be given in pixel with

             vc:800x600

             It is also possible to specify width or height in characters:

             vc:80Cx24C

      pty
             [Linux only] Pseudo TTY (a new PTY is automatically allocated)
      none
             No device is allocated.
      null
             void device
      chardev:id
          Use a named character device defined with the -chardev option.
      /dev/XXX
          [Linux only] Use host tty, e.g. /dev/ttyS0. The host serial port parameters are set according to the
          emulated ones.
      /dev/parportN
          [Linux only, parallel port only] Use host parallel port N. Currently SPP and EPP parallel port features can
          be used.
      file:filename
          Write output to filename. No character can be read.
      stdio
          [Unix only] standard input/output
      pipe:filename
          name pipe filename
      COMn
             [Windows only] Use host serial port n
      udp:[remote_host]:remote_port[@[src_ip]:src_port]
          This implements UDP Net Console. When remote_host or src_ip are not specified they default to 0.0.0.0.
          When not using a specified src_port a random port is automatically chosen.
             If you just want a simple readonly console you can use netcat or nc, by starting QEMU with: -serial
             udp::4555 and nc as: nc -u -l -p 4555. Any time QEMU writes something to that port it will appear
             in the netconsole session.
             If you plan to send characters back via netconsole or you want to stop and start QEMU a lot of
             times, you should have QEMU use the same source port each time by using something like -serial
             udp::4555@:4556 to QEMU. Another approach is to use a patched version of netcat which can listen


76                                                                                 Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


          to a TCP port and send and receive characters via udp. If you have a patched version of netcat which acti-
          vates telnet remote echo and single char transfer, then you can use the following options to set up a netcat
          redirector to allow telnet on port 5555 to access the QEMU port.
          QEMU Options:
              -serial udp::4555@:4556
          netcat options:
              -u -P 4555 -L 0.0.0.0:4556 -t -p 5555 -I -T
          telnet options:
              localhost 5555
     tcp:[host]:port[,server=on|off][,wait=on|off][,nodelay=on|off][,reconnect=seconds]
         The TCP Net Console has two modes of operation. It can send the serial I/O to a location or wait for a
         connection from a location. By default the TCP Net Console is sent to host at the port. If you use the
         server=on option QEMU will wait for a client socket application to connect to the port before continuing,
         unless the wait=on|off option was specified. The nodelay=on|off option disables the Nagle buffering
         algorithm. The reconnect=on option only applies if server=no is set, if the connection goes down it will
         attempt to reconnect at the given interval. If host is omitted, 0.0.0.0 is assumed. Only one TCP connection
         at a time is accepted. You can use telnet=on to connect to the corresponding character device.
          Example to send tcp console to 192.168.0.2 port 4444
              -serial tcp:192.168.0.2:4444
          Example to listen and wait on port 4444 for connection
              -serial tcp::4444,server=on
          Example to not wait and listen on ip 192.168.0.100 port 4444
              -serial tcp:192.168.0.100:4444,server=on,wait=off
     telnet:host:port[,server=on|off][,wait=on|off][,nodelay=on|off]
         The telnet protocol is used instead of raw tcp sockets. The options work the same as if you had specified
         -serial tcp. The difference is that the port acts like a telnet server or client using telnet option nego-
         tiation. This will also allow you to send the MAGIC_SYSRQ sequence if you use a telnet that supports
         sending the break sequence. Typically in unix telnet you do it with Control-] and then type “send break”
         followed by pressing the enter key.
     websocket:host:port,server=on[,wait=on|off][,nodelay=on|off]
         The WebSocket protocol is used instead of raw tcp socket. The port acts as a WebSocket server. Client
         mode is not supported.
     unix:path[,server=on|off][,wait=on|off][,reconnect=seconds]
         A unix domain socket is used instead of a tcp socket. The option works the same as if you had specified
         -serial tcp except the unix domain socket path is used for connections.
     mon:dev_string
         This is a special option to allow the monitor to be multiplexed onto another serial port. The monitor is
         accessed with key sequence of Control-a and then pressing c. dev_string should be any one of the serial
         devices specified above. An example to multiplex the monitor onto a telnet server listening on port 4444
         would be:
          -serial mon:telnet::4444,server=on,wait=off
          When the monitor is multiplexed to stdio in this way, Ctrl+C will not terminate QEMU any more but will
          be passed to the guest instead.
     braille
         Braille device. This will use BrlAPI to display the braille output on a real or fake device.




2.2. Invocation                                                                                                    77
QEMU Documentation, Release 8.1.5


      msmouse
          Three button serial mouse. Configure the guest to use Microsoft protocol.
-parallel dev
     Redirect the virtual parallel port to host device dev (same devices as the serial port). On Linux hosts, /dev/
     parportN can be used to use hardware devices connected on the corresponding host parallel port.
      This option can be used several times to simulate up to 3 parallel ports.
      Use -parallel none to disable all parallel ports.
-monitor dev
     Redirect the monitor to host device dev (same devices as the serial port). The default device is vc in graphical
     mode and stdio in non graphical mode. Use -monitor none to disable the default monitor.
-qmp dev
     Like -monitor but opens in ‘control’ mode. For example, to make QMP available on localhost port 4444:

      -qmp tcp:localhost:4444,server=on,wait=off

      Not all options are configurable via this syntax; for maximum flexibility use the -mon option and an accompa-
      nying -chardev.
-qmp-pretty dev
     Like -qmp but uses pretty JSON formatting.
-mon [chardev=]name[,mode=readline|control][,pretty[=on|off]]
     Set up a monitor connected to the chardev name. QEMU supports two monitors: the Human Monitor Protocol
     (HMP; for human interaction), and the QEMU Monitor Protocol (QMP; a JSON RPC-style protocol). The default
     is HMP; mode=control selects QMP instead. pretty is only valid when mode=control, turning on JSON
     pretty printing to ease human reading and debugging.
      For example:

      -chardev socket,id=mon1,host=localhost,port=4444,server=on,wait=off \
      -mon chardev=mon1,mode=control,pretty=on

      enables the QMP monitor on localhost port 4444 with pretty-printing.
-debugcon dev
     Redirect the debug console to host device dev (same devices as the serial port). The debug console is an I/O
     port which is typically port 0xe9; writing to that I/O port sends output to this device. The default device is vc in
     graphical mode and stdio in non graphical mode.
-pidfile file
     Store the QEMU process PID in file. It is useful if you launch QEMU from a script.
-singlestep
     This is a deprecated synonym for the TCG accelerator property one-insn-per-tb.
--preconfig
     Pause QEMU for interactive configuration before the machine is created, which allows querying and configuring
     properties that will affect machine initialization. Use QMP command ‘x-exit-preconfig’ to exit the preconfig
     state and move to the next state (i.e. run guest if -S isn’t used or pause the second time if -S is used). This option
     is experimental.
-S
      Do not start CPU at startup (you must type ‘c’ in the monitor).
-overcommit mem-lock=on|off




78                                                                                   Chapter 2. System Emulation
                                                                               QEMU Documentation, Release 8.1.5


-overcommit cpu-pm=on|off
     Run qemu with hints about host resource overcommit. The default is to assume that host overcommits all re-
     sources.
      Locking qemu and guest memory can be enabled via mem-lock=on (disabled by default). This works when host
      memory is not overcommitted and reduces the worst-case latency for guest.
      Guest ability to manage power state of host cpus (increasing latency for other processes on the same host cpu,
      but decreasing latency for guest) can be enabled via cpu-pm=on (disabled by default). This works best when
      host CPU is not overcommitted. When used, host estimates of CPU cycle and power utilization will be incorrect,
      not taking into account guest idle time.
-gdb dev
     Accept a gdb connection on device dev (see the GDB usage chapter in the System Emulation Users Guide). Note
     that this option does not pause QEMU execution – if you want QEMU to not start the guest until you connect
     with gdb and issue a continue command, you will need to also pass the -S option to QEMU.
      The most usual configuration is to listen on a local TCP socket:

      -gdb tcp::3117

      but you can specify other backends; UDP, pseudo TTY, or even stdio are all reasonable use cases. For example,
      a stdio connection allows you to start QEMU from within gdb and establish the connection via a pipe:
      (gdb) target remote | exec qemu-system-x86_64 -gdb stdio ...
-s
      Shorthand for -gdb tcp::1234, i.e. open a gdbserver on TCP port 1234 (see the GDB usage chapter in the System
      Emulation Users Guide).
-d item1[,...]
     Enable logging of specified items. Use ‘-d help’ for a list of log items.
-D logfile
     Output log in logfile instead of to stderr
-dfilter range1[,...]
     Filter debug output to that relevant to a range of target addresses. The filter spec can be either start+size, start-size
     or start..end where start end and size are the addresses and sizes required. For example:

      -dfilter 0x8000..0x8fff,0xffffffc000080000+0x200,0xffffffc000060000-0x1000

      Will dump output for any code in the 0x1000 sized block starting at 0x8000 and the 0x200 sized block starting
      at 0xffffffc000080000 and another 0x1000 sized block starting at 0xffffffc00005f000.
-seed number
     Force the guest to use a deterministic pseudo-random number generator, seeded with number. This does not
     affect crypto routines within the host.
-L path
     Set the directory for the BIOS, VGA BIOS and keymaps.
      To list all the data directories, use -L help.
-enable-kvm
     Enable KVM full virtualization support. This option is only available if KVM support is enabled when compiling.
-xen-domid id
     Specify xen guest domain id (XEN only).




2.2. Invocation                                                                                                            79
QEMU Documentation, Release 8.1.5


-xen-attach
     Attach to existing xen domain. libxl will use this when starting QEMU (XEN only). Restrict set of available xen
     operations to specified domain id (XEN only).
-no-reboot
     Exit instead of rebooting.
-no-shutdown
     Don’t exit QEMU on guest shutdown, but instead only stop the emulation. This allows for instance switching to
     monitor to commit changes to the disk image.
-action event=action
     The action parameter serves to modify QEMU’s default behavior when certain guest events occur. It provides
     a generic method for specifying the same behaviors that are modified by the -no-reboot and -no-shutdown
     parameters.
      Examples:
      -action panic=none          -action reboot=shutdown,shutdown=pause                 -device i6300esb -action
      watchdog=pause
-loadvm file
     Start right away with a saved state (loadvm in monitor)
-daemonize
     Daemonize the QEMU process after initialization. QEMU will not detach from standard IO until it is ready to
     receive connections on any of its devices. This option is a useful way for external programs to launch QEMU
     without having to cope with initialization race conditions.
-option-rom file
     Load the contents of file as an option ROM. This option is useful to load things like EtherBoot.
-rtc [base=utc|localtime|datetime][,clock=host|rt|vm][,driftfix=none|slew]
     Specify base as utc or localtime to let the RTC start at the current UTC or local time, respectively. localtime
     is required for correct date in MS-DOS or Windows. To start at a specific point in time, provide datetime in the
     format 2006-06-17T16:01:21 or 2006-06-17. The default base is UTC.
      By default the RTC is driven by the host system time. This allows using of the RTC as accurate reference clock
      inside the guest, specifically if the host time is smoothly following an accurate external reference clock, e.g. via
      NTP. If you want to isolate the guest time from the host, you can set clock to rt instead, which provides a
      host monotonic clock if host support it. To even prevent the RTC from progressing during suspension, you can
      set clock to vm (virtual clock). ‘clock=vm‘ is recommended especially in icount mode in order to preserve
      determinism; however, note that in icount mode the speed of the virtual clock is variable and can in general differ
      from the host clock.
      Enable driftfix (i386 targets only) if you experience time drift problems, specifically with Windows’ ACPI
      HAL. This option will try to figure out how many timer interrupts were not processed by the Windows guest and
      will re-inject them.
-icount [shift=N|auto][,align=on|off][,sleep=on|off][,rr=record|replay,rrfile=filename[,
rrsnapshot=snapshot]]
     Enable virtual instruction counter. The virtual cpu will execute one instruction every 2^N ns of virtual time.
     If auto is specified then the virtual cpu speed will be automatically adjusted to keep virtual time within a few
     seconds of real time.
      Note that while this option can give deterministic behavior, it does not provide cycle accurate emulation. Modern
      CPUs contain superscalar out of order cores with complex cache hierarchies. The number of instructions executed
      often has little or no correlation with actual performance.
      When the virtual cpu is sleeping, the virtual time will advance at default speed unless sleep=on is specified.
      With sleep=on, the virtual time will jump to the next timer deadline instantly whenever the virtual cpu goes to


80                                                                                   Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5


      sleep mode and will not advance if no timer is enabled. This behavior gives deterministic execution times from
      the guest point of view. The default if icount is enabled is sleep=off. sleep=on cannot be used together with
      either shift=auto or align=on.
      align=on will activate the delay algorithm which will try to synchronise the host clock and the virtual clock.
      The goal is to have a guest running at the real frequency imposed by the shift option. Whenever the guest clock
      is behind the host clock and if align=on is specified then we print a message to the user to inform about the
      delay. Currently this option does not work when shift is auto. Note: The sync algorithm will work for those
      shift values for which the guest clock runs ahead of the host clock. Typically this happens when the shift value
      is high (how high depends on the host machine). The default if icount is enabled is align=off.
      When the rr option is specified deterministic record/replay is enabled. The rrfile= option must also be pro-
      vided to specify the path to the replay log. In record mode data is written to this file, and in replay mode it is
      read back. If the rrsnapshot option is given then it specifies a VM snapshot name. In record mode, a new VM
      snapshot with the given name is created at the start of execution recording. In replay mode this option specifies
      the snapshot name used to load the initial VM state.
-watchdog-action action
     The action controls what QEMU will do when the watchdog timer expires. The default is reset (forcefully
     reset the guest). Other possible actions are: shutdown (attempt to gracefully shutdown the guest), poweroff
     (forcefully poweroff the guest), inject-nmi (inject a NMI into the guest), pause (pause the guest), debug (print
     a debug message and continue), or none (do nothing).
      Note that the shutdown action requires that the guest responds to ACPI signals, which it may not be able to do in
      the sort of situations where the watchdog would have expired, and thus -watchdog-action shutdown is not
      recommended for production use.
      Examples:
      -device i6300esb -watchdog-action pause
-echr numeric_ascii_value
     Change the escape character used for switching to the monitor when using monitor and serial sharing. The default
     is 0x01 when using the -nographic option. 0x01 is equal to pressing Control-a. You can select a different
     character from the ascii control keys where 1 through 26 map to Control-a through Control-z. For instance you
     could use the either of the following to change the escape character to Control-t.
      -echr 0x14; -echr 20
-incoming tcp:[host]:port[,to=maxport][,ipv4=on|off][,ipv6=on|off]
-incoming rdma:host:port[,ipv4=on|off][,ipv6=on|off]
     Prepare for incoming migration, listen on a given tcp port.
-incoming unix:socketpath
     Prepare for incoming migration, listen on a given unix socket.
-incoming fd:fd
     Accept incoming migration from a given filedescriptor.
-incoming exec:cmdline
     Accept incoming migration as an output from specified external command.
-incoming defer
     Wait for the URI to be specified via migrate_incoming. The monitor can be used to change settings (such as
     migration parameters) prior to issuing the migrate_incoming to allow the migration to begin.
-only-migratable
     Only allow migratable devices. Devices will not be allowed to enter an unmigratable state.
-nodefaults
     Don’t create default devices. Normally, QEMU sets the default devices like serial port, parallel port, virtual


2.2. Invocation                                                                                                      81
QEMU Documentation, Release 8.1.5


      console, monitor device, VGA adapter, floppy and CD-ROM drive and others. The -nodefaults option will
      disable all those default devices.
-chroot dir
     Deprecated, use ‘-run-with chroot=. . . ’ instead. Immediately before starting guest execution, chroot to the spec-
     ified directory. Especially useful in combination with -runas.
-runas user
     Immediately before starting guest execution, drop root privileges, switching to the specified user.
-prom-env variable=value
     Set OpenBIOS nvram variable to given value (PPC, SPARC only).

      qemu-system-sparc -prom-env 'auto-boot?=false' \
       -prom-env 'boot-device=sd(0,2,0):d' -prom-env 'boot-args=linux single'

      qemu-system-ppc -prom-env 'auto-boot?=false' \
       -prom-env 'boot-device=hd:2,\yaboot' \
       -prom-env 'boot-args=conf=hd:2,\yaboot.conf'

-semihosting
     Enable Semihosting mode (ARM, M68K, Xtensa, MIPS, Nios II, RISC-V only).


        Warning: Note that this allows guest direct access to the host filesystem, so should only be used with a
        trusted guest OS.


      See the -semihosting-config option documentation for further information about the facilities this enables.
-semihosting-config
[enable=on|off][,target=native|gdb|auto][,chardev=id][,userspace=on|off][,arg=str[,...]]
     Enable and configure Semihosting (ARM, M68K, Xtensa, MIPS, Nios II, RISC-V only).


        Warning: Note that this allows guest direct access to the host filesystem, so should only be used with a
        trusted guest OS.

      target=native|gdb|auto
          Defines where the semihosting calls will be addressed, to QEMU (native) or to GDB (gdb). The default
          is auto, which means gdb during debug sessions and native otherwise.
      chardev=str1
          Send the output to a chardev backend output for native or auto output when not in gdb
      userspace=on|off
          Allows code running in guest userspace to access the semihosting interface. The default is that only priv-
          ileged guest code can make semihosting calls. Note that setting userspace=on should only be used if all
          guest code is trusted (for example, in bare-metal test case code).
      arg=str1,arg=str2,...
          Allows the user to pass input arguments, and can be used multiple times to build up a list. The old-style
          -kernel/-append method of passing a command line is still supported for backward compatibility. If
          both the --semihosting-config arg and the -kernel/-append are specified, the former is passed to
          semihosting as it always takes precedence.
-old-param
     Old param mode (ARM only).



82                                                                                 Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5


-sandbox
arg[,obsolete=string][,elevateprivileges=string][,spawn=string][,resourcecontrol=string]
     Enable Seccomp mode 2 system call filter. ‘on’ will enable syscall filtering and ‘off’ will disable it. The default
     is ‘off’.
      obsolete=string
          Enable Obsolete system calls
      elevateprivileges=string
          Disable set*uid|gid system calls
      spawn=string
          Disable *fork and execve
      resourcecontrol=string
          Disable process affinity and schedular priority
-readconfig file
     Read device configuration from file. This approach is useful when you want to spawn QEMU process with many
     command line options but you don’t want to exceed the command line character limit.
-no-user-config
     The -no-user-config option makes QEMU not load any of the user-provided config files on sysconfdir.
-trace [[enable=]pattern][,events=file][,file=file]
     Specify tracing options.
      [enable=]PATTERN
            Immediately enable events matching PATTERN (either event name or a globbing pattern). This option
            is only available if QEMU has been compiled with the simple, log or ftrace tracing backend. To
            specify multiple events or patterns, specify the -trace option multiple times.
            Use -trace help to print a list of names of trace points.
      events=FILE
            Immediately enable events listed in FILE. The file must contain one event name (as listed in the
            trace-events-all file) per line; globbing patterns are accepted too. This option is only available
            if QEMU has been compiled with the simple, log or ftrace tracing backend.
      file=FILE
            Log output traces to FILE. This option is only available if QEMU has been compiled with the simple
            tracing backend.
-plugin file=file[,argname=argvalue]
     Load a plugin.
      file=file
          Load the given plugin from a shared library file.
      argname=argvalue
          Argument passed to the plugin. (Can be given multiple times.)
-async-teardown
     This option is deprecated and should no longer be used. The new option -run-with async-teardown=on is
     a replacement.
-run-with [async-teardown=on|off][,chroot=dir]
     Set QEMU process lifecycle options.
      async-teardown=on enables asynchronous teardown. A new process called “cleanup/<QEMU_PID>” will be
      created at startup sharing the address space with the main QEMU process, using clone. It will wait for the main


2.2. Invocation                                                                                                      83
QEMU Documentation, Release 8.1.5


      QEMU process to terminate completely, and then exit. This allows QEMU to terminate very quickly even if the
      guest was huge, leaving the teardown of the address space to the cleanup process. Since the cleanup process
      shares the same cgroups as the main QEMU process, accounting is performed correctly. This only works if the
      cleanup process is not forcefully killed with SIGKILL before the main QEMU process has terminated completely.
      chroot=dir can be used for doing a chroot to the specified directory immediately before starting the guest
      execution. This is especially useful in combination with -runas.
-msg [timestamp[=on|off]][,guest-name[=on|off]]
     Control error message format.
      timestamp=on|off
          Prefix messages with a timestamp. Default is off.
      guest-name=on|off
          Prefix messages with guest name but only if -name guest option is set otherwise the option is ignored.
          Default is off.
-dump-vmstate file
     Dump json-encoded vmstate information for current machine type to file in file
-enable-sync-profile
     Enable synchronization profiling.
-perfmap
     Generate a map file for Linux perf tools that will allow basic profiling information to be broken down into basic
     blocks.
-jitdump
     Generate a dump file for Linux perf tools that maps basic blocks to symbol names, line numbers and JITted code.


2.2.11 Generic object creation

-object typename[,prop1=value1,...]
     Create a new object of type typename setting properties in the order they are specified. Note that the ‘id’ property
     must be set. These objects are placed in the ‘/objects’ path.
      -object memory-backend-file,id=id,size=size,mem-path=dir,share=on|off,
      discard-data=on|off,merge=on|off,dump=on|off,prealloc=on|off,host-nodes=host-nodes,
      policy=default|preferred|bind|interleave,align=align,offset=offset,readonly=on|off
          Creates a memory file backend object, which can be used to back the guest RAM with huge pages.
            The id parameter is a unique ID that will be used to reference this memory region in other parameters, e.g.
            -numa, -device nvdimm, etc.
            The size option provides the size of the memory region, and accepts common suffixes, e.g. 500M.
            The mem-path provides the path to either a shared memory or huge page filesystem mount.
            The share boolean option determines whether the memory region is marked as private to QEMU, or shared.
            The latter allows a co-operating external process to access the QEMU memory region.
            The share is also required for pvrdma devices due to limitations in the RDMA API provided by Linux.
            Setting share=on might affect the ability to configure NUMA bindings for the memory backend under
            some circumstances, see Documentation/vm/numa_memory_policy.txt on the Linux kernel source tree for
            additional details.
            Setting the discard-data boolean option to on indicates that file contents can be destroyed when QEMU
            exits, to avoid unnecessarily flushing data to the backing file. Note that discard-data is only an optimiza-
            tion, and QEMU might not discard file contents if it aborts unexpectedly or is terminated using SIGKILL.


84                                                                                  Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


          The merge boolean option enables memory merge, also known as MADV_MERGEABLE, so that Kernel
          Samepage Merging will consider the pages for memory deduplication.
          Setting the dump boolean option to off excludes the memory from core dumps. This feature is also known
          as MADV_DONTDUMP.
          The prealloc boolean option enables memory preallocation.
          The host-nodes option binds the memory range to a list of NUMA host nodes.
          The policy option sets the NUMA policy to one of the following values:
          default
              default host policy
          preferred
              prefer the given host node list for allocation
          bind
              restrict memory allocation to the given host node list
          interleave
              interleave memory allocations across the given host node list
          The align option specifies the base address alignment when QEMU mmap(2) mem-path, and accepts
          common suffixes, eg 2M. Some backend store specified by mem-path requires an alignment different than
          the default one used by QEMU, eg the device DAX /dev/dax0.0 requires 2M alignment rather than 4K. In
          such cases, users can specify the required alignment via this option.
          The offset option specifies the offset into the target file that the region starts at. You can use this parameter
          to back multiple regions with a single file.
          The pmem option specifies whether the backing file specified by mem-path is in host persistent memory
          that can be accessed using the SNIA NVM programming model (e.g. Intel NVDIMM). If pmem is set to
          ‘on’, QEMU will take necessary operations to guarantee the persistence of its own writes to mem-path (e.g.
          in vNVDIMM label emulation and live migration). Also, we will map the backend-file with MAP_SYNC
          flag, which ensures the file metadata is in sync for mem-path in case of host crash or a power failure.
          MAP_SYNC requires support from both the host kernel (since Linux kernel 4.15) and the filesystem of
          mem-path mounted with DAX option.
          The readonly option specifies whether the backing file is opened read-only or read-write (default).
     -object
     memory-backend-ram,id=id,merge=on|off,dump=on|off,share=on|off,prealloc=on|off,
     size=size,host-nodes=host-nodes,policy=default|preferred|bind|interleave
         Creates a memory backend object, which can be used to back the guest RAM. Memory backend ob-
         jects offer more control than the -m option that is traditionally used to define guest RAM. Please refer
         to memory-backend-file for a description of the options.
     -object memory-backend-memfd,id=id,merge=on|off,dump=on|off,share=on|off,
     prealloc=on|off,size=size,host-nodes=host-nodes,
     policy=default|preferred|bind|interleave,seal=on|off,hugetlb=on|off,hugetlbsize=size
         Creates an anonymous memory file backend object, which allows QEMU to share the memory with an
         external process (e.g. when using vhost-user). The memory is allocated with memfd and optional sealing.
         (Linux only)
          The seal option creates a sealed-file, that will block further resizing the memory (‘on’ by default).
          The hugetlb option specify the file to be created resides in the hugetlbfs filesystem (since Linux 4.14).
          Used in conjunction with the hugetlb option, the hugetlbsize option specify the hugetlb page size on
          systems that support multiple hugetlb page sizes (it must be a power of 2 value supported by the system).




2.2. Invocation                                                                                                         85
QEMU Documentation, Release 8.1.5


          In some versions of Linux, the hugetlb option is incompatible with the seal option (requires at least
          Linux 4.16).
          Please refer to memory-backend-file for a description of the other options.
          The share boolean option is on by default with memfd.
     -object rng-builtin,id=id
         Creates a random number generator backend which obtains entropy from QEMU builtin functions. The id
         parameter is a unique ID that will be used to reference this entropy backend from the virtio-rng device.
         By default, the virtio-rng device uses this RNG backend.
     -object rng-random,id=id,filename=/dev/random
         Creates a random number generator backend which obtains entropy from a device on the host. The id
         parameter is a unique ID that will be used to reference this entropy backend from the virtio-rng device.
         The filename parameter specifies which file to obtain entropy from and if omitted defaults to /dev/
         urandom.
     -object rng-egd,id=id,chardev=chardevid
         Creates a random number generator backend which obtains entropy from an external daemon running on
         the host. The id parameter is a unique ID that will be used to reference this entropy backend from the
         virtio-rng device. The chardev parameter is the unique ID of a character device backend that provides
         the connection to the RNG daemon.
     -object
     tls-creds-anon,id=id,endpoint=endpoint,dir=/path/to/cred/dir,verify-peer=on|off
         Creates a TLS anonymous credentials object, which can be used to provide TLS support on network back-
         ends. The id parameter is a unique ID which network backends will use to access the credentials. The
         endpoint is either server or client depending on whether the QEMU network backend that uses the
         credentials will be acting as a client or as a server. If verify-peer is enabled (the default) then once
         the handshake is completed, the peer credentials will be verified, though this is a no-op for anonymous
         credentials.
          The dir parameter tells QEMU where to find the credential files. For server endpoints, this directory may
          contain a file dh-params.pem providing diffie-hellman parameters to use for the TLS server. If the file
          is missing, QEMU will generate a set of DH parameters at startup. This is a computationally expensive
          operation that consumes random pool entropy, so it is recommended that a persistent set of parameters be
          generated upfront and saved.
     -object
     tls-creds-psk,id=id,endpoint=endpoint,dir=/path/to/keys/dir[,username=username]
         Creates a TLS Pre-Shared Keys (PSK) credentials object, which can be used to provide TLS support on
         network backends. The id parameter is a unique ID which network backends will use to access the creden-
         tials. The endpoint is either server or client depending on whether the QEMU network backend that
         uses the credentials will be acting as a client or as a server. For clients only, username is the username
         which will be sent to the server. If omitted it defaults to “qemu”.
          The dir parameter tells QEMU where to find the keys file. It is called “dir/keys.psk” and contains “user-
          name:key” pairs. This file can most easily be created using the GnuTLS psktool program.
          For server endpoints, dir may also contain a file dh-params.pem providing diffie-hellman parameters to use
          for the TLS server. If the file is missing, QEMU will generate a set of DH parameters at startup. This is
          a computationally expensive operation that consumes random pool entropy, so it is recommended that a
          persistent set of parameters be generated up front and saved.
     -object tls-creds-x509,id=id,endpoint=endpoint,dir=/path/to/cred/dir,
     priority=priority,verify-peer=on|off,passwordid=id
         Creates a TLS anonymous credentials object, which can be used to provide TLS support on network back-
         ends. The id parameter is a unique ID which network backends will use to access the credentials. The
         endpoint is either server or client depending on whether the QEMU network backend that uses the


86                                                                              Chapter 2. System Emulation
                                                                               QEMU Documentation, Release 8.1.5


          credentials will be acting as a client or as a server. If verify-peer is enabled (the default) then once the
          handshake is completed, the peer credentials will be verified. With x509 certificates, this implies that the
          clients must be provided with valid client certificates too.
          The dir parameter tells QEMU where to find the credential files. For server endpoints, this directory may
          contain a file dh-params.pem providing diffie-hellman parameters to use for the TLS server. If the file
          is missing, QEMU will generate a set of DH parameters at startup. This is a computationally expensive
          operation that consumes random pool entropy, so it is recommended that a persistent set of parameters be
          generated upfront and saved.
          For x509 certificate credentials the directory will contain further files providing the x509 certificates.
          The certificates must be stored in PEM format, in filenames ca-cert.pem, ca-crl.pem (optional), server-
          cert.pem (only servers), server-key.pem (only servers), client-cert.pem (only clients), and client-key.pem
          (only clients).
          For the server-key.pem and client-key.pem files which contain sensitive private keys, it is possible to use
          an encrypted version by providing the passwordid parameter. This provides the ID of a previously created
          secret object containing the password for decryption.
          The priority parameter allows to override the global default priority used by gnutls. This can be useful if the
          system administrator needs to use a weaker set of crypto priorities for QEMU without potentially forcing
          the weakness onto all applications. Or conversely if one wants wants a stronger default for QEMU than
          for all other applications, they can do this through this parameter. Its format is a gnutls priority string as
          described at https://gnutls.org/manual/html_node/Priority-Strings.html.
     -object tls-cipher-suites,id=id,priority=priority
         Creates a TLS cipher suites object, which can be used to control the TLS cipher/protocol algorithms that
         applications are permitted to use.
          The id parameter is a unique ID which frontends will use to access the ordered list of permitted TLS cipher
          suites from the host.
          The priority parameter allows to override the global default priority used by gnutls. This can be useful
          if the system administrator needs to use a weaker set of crypto priorities for QEMU without potentially
          forcing the weakness onto all applications. Or conversely if one wants wants a stronger default for QEMU
          than for all other applications, they can do this through this parameter. Its format is a gnutls priority string
          as described at https://gnutls.org/manual/html_node/Priority-Strings.html.
          An example of use of this object is to control UEFI HTTPS Boot. The tls-cipher-suites object exposes the
          ordered list of permitted TLS cipher suites from the host side to the guest firmware, via fw_cfg. The list
          is represented as an array of IANA_TLS_CIPHER objects. The firmware uses the IANA_TLS_CIPHER
          array for configuring guest-side TLS.
          In the following example, the priority at which the host-side policy is retrieved is given by the priority
          property. Given that QEMU uses GNUTLS, priority=@SYSTEM may be used to refer to /etc/crypto-
          policies/back-ends/gnutls.config.
          # qemu-system-x86_64 \
              -object tls-cipher-suites,id=mysuite0,priority=@SYSTEM \
              -fw_cfg name=etc/edk2/https/ciphers,gen_id=mysuite0
     -object filter-buffer,id=id,netdev=netdevid,interval=t[,queue=all|rx|tx][,
     status=on|off][,position=head|tail|id=<id>][,insert=behind|before]
         Interval t can’t be 0, this filter batches the packet delivery: all packets arriving in a given interval on netdev
         netdevid are delayed until the end of the interval. Interval is in microseconds. status is optional that
         indicate whether the netfilter is on (enabled) or off (disabled), the default status for netfilter will be ‘on’.
          queue all|rx|tx is an option that can be applied to any netfilter.
          all: the filter is attached both to the receive and the transmit queue of the netdev (default).



2.2. Invocation                                                                                                         87
QEMU Documentation, Release 8.1.5


          rx: the filter is attached to the receive queue of the netdev, where it will receive packets sent to the netdev.
          tx: the filter is attached to the transmit queue of the netdev, where it will receive packets sent by the netdev.
          position head|tail|id=<id> is an option to specify where the filter should be inserted in the filter list. It can
          be applied to any netfilter.
          head: the filter is inserted at the head of the filter list, before any existing filters.
          tail: the filter is inserted at the tail of the filter list, behind any existing filters (default).
          id=<id>: the filter is inserted before or behind the filter specified by <id>, see the insert option below.
          insert behind|before is an option to specify where to insert the new filter relative to the one specified with
          position=id=<id>. It can be applied to any netfilter.
          before: insert before the specified filter.
          behind: insert behind the specified filter (default).
     -object filter-mirror,id=id,netdev=netdevid,outdev=chardevid,queue=all|rx|tx[,
     vnet_hdr_support][,position=head|tail|id=<id>][,insert=behind|before]
         filter-mirror on netdev netdevid,mirror net packet to chardevchardevid, if it has the vnet_hdr_support flag,
         filter-mirror will mirror packet with vnet_hdr_len.
     -object filter-redirector,id=id,netdev=netdevid,indev=chardevid,outdev=chardevid,
     queue=all|rx|tx[,vnet_hdr_support][,position=head|tail|id=<id>][,
     insert=behind|before]
         filter-redirector on netdev netdevid,redirect filter’s net packet to chardev chardevid,and redirect indev’s
         packet to filter.if it has the vnet_hdr_support flag, filter-redirector will redirect packet with vnet_hdr_len.
         Create a filter-redirector we need to differ outdev id from indev id, id can not be the same. we can just use
         indev or outdev, but at least one of indev or outdev need to be specified.
     -object filter-rewriter,id=id,netdev=netdevid,queue=all|rx|tx,[vnet_hdr_support][,
     position=head|tail|id=<id>][,insert=behind|before]
         Filter-rewriter is a part of COLO project.It will rewrite tcp packet to secondary from primary to keep
         secondary tcp connection,and rewrite tcp packet to primary from secondary make tcp packet can be handled
         by client.if it has the vnet_hdr_support flag, we can parse packet with vnet header.
          usage: colo secondary: -object filter-redirector,id=f1,netdev=hn0,queue=tx,indev=red0 -object filter-
          redirector,id=f2,netdev=hn0,queue=rx,outdev=red1 -object filter-rewriter,id=rew0,netdev=hn0,queue=all
     -object filter-dump,id=id,netdev=dev[,file=filename][,maxlen=len][,
     position=head|tail|id=<id>][,insert=behind|before]
         Dump the network traffic on netdev dev to the file specified by filename. At most len bytes (64k by default)
         per packet are stored. The file format is libpcap, so it can be analyzed with tools such as tcpdump or
         Wireshark.
     -object colo-compare,id=id,primary_in=chardevid,secondary_in=chardevid,
     outdev=chardevid,iothread=id[,vnet_hdr_support][,notify_dev=id][,
     compare_timeout=@var{ms}][,expired_scan_cycle=@var{ms}][,max_queue_size=@var{size}]
         Colo-compare gets packet from primary_in chardevid and secondary_in, then compare whether the payload
         of primary packet and secondary packet are the same. If same, it will output primary packet to out_dev, else
         it will notify COLO-framework to do checkpoint and send primary packet to out_dev. In order to improve
         efficiency, we need to put the task of comparison in another iothread. If it has the vnet_hdr_support flag,
         colo compare will send/recv packet with vnet_hdr_len. The compare_timeout=@var{ms} determines the
         maximum time of the colo-compare hold the packet. The expired_scan_cycle=@var{ms} is to set the
         period of scanning expired primary node network packets. The max_queue_size=@var{size} is to set the
         max compare queue size depend on user environment. If user want to use Xen COLO, need to add the
         notify_dev to notify Xen colo-frame to do checkpoint.
          COLO-compare must be used with the help of filter-mirror, filter-redirector and filter-rewriter.



88                                                                                      Chapter 2. System Emulation
                                                        QEMU Documentation, Release 8.1.5



         KVM COLO

         primary:
         -netdev tap,id=hn0,vhost=off,script=/etc/qemu-ifup,downscript=/etc/qemu-ifdown
         -device e1000,id=e0,netdev=hn0,mac=52:a4:00:12:78:66
         -chardev socket,id=mirror0,host=3.3.3.3,port=9003,server=on,wait=off
         -chardev socket,id=compare1,host=3.3.3.3,port=9004,server=on,wait=off
         -chardev socket,id=compare0,host=3.3.3.3,port=9001,server=on,wait=off
         -chardev socket,id=compare0-0,host=3.3.3.3,port=9001
         -chardev socket,id=compare_out,host=3.3.3.3,port=9005,server=on,wait=off
         -chardev socket,id=compare_out0,host=3.3.3.3,port=9005
         -object iothread,id=iothread1
         -object filter-mirror,id=m0,netdev=hn0,queue=tx,outdev=mirror0
         -object filter-redirector,netdev=hn0,id=redire0,queue=rx,indev=compare_out
         -object filter-redirector,netdev=hn0,id=redire1,queue=rx,outdev=compare0
         -object colo-compare,id=comp0,primary_in=compare0-0,secondary_in=compare1,
          ˓→outdev=compare_out0,iothread=iothread1



         secondary:
         -netdev tap,id=hn0,vhost=off,script=/etc/qemu-ifup,down script=/etc/qemu-ifdown
         -device e1000,netdev=hn0,mac=52:a4:00:12:78:66
         -chardev socket,id=red0,host=3.3.3.3,port=9003
         -chardev socket,id=red1,host=3.3.3.3,port=9004
         -object filter-redirector,id=f1,netdev=hn0,queue=tx,indev=red0
         -object filter-redirector,id=f2,netdev=hn0,queue=rx,outdev=red1


         Xen COLO

         primary:
         -netdev tap,id=hn0,vhost=off,script=/etc/qemu-ifup,downscript=/etc/qemu-ifdown
         -device e1000,id=e0,netdev=hn0,mac=52:a4:00:12:78:66
         -chardev socket,id=mirror0,host=3.3.3.3,port=9003,server=on,wait=off
         -chardev socket,id=compare1,host=3.3.3.3,port=9004,server=on,wait=off
         -chardev socket,id=compare0,host=3.3.3.3,port=9001,server=on,wait=off
         -chardev socket,id=compare0-0,host=3.3.3.3,port=9001
         -chardev socket,id=compare_out,host=3.3.3.3,port=9005,server=on,wait=off
         -chardev socket,id=compare_out0,host=3.3.3.3,port=9005
         -chardev socket,id=notify_way,host=3.3.3.3,port=9009,server=on,wait=off
         -object filter-mirror,id=m0,netdev=hn0,queue=tx,outdev=mirror0
         -object filter-redirector,netdev=hn0,id=redire0,queue=rx,indev=compare_out
         -object filter-redirector,netdev=hn0,id=redire1,queue=rx,outdev=compare0
         -object iothread,id=iothread1
         -object colo-compare,id=comp0,primary_in=compare0-0,secondary_in=compare1,
          ˓→outdev=compare_out0,notify_dev=nofity_way,iothread=iothread1



         secondary:
         -netdev tap,id=hn0,vhost=off,script=/etc/qemu-ifup,down script=/etc/qemu-ifdown
         -device e1000,netdev=hn0,mac=52:a4:00:12:78:66
         -chardev socket,id=red0,host=3.3.3.3,port=9003
         -chardev socket,id=red1,host=3.3.3.3,port=9004
         -object filter-redirector,id=f1,netdev=hn0,queue=tx,indev=red0
         -object filter-redirector,id=f2,netdev=hn0,queue=rx,outdev=red1



2.2. Invocation                                                                           89
QEMU Documentation, Release 8.1.5


          If you want to know the detail of above command line, you can read the colo-compare git log.
     -object cryptodev-backend-builtin,id=id[,queues=queues]
         Creates a cryptodev backend which executes crypto operations from the QEMU cipher APIs. The id param-
         eter is a unique ID that will be used to reference this cryptodev backend from the virtio-crypto device.
         The queues parameter is optional, which specify the queue number of cryptodev backend, the default of
         queues is 1.
          # qemu-system-x86_64 \
            [...] \
                -object cryptodev-backend-builtin,id=cryptodev0 \
                -device virtio-crypto-pci,id=crypto0,cryptodev=cryptodev0 \
            [...]
     -object cryptodev-vhost-user,id=id,chardev=chardevid[,queues=queues]
         Creates a vhost-user cryptodev backend, backed by a chardev chardevid. The id parameter is a unique ID
         that will be used to reference this cryptodev backend from the virtio-crypto device. The chardev should
         be a unix domain socket backed one. The vhost-user uses a specifically defined protocol to pass vhost ioctl
         replacement messages to an application on the other end of the socket. The queues parameter is optional,
         which specify the queue number of cryptodev backend for multiqueue vhost-user, the default of queues is
         1.
          # qemu-system-x86_64 \
            [...] \
                -chardev socket,id=chardev0,path=/path/to/socket \
                -object cryptodev-vhost-user,id=cryptodev0,chardev=chardev0 \
                -device virtio-crypto-pci,id=crypto0,cryptodev=cryptodev0 \
            [...]
     -object secret,id=id,data=string,format=raw|base64[,keyid=secretid,iv=string]
     -object secret,id=id,file=filename,format=raw|base64[,keyid=secretid,iv=string]
         Defines a secret to store a password, encryption key, or some other sensitive data. The sensitive data can
         either be passed directly via the data parameter, or indirectly via the file parameter. Using the data parameter
         is insecure unless the sensitive data is encrypted.
          The sensitive data can be provided in raw format (the default), or base64. When encoded as JSON, the raw
          format only supports valid UTF-8 characters, so base64 is recommended for sending binary data. QEMU
          will convert from which ever format is provided to the format it needs internally. eg, an RBD password can
          be provided in raw format, even though it will be base64 encoded when passed onto the RBD sever.
          For added protection, it is possible to encrypt the data associated with a secret using the AES-256-CBC
          cipher. Use of encryption is indicated by providing the keyid and iv parameters. The keyid parameter
          provides the ID of a previously defined secret that contains the AES-256 decryption key. This key should
          be 32-bytes long and be base64 encoded. The iv parameter provides the random initialization vector used
          for encryption of this particular secret and should be a base64 encrypted string of the 16-byte IV.
          The simplest (insecure) usage is to provide the secret inline
          # qemu-system-x86_64 -object secret,id=sec0,data=letmein,format=raw
          The simplest secure usage is to provide the secret via a file
          #     printf     “letmein”  >     mypasswd.txt          #       QEMU_SYSTEM_MACRO               -object    se-
          cret,id=sec0,file=mypasswd.txt,format=raw
          For greater security, AES-256-CBC should be used. To illustrate usage, consider the openssl command
          line tool which can encrypt the data. Note that when encrypting, the plaintext must be padded to the cipher
          block size (32 bytes) using the standard PKCS#5/6 compatible padding algorithm.
          First a master key needs to be created in base64 encoding:


90                                                                                  Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5



          # openssl rand -base64 32 > key.b64
          # KEY=$(base64 -d key.b64 | hexdump               -v -e '/1 "%02X"')

          Each secret to be encrypted needs to have a random initialization vector generated. These do not need to
          be kept secret

          # openssl rand -base64 16 > iv.b64
          # IV=$(base64 -d iv.b64 | hexdump -v -e '/1 "%02X"')

          The secret to be defined can now be encrypted, in this case we’re telling openssl to base64 encode the result,
          but it could be left as raw bytes if desired.

          # SECRET=$(printf "letmein" |
                     openssl enc -aes-256-cbc -a -K $KEY -iv $IV)

          When launching QEMU, create a master secret pointing to key.b64 and specify that to be used to decrypt
          the user password. Pass the contents of iv.b64 to the second secret
          # qemu-system-x86_64 \
              -object secret,id=secmaster0,format=base64,file=key.b64 \
              -object secret,id=sec0,keyid=secmaster0,format=base64,\
                  data=$SECRET,iv=$(<iv.b64)
     -object sev-guest,id=id,cbitpos=cbitpos,reduced-phys-bits=val,[sev-device=string,
     policy=policy,handle=handle,dh-cert-file=file,session-file=file,
     kernel-hashes=on|off]
         Create a Secure Encrypted Virtualization (SEV) guest object, which can be used to provide the guest mem-
         ory encryption support on AMD processors.
          When memory encryption is enabled, one of the physical address bit (aka the C-bit) is utilized to mark if a
          memory page is protected. The cbitpos is used to provide the C-bit position. The C-bit position is Host
          family dependent hence user must provide this value. On EPYC, the value should be 47.
          When memory encryption is enabled, we loose certain bits in physical address space.                The
          reduced-phys-bits is used to provide the number of bits we loose in physical address space. Simi-
          lar to C-bit, the value is Host family dependent. On EPYC, a guest will lose a maximum of 1 bit, so the
          value should be 1.
          The sev-device provides the device file to use for communicating with the SEV firmware running inside
          AMD Secure Processor. The default device is ‘/dev/sev’. If hardware supports memory encryption then
          /dev/sev devices are created by CCP driver.
          The policy provides the guest policy to be enforced by the SEV firmware and restrict what configuration
          and operational commands can be performed on this guest by the hypervisor. The policy should be provided
          by the guest owner and is bound to the guest and cannot be changed throughout the lifetime of the guest.
          The default is 0.
          If guest policy allows sharing the key with another SEV guest then handle can be use to provide handle
          of the guest from which to share the key.
          The dh-cert-file and session-file provides the guest owner’s Public Diffie-Hillman key defined in
          SEV spec. The PDH and session parameters are used for establishing a cryptographic session with the
          guest owner to negotiate keys used for attestation. The file must be encoded in base64.
          The kernel-hashes adds the hashes of given kernel/initrd/ cmdline to a designated guest firmware page
          for measured Linux boot with -kernel. The default is off. (Since 6.2)
          e.g to launch a SEV guest



2.2. Invocation                                                                                                      91
QEMU Documentation, Release 8.1.5


          # qemu-system-x86_64 \
              ...... \
              -object sev-guest,id=sev0,cbitpos=47,reduced-phys-bits=1 \
              -machine ...,memory-encryption=sev0 \
              .....
     -object authz-simple,id=id,identity=string
         Create an authorization object that will control access to network services.
          The identity parameter is identifies the user and its format depends on the network service that autho-
          rization object is associated with. For authorizing based on TLS x509 certificates, the identity must be the
          x509 distinguished name. Note that care must be taken to escape any commas in the distinguished name.
          An example authorization object to validate a x509 distinguished name would look like:
          # qemu-system-x86_64 \
               ... \
               -object 'authz-simple,id=auth0,identity=CN=laptop.example.com,,O=Example␣
           ˓→Org,,L=London,,ST=London,,C=GB' \

               ...
          Note the use of quotes due to the x509 distinguished name containing whitespace, and escaping of ‘,’.
     -object authz-listfile,id=id,filename=path,refresh=on|off
         Create an authorization object that will control access to network services.
          The filename parameter is the fully qualified path to a file containing the access control list rules in JSON
          format.
          An example set of rules that match against SASL usernames might look like:

          {
              "rules": [
                 { "match": "fred", "policy": "allow", "format": "exact" },
                 { "match": "bob", "policy": "allow", "format": "exact" },
                 { "match": "danb", "policy": "deny", "format": "glob" },
                 { "match": "dan*", "policy": "allow", "format": "exact" },
              ],
              "policy": "deny"
          }

          When checking access the object will iterate over all the rules and the first rule to match will have its policy
          value returned as the result. If no rules match, then the default policy value is returned.
          The rules can either be an exact string match, or they can use the simple UNIX glob pattern matching to
          allow wildcards to be used.
          If refresh is set to true the file will be monitored and automatically reloaded whenever its content changes.
          As with the authz-simple object, the format of the identity strings being matched depends on the network
          service, but is usually a TLS x509 distinguished name, or a SASL username.
          An example authorization object to validate a SASL username would look like:
          # qemu-system-x86_64 \
              ... \
              -object authz-simple,id=auth0,filename=/etc/qemu/vnc-sasl.acl,refresh=on \
              ...
     -object authz-pam,id=id,service=string
         Create an authorization object that will control access to network services.


92                                                                                  Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


          The service parameter provides the name of a PAM service to use for authorization. It requires that a file
          /etc/pam.d/service exist to provide the configuration for the account subsystem.
          An example authorization object to validate a TLS x509 distinguished name would look like:
          # qemu-system-x86_64 \
              ... \
              -object authz-pam,id=auth0,service=qemu-vnc \
              ...
          There would then be a corresponding config file for PAM at /etc/pam.d/qemu-vnc that contains:

          account requisite pam_listfile.so item=user sense=allow \
                     file=/etc/qemu/vnc.allow

          Finally the /etc/qemu/vnc.allow file would contain the list of x509 distinguished names that are per-
          mitted access

          CN=laptop.example.com,O=Example Home,L=London,ST=London,C=GB

     -object iothread,id=id,poll-max-ns=poll-max-ns,poll-grow=poll-grow,
     poll-shrink=poll-shrink,aio-max-batch=aio-max-batch
         Creates a dedicated event loop thread that devices can be assigned to. This is known as an IOThread.
         By default device emulation happens in vCPU threads or the main event loop thread. This can become a
         scalability bottleneck. IOThreads allow device emulation and I/O to run on other host CPUs.
          The id parameter is a unique ID that will be used to reference this IOThread from -device ...,
          iothread=id. Multiple devices can be assigned to an IOThread. Note that not all devices support an
          iothread parameter.
          The query-iothreads QMP command lists IOThreads and reports their thread IDs so that the user can
          configure host CPU pinning/affinity.
          IOThreads use an adaptive polling algorithm to reduce event loop latency. Instead of entering a blocking
          system call to monitor file descriptors and then pay the cost of being woken up when an event occurs, the
          polling algorithm spins waiting for events for a short time. The algorithm’s default parameters are suitable
          for many cases but can be adjusted based on knowledge of the workload and/or host device latency.
          The poll-max-ns parameter is the maximum number of nanoseconds to busy wait for events. Polling can
          be disabled by setting this value to 0.
          The poll-grow parameter is the multiplier used to increase the polling time when the algorithm detects it
          is missing events due to not polling long enough.
          The poll-shrink parameter is the divisor used to decrease the polling time when the algorithm detects it
          is spending too long polling without encountering events.
          The aio-max-batch parameter is the maximum number of requests in a batch for the AIO engine, 0 means
          that the engine will use its default.
          The IOThread parameters can be modified at run-time using the qom-set command (where iothread1 is
          the IOThread’s id):

          (qemu) qom-set /objects/iothread1 poll-max-ns 100000




2.2. Invocation                                                                                                    93
QEMU Documentation, Release 8.1.5


2.2.12 Device URL Syntax

In addition to using normal file images for the emulated storage devices, QEMU can also use networked resources such
as iSCSI devices. These are specified using a special URL syntax.
iSCSI
     iSCSI support allows QEMU to access iSCSI resources directly and use as images for the guest storage. Both
     disk and cdrom images are supported.
      Syntax for specifying iSCSI LUNs is “iscsi://<target-ip>[:<port>]/<target-iqn>/<lun>”
      By default qemu will use the iSCSI initiator-name ‘iqn.2008-11.org.linux-kvm[:<name>]’ but this can also be
      set from the command line or a configuration file.
      Since version QEMU 2.4 it is possible to specify a iSCSI request timeout to detect stalled requests and force a
      reestablishment of the session. The timeout is specified in seconds. The default is 0 which means no timeout.
      Libiscsi 1.15.0 or greater is required for this feature.
      Example (without authentication):
      qemu-system-x86_64 -iscsi initiator-name=iqn.2001-04.com.example:my-initiator \
                       -cdrom iscsi://192.0.2.1/iqn.2001-04.com.example/2 \
                       -drive file=iscsi://192.0.2.1/iqn.2001-04.com.example/1
      Example (CHAP username/password via URL):
      qemu-system-x86_64 -drive file=iscsi://user%password@192.0.2.1/iqn.2001-04.com.
       ˓→example/1


      Example (CHAP username/password via environment variables):
      LIBISCSI_CHAP_USERNAME="user" \
      LIBISCSI_CHAP_PASSWORD="password" \
      qemu-system-x86_64 -drive file=iscsi://192.0.2.1/iqn.2001-04.com.example/1
NBD
      QEMU supports NBD (Network Block Devices) both using TCP protocol as well as Unix Domain Sockets. With
      TCP, the default port is 10809.
      Syntax for specifying a NBD device using TCP, in preferred URI form: “nbd://<server-ip>[:<port>]/[<export>]”
      Syntax for specifying a NBD device using Unix Domain Sockets; remember that ‘?’ is a shell glob character and
      may need quoting: “nbd+unix:///[<export>]?socket=<domain-socket>”
      Older syntax that is also recognized: “nbd:<server-ip>:<port>[:exportname=<export>]”
      Syntax for specifying a NBD               device   using    Unix    Domain     Sockets    “nbd:unix:<domain-
      socket>[:exportname=<export>]”
      Example for TCP
      qemu-system-x86_64 --drive file=nbd:192.0.2.1:30000
      Example for Unix Domain Sockets
      qemu-system-x86_64 --drive file=nbd:unix:/tmp/nbd-socket
SSH
      QEMU supports SSH (Secure Shell) access to remote disks.
      Examples:
      qemu-system-x86_64 -drive file=ssh://user@host/path/to/disk.img
      qemu-system-x86_64 -drive file.driver=ssh,file.user=user,file.host=host,file.
       ˓→port=22,file.path=/path/to/disk.img




94                                                                               Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


      Currently authentication must be done using ssh-agent. Other authentication methods may be supported in future.
GlusterFS
     GlusterFS is a user space distributed file system. QEMU supports the use of GlusterFS volumes for hosting VM
     disk images using TCP, Unix Domain Sockets and RDMA transport protocols.
      Syntax for specifying a VM disk image on GlusterFS volume is

      URI:
      gluster[+type]://[host[:port]]/volume/path[?socket=...][,debug=N][,logfile=...]

      JSON:
      'json:{"driver":"qcow2","file":{"driver":"gluster","volume":"testvol","path":"a.img
       ˓→","debug":N,"logfile":"...",

                                       "server":[{"type":"tcp","host":"...","port":"..."},
                                                 {"type":"unix","socket":"..."}]}}'

      Example
      URI:
      qemu-system-x86_64 --drive file=gluster://192.0.2.1/testvol/a.img,
                                     file.debug=9,file.logfile=/var/log/qemu-gluster.log

      JSON:
      qemu-system-x86_64 'json:{"driver":"qcow2",
                                 "file":{"driver":"gluster",
                                          "volume":"testvol","path":"a.img",
                                          "debug":9,"logfile":"/var/log/qemu-gluster.log",
                                          "server":[{"type":"tcp","host":"1.2.3.4",
       ˓→"port":24007},

                                                    {"type":"unix","socket":"/var/run/
       ˓→glusterd.socket"}]}}'

      qemu-system-x86_64 -drive driver=qcow2,file.driver=gluster,file.volume=testvol,file.
       ˓→path=/path/a.img,

                                             file.debug=9,file.logfile=/var/log/
       ˓→qemu-gluster.log,

                                             file.server.0.type=tcp,file.server.0.host=1.2.
       ˓→3.4,file.server.0.port=24007,

                                             file.server.1.type=unix,file.server.1.socket=/
       ˓→var/run/glusterd.socket


      See also http://www.gluster.org.
HTTP/HTTPS/FTP/FTPS
     QEMU supports read-only access to files accessed over http(s) and ftp(s).
      Syntax using a single filename:

      <protocol>://[<username>[:<password>]@]<host>/<path>

      where:
      protocol
          ‘http’, ‘https’, ‘ftp’, or ‘ftps’.
      username
          Optional username for authentication to the remote server.




2.2. Invocation                                                                                                   95
QEMU Documentation, Release 8.1.5


     password
         Optional password for authentication to the remote server.
     host
            Address of the remote server.
     path
            Path on the remote server, including any query string.
     The following options are also supported:
     url
            The full URL when passing options to the driver explicitly.
     readahead
         The amount of data to read ahead with each range request to the remote server. This value may optionally
         have the suffix ‘T’, ‘G’, ‘M’, ‘K’, ‘k’ or ‘b’. If it does not have a suffix, it will be assumed to be in bytes.
         The value must be a multiple of 512 bytes. It defaults to 256k.
     sslverify
         Whether to verify the remote server’s certificate when connecting over SSL. It can have the value ‘on’ or
         ‘off’. It defaults to ‘on’.
     cookie
         Send this cookie (it can also be a list of cookies separated by ‘;’) with each outgoing request. Only supported
         when using protocols such as HTTP which support cookies, otherwise ignored.
     timeout
         Set the timeout in seconds of the CURL connection. This timeout is the time that CURL waits for a response
         from the remote server to get the size of the image to be downloaded. If not set, the default timeout of 5
         seconds is used.
     Note that when passing options to qemu explicitly, driver is the value of <protocol>.
     Example: boot from a remote Fedora 20 live ISO image
     qemu-system-x86_64 --drive media=cdrom,file=https://archives.fedoraproject.org/pub/
      ˓→archive/fedora/linux/releases/20/Live/x86_64/Fedora-Live-Desktop-x86_64-20-1.iso,

      ˓→readonly



     qemu-system-x86_64 --drive media=cdrom,file.driver=http,file.url=http:/
      ˓→/archives.fedoraproject.org/pub/fedora/linux/releases/20/Live/x86_64/

      ˓→Fedora-Live-Desktop-x86_64-20-1.iso,readonly


     Example: boot from a remote Fedora 20 cloud image using a local overlay for writes, copy-on-read, and a
     readahead of 64k
     qemu-img create -f qcow2 -o backing_file='json:{"file.driver":"http",,␣
      ˓→"file.url":"http://archives.fedoraproject.org/pub/archive/fedora/linux/

     releases/20/Images/x86_64/Fedora\unhbox\voidb@x\kern\z@\char‘\protect\
     discretionary{\char\defaulthyphenchar}{}{}x86_64\unhbox\voidb@x\kern\z@\char‘\
     protect\discretionary{\char\defaulthyphenchar}{}{}20\unhbox\voidb@x\kern\z@\char‘\
     protect\discretionary{\char\defaulthyphenchar}{}{}20131211.1\unhbox\voidb@x\kern\
     z@\char‘\protect\discretionary{\char\defaulthyphenchar}{}{}sda.qcow2",, "file.
      ˓→readahead":"64k"}' /tmp/Fedora-x86_64-20-20131211.1-sda.qcow2



     qemu-system-x86_64 -drive file=/tmp/Fedora-x86_64-20-20131211.1-sda.qcow2,
      ˓→copy-on-read=on


     Example: boot from an image stored on a VMware vSphere server with a self-signed certificate using a local
     overlay for writes, a readahead of 64k and a timeout of 10 seconds.


96                                                                                  Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


      qemu-img create -f qcow2 -o backing_file='json:{"file.driver":"https",, "file.
       ˓→url":"https://user:password@vsphere.example.com/folder/test/test\unhbox\voidb@

      x\kern\z@\char‘\protect\discretionary{\char\defaulthyphenchar}{}{}flat.vmdk?dcPath=
      Datacenter&dsName=datastore1",, "file.sslverify":"off",, "file.readahead":"64k",,␣
       ˓→"file.timeout":10}' /tmp/test.qcow2



      qemu-system-x86_64 -drive file=/tmp/test.qcow2


2.3 Device Emulation

QEMU supports the emulation of a large number of devices from peripherals such network cards and USB devices
to integrated systems on a chip (SoCs). Configuration of these is often a source of confusion so it helps to have an
understanding of some of the terms used to describes devices within QEMU.


2.3.1 Common Terms

Device Front End

A device front end is how a device is presented to the guest. The type of device presented should match the hardware
that the guest operating system is expecting to see. All devices can be specified with the --device command line
option. Running QEMU with the command line options --device help will list all devices it is aware of. Using the
command line --device foo,help will list the additional configuration options available for that device.
A front end is often paired with a back end, which describes how the host’s resources are used in the emulation.


Device Buses

Most devices will exist on a BUS of some sort. Depending on the machine model you choose (-M foo) a number
of buses will have been automatically created. In most cases the BUS a device is attached to can be inferred, for
example PCI devices are generally automatically allocated to the next free address of first PCI bus found. However in
complicated configurations you can explicitly specify what bus (bus=ID) a device is attached to along with its address
(addr=N).
Some devices, for example a PCI SCSI host controller, will add an additional buses to the system that other devices
can be attached to. A hypothetical chain of devices might look like:
      –device foo,bus=pci.0,addr=0,id=foo –device bar,bus=foo.0,addr=1,id=baz
which would be a bar device (with the ID of baz) which is attached to the first foo bus (foo.0) at address 1. The foo
device which provides that bus is itself is attached to the first PCI bus (pci.0).


Device Back End

The back end describes how the data from the emulated device will be processed by QEMU. The configuration of
the back end is usually specific to the class of device being emulated. For example serial devices will be backed by
a --chardev which can redirect the data to a file or socket or some other system. Storage devices are handled by
--blockdev which will specify how blocks are handled, for example being stored in a qcow2 file or accessing a raw
host disk partition. Back ends can sometimes be stacked to implement features like snapshots.
While the choice of back end is generally transparent to the guest, there are cases where features will not be reported
to the guest if the back end is unable to support it.




2.3. Device Emulation                                                                                               97
QEMU Documentation, Release 8.1.5


Device Pass Through

Device pass through is where the device is actually given access to the underlying hardware. This can be as simple as
exposing a single USB device on the host system to the guest or dedicating a video card in a PCI slot to the exclusive
use of the guest.


2.3.2 Emulated Devices

CAN Bus Emulation Support

The CAN bus emulation provides mechanism to connect multiple emulated CAN controller chips together by one or
multiple CAN busses (the controller device “canbus” parameter). The individual busses can be connected to host
system CAN API (at this time only Linux SocketCAN is supported).
The concept of busses is generic and different CAN controllers can be implemented.
The initial submission implemented SJA1000 controller which is common and well supported by by drivers for the
most operating systems.
The PCI addon card hardware has been selected as the first CAN interface to implement because such device can be
easily connected to systems with different CPU architectures (x86, PowerPC, Arm, etc.).
In 2020, CTU CAN FD controller model has been added as part of the bachelor thesis of Jan Charvat. This controller
is complete open-source/design/hardware solution. The core designer of the project is Ondrej Ille, the financial support
has been provided by CTU, and more companies including Volkswagen subsidiaries.
The project has been initially started in frame of RTEMS GSoC 2013 slot by Jin Yang under our mentoring The initial
idea was to provide generic CAN subsystem for RTEMS. But lack of common environment for code and RTEMS testing
lead to goal change to provide environment which provides complete emulated environment for testing and RTEMS
GSoC slot has been donated to work on CAN hardware emulation on QEMU.


Examples how to use CAN emulation for SJA1000 based boards

When QEMU with CAN PCI support is compiled then one of the next CAN boards can be selected
  (1) CAN bus Kvaser PCI CAN-S (single SJA1000 channel) board. QEMU startup options:

      -object can-bus,id=canbus0
      -device kvaser_pci,canbus=canbus0

Add “can-host-socketcan” object to connect device to host system CAN bus:

-object can-host-socketcan,id=canhost0,if=can0,canbus=canbus0

  (2) CAN bus PCM-3680I PCI (dual SJA1000 channel) emulation:

      -object can-bus,id=canbus0
      -device pcm3680_pci,canbus0=canbus0,canbus1=canbus0

Another example:

-object can-bus,id=canbus0
-object can-bus,id=canbus1
-device pcm3680_pci,canbus0=canbus0,canbus1=canbus1

  (3) CAN bus MIOe-3680 PCI (dual SJA1000 channel) emulation:


98                                                                                 Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5



      -device mioe3680_pci,canbus0=canbus0

The ‘’kvaser_pci” board/device model is compatible with and has been tested with the ‘’kvaser_pci” driver included
in mainline Linux kernel. The tested setup was Linux 4.9 kernel on the host and guest side.
Example for qemu-system-x86_64:

qemu-system-x86_64 -accel kvm -kernel /boot/vmlinuz-4.9.0-4-amd64 \
  -initrd ramdisk.cpio \
  -virtfs local,path=shareddir,security_model=none,mount_tag=shareddir \
  -object can-bus,id=canbus0 \
  -object can-host-socketcan,id=canhost0,if=can0,canbus=canbus0 \
  -device kvaser_pci,canbus=canbus0 \
  -nographic -append "console=ttyS0"

Example for qemu-system-arm:

qemu-system-arm -cpu arm1176 -m 256 -M versatilepb \
  -kernel kernel-qemu-arm1176-versatilepb \
  -hda rpi-wheezy-overlay \
  -append "console=ttyAMA0 root=/dev/sda2 ro init=/sbin/init-overlay" \
  -nographic \
  -virtfs local,path=shareddir,security_model=none,mount_tag=shareddir \
  -object can-bus,id=canbus0 \
  -object can-host-socketcan,id=canhost0,if=can0,canbus=canbus0 \
  -device kvaser_pci,canbus=canbus0,host=can0 \

The CAN interface of the host system has to be configured for proper bitrate and set up. Configuration is not propagated
from emulated devices through bus to the physical host device. Example configuration for 1 Mbit/s:

ip link set can0 type can bitrate 1000000
ip link set can0 up

Virtual (host local only) can interface can be used on the host side instead of physical interface:

ip link add dev can0 type vcan

The CAN interface on the host side can be used to analyze CAN traffic with “candump” command which is included
in “can-utils”:

candump can0


CTU CAN FD support examples

This open-source core provides CAN FD support. CAN FD drames are delivered even to the host systems when
SocketCAN interface is found CAN FD capable.
The PCIe board emulation is provided for now (the device identifier is ctucan_pci). The default build defines two CTU
CAN FD cores on the board.
Example how to connect the canbus0-bus (virtual wire) to the host Linux system (SocketCAN used) and to both CTU
CAN FD cores emulated on the corresponding PCI card expects that host system CAN bus is setup according to the
previous SJA1000 section:




2.3. Device Emulation                                                                                                99
QEMU Documentation, Release 8.1.5



qemu-system-x86_64 -enable-kvm -kernel /boot/vmlinuz-4.19.52+ \
    -initrd ramdisk.cpio \
    -virtfs local,path=shareddir,security_model=none,mount_tag=shareddir \
    -vga cirrus \
    -append "console=ttyS0" \
    -object can-bus,id=canbus0-bus \
    -object can-host-socketcan,if=can0,canbus=canbus0-bus,id=canbus0-socketcan \
    -device ctucan_pci,canbus0=canbus0-bus,canbus1=canbus0-bus \
    -nographic

Setup of CTU CAN FD controller in a guest Linux system:

insmod ctucanfd.ko || modprobe ctucanfd
insmod ctucanfd_pci.ko || modprobe ctucanfd_pci

for ifc in /sys/class/net/can* ; do
  if [ -e $ifc/device/vendor ] ; then
    if ! grep -q 0x1760 $ifc/device/vendor ; then
       continue;
    fi
  else
    continue;
  fi
  if [ -e $ifc/device/device ] ; then
      if ! grep -q 0xff00 $ifc/device/device ; then
         continue;
      fi
  else
     continue;
  fi
  ifc=$(basename $ifc)
  /bin/ip link set $ifc type can bitrate 1000000 dbitrate 10000000 fd on
  /bin/ip link set $ifc up
done

The test can run for example:

candump can1

in the guest system and next commands in the host system for basic CAN:

cangen can0

for CAN FD without bitrate switch:

cangen can0 -f

and with bitrate switch:

cangen can0 -b

The test can also be run the other way around, generating messages in the guest system and capturing them in the host
system. Other combinations are also possible.




100                                                                              Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5


Links to other resources

  (1) CAN related projects at Czech Technical University, Faculty of Electrical Engineering
  (2) Repository with development can-pci branch at Czech Technical University
  (3) RTEMS page describing project
  (4) RTLWS 2015 article about the project and its use with CANopen emulation
  (5) GNU/Linux, CAN and CANopen in Real-time Control Applications Slides from LinuxDays 2017 (include up-
      dated RTLWS 2015 content)
  (6) Linux SocketCAN utilities
  (7) CTU CAN FD project including core VHDL design, Linux driver, test utilities etc.
  (8) CTU CAN FD Core Datasheet Documentation
  (9) CTU CAN FD Core System Architecture Documentation
(10) CTU CAN FD Driver Documentation
(11) Integration with PCIe interfacing for Intel/Altera Cyclone IV based board


Chip Card Interface Device (CCID)

USB CCID device

The USB CCID device is a USB device implementing the CCID specification, which lets one connect smart card
readers that implement the same spec. For more information see the specification:

Universal Serial Bus
Device Class: Smart Card
CCID
Specification for
Integrated Circuit(s) Cards Interface Devices
Revision 1.1
April 22rd, 2005

Smartcards are used for authentication, single sign on, decryption in public/private schemes and digital signatures. A
smartcard reader on the client cannot be used on a guest with simple usb passthrough since it will then not be available
on the client, possibly locking the computer when it is “removed”. On the other hand this device can let you use the
smartcard on both the client and the guest machine. It is also possible to have a completely virtual smart card reader
and smart card (i.e. not backed by a physical device) using this device.


Building

The cryptographic functions and access to the physical card is done via the libcacard library, whose development
package must be installed prior to building QEMU:
In redhat/fedora:

yum install libcacard-devel

In ubuntu:




2.3. Device Emulation                                                                                              101
QEMU Documentation, Release 8.1.5



apt-get install libcacard-dev

Configuring and building:

./configure --enable-smartcard && make


Using ccid-card-emulated with hardware

Assuming you have a working smartcard on the host with the current user, using libcacard, QEMU acts as another
client using ccid-card-emulated:

qemu -usb -device usb-ccid -device ccid-card-emulated


Using ccid-card-emulated with certificates stored in files

You must create the CA and card certificates. This is a one time process. We use NSS certificates:

mkdir fake-smartcard
cd fake-smartcard
certutil -N -d sql:$PWD
certutil -S -d sql:$PWD -s "CN=Fake Smart Card CA" -x -t TC,TC,TC -n fake-smartcard-ca
certutil -S -d sql:$PWD -t ,, -s "CN=John Doe" -n id-cert -c fake-smartcard-ca
certutil -S -d sql:$PWD -t ,, -s "CN=John Doe (signing)" --nsCertType smime -n signing-
 ˓→cert -c fake-smartcard-ca

certutil -S -d sql:$PWD -t ,, -s "CN=John Doe (encryption)" --nsCertType sslClient -n␣
 ˓→encryption-cert -c fake-smartcard-ca




Note: you must have exactly three certificates.
You can use the emulated card type with the certificates backend:

qemu -usb -device usb-ccid -device ccid-card-emulated,backend=certificates,db=sql:$PWD,
 ˓→cert1=id-cert,cert2=signing-cert,cert3=encryption-cert




To use the certificates in the guest, export the CA certificate:

certutil -L -r -d sql:$PWD -o fake-smartcard-ca.cer -n fake-smartcard-ca

and import it in the guest:

certutil -A -d /etc/pki/nssdb -i fake-smartcard-ca.cer -t TC,TC,TC -n fake-smartcard-ca

In a Linux guest you can then use the CoolKey PKCS #11 module to access the card:

certutil -d /etc/pki/nssdb -L -h all

It will prompt you for the PIN (which is the password you assigned to the certificate database early on), and then show
you all three certificates together with the manually imported CA cert:

Certificate Nickname                                         Trust Attributes
fake-smartcard-ca                                            CT,C,C
John Doe:CAC ID Certificate                                  u,u,u
                                                                                                   (continues on next page)


102                                                                                Chapter 2. System Emulation
                                                                             QEMU Documentation, Release 8.1.5


                                                                                             (continued from previous page)
John Doe:CAC Email Signature Certificate                    u,u,u
John Doe:CAC Email Encryption Certificate                   u,u,u

If this does not happen, CoolKey is not installed or not registered with NSS. Registration can be done from Firefox or
the command line:

modutil -dbdir /etc/pki/nssdb -add "CAC Module" -libfile /usr/lib64/pkcs11/
 ˓→libcoolkeypk11.so

modutil -dbdir /etc/pki/nssdb -list


Using ccid-card-passthru with client side hardware

On the host specify the ccid-card-passthru device with a suitable chardev:

qemu -chardev socket,server=on,host=0.0.0.0,port=2001,id=ccid,wait=off \
     -usb -device usb-ccid -device ccid-card-passthru,chardev=ccid

On the client run vscclient, built when you built QEMU:

vscclient <qemu-host> 2001


Using ccid-card-passthru with client side certificates

This case is not particularly useful, but you can use it to debug your setup.
Follow instructions above, except run QEMU and vscclient as follows.
Run qemu as per above, and run vscclient from the “fake-smartcard” directory as follows:

qemu -chardev socket,server=on,host=0.0.0.0,port=2001,id=ccid,wait=off \
      -usb -device usb-ccid -device ccid-card-passthru,chardev=ccid
vscclient -e "db=\"sql:$PWD\" use_hw=no soft=(,Test,CAC,,id-cert,signing-cert,encryption-
 ˓→cert)" <qemu-host> 2001




Passthrough protocol scenario

This is a typical interchange of messages when using the passthru card device. usb-ccid is a usb device. It de-
faults to an unattached usb device on startup. usb-ccid expects a chardev and expects the protocol defined in
cac_card/vscard_common.h to be passed over that. The usb-ccid device can be in one of three modes:
    • detached
    • attached with no card
    • attached with card
A typical interchange is (the arrow shows who started each exchange, it can be client originated or guest originated):

client event        |    vscclient           |    passthru    |    usb-ccid | guest␣
 ˓→event

-----------------------------------------------------------------------------------------
                                                                                                   (continues on next page)



2.3. Device Emulation                                                                                                 103
QEMU Documentation, Release 8.1.5


                                                                                               (continued from previous page)
˓→-------
                           |     VSC_Init                   |                      |                     |
                           |     VSC_ReaderAdd              |                      |      attach         |
                           |                                |                      |                     |    sees new␣
˓→usb device.
  card inserted ->         |                                |                      |                     |
                           |     VSC_ATR                    |     insert           |      insert         |    see new␣
˓→card
                           |                                |                      |                     |
                           |     VSC_APDU                   |     VSC_APDU         |                     | <- guest␣
 ˓→sends APDU

client <-> physical |                                       |                      |                     |
  card APDU exchange |                                      |                      |                     |
  client response -> |           VSC_APDU                   |     VSC_APDU         |                     |    receive␣
 ˓→APDU response

                                                            ...
                                            [APDU<->APDU repeats several times]
                                                            ...
     card removed     -> |                           |                |                                  |
                         |       VSC_CardRemove      |   remove       |   remove                         |     card␣
˓→removed
                                                            ...
                                            [(card insert, apdu's, card remove) repeat]
                                                            ...
  kill/quit                |                         |                |              |
    vscclient              |                         |                |              |
                           |     VSC_ReaderRemove    |                |   detach     |
                           |                         |                |              |                         usb␣
˓→device removed.


libcacard

Both ccid-card-emulated and vscclient use libcacard as the card emulator. libcacard implements a completely virtual
CAC (DoD standard for smart cards) compliant card and uses NSS to retrieve certificates and do any encryption. The
backend can then be a real reader and card, or certificates stored in files.


Compute Express Link (CXL)

From the view of a single host, CXL is an interconnect standard that targets accelerators and memory devices attached
to a CXL host. This description will focus on those aspects visible either to software running on a QEMU emulated
host or to the internals of functional emulation. As such, it will skip over many of the electrical and protocol elements
that would be more of interest for real hardware and will dominate more general introductions to CXL. It will also
completely ignore the fabric management aspects of CXL by considering only a single host and a static configuration.
CXL shares many concepts and much of the infrastructure of PCI Express, with CXL Host Bridges, which have CXL
Root Ports which may be directly attached to CXL or PCI End Points. Alternatively there may be CXL Switches with
CXL and PCI Endpoints attached below them. In many cases additional control and capabilities are exposed via PCI
Express interfaces. This sharing of interfaces and hence emulation code is reflected in how the devices are emulated
in QEMU. In most cases the various CXL elements are built upon an equivalent PCIe devices.
CXL devices support the following interfaces:



104                                                                                 Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5


    • Most conventional PCIe interfaces
         – Configuration space access
         – BAR mapped memory accesses used for registers and mailboxes.
         – MSI/MSI-X
         – AER
         – DOE mailboxes
         – IDE
         – Many other PCI express defined interfaces..
    • Memory operations
         – Equivalent of accessing DRAM / NVDIMMs. Any access / feature supported by the host for normal mem-
           ory should also work for CXL attached memory devices.
    • Cache operations. The are mostly irrelevant to QEMU emulation as QEMU is not emulating a coherency proto-
      col. Any emulation related to these will be device specific and is out of the scope of this document.


CXL 2.0 Device Types

CXL 2.0 End Points are often categorized into three types.
Type 1: These support coherent caching of host memory. Example might be a crypto accelerators. May also have
device private memory accessible via means such as PCI memory reads and writes to BARs.
Type 2: These support coherent caching of host memory and host managed device memory (HDM) for which the
coherency protocol is managed by the host. This is a complex topic, so for more information on CXL coherency see
the CXL 2.0 specification.
Type 3 Memory devices: These devices act as a means of attaching additional memory (HDM) to a CXL host including
both volatile and persistent memory. The CXL topology may support interleaving across a number of Type 3 memory
devices using HDM Decoders in the host, host bridge, switch upstream port and endpoints.


Scope of CXL emulation in QEMU

The focus of CXL emulation is CXL revision 2.0 and later. Earlier CXL revisions defined a smaller set of features, leav-
ing much of the control interface as implementation defined or device specific, making generic emulation challenging
with host specific firmware being responsible for setup and the Endpoints being presented to operating systems as Root
Complex Integrated End Points. CXL rev 2.0 looks a lot more like PCI Express, with fully specified discoverability of
the CXL topology.


CXL System components

A CXL system is made up a Host with a number of ‘standard components’ the control and capabilities of which are
discoverable by system software using means described in the CXL 2.0 specification.




2.3. Device Emulation                                                                                              105
QEMU Documentation, Release 8.1.5


CXL Fixed Memory Windows (CFMW)

A CFMW consists of a particular range of Host Physical Address space which is routed to particular CXL Host Bridges.
At time of generic software initialization it will have a particularly interleaving configuration and associated Quality
of Service Throttling Group (QTG). This information is available to system software, when making decisions about
how to configure interleave across available CXL memory devices. It is provide as CFMW Structures (CFMWS) in
the CXL Early Discovery Table, an ACPI table.
Note: QTG 0 is the only one currently supported in QEMU.


CXL Host Bridge (CXL HB)

A CXL host bridge is similar to the PCIe equivalent, but with a specification defined register interface called CXL Host
Bridge Component Registers (CHBCR). The location of this CHBCR MMIO space is described to system software via
a CXL Host Bridge Structure (CHBS) in the CEDT ACPI table. The actual interfaces are identical to those used for
other parts of the CXL hierarchy as CXL Component Registers in PCI BARs.
Interfaces provided include:
    • Configuration of HDM Decoders to route CXL Memory accesses with a particularly Host Physical Address range
      to the target port below which the CXL device servicing that address lies. This may be a mapping to a single
      Root Port (RP) or across a set of target RPs.


CXL Root Ports (CXL RP)

A CXL Root Port serves the same purpose as a PCIe Root Port. There are a number of CXL specific Designated Vendor
Specific Extended Capabilities (DVSEC) in PCIe Configuration Space and associated component register access via
PCI bars.


CXL Switch

Here we consider a simple CXL switch with only a single virtual hierarchy. Whilst more complex devices exist, their
visibility to a particular host is generally the same as for a simple switch design. Hosts often have no awareness of
complex rerouting and device pooling, they simply see devices being hot added or hot removed.
A CXL switch has a similar architecture to those in PCIe, with a single upstream port, internal PCI bus and multiple
downstream ports.
Both the CXL upstream and downstream ports have CXL specific DVSECs in configuration space, and component
registers in PCI BARs. The Upstream Port has the configuration interfaces for the HDM decoders which route incoming
memory accesses to the appropriate downstream port.
A CXL switch is created in a similar fashion to PCI switches by creating an upstream port (cxl-upstream) and a number
of downstream ports on the internal switch bus (cxl-downstream).




106                                                                                Chapter 2. System Emulation
                                                                        QEMU Documentation, Release 8.1.5


CXL Memory Devices - Type 3

CXL type 3 devices use a PCI class code and are intended to be supported by a generic operating system driver. They
have HDM decoders though in these EP devices, the decoder is responsible not for routing but for translation of the
incoming host physical address (HPA) into a Device Physical Address (DPA).


CXL Memory Interleave

To understand the interaction of different CXL hardware components which are emulated in QEMU, let us consider
a memory read in a fully configured CXL topology. Note that system software is responsible for configuration of all
components with the exception of the CFMWs. System software is responsible for allocating appropriate ranges from
within the CFMWs and exposing those via normal memory configurations as would be done for system RAM.
Example system Topology. x marks the match in each decoder level:
|<------------------SYSTEM PHYSICAL ADDRESS MAP (1)----------------->|
|     __________      __________________________________        __________      |
|   |             | |                                       | |           |     |
|   | CFMW 0      | | CXL Fixed Memory Window 1             | | CFMW 2    |     |
|   | HB0 only | | Configured to interleave memory | | HB1 only |               |
|   |             | | memory accesses across HB0/HB1 | |                  |     |
|   |__________| |_____x____________________________| |__________|              |
          |                 |                       |               |
          |                 |                       |               |
          |                 |                       |               |
          |         Interleave Decoder              |               |
          |         Matches this HB                 |               |
          \_____________|                           |_____________/
                __________|__________          _____|_______________
              |                        |     |                         |
      (2)     | CXL HB 0                |     | CXL HB 1               |
              | HB IntLv Decoders      |     | HB IntLv Decoders       |
              | PCI/CXL Root Bus 0c |        | PCI/CXL Root Bus 0d |
              |                        |     |                         |
              |___x_________________|        |_____________________|
                   |                  |         |                 |
                   |                  |         |                 |
      A HB 0 HDM Decoder              |         |                 |
      matches this Port               |         |                 |
                   |                  |         |                 |
      ___________|___       __________|__     __|_________    ___|_________
 (3)| Root Port 0 | | Root Port 1 | | Root Port 2| | Root Port 3 |
    | Appears in        | | Appears in | | Appears in | | Appear in         |
    | PCI topology | | PCI Topology| | PCI Topo           | | PCI Topo      |
    | As 0c:00.0        | | as 0c:01.0 | | as de:00.0 | | as de:01.0 |
    |_______________| |_____________| |____________| |_____________|
            |                     |                 |                 |
            |                     |                 |                 |
      _____|_________       ______|______     ______|_____    ______|_______
 (4)|      x            | |               | |             | |                 |
    | CXL Type3 0       | | CXL Type3 1 | | CXL type3 2| | CLX Type 3 3 |
    |                   | |               | |              | |                |
    | PMEM0(Vol LSA)| | PMEM1 (...) | | PMEM2 (...)| | PMEM3 (...) |
                                                                                               (continues on next page)


2.3. Device Emulation                                                                                            107
QEMU Documentation, Release 8.1.5


                                                                                            (continued from previous page)
      | Decoder to go | |             | |            | |              |
      | from host PA | | PCI 0e:00.0 | | PCI df:00.0| | PCI e0:00.0 |
      | to device PA | |              | |            | |              |
      | PCI as 0d:00.0| |             | |            | |              |
      |_______________| |_____________| |____________| |______________|

Notes:
 (1) 3 CXL Fixed Memory Windows (CFMW) corresponding to different ranges of the system physical address
     map. Each CFMW has particular interleave setup across the CXL Host Bridges (HB) CFMW0 provides uninter-
     leaved access to HB0, CFMW2 provides uninterleaved access to HB1. CFMW1 provides interleaved memory
     access across HB0 and HB1.
 (2) Two CXL Host Bridges. Each of these has 2 CXL Root Ports and programmable HDM decoders to route
     memory accesses either to a single port or interleave them across multiple ports. A complex configuration here,
     might be to use the following HDM decoders in HB0. HDM0 routes CFMW0 requests to RP0 and hence part
     of CXL Type3 0. HDM1 routes CFMW0 requests from a different region of the CFMW0 PA range to RP2 and
     hence part of CXL Type 3 1. HDM2 routes yet another PA range from within CFMW0 to be interleaved across
     RP0 and RP1, providing 2 way interleave of part of the memory provided by CXL Type3 0 and CXL Type 3 1.
     HDM3 routes those interleaved accesses from CFMW1 that target HB0 to RP 0 and another part of the memory
     of CXL Type 3 0 (as part of a 2 way interleave at the system level across for example CXL Type3 0 and CXL
     Type3 2. HDM4 is used to enable system wide 4 way interleave across all the present CXL type3 devices, by
     interleaving those (interleaved) requests that HB0 receives from from CFMW1 across RP 0 and RP 1 and hence
     to yet more regions of the memory of the attached Type3 devices. Note this is a representative subset of the full
     range of possible HDM decoder configurations in this topology.
 (3) Four CXL Root Ports. In this case the CXL Type 3 devices are directly attached to these ports.
 (4) Four CXL Type3 memory expansion devices. These will each have HDM decoders, but in this case rather than
     performing interleave they will take the Host Physical Addresses of accesses and map them to their own local
     Device Physical Address Space (DPA).
Example topology involving a switch:
|<------------------SYSTEM PHYSICAL ADDRESS MAP (1)----------------->|
|    __________     __________________________________     __________  |
|   |           | |                                    | |           | |
|   | CFMW 0    | | CXL Fixed Memory Window 1          | | CFMW 2    | |
|   | HB0 only | | Configured to interleave memory | | HB1 only |      |
|   |           | | memory accesses across HB0/HB1 | |               | |
|   |____x_____| |__________________________________| |__________|     |
         |               |                      |              |
         |               |                      |              |
         |               |                      |
Interleave Decoder       |                      |              |
 Matches this HB         |                      |              |
         \_____________|                        |_____________/
              __________|__________        _____|_______________
            |                       |    |                       |
            | CXL HB 0              |    | CXL HB 1              |
            | HB IntLv Decoders     |    | HB IntLv Decoders     |
            | PCI/CXL Root Bus 0c |      | PCI/CXL Root Bus 0d |
            |                       |    |                       |
            |___x_________________|      |_____________________|
                 |               |           |                |
                                                                                                  (continues on next page)


108                                                                               Chapter 2. System Emulation
                                                                       QEMU Documentation, Release 8.1.5


                                                                                           (continued from previous page)
                  |
      A HB 0 HDM Decoder
      matches this Port
      ___________|___
    | Root Port 0 |
    | Appears in      |
    | PCI topology |
    | As 0c:00.0      |
    |___________x___|
                  |
                  |
                  \_____________________
                                          |
                                          |
            ---------------------------------------------------
          |     Switch 0 USP as PCI 0d:00.0                        |
          |     USP has HDM decoder which direct traffic to        |
          |     appropriate downstream port                        |
          |     Switch BUS appears as 0e                           |
          |x__________________________________________________|
            |                   |                 |               |
            |                   |                 |               |
      _____|_________     ______|______     ______|_____    ______|_______
 (4)|      x          | |              | |              | |               |
    | CXL Type3 0     | | CXL Type3 1 | | CXL type3 2| | CLX Type 3 3 |
    |                 | |              | |              | |               |
    | PMEM0(Vol LSA)| | PMEM1 (...) | | PMEM2 (...)| | PMEM3 (...) |
    | Decoder to go | |                | |              | |               |
    | from host PA | | PCI 10:00.0 | | PCI 11:00.0| | PCI 12:00.0 |
    | to device PA | |                 | |              | |               |
    | PCI as 0f:00.0| |                 | |              | |               |
    |_______________| |_____________| |____________| |______________|


Example command lines

A very simple setup with just one directly attached CXL Type 3 Persistent Memory device:

qemu-system-x86_64 -M q35,cxl=on -m 4G,maxmem=8G,slots=8 -smp 4 \
...
-object memory-backend-file,id=cxl-mem1,share=on,mem-path=/tmp/cxltest.raw,size=256M \
-object memory-backend-file,id=cxl-lsa1,share=on,mem-path=/tmp/lsa.raw,size=256M \
-device pxb-cxl,bus_nr=12,bus=pcie.0,id=cxl.1 \
-device cxl-rp,port=0,bus=cxl.1,id=root_port13,chassis=0,slot=2 \
-device cxl-type3,bus=root_port13,persistent-memdev=cxl-mem1,lsa=cxl-lsa1,id=cxl-pmem0 \
-M cxl-fmw.0.targets.0=cxl.1,cxl-fmw.0.size=4G

A very simple setup with just one directly attached CXL Type 3 Volatile Memory device:

qemu-system-aarch64 -M virt,gic-version=3,cxl=on -m 4g,maxmem=8G,slots=8 -cpu max \
...
-object memory-backend-ram,id=vmem0,share=on,size=256M \
                                                                                                 (continues on next page)


2.3. Device Emulation                                                                                               109
QEMU Documentation, Release 8.1.5


                                                                                          (continued from previous page)
-device pxb-cxl,bus_nr=12,bus=pcie.0,id=cxl.1 \
-device cxl-rp,port=0,bus=cxl.1,id=root_port13,chassis=0,slot=2 \
-device cxl-type3,bus=root_port13,volatile-memdev=vmem0,id=cxl-vmem0 \
-M cxl-fmw.0.targets.0=cxl.1,cxl-fmw.0.size=4G

The same volatile setup may optionally include an LSA region:
qemu-system-aarch64 -M virt,gic-version=3,cxl=on -m 4g,maxmem=8G,slots=8 -cpu max \
...
-object memory-backend-ram,id=vmem0,share=on,size=256M \
-object memory-backend-file,id=cxl-lsa0,share=on,mem-path=/tmp/lsa.raw,size=256M \
-device pxb-cxl,bus_nr=12,bus=pcie.0,id=cxl.1 \
-device cxl-rp,port=0,bus=cxl.1,id=root_port13,chassis=0,slot=2 \
-device cxl-type3,bus=root_port13,volatile-memdev=vmem0,lsa=cxl-lsa0,id=cxl-vmem0 \
-M cxl-fmw.0.targets.0=cxl.1,cxl-fmw.0.size=4G

A setup suitable for 4 way interleave. Only one fixed window provided, to enable 2 way interleave across 2 CXL host
bridges. Each host bridge has 2 CXL Root Ports, with the CXL Type3 device directly attached (no switches).:
qemu-system-x86_64 -M q35,cxl=on -m 4G,maxmem=8G,slots=8 -smp 4 \
...
-object memory-backend-file,id=cxl-mem1,share=on,mem-path=/tmp/cxltest.raw,size=256M \
-object memory-backend-file,id=cxl-mem2,share=on,mem-path=/tmp/cxltest2.raw,size=256M \
-object memory-backend-file,id=cxl-mem3,share=on,mem-path=/tmp/cxltest3.raw,size=256M \
-object memory-backend-file,id=cxl-mem4,share=on,mem-path=/tmp/cxltest4.raw,size=256M \
-object memory-backend-file,id=cxl-lsa1,share=on,mem-path=/tmp/lsa.raw,size=256M \
-object memory-backend-file,id=cxl-lsa2,share=on,mem-path=/tmp/lsa2.raw,size=256M \
-object memory-backend-file,id=cxl-lsa3,share=on,mem-path=/tmp/lsa3.raw,size=256M \
-object memory-backend-file,id=cxl-lsa4,share=on,mem-path=/tmp/lsa4.raw,size=256M \
-device pxb-cxl,bus_nr=12,bus=pcie.0,id=cxl.1 \
-device pxb-cxl,bus_nr=222,bus=pcie.0,id=cxl.2 \
-device cxl-rp,port=0,bus=cxl.1,id=root_port13,chassis=0,slot=2 \
-device cxl-type3,bus=root_port13,persistent-memdev=cxl-mem1,lsa=cxl-lsa1,id=cxl-pmem0 \
-device cxl-rp,port=1,bus=cxl.1,id=root_port14,chassis=0,slot=3 \
-device cxl-type3,bus=root_port14,persistent-memdev=cxl-mem2,lsa=cxl-lsa2,id=cxl-pmem1 \
-device cxl-rp,port=0,bus=cxl.2,id=root_port15,chassis=0,slot=5 \
-device cxl-type3,bus=root_port15,persistent-memdev=cxl-mem3,lsa=cxl-lsa3,id=cxl-pmem2 \
-device cxl-rp,port=1,bus=cxl.2,id=root_port16,chassis=0,slot=6 \
-device cxl-type3,bus=root_port16,persistent-memdev=cxl-mem4,lsa=cxl-lsa4,id=cxl-pmem3 \
-M cxl-fmw.0.targets.0=cxl.1,cxl-fmw.0.targets.1=cxl.2,cxl-fmw.0.size=4G,cxl-fmw.0.
 ˓→interleave-granularity=8k



An example of 4 devices below a switch suitable for 1, 2 or 4 way interleave:
qemu-system-x86_64 -M q35,cxl=on -m 4G,maxmem=8G,slots=8 -smp 4 \
...
-object memory-backend-file,id=cxl-mem0,share=on,mem-path=/tmp/cxltest.raw,size=256M \
-object memory-backend-file,id=cxl-mem1,share=on,mem-path=/tmp/cxltest1.raw,size=256M \
-object memory-backend-file,id=cxl-mem2,share=on,mem-path=/tmp/cxltest2.raw,size=256M \
-object memory-backend-file,id=cxl-mem3,share=on,mem-path=/tmp/cxltest3.raw,size=256M \
-object memory-backend-file,id=cxl-lsa0,share=on,mem-path=/tmp/lsa0.raw,size=256M \
-object memory-backend-file,id=cxl-lsa1,share=on,mem-path=/tmp/lsa1.raw,size=256M \
-object memory-backend-file,id=cxl-lsa2,share=on,mem-path=/tmp/lsa2.raw,size=256M \
                                                                                                (continues on next page)


110                                                                             Chapter 2. System Emulation
                                                                        QEMU Documentation, Release 8.1.5


                                                                                          (continued from previous page)
-object memory-backend-file,id=cxl-lsa3,share=on,mem-path=/tmp/lsa3.raw,size=256M \
-device pxb-cxl,bus_nr=12,bus=pcie.0,id=cxl.1 \
-device cxl-rp,port=0,bus=cxl.1,id=root_port0,chassis=0,slot=0 \
-device cxl-rp,port=1,bus=cxl.1,id=root_port1,chassis=0,slot=1 \
-device cxl-upstream,bus=root_port0,id=us0 \
-device cxl-downstream,port=0,bus=us0,id=swport0,chassis=0,slot=4 \
-device cxl-type3,bus=swport0,persistent-memdev=cxl-mem0,lsa=cxl-lsa0,id=cxl-pmem0 \
-device cxl-downstream,port=1,bus=us0,id=swport1,chassis=0,slot=5 \
-device cxl-type3,bus=swport1,persistent-memdev=cxl-mem1,lsa=cxl-lsa1,id=cxl-pmem1 \
-device cxl-downstream,port=2,bus=us0,id=swport2,chassis=0,slot=6 \
-device cxl-type3,bus=swport2,persistent-memdev=cxl-mem2,lsa=cxl-lsa2,id=cxl-pmem2 \
-device cxl-downstream,port=3,bus=us0,id=swport3,chassis=0,slot=7 \
-device cxl-type3,bus=swport3,persistent-memdev=cxl-mem3,lsa=cxl-lsa3,id=cxl-pmem3 \
-M cxl-fmw.0.targets.0=cxl.1,cxl-fmw.0.size=4G,cxl-fmw.0.interleave-granularity=4k


Deprecations

The Type 3 device [memdev] attribute has been deprecated in favor of the [persistent-memdev] attributes. [memdev]
will default to a persistent memory device for backward compatibility and is incapable of being used in combination
with [persistent-memdev].


Kernel Configuration Options

In Linux 5.18 the following options are necessary to make use of OS management of CXL memory devices as described
here.
    • CONFIG_CXL_BUS
    • CONFIG_CXL_PCI
    • CONFIG_CXL_ACPI
    • CONFIG_CXL_PMEM
    • CONFIG_CXL_MEM
    • CONFIG_CXL_PORT
    • CONFIG_CXL_REGION


References

    • Consortium website for specifications etc: http://www.computeexpresslink.org
    • Compute Express link Revision 2 specification, October 2020
    • CEDT CFMWS & QTG _DSM ECN May 2021




2.3. Device Emulation                                                                                              111
QEMU Documentation, Release 8.1.5


Inter-VM Shared Memory device

On Linux hosts, a shared memory device is available. The basic syntax is:
qemu-system-x86_64 -device ivshmem-plain,memdev=hostmem
where hostmem names a host memory backend. For a POSIX shared memory backend, use something like

-object memory-backend-file,size=1M,share,mem-path=/dev/shm/ivshmem,id=hostmem

If desired, interrupts can be sent between guest VMs accessing the same shared memory region. Interrupt support
requires using a shared memory server and using a chardev socket to connect to it. The code for the shared memory
server is qemu.git/contrib/ivshmem-server. An example syntax when using the shared memory server is:
# First start the ivshmem server once and for all
ivshmem-server -p pidfile -S path -m shm-name -l shm-size -n vectors

# Then start your qemu instances with matching arguments
qemu-system-x86_64 -device ivshmem-doorbell,vectors=vectors,chardev=id
                 -chardev socket,path=path,id=id
When using the server, the guest will be assigned a VM ID (>=0) that allows guests using the same server to commu-
nicate via interrupts. Guests can read their VM ID from a device register (see ivshmem-spec.txt).


Migration with ivshmem

With device property master=on, the guest will copy the shared memory on migration to the destination host. With
master=off, the guest will not be able to migrate with the device attached. In the latter case, the device should be
detached and then reattached after migration using the PCI hotplug support.
At most one of the devices sharing the same memory can be master. The master must complete migration before you
plug back the other devices.


ivshmem and hugepages

Instead of specifying the <shm size> using POSIX shm, you may specify a memory backend that has hugepage support:
qemu-system-x86_64 -object memory-backend-file,size=1G,mem-path=/dev/hugepages/
 ˓→my-shmem-file,share,id=mb1

                  -device ivshmem-plain,memdev=mb1
ivshmem-server also supports hugepages mount points with the -m memory path argument.


Sparc32 keyboard

SUN Type 4, 5 and 5c keyboards have dip switches to choose the language layout of the keyboard. Solaris makes an
ioctl to query the value of the dipswitches and uses that value to select keyboard layout. Also the SUN bios like the
one in the file ss5.bin uses this value to support at least some keyboard layouts. However, the OpenBIOS provided with
qemu is hardcoded to always use an US keyboard layout.
With the escc.chnA-sunkbd-layout driver property it is possible to select keyboard layout. Example:
-global escc.chnA-sunkbd-layout=de
Depending on type of keyboard, the keyboard can have 6 or 5 dip-switches to select keyboard layout, giving up to 64
different layouts. Not all combinations are supported by Solaris and even less by Sun OpenBoot BIOS.


112                                                                               Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


The dip switch settings can be given as hexadecimal number, decimal number or in some cases as a language string.
Examples:
-global escc.chnA-sunkbd-layout=0x2b
-global escc.chnA-sunkbd-layout=43
-global escc.chnA-sunkbd-layout=sv
The above 3 examples all select a swedish keyboard layout. Table 3-15 at https://docs.oracle.com/cd/E19683-01/
806-6642/new-43/index.html explains which keytable file is used for different dip switch settings. The information in
that table can be summarized in this table:

                          Table 3: Language selection values for escc.chnA-sunkbd-layout
 Hexadecimal value                     Decimal value                          Language code
 0x21                                  33                                     en-us
 0x23                                  35                                     fr
 0x24                                  36                                     da
 0x25                                  37                                     de
 0x26                                  38                                     it
 0x27                                  39                                     nl
 0x28                                  40                                     no
 0x29                                  41                                     pt
 0x2a                                  42                                     es
 0x2b                                  43                                     sv
 0x2c                                  44                                     fr-ch
 0x2d                                  45                                     de-ch
 0x2e                                  46                                     en-gb
 0x2f                                  47                                     ko
 0x30                                  48                                     tw
 0x31                                  49                                     ja
 0x32                                  50                                     fr-ca
 0x33                                  51                                     hu
 0x34                                  52                                     pl
 0x35                                  53                                     cz
 0x36                                  54                                     ru
 0x37                                  55                                     lv
 0x38                                  56                                     tr
 0x39                                  57                                     gr
 0x3a                                  58                                     ar
 0x3b                                  59                                     lt
 0x3c                                  60                                     nl-be
 0x3c                                  60                                     be

Not all dip switch values have a corresponding language code and both “be” and “nl-be” correspond to the same dip
switch value. By default, if no value is given to escc.chnA-sunkbd-layout 0x21 (en-us) will be used.




2.3. Device Emulation                                                                                           113
QEMU Documentation, Release 8.1.5


Network emulation

QEMU can simulate several network cards (e.g. PCI or ISA cards on the PC target) and can connect them to a network
backend on the host or an emulated hub. The various host network backends can either be used to connect the NIC of
the guest to a real network (e.g. by using a TAP devices or the non-privileged user mode network stack), or to other
guest instances running in another QEMU process (e.g. by using the socket host network backend).


Using TAP network interfaces

This is the standard way to connect QEMU to a real network. QEMU adds a virtual network device on your host (called
tapN), and you can then configure it as if it was a real ethernet card.


Linux host

As an example, you can download the linux-test-xxx.tar.gz archive and copy the script qemu-ifup in /etc and
configure properly sudo so that the command ifconfig contained in qemu-ifup can be executed as root. You must
verify that your host kernel supports the TAP network interfaces: the device /dev/net/tun must be present.
See Invocation to have examples of command lines using the TAP network interfaces.


Windows host

There is a virtual ethernet driver for Windows 2000/XP systems, called TAP-Win32. But it is not included in standard
QEMU for Windows, so you will need to get it separately. It is part of OpenVPN package, so download OpenVPN
from : https://openvpn.net/.


Using the user mode network stack

By using the option -net user (default configuration if no -net option is specified), QEMU uses a completely user
mode network stack (you don’t need root privilege to use the virtual network). The virtual network configuration is the
following:

guest (10.0.2.15)        <------>     Firewall/DHCP server <-----> Internet
                            |             (10.0.2.2)
                            |
                            ---->     DNS server (10.0.2.3)
                            |
                            ---->     SMB server (10.0.2.4)

The QEMU VM behaves as if it was behind a firewall which blocks all incoming connections. You can use a DHCP
client to automatically configure the network in the QEMU VM. The DHCP server assign addresses to the hosts starting
from 10.0.2.15.
In order to check that the user mode network is working, you can ping the address 10.0.2.2 and verify that you got an
address in the range 10.0.2.x from the QEMU virtual DHCP server.
Note that ICMP traffic in general does not work with user mode networking. ping, aka. ICMP echo, to the local router
(10.0.2.2) shall work, however. If you’re using QEMU on Linux >= 3.0, it can use unprivileged ICMP ping sockets to
allow ping to the Internet. The host admin has to set the ping_group_range in order to grant access to those sockets.
To allow ping for GID 100 (usually users group):




114                                                                                Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5



echo 100 100 > /proc/sys/net/ipv4/ping_group_range

When using the built-in TFTP server, the router is also the TFTP server.
When using the '-netdev user,hostfwd=...' option, TCP or UDP connections can be redirected from the host to
the guest. It allows for example to redirect X11, telnet or SSH connections.


Hubs

QEMU can simulate several hubs. A hub can be thought of as a virtual connection between several network devices.
These devices can be for example QEMU virtual ethernet cards or virtual Host ethernet devices (TAP devices). You can
connect guest NICs or host network backends to such a hub using the -netdev hubport or -nic hubport options.
The legacy -net option also connects the given device to the emulated hub with ID 0 (i.e. the default hub) unless you
specify a netdev with -net nic,netdev=xxx here.


Connecting emulated networks between QEMU instances

Using the -netdev socket (or -nic socket or -net socket) option, it is possible to create emulated networks
that span several QEMU instances. See the description of the -netdev socket option in Invocation to have a basic
example.


NVMe Emulation

QEMU provides NVMe emulation through the nvme, nvme-ns and nvme-subsys devices.
See the following sections for specific information on
    • Adding NVMe Devices, additional namespaces and NVM subsystems.
    • Configuration of Optional Features such as Controller Memory Buffer, Simple Copy, Zoned Namespaces, meta-
      data and End-to-End Data Protection,


Adding NVMe Devices

Controller Emulation

The QEMU emulated NVMe controller implements version 1.4 of the NVM Express specification. All mandatory
features are implement with a couple of exceptions and limitations:
    • Accounting numbers in the SMART/Health log page are reset when the device is power cycled.
    • Interrupt Coalescing is not supported and is disabled by default.
The simplest way to attach an NVMe controller on the QEMU PCI bus is to add the following parameters:

-drive file=nvm.img,if=none,id=nvm
-device nvme,serial=deadbeef,drive=nvm

There are a number of optional general parameters for the nvme device. Some are mentioned here, but see -device
nvme,help to list all possible parameters.
max_ioqpairs=UINT32 (default: 64)
     Set the maximum number of allowed I/O queue pairs. This replaces the deprecated num_queues parameter.



2.3. Device Emulation                                                                                           115
QEMU Documentation, Release 8.1.5


msix_qsize=UINT16 (default: 65)
     The number of MSI-X vectors that the device should support.
mdts=UINT8 (default: 7)
     Set the Maximum Data Transfer Size of the device.
use-intel-id (default: off)
     Since QEMU 5.2, the device uses a QEMU allocated “Red Hat” PCI Device and Vendor ID. Set this to on to
     revert to the unallocated Intel ID previously used.


Additional Namespaces

In the simplest possible invocation sketched above, the device only support a single namespace with the namespace
identifier 1. To support multiple namespaces and additional features, the nvme-ns device must be used.

-device nvme,id=nvme-ctrl-0,serial=deadbeef
-drive file=nvm-1.img,if=none,id=nvm-1
-device nvme-ns,drive=nvm-1
-drive file=nvm-2.img,if=none,id=nvm-2
-device nvme-ns,drive=nvm-2

The namespaces defined by the nvme-ns device will attach to the most recently defined nvme-bus that is created by
the nvme device. Namespace identifiers are allocated automatically, starting from 1.
There are a number of parameters available:
nsid (default: 0)
     Explicitly set the namespace identifier.
uuid (default: autogenerated)
     Set the UUID of the namespace. This will be reported as a “Namespace UUID” descriptor in the Namespace
     Identification Descriptor List.
eui64
     Set the EUI-64 of the namespace. This will be reported as a “IEEE Extended Unique Identifier” descriptor in
     the Namespace Identification Descriptor List. Since machine type 6.1 a non-zero default value is used if the
     parameter is not provided. For earlier machine types the field defaults to 0.
bus
      If there are more nvme devices defined, this parameter may be used to attach the namespace to a specific nvme
      device (identified by an id parameter on the controller device).


NVM Subsystems

Additional features becomes available if the controller device (nvme) is linked to an NVM Subsystem device
(nvme-subsys).
The NVM Subsystem emulation allows features such as shared namespaces and multipath I/O.

-device nvme-subsys,id=nvme-subsys-0,nqn=subsys0
-device nvme,serial=deadbeef,subsys=nvme-subsys-0
-device nvme,serial=deadbeef,subsys=nvme-subsys-0

This will create an NVM subsystem with two controllers. Having controllers linked to an nvme-subsys device allows
additional nvme-ns parameters:




116                                                                             Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5


shared (default: on since 6.2)
     Specifies that the namespace will be attached to all controllers in the subsystem. If set to off, the namespace
     will remain a private namespace and may only be attached to a single controller at a time. Shared namespaces
     are always automatically attached to all controllers (also when controllers are hotplugged).
detached (default: off)
     If set to on, the namespace will be be available in the subsystem, but not attached to any controllers initially. A
     shared namespace with this set to on will never be automatically attached to controllers.
Thus, adding

-drive file=nvm-1.img,if=none,id=nvm-1
-device nvme-ns,drive=nvm-1,nsid=1
-drive file=nvm-2.img,if=none,id=nvm-2
-device nvme-ns,drive=nvm-2,nsid=3,shared=off,detached=on

will cause NSID 1 will be a shared namespace that is initially attached to both controllers. NSID 3 will be a private
namespace due to shared=off and only attachable to a single controller at a time. Additionally it will not be attached
to any controller initially (due to detached=on) or to hotplugged controllers.


Optional Features

Controller Memory Buffer

nvme device parameters related to the Controller Memory Buffer support:
cmb_size_mb=UINT32 (default: 0)
     This adds a Controller Memory Buffer of the given size at offset zero in BAR 2.
legacy-cmb (default: off)
     By default, the device uses the “v1.4 scheme” for the Controller Memory Buffer support (i.e, the CMB is initially
     disabled and must be explicitly enabled by the host). Set this to on to behave as a v1.3 device wrt. the CMB.


Simple Copy

The device includes support for TP 4065 (“Simple Copy Command”). A number of additional nvme-ns device param-
eters may be used to control the Copy command limits:
mssrl=UINT16 (default: 128)
     Set the Maximum Single Source Range Length (MSSRL). This is the maximum number of logical blocks that may
     be specified in each source range.
mcl=UINT32 (default: 128)
     Set the Maximum Copy Length (MCL). This is the maximum number of logical blocks that may be specified in a
     Copy command (the total for all source ranges).
msrc=UINT8 (default: 127)
     Set the Maximum Source Range Count (MSRC). This is the maximum number of source ranges that may be used
     in a Copy command. This is a 0’s based value.




2.3. Device Emulation                                                                                              117
QEMU Documentation, Release 8.1.5


Zoned Namespaces

A namespaces may be “Zoned” as defined by TP 4053 (“Zoned Namespaces”). Set zoned=on on an nvme-ns device
to configure it as a zoned namespace.
The namespace may be configured with additional parameters
zoned.zone_size=SIZE (default: 128MiB)
     Define the zone size (ZSZE).
zoned.zone_capacity=SIZE (default: 0)
     Define the zone capacity (ZCAP). If left at the default (0), the zone capacity will equal the zone size.
zoned.descr_ext_size=UINT32 (default: 0)
     Set the Zone Descriptor Extension Size (ZDES). Must be a multiple of 64 bytes.
zoned.cross_read=BOOL (default: off)
     Set to on to allow reads to cross zone boundaries.
zoned.max_active=UINT32 (default: 0)
     Set the maximum number of active resources (MAR). The default (0) allows all zones to be active.
zoned.max_open=UINT32 (default: 0)
     Set the maximum number of open resources (MOR). The default (0) allows all zones to be open. If zoned.
     max_active is specified, this value must be less than or equal to that.
zoned.zasl=UINT8 (default: 0)
     Set the maximum data transfer size for the Zone Append command. Like mdts, the value is specified as a power
     of two (2^n) and is in units of the minimum memory page size (CAP.MPSMIN). The default value (0) has this
     property inherit the mdts value.


Flexible Data Placement

The device may be configured to support TP4146 (“Flexible Data Placement”) by configuring it (fdp=on) on the
subsystem:

-device nvme-subsys,id=nvme-subsys-0,nqn=subsys0,fdp=on,fdp.nruh=16

The subsystem emulates a single Endurance Group, on which Flexible Data Placement will be supported. Also note
that the device emulation deviates slightly from the specification, by always enabling the “FDP Mode” feature on the
controller if the subsystems is configured for Flexible Data Placement.
Enabling Flexible Data Placement on the subsyste enables the following parameters:
fdp.nrg (default: 1)
     Set the number of Reclaim Groups.
fdp.nruh (default: 0)
     Set the number of Reclaim Unit Handles. This is a mandatory parameter and must be non-zero.
fdp.runs (default: 96M)
     Set the Reclaim Unit Nominal Size. Defaults to 96 MiB.
Namespaces within this subsystem may requests Reclaim Unit Handles:

-device nvme-ns,drive=nvm-1,fdp.ruhs=RUHLIST

The RUHLIST is a semicolon separated list (i.e. 0;1;2;3) and may include ranges (i.e. 0;8-15). If no reclaim
unit handle list is specified, the controller will assign the controller-specified reclaim unit handle to placement handle
identifier 0.


118                                                                                  Chapter 2. System Emulation
                                                                             QEMU Documentation, Release 8.1.5


Metadata

The virtual namespace device supports LBA metadata in the form separate metadata (MPTR-based) and extended LBAs.
ms=UINT16 (default: 0)
     Defines the number of metadata bytes per LBA.
mset=UINT8 (default: 0)
     Set to 1 to enable extended LBAs.


End-to-End Data Protection

The virtual namespace device supports DIF- and DIX-based protection information (depending on mset).
pi=UINT8 (default: 0)
     Enable protection information of the specified type (type 1, 2 or 3).
pil=UINT8 (default: 0)
     Controls the location of the protection information within the metadata. Set to 1 to transfer protection information
     as the first bytes of metadata. Otherwise, the protection information is transferred as the last bytes of metadata.
pif=UINT8 (default: 0)
     By default, the namespace device uses 16 bit guard protection information format (pif=0). Set to 2 to enable
     64 bit guard protection information format. This requires at least 16 bytes of metadata. Note that pif=1 (32 bit
     guards) is currently not supported.


Virtualization Enhancements and SR-IOV (Experimental Support)

The nvme device supports Single Root I/O Virtualization and Sharing along with Virtualization Enhancements. The
controller has to be linked to an NVM Subsystem device (nvme-subsys) for use with SR-IOV.
A number of parameters are present (please note, that they may be subject to change):
sriov_max_vfs (default: 0)
     Indicates the maximum number of PCIe virtual functions supported by the controller. Specifying a non-zero
     value enables reporting of both SR-IOV and ARI (Alternative Routing-ID Interpretation) capabilities by the
     NVMe device. Virtual function controllers will not report SR-IOV.
sriov_vq_flexible
     Indicates the total number of flexible queue resources assignable to all the secondary controllers. Implicitly sets
     the number of primary controller’s private resources to (max_ioqpairs - sriov_vq_flexible).
sriov_vi_flexible
     Indicates the total number of flexible interrupt resources assignable to all the secondary controllers. Implicitly
     sets the number of primary controller’s private resources to (msix_qsize - sriov_vi_flexible).
sriov_max_vi_per_vf (default: 0)
     Indicates the maximum number of virtual interrupt resources assignable to a secondary controller. The default
     0 resolves to (sriov_vi_flexible / sriov_max_vfs)
sriov_max_vq_per_vf (default: 0)
     Indicates the maximum number of virtual queue resources assignable to a secondary controller. The default 0
     resolves to (sriov_vq_flexible / sriov_max_vfs)
The simplest possible invocation enables the capability to set up one VF controller and assign an admin queue, an IO
queue, and a MSI-X interrupt.




2.3. Device Emulation                                                                                               119
QEMU Documentation, Release 8.1.5



-device nvme-subsys,id=subsys0
-device nvme,serial=deadbeef,subsys=subsys0,sriov_max_vfs=1,
 sriov_vq_flexible=2,sriov_vi_flexible=1

The minimum steps required to configure a functional NVMe secondary controller are:
    • unbind flexible resources from the primary controller

 nvme virt-mgmt /dev/nvme0 -c 0 -r 1 -a 1 -n 0
 nvme virt-mgmt /dev/nvme0 -c 0 -r 0 -a 1 -n 0

* perform a Function Level Reset on the primary controller to actually
  release the resources

 echo 1 > /sys/bus/pci/devices/0000:01:00.0/reset

* enable VF

 echo 1 > /sys/bus/pci/devices/0000:01:00.0/sriov_numvfs

* assign the flexible resources to the VF and set it ONLINE

 nvme virt-mgmt /dev/nvme0 -c 1 -r 1 -a 8 -n 1
 nvme virt-mgmt /dev/nvme0 -c 1 -r 0 -a 8 -n 2
 nvme virt-mgmt /dev/nvme0 -c 1 -r 0 -a 9 -n 0

* bind the NVMe driver to the VF

echo 0000:01:00.1 > /sys/bus/pci/drivers/nvme/bind


USB emulation

QEMU can emulate a PCI UHCI, OHCI, EHCI or XHCI USB controller. You can plug virtual USB devices or real host
USB devices (only works with certain host operating systems). QEMU will automatically create and connect virtual
USB hubs as necessary to connect multiple USB devices.


USB controllers

XHCI controller support

QEMU has XHCI host adapter support. The XHCI hardware design is much more virtualization-friendly when com-
pared to EHCI and UHCI, thus XHCI emulation uses less resources (especially CPU). So if your guest supports XHCI
(which should be the case for any operating system released around 2010 or later) we recommend using it:
      qemu -device qemu-xhci
XHCI supports USB 1.1, USB 2.0 and USB 3.0 devices, so this is the only controller you need. With only a single
USB controller (and therefore only a single USB bus) present in the system there is no need to use the bus= parameter
when adding USB devices.




120                                                                              Chapter 2. System Emulation
                                                                             QEMU Documentation, Release 8.1.5


EHCI controller support

The QEMU EHCI Adapter supports USB 2.0 devices. It can be used either standalone or with companion controllers
(UHCI, OHCI) for USB 1.1 devices. The companion controller setup is more convenient to use because it provides a
single USB bus supporting both USB 2.0 and USB 1.1 devices. See next section for details.
When running EHCI in standalone mode you can add UHCI or OHCI controllers for USB 1.1 devices too. Each
controller creates its own bus though, so there are two completely separate USB buses: One USB 1.1 bus driven by the
UHCI controller and one USB 2.0 bus driven by the EHCI controller. Devices must be attached to the correct controller
manually.
The easiest way to add a UHCI controller to a pc machine is the -usb switch. QEMU will create the UHCI controller
as function of the PIIX3 chipset. The USB 1.1 bus will carry the name usb-bus.0.
You can use the standard -device switch to add a EHCI controller to your virtual machine. It is strongly recommended
to specify an ID for the controller so the USB 2.0 bus gets an individual name, for example -device usb-ehci,
id=ehci. This will give you a USB 2.0 bus named ehci.0.
When adding USB devices using the -device switch you can specify the bus they should be attached to. Here is a
complete example:
qemu-system-x86_64 -M pc ${otheroptions}                                                \
    -drive if=none,id=usbstick,format=raw,file=/path/to/image                           \
    -usb                                                                                \
    -device usb-ehci,id=ehci                                                            \
    -device usb-tablet,bus=usb-bus.0                                                    \
    -device usb-storage,bus=ehci.0,drive=usbstick
This attaches a USB tablet to the UHCI adapter and a USB mass storage device to the EHCI adapter.


Companion controller support

The UHCI and OHCI controllers can attach to a USB bus created by EHCI as companion controllers. This is done by
specifying the masterbus and firstport properties. masterbus specifies the bus name the controller should attach
to. firstport specifies the first port the controller should attach to, which is needed as usually one EHCI controller
with six ports has three UHCI companion controllers with two ports each.
There is a config file in docs which will do all this for you, which you can use like this:
qemu-system-x86_64 -readconfig docs/config/ich9-ehci-uhci.cfg
Then use bus=ehci.0 to assign your USB devices to that bus.
Using the -usb switch for q35 machines will create a similar USB controller configuration.


Connecting USB devices

USB devices can be connected with the -device usb-... command line option or the device_add monitor com-
mand. Available devices are:
usb-mouse
     Virtual Mouse. This will override the PS/2 mouse emulation when activated.
usb-tablet
     Pointer device that uses absolute coordinates (like a touchscreen). This means QEMU is able to report the mouse
     position without having to grab the mouse. Also overrides the PS/2 mouse emulation when activated.




2.3. Device Emulation                                                                                            121
QEMU Documentation, Release 8.1.5


usb-storage,drive=drive_id
     Mass storage device backed by drive_id (see the Disk Images chapter in the System Emulation Users Guide).
     This is the classic bulk-only transport protocol used by 99% of USB sticks. This example shows it connected to
     an XHCI USB controller and with a drive backed by a raw format disk image:
      qemu-system-x86_64 [...]                                   \
       -drive if=none,id=stick,format=raw,file=/path/to/file.img \
       -device nec-usb-xhci,id=xhci                              \
       -device usb-storage,bus=xhci.0,drive=stick
usb-uas
     USB attached SCSI device. This does not create a SCSI disk, so you need to explicitly create a scsi-hd or
     scsi-cd device on the command line, as well as using the -drive option to specify what those disks are backed
     by. One usb-uas device can handle multiple logical units (disks). This example creates three logical units: two
     disks and one cdrom drive:
      qemu-system-x86_64 [...]                                         \
       -drive if=none,id=uas-disk1,format=raw,file=/path/to/file1.img \
       -drive if=none,id=uas-disk2,format=raw,file=/path/to/file2.img \
       -drive if=none,id=uas-cdrom,media=cdrom,format=raw,file=/path/to/image.iso \
       -device nec-usb-xhci,id=xhci                                    \
       -device usb-uas,id=uas,bus=xhci.0                               \
       -device scsi-hd,bus=uas.0,scsi-id=0,lun=0,drive=uas-disk1       \
       -device scsi-hd,bus=uas.0,scsi-id=0,lun=1,drive=uas-disk2       \
       -device scsi-cd,bus=uas.0,scsi-id=0,lun=5,drive=uas-cdrom
usb-bot
     Bulk-only transport storage device. This presents the guest with the same USB bulk-only transport protocol
     interface as usb-storage, but the QEMU command line option works like usb-uas and does not automatically
     create SCSI disks for you. usb-bot supports up to 16 LUNs. Unlike usb-uas, the LUN numbers must be
     continuous, i.e. for three devices you must use 0+1+2. The 0+1+5 numbering from the usb-uas example above
     won’t work with usb-bot.
usb-mtp,rootdir=dir
     Media transfer protocol device, using dir as root of the file tree that is presented to the guest.
usb-host,hostbus=bus,hostaddr=addr
     Pass through the host device identified by bus and addr
usb-host,vendorid=vendor,productid=product
     Pass through the host device identified by vendor and product ID
usb-wacom-tablet
     Virtual Wacom PenPartner tablet. This device is similar to the tablet above but it can be used with the tslib
     library because in addition to touch coordinates it reports touch pressure.
usb-kbd
     Standard USB keyboard. Will override the PS/2 keyboard (if present).
usb-serial,chardev=id
     Serial converter. This emulates an FTDI FT232BM chip connected to host character device id.
usb-braille,chardev=id
     Braille device. This emulates a Baum Braille device USB port. id has to specify a character device defined with
     -chardev ...,id=id. One will normally use BrlAPI to display the braille output on a BRLTTY-supported
     device with
      qemu-system-x86_64 [...] -chardev braille,id=brl -device usb-braille,chardev=brl
      or alternatively, use the following equivalent shortcut:



122                                                                                  Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


      qemu-system-x86_64 [...] -usbdevice braille
usb-net[,netdev=id]
     Network adapter that supports CDC ethernet and RNDIS protocols. id specifies a netdev defined with -netdev
     ...,id=id. For instance, user-mode networking can be used with
      qemu-system-x86_64 [...] -netdev user,id=net0 -device usb-net,netdev=net0
usb-ccid
     Smartcard reader device
usb-audio
     USB audio device
u2f-{emulated,passthru}
     Universal Second Factor (U2F) USB Key Device
canokey
     An Open-source Secure Key implementing FIDO2, OpenPGP, PIV and more. For more information, see
     CanoKey QEMU.


Physical port addressing

For all the above USB devices, by default QEMU will plug the device into the next available port on the specified
USB bus, or onto some available USB bus if you didn’t specify one explicitly. If you need to, you can also specify the
physical port where the device will show up in the guest. This can be done using the port property. UHCI has two
root ports (1,2). EHCI has six root ports (1-6), and the emulated (1.1) USB hub has eight ports.
Plugging a tablet into UHCI port 1 works like this:

-device usb-tablet,bus=usb-bus.0,port=1

Plugging a hub into UHCI port 2 works like this:

-device usb-hub,bus=usb-bus.0,port=2

Plugging a virtual USB stick into port 4 of the hub just plugged works this way:

-device usb-storage,bus=usb-bus.0,port=2.4,drive=...

In the monitor, the device_add` command also accepts a ``port property specification. If you want to unplug
devices too you should specify some unique id which you can use to refer to the device. You can then use device_del
to unplug the device later. For example:

(qemu) device_add usb-tablet,bus=usb-bus.0,port=1,id=my-tablet
(qemu) device_del my-tablet




2.3. Device Emulation                                                                                            123
QEMU Documentation, Release 8.1.5


Hotplugging USB storage

The usb-bot and usb-uas devices can be hotplugged. In the hotplug case they are added with attached = false
so the guest will not see the device until the attached property is explicitly set to true. That allows you to attach one
or more scsi devices before making the device visible to the guest. The workflow looks like this:
   1. device-add usb-bot,id=foo
   2. device-add scsi-{hd,cd},bus=foo.0,lun=0
   3. optionally add more devices (luns 1 . . . 15)
   4. scripts/qmp/qom-set foo.attached = true


Using host USB devices on a Linux host

WARNING: this is an experimental feature. QEMU will slow down when using it. USB devices requiring real time
streaming (i.e. USB Video Cameras) are not supported yet.
   1. If you use an early Linux 2.4 kernel, verify that no Linux driver is actually using the USB device. A simple way
      to do that is simply to disable the corresponding kernel module by renaming it from mydriver.o to mydriver.
      o.disabled.
   2. Verify that /proc/bus/usb is working (most Linux distributions should enable it by default). You should see
      something like that:

      ls /proc/bus/usb
      001 devices drivers

   3. Since only root can access to the USB devices directly, you can either launch QEMU as root or change the
      permissions of the USB devices you want to use. For testing, the following suffices:

      chown -R myuid /proc/bus/usb

   4. Launch QEMU and do in the monitor:

      info usbhost
        Device 1.2, speed 480 Mb/s
          Class 00: USB device 1234:5678, USB DISK

      You should see the list of the devices you can use (Never try to use hubs, it won’t work).
   5. Add the device in QEMU by using:

      device_add usb-host,vendorid=0x1234,productid=0x5678

      Normally the guest OS should report that a new USB device is plugged. You can use the option -device
      usb-host,... to do the same.
   6. Now you can try to use the host USB device in QEMU.
When relaunching QEMU, you may have to unplug and plug again the USB device to make it work again (this is a
bug).




124                                                                                 Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


usb-host properties for specifying the host device

The example above uses the vendorid and productid to specify which host device to pass through, but this is not
the only way to specify the host device. usb-host supports the following properties:
hostbus=<nr>
     Specifies the bus number the device must be attached to
hostaddr=<nr>
     Specifies the device address the device got assigned by the guest os
hostport=<str>
     Specifies the physical port the device is attached to
vendorid=<hexnr>
     Specifies the vendor ID of the device
productid=<hexnr>
     Specifies the product ID of the device.
In theory you can combine all these properties as you like. In practice only a few combinations are useful:
    • vendorid and productid – match for a specific device, pass it to the guest when it shows up somewhere in the
      host.
    • hostbus and hostport – match for a specific physical port in the host, any device which is plugged in there
      gets passed to the guest.
    • hostbus and hostaddr – most useful for ad-hoc pass through as the hostaddr isn’t stable. The next time you
      plug the device into the host it will get a new hostaddr.
Note that on the host USB 1.1 devices are handled by UHCI/OHCI and USB 2.0 by EHCI. That means different USB
devices plugged into the very same physical port on the host may show up on different host buses depending on the
speed. Supposing that devices plugged into a given physical port appear as bus 1 + port 1 for 2.0 devices and bus 3 +
port 1 for 1.1 devices, you can pass through any device plugged into that port and also assign it to the correct USB bus
in QEMU like this:
qemu-system-x86_64 -M pc [...]                                               \
     -usb                                                                    \
     -device usb-ehci,id=ehci                                                \
     -device usb-host,bus=usb-bus.0,hostbus=3,hostport=1                     \
     -device usb-host,bus=ehci.0,hostbus=1,hostport=1


usb-host properties for reset behavior

The guest-reset and guest-reset-all properties control whenever the guest is allowed to reset the physical usb
device on the host. There are three cases:
guest-reset=false
     The guest is not allowed to reset the (physical) usb device.
guest-reset=true,guest-resets-all=false
     The guest is allowed to reset the device when it is not yet initialized (aka no usb bus address assigned). Usually
     this results in one guest reset being allowed. This is the default behavior.
guest-reset=true,guest-resets-all=true
     The guest is allowed to reset the device as it pleases.




2.3. Device Emulation                                                                                              125
QEMU Documentation, Release 8.1.5


The reason for this existing are broken usb devices. In theory one should be able to reset (and re-initialize) usb devices
at any time. In practice that may result in shitty usb device firmware crashing and the device not responding any more
until you power-cycle (aka un-plug and re-plug) it.
What works best pretty much depends on the behavior of the specific usb device at hand, so it’s a trial-and-error game.
If the default doesn’t work, try another option and see whenever the situation improves.


record usb transfers

All usb devices have support for recording the usb traffic. This can be enabled using the pcap=<file> property, for
example:
-device usb-mouse,pcap=mouse.pcap
The pcap files are compatible with the linux kernels usbmon. Many tools, including wireshark, can decode and
inspect these trace files.


vhost-user back ends

vhost-user back ends are way to service the request of VirtIO devices outside of QEMU itself. To do this there are a
number of things required.


vhost-user device

These are simple stub devices that ensure the VirtIO device is visible to the guest. The code is mostly boilerplate
although each device has a chardev option which specifies the ID of the --chardev device that connects via a socket
to the vhost-user daemon.


vhost-user daemon

This is a separate process that is connected to by QEMU via a socket following the Vhost-user Protocol. There are a
number of daemons that can be built when enabled by the project although any daemon that meets the specification for
a given device can be used.


Shared memory object

In order for the daemon to access the VirtIO queues to process the requests it needs access to the guest’s address space.
This is achieved via the memory-backend-file or memory-backend-memfd objects. A reference to a file-descriptor
which can access this object will be passed via the socket as part of the protocol negotiation.
Currently the shared memory object needs to match the size of the main system memory as defined by the -m argument.




126                                                                                  Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


Example

First start your daemon.

$ virtio-foo --socket-path=/var/run/foo.sock $OTHER_ARGS

Then you start your QEMU instance specifying the device, chardev and memory objects.
$ qemu-system-x86_64 \
    -m 4096 \
    -chardev socket,id=ba1,path=/var/run/foo.sock \
    -device vhost-user-foo,chardev=ba1,$OTHER_ARGS \
    -object memory-backend-memfd,id=mem,size=4G,share=on \
    -numa node,memdev=mem \
      ...


virtio pmem

This document explains the setup and usage of the virtio pmem device. The virtio pmem device is a paravirtualized
persistent memory device on regular (i.e non-NVDIMM) storage.


Usecase

Virtio pmem allows to bypass the guest page cache and directly use host page cache. This reduces guest memory
footprint as the host can make efficient memory reclaim decisions under memory pressure.


How does virtio-pmem compare to the nvdimm emulation?

NVDIMM emulation on regular (i.e. non-NVDIMM) host storage does not persist the guest writes as there are no de-
fined semantics in the device specification. The virtio pmem device provides guest write persistence on non-NVDIMM
host storage.


virtio pmem usage

A virtio pmem device backed by a memory-backend-file can be created on the QEMU command line as in the following
example:

-object memory-backend-file,id=mem1,share,mem-path=./virtio_pmem.img,size=4G
-device virtio-pmem-pci,memdev=mem1,id=nv1

where:
    • “object memory-backend-file,id=mem1,share,mem-path=<image>, size=<image size>” creates a backend file
      with the specified size.
    • “device virtio-pmem-pci,id=nvdimm1,memdev=mem1” creates a virtio pmem pci device whose storage is pro-
      vided by above memory backend device.
Multiple virtio pmem devices can be created if multiple pairs of “-object” and “-device” are provided.




2.3. Device Emulation                                                                                        127
QEMU Documentation, Release 8.1.5


Hotplug

Virtio pmem devices can be hotplugged via the QEMU monitor. First, the memory backing has to be added via
‘object_add’; afterwards, the virtio pmem device can be added via ‘device_add’.
For example, the following commands add another 4GB virtio pmem device to the guest:

(qemu) object_add memory-backend-file,id=mem2,share=on,mem-path=virtio_pmem2.img,size=4G
(qemu) device_add virtio-pmem-pci,id=virtio_pmem2,memdev=mem2


Guest Data Persistence

Guest data persistence on non-NVDIMM requires guest userspace applications to perform fsync/msync. This is dif-
ferent from a real nvdimm backend where no additional fsync/msync is required. This is to persist guest writes in host
backing file which otherwise remains in host page cache and there is risk of losing the data in case of power failure.
With virtio pmem device, MAP_SYNC mmap flag is not supported. This provides a hint to application to perform
fsync for write persistence.


Limitations

    • Real nvdimm device backend is not supported.
    • virtio pmem hotunplug is not supported.
    • ACPI NVDIMM features like regions/namespaces are not supported.
    • ndctl command is not supported.


QEMU vhost-user-rng - RNG emulation

Background

What follows builds on the material presented in vhost-user.rst - it should be reviewed before moving forward with the
content in this file.


Description

The vhost-user-rng device implementation was designed to work with a random number generator daemon such as the
one found in the vhost-device crate of the rust-vmm project available on github [1].
[1]. https://github.com/rust-vmm/vhost-device




128                                                                               Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


Examples

The daemon should be started first:

host# vhost-device-rng --socket-path=rng.sock -c 1 -m 512 -p 1000

The QEMU invocation needs to create a chardev socket the device can use to communicate as well as share the guests
memory over a memfd.

host# qemu-system                                                                                      \
    -chardev socket,path=$(PATH)/rng.sock,id=rng0                                                      \
    -device vhost-user-rng-pci,chardev=rng0                                                            \
    -m 4096                                                                                            \
    -object memory-backend-file,id=mem,size=4G,mem-path=/dev/shm,share=on                              \
    -numa node,memdev=mem                                                                              \
    ...


CanoKey QEMU

CanoKey1 is an open-source secure key with supports of
    • U2F / FIDO2 with Ed25519 and HMAC-secret
    • OpenPGP Card V3.4 with RSA4096, Ed25519 and more2
    • PIV (NIST SP 800-73-4)
    • HOTP / TOTP
    • NDEF
All these platform-independent features are in canokey-core3 .
For different platforms, CanoKey has different implementations, including both hardware implementions and virtual
cards:
    • CanoKey STM324
    • CanoKey Pigeon5
    • (virt-card) CanoKey USB/IP
    • (virt-card) CanoKey FunctionFS
In QEMU, yet another CanoKey virt-card is implemented. CanoKey QEMU exposes itself as a USB device to the guest
OS.
With the same software configuration as a hardware key, the guest OS can use all the functionalities of a secure key as
if there was actually an hardware key plugged in.
CanoKey QEMU provides much convenience for debugging:
    • libcanokey-qemu supports debugging output thus developers can inspect what happens inside a secure key
    • CanoKey QEMU supports trace event thus event
    • QEMU USB stack supports pcap thus USB packet between the guest and key can be captured and analysed
  1 https://canokeys.org
  2 https://docs.canokeys.org/userguide/openpgp/#supported-algorithm
  3 https://github.com/canokeys/canokey-core
  4 https://github.com/canokeys/canokey-stm32
  5 https://github.com/canokeys/canokey-pigeon




2.3. Device Emulation                                                                                             129
QEMU Documentation, Release 8.1.5


Then for developers:
    • For developers on software with secure key support (e.g. FIDO2, OpenPGP), they can see what happens inside
      the secure key
    • For secure key developers, USB packets between guest OS and CanoKey can be easily captured and analysed
Also since this is a virtual card, it can be easily used in CI for testing on code coping with secure key.


Building

libcanokey-qemu is required to use CanoKey QEMU.

git clone https://github.com/canokeys/canokey-qemu
mkdir canokey-qemu/build
pushd canokey-qemu/build

If you want to install libcanokey-qemu in a different place, add -DCMAKE_INSTALL_PREFIX=/path/to/your/place
to cmake below.

cmake ..
make
make install # may need sudo
popd

Then configuring and building:

# depending on your env, lib/pkgconfig can be lib64/pkgconfig
export PKG_CONFIG_PATH=/path/to/your/place/lib/pkgconfig:$PKG_CONFIG_PATH
./configure --enable-canokey && make


Using CanoKey QEMU

CanoKey QEMU stores all its data on a file of the host specified by the argument when invoking qemu.
qemu-system-x86_64 -usb -device canokey,file=$HOME/.canokey-file
Note: you should keep this file carefully as it may contain your private key!
The first time when the file is used, it is created and initialized by CanoKey, afterwards CanoKey QEMU would just
read this file.
After the guest OS boots, you can check that there is a USB device.
For example, If the guest OS is an Linux machine. You may invoke lsusb and find CanoKey QEMU there:

$ lsusb
Bus 001 Device 002: ID 20a0:42d4 Clay Logic CanoKey QEMU

You may setup the key as guided in6 . The console for the key is at7 .
  6 https://docs.canokeys.org/
  7 https://console.canokeys.org/




130                                                                                  Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


Debugging

CanoKey QEMU consists of two parts, libcanokey-qemu.so and canokey.c, the latter of which resides in QEMU.
The former provides core functionality of a secure key while the latter provides platform-dependent functions: USB
packet handling.
If you want to trace what happens inside the secure key, when compiling libcanokey-qemu, you should add
-DQEMU_DEBUG_OUTPUT=ON in cmake command line:

cmake .. -DQEMU_DEBUG_OUTPUT=ON

If you want to trace events happened in canokey.c, use
qemu-system-x86_64 --trace "canokey_*" \
    -usb -device canokey,file=$HOME/.canokey-file
If you want to capture USB packets between the guest and the host, you can:
qemu-system-x86_64 -usb -device canokey,file=$HOME/.canokey-file,pcap=key.pcap


Limitations

Currently libcanokey-qemu.so has dozens of global variables as it was originally designed for embedded systems. Thus
one qemu instance can not have multiple CanoKey QEMU running, namely you can not
qemu-system-x86_64 -usb -device canokey,file=$HOME/.canokey-file \
     -device canokey,file=$HOME/.canokey-file2
Also, there is no lock on canokey-file, thus two CanoKey QEMU instance can not read one canokey-file at the same
time.


References

Universal Second Factor (U2F) USB Key Device

U2F is an open authentication standard that enables relying parties exposed to the internet to offer a strong second
factor option for end user authentication.
The second factor is provided by a device implementing the U2F protocol. In case of a USB U2F security key, it is a
USB HID device that implements the U2F protocol.
QEMU supports both pass-through of a host U2F key device to a VM, and software emulation of a U2F key.


u2f-passthru

The u2f-passthru device allows you to connect a real hardware U2F key on your host to a guest VM. All requests
made from the guest are passed through to the physical security key connected to the host machine and vice versa.
In addition, the dedicated pass-through allows you to share a single U2F security key with several guest VMs, which
is not possible with a simple host device assignment pass-through.
You can specify the host U2F key to use with the hidraw option, which takes the host path to a Linux /dev/hidrawN
device:
qemu-system-x86_64 -usb -device u2f-passthru,hidraw=/dev/hidraw0




2.3. Device Emulation                                                                                          131
QEMU Documentation, Release 8.1.5


If you don’t specify the device, the u2f-passthru device will autoscan to take the first U2F device it finds on the host
(this requires a working libudev):
qemu-system-x86_64 -usb -device u2f-passthru


u2f-emulated

u2f-emulated is a completely software emulated U2F device. It uses libu2f-emu for the U2F key emulation. libu2f-
emu provides a complete implementation of the U2F protocol device part for all specified transports given by the FIDO
Alliance.
To work, an emulated U2F device must have four elements:
    • ec x509 certificate
    • ec private key
    • counter (four bytes value)
    • 48 bytes of entropy (random bits)
To use this type of device, these have to be configured, and these four elements must be passed one way or another.
Assuming that you have a working libu2f-emu installed on the host, there are three possible ways to configure the
u2f-emulated device:
    • ephemeral
    • setup directory
    • manual
Ephemeral is the simplest way to configure; it lets the device generate all the elements it needs for a single use of the
lifetime of the device. It is the default if you do not pass any other options to the device.
qemu-system-x86_64 -usb -device u2f-emulated
You can pass the device the path of a setup directory on the host using the dir option; the directory must contain these
four files:
    • certificate.pem: ec x509 certificate
    • private-key.pem: ec private key
    • counter: counter value
    • entropy: 48 bytes of entropy
qemu-system-x86_64 -usb -device u2f-emulated,dir=$dir
You can also manually pass the device the paths to each of these files, if you don’t want them all to be in the same
directory, using the options
    • cert
    • priv
    • counter
    • entropy
qemu-system-x86_64 -usb -device u2f-emulated,cert=$DIR1/$FILE1,priv=$DIR2/$FILE2,counter=
 ˓→$DIR3/$FILE3,entropy=$DIR4/$FILE4




132                                                                                 Chapter 2. System Emulation
                                                                                     QEMU Documentation, Release 8.1.5


igb

igb is a family of Intel’s gigabit ethernet controllers. In QEMU, 82576 emulation is implemented in particular. Its
datasheet is available at1 .
This implementation is expected to be useful to test SR-IOV networking without requiring physical hardware.


Limitations

This igb implementation was tested with Linux Test Project2 and Windows HLK3 during the initial development. Later
it was also tested with DPDK Test Suite4 . The command used when testing with LTP is:

network.sh -6mta

Be aware that this implementation lacks many functionalities available with the actual hardware, and you may experi-
ence various failures if you try to use it with a different operating system other than DPDK, Linux, and Windows or if
you try functionalities not covered by the tests.


Using igb

Using igb should be nothing different from using another network device. See Network emulation in general.
However, you may also need to perform additional steps to activate SR-IOV feature on your guest. For Linux, refer to5 .


Developing igb

igb is the successor of e1000e, and e1000e is the successor of e1000 in turn. As these devices are very similar, if you
make a change for igb and the same change can be applied to e1000e and e1000, please do so.
Please do not forget to run tests before submitting a change. As tests included in QEMU is very minimal, run some
application which is likely to be affected by the change to confirm it works in an integrated system.


Testing igb

A qtest of the basic functionality is available. Run the below at the build directory:

meson test qtest-x86_64/qos-test

ethtool can test register accesses, interrupts, etc. It is automated as an Avocado test and can be ran with the following
command:

make check-avocado AVOCADO_TESTS=tests/avocado/netdev-ethtool.py

  1 https://www.intel.com/content/dam/www/public/us/en/documents/datasheets/82576eb-gigabit-ethernet-controller-datasheet.pdf
  2 https://github.com/linux-test-project/ltp
  3 https://learn.microsoft.com/en-us/windows-hardware/test/hlk/
  4 https://doc.dpdk.org/dts/gsg/
  5 https://docs.kernel.org/PCI/pci-iov-howto.html




2.3. Device Emulation                                                                                                           133
QEMU Documentation, Release 8.1.5


References


2.4 Keys in the graphical frontends

During the graphical emulation, you can use special key combinations to change modes. The default key mappings
are shown below, but if you use -alt-grab then the modifier is Ctrl-Alt-Shift (instead of Ctrl-Alt) and if you use
-ctrl-grab then the modifier is the right Ctrl key (instead of Ctrl-Alt):
Ctrl-Alt-f
      Toggle full screen
Ctrl-Alt-+
      Enlarge the screen
Ctrl-Alt--
      Shrink the screen
Ctrl-Alt-u
      Restore the screen’s un-scaled dimensions
Ctrl-Alt-n
      Switch to virtual console ‘n’. Standard console mappings are:
      1
            Target system display
      2
            Monitor
      3
            Serial port
Ctrl-Alt
      Toggle mouse and keyboard grab.
In the virtual consoles, you can use Ctrl-Up, Ctrl-Down, Ctrl-PageUp and Ctrl-PageDown to move in the back log.


2.5 Keys in the character backend multiplexer

During emulation, if you are using a character backend multiplexer (which is the default if you are using -nographic)
then several commands are available via an escape sequence. These key sequences all start with an escape character,
which is Ctrl-a by default, but can be changed with -echr. The list below assumes you’re using the default.
Ctrl-a h
      Print this help
Ctrl-a x
      Exit emulator
Ctrl-a s
      Save disk data back to file (if -snapshot)
Ctrl-a t
      Toggle console timestamps
Ctrl-a b
      Send break (magic sysrq in Linux)




134                                                                              Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


Ctrl-a c
      Rotate between the frontends connected to the multiplexer (usually this switches between the monitor and the
      console)
Ctrl-a Ctrl-a
      Send the escape character to the frontend


2.6 QEMU Monitor

The QEMU monitor is used to give complex commands to the QEMU emulator. You can use it to:
    • Remove or insert removable media images (such as CD-ROM or floppies).
    • Freeze/unfreeze the Virtual Machine (VM) and save or restore its state from a disk file.
    • Inspect the VM state without an external debugger.


2.6.1 Commands

The following commands are available:
help or ? [cmd]
     Show the help for all commands or just for command cmd.
commit
     Commit changes to the disk images (if -snapshot is used) or backing files. If the backing file is smaller than the
     snapshot, then the backing file will be resized to be the same size as the snapshot. If the snapshot is smaller
     than the backing file, the backing file will not be truncated. If you want the backing file to match the size of the
     smaller snapshot, you can safely truncate it yourself once the commit operation successfully completes.
quit or q
     Quit the emulator.
exit_preconfig
     This command makes QEMU exit the preconfig state and proceed with VM initialization using configuration
     data provided on the command line and via the QMP monitor during the preconfig state. The command is only
     available during the preconfig state (i.e. when the –preconfig command line option was in use).
block_resize
     Resize a block image while a guest is running. Usually requires guest action to see the updated size. Resize to
     a lower size is supported, but should be used with extreme caution. Note that this command only resizes image
     files, it can not resize block devices like LVM volumes.
block_stream
     Copy data from a backing file into a block device.
block_job_set_speed
     Set maximum speed for a background block operation.
block_job_cancel
     Stop an active background block operation (streaming, mirroring).
block_job_complete
     Manually trigger completion of an active background block operation. For mirroring, this will switch the device
     to the destination path.
block_job_pause
     Pause an active block streaming operation.


2.6. QEMU Monitor                                                                                                   135
QEMU Documentation, Release 8.1.5


block_job_resume
     Resume a paused block streaming operation.
eject [-f] device
     Eject a removable medium (use -f to force it).
drive_del device
     Remove host block device. The result is that guest generated IO is no longer submitted against the host device
     underlying the disk. Once a drive has been deleted, the QEMU Block layer returns -EIO which results in IO
     errors in the guest for applications that are reading/writing to the device. These errors are always reported to the
     guest, regardless of the drive’s error actions (drive options rerror, werror).
change device setting
     Change the configuration of a device.
      change diskdevice [-f] filename [format [read-only-mode]]
          Change the medium for a removable disk device to point to filename. eg:

            (qemu) change ide1-cd0 /path/to/some.iso

            -f
                 forces the operation even if the guest has locked the tray.
            format is optional.
            read-only-mode may be used to change the read-only status of the device. It accepts the following values:
            retain
                 Retains the current status; this is the default.
            read-only
                Makes the device read-only.
            read-write
                Makes the device writable.
      change vnc password [password]
            Change the password associated with the VNC server. If the new password is not supplied, the monitor
            will prompt for it to be entered. VNC passwords are only significant up to 8 letters. eg:

            (qemu) change vnc password
            Password: ********

screendump filename
     Save screen into PPM image filename.
logfile filename
     Output logs to filename.
trace-event
     changes status of a trace event
trace-file on|off|flush
     Open, close, or flush the trace file. If no argument is given, the status of the trace file is displayed.
log item1[,. . . ]
      Activate logging of the specified items.
savevm tag
     Create a snapshot of the whole virtual machine. If tag is provided, it is used as human readable identifier. If
     there is already a snapshot with the same tag, it is replaced. More info at VM snapshots.


136                                                                                   Chapter 2. System Emulation
                                                                              QEMU Documentation, Release 8.1.5


      Since 4.0, savevm stopped allowing the snapshot id to be set, accepting only tag as parameter.
loadvm tag
     Set the whole virtual machine to the snapshot identified by the tag tag.
      Since 4.0, loadvm stopped accepting snapshot id as parameter.
delvm tag
     Delete the snapshot identified by tag.
      Since 4.0, delvm stopped deleting snapshots by snapshot id, accepting only tag as parameter.
one-insn-per-tb [off]
     Run the emulation with one guest instruction per translation block. This slows down emulation a lot, but can be
     useful in some situations, such as when trying to analyse the logs produced by the -d option. This only has an
     effect when using TCG, not with KVM or other accelerators.
      If called with option off, the emulation returns to normal mode.
singlestep [off]
     This is a deprecated synonym for the one-insn-per-tb command.
stop or s
     Stop emulation.
cont or c
     Resume emulation.
system_wakeup
     Wakeup guest from suspend.
gdbserver [port]
     Start gdbserver session (default port=1234)
x/fmt addr
     Virtual memory dump starting at addr.
xp /fmt addr
     Physical memory dump starting at addr.
      fmt is a format which tells the command how to format the data. Its syntax is: /{count}{format}{size}
      count
           is the number of items to be dumped.
      format
          can be x (hex), d (signed decimal), u (unsigned decimal), o (octal), c (char) or i (asm instruction).
      size
             can be b (8 bits), h (16 bits), w (32 bits) or g (64 bits). On x86, h or w can be specified with the i format to
             respectively select 16 or 32 bit code instruction size.
      Examples:
      Dump 10 instructions at the current instruction pointer:

      (qemu) x/10i $eip
      0x90107063: ret
      0x90107064: sti
      0x90107065: lea             0x0(%esi,1),%esi
      0x90107069: lea             0x0(%edi,1),%edi
      0x90107070: ret
      0x90107071: jmp             0x90107080
                                                                                                        (continues on next page)


2.6. QEMU Monitor                                                                                                         137
QEMU Documentation, Release 8.1.5


                                                                                             (continued from previous page)
      0x90107073:      nop
      0x90107074:      nop
      0x90107075:      nop
      0x90107076:      nop

      Dump 80 16 bit values at the start of the video memory:

      (qemu) xp/80hx 0xb8000
      0x000b8000: 0x0b50 0x0b6c 0x0b65 0x0b78 0x0b38 0x0b36 0x0b2f 0x0b42
      0x000b8010: 0x0b6f 0x0b63 0x0b68 0x0b73 0x0b20 0x0b56 0x0b47 0x0b41
      0x000b8020: 0x0b42 0x0b69 0x0b6f 0x0b73 0x0b20 0x0b63 0x0b75 0x0b72
      0x000b8030: 0x0b72 0x0b65 0x0b6e 0x0b74 0x0b2d 0x0b63 0x0b76 0x0b73
      0x000b8040: 0x0b20 0x0b30 0x0b35 0x0b20 0x0b4e 0x0b6f 0x0b76 0x0b20
      0x000b8050: 0x0b32 0x0b30 0x0b30 0x0b33 0x0720 0x0720 0x0720 0x0720
      0x000b8060: 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720
      0x000b8070: 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720
      0x000b8080: 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720
      0x000b8090: 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720 0x0720

gpa2hva addr
     Print the host virtual address at which the guest’s physical address addr is mapped.
gpa2hpa addr
     Print the host physical address at which the guest’s physical address addr is mapped.
gva2gpa addr
     Print the guest physical address at which the guest’s virtual address addr is mapped based on the mapping for
     the current CPU.
print or p/fmt expr
     Print expression value. Only the format part of fmt is used.
i/fmt addr [.index]
     Read I/O port.
o/fmt addr val
     Write to I/O port.
sendkey keys
     Send keys to the guest. keys could be the name of the key or the raw value in hexadecimal format. Use - to press
     several keys simultaneously. Example:

      sendkey ctrl-alt-f1

      This command is useful to send keys that your graphical user interface intercepts at low level, such as
      ctrl-alt-f1 in X Window.
sync-profile [on|off|reset]
     Enable, disable or reset synchronization profiling. With no arguments, prints whether profiling is on or off.
system_reset
     Reset the system.
system_powerdown
     Power down the system (if supported).
sum addr size
     Compute the checksum of a memory region.


138                                                                               Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


device_add config
     Add device.
device_del id
     Remove device id. id may be a short ID or a QOM object path.
cpu index
      Set the default CPU.
mouse_move dx dy [dz]
     Move the active mouse to the specified coordinates dx dy with optional scroll axis dz.
mouse_button val
     Change the active mouse button state val (1=L, 2=M, 4=R).
mouse_set index
     Set which mouse device receives events at given index, index can be obtained with:

      info mice

wavcapture filename audiodev [frequency [bits [channels]]]
     Capture audio into filename from audiodev, using sample rate frequency bits per sample bits and number of
     channels channels.
      Defaults:
         • Sample rate = 44100 Hz - CD quality
         • Bits = 16
         • Number of channels = 2 - Stereo
stopcapture index
     Stop capture with a given index, index can be obtained with:

      info capture

memsave addr size file
     save to disk virtual memory dump starting at addr of size size.
pmemsave addr size file
     save to disk physical memory dump starting at addr of size size.
boot_set bootdevicelist
     Define new values for the boot device list. Those values will override the values specified on the command line
     through the -boot option.
      The values that can be specified here depend on the machine type, but are the same that can be specified in the
      -boot command line option.
nmi cpu
     Inject an NMI on the default CPU (x86/s390) or all CPUs (ppc64).
ringbuf_write device data
     Write data to ring buffer character device device. data must be a UTF-8 string.
ringbuf_read device
     Read and print up to size bytes from ring buffer character device device. Certain non-printable characters are
     printed \uXXXX, where XXXX is the character code in hexadecimal. Character \ is printed \\. Bug: can screw
     up when the buffer contains invalid UTF-8 sequences, NUL characters, after the ring buffer lost data, and when
     reading stops because the size limit is reached.




2.6. QEMU Monitor                                                                                               139
QEMU Documentation, Release 8.1.5


announce_self
     Trigger a round of GARP/RARP broadcasts; this is useful for explicitly updating the network infrastructure after
     a reconfiguration or some forms of migration. The timings of the round are set by the migration announce
     parameters. An optional comma separated interfaces list restricts the announce to the named set of interfaces.
     An optional id can be used to start a separate announce timer and to change the parameters of it later.
migrate [-d] [-b] [-i] uri
     Migrate to uri (using -d to not wait for completion).
      -b
           for migration with full copy of disk
      -i
           for migration with incremental copy of disk (base image is shared)
migrate_cancel
     Cancel the current VM migration.
migrate_continue state
     Continue migration from the paused state state
migrate_incoming uri
     Continue an incoming migration using the uri (that has the same syntax as the -incoming option).
migrate_recover uri
     Continue a paused incoming postcopy migration using the uri.
migrate_pause
     Pause an ongoing migration. Currently it only supports postcopy.
migrate_set_capability capability state
     Enable/Disable the usage of a capability capability for migration.
migrate_set_parameter parameter value
     Set the parameter parameter for migration.
migrate_start_postcopy
     Switch in-progress migration to postcopy mode. Ignored after the end of migration (or once already in postcopy).
x_colo_lost_heartbeat
     Tell COLO that heartbeat is lost, a failover or takeover is needed.
client_migrate_info protocol hostname port tls-port cert-subject
     Set migration information for remote display. This makes the server ask the client to automatically reconnect
     using the new parameters once migration finished successfully. Only implemented for SPICE.
dump-guest-memory [-p] filename begin length
dump-guest-memory [-z|-l|-s|-w] filename
     Dump guest memory to protocol. The file can be processed with crash or gdb. Without -z|-l|-s|-w, the dump
     format is ELF.
      -p
           do paging to get guest’s memory mapping.
      -z
           dump in kdump-compressed format, with zlib compression.
      -l
           dump in kdump-compressed format, with lzo compression.
      -s
           dump in kdump-compressed format, with snappy compression.



140                                                                              Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5


      -w
            dump in Windows crashdump format (can be used instead of ELF-dump converting), for Windows x64
            guests with vmcoreinfo driver only
      filename
            dump file name.
      begin
           the starting physical address. It’s optional, and should be specified together with length.
      length
           the memory size, in bytes. It’s optional, and should be specified together with begin.
dump-skeys filename
     Save guest storage keys to a file.
migration_mode mode
     Enables or disables migration mode.
snapshot_blkdev
     Snapshot device, using snapshot file as target if provided
snapshot_blkdev_internal
     Take an internal snapshot on device if it support
snapshot_delete_blkdev_internal
     Delete an internal snapshot on device if it support
drive_mirror
     Start mirroring a block device’s writes to a new destination, using the specified target.
drive_backup
     Start a point-in-time copy of a block device to a specified target.
drive_add
     Add drive to PCI storage controller.
pcie_aer_inject_error
     Inject PCIe AER error
netdev_add
     Add host network device.
netdev_del
     Remove host network device.
object_add
     Create QOM object.
object_del
     Destroy QOM object.
hostfwd_add
     Redirect TCP or UDP connections from host to guest (requires -net user).
hostfwd_remove
     Remove host-to-guest TCP or UDP redirection.
balloon value
     Request VM to change its memory allocation to value (in MB).
set_link name [on|off]
     Switch link name on (i.e. up) or off (i.e. down).



2.6. QEMU Monitor                                                                                        141
QEMU Documentation, Release 8.1.5


watchdog_action
     Change watchdog action.
nbd_server_start host:port
     Start an NBD server on the given host and/or port. If the -a option is included, all of the virtual machine’s block
     devices that have an inserted media on them are automatically exported; in this case, the -w option makes the
     devices writable too.
nbd_server_add device [ name ]
     Export a block device through QEMU’s NBD server, which must be started beforehand with
     nbd_server_start. The -w option makes the exported device writable too. The export name is con-
     trolled by name, defaulting to device.
nbd_server_remove [-f] name
     Stop exporting a block device through QEMU’s NBD server, which was previously started with
     nbd_server_add. The -f option forces the server to drop the export immediately even if clients are connected;
     otherwise the command fails unless there are no clients.
nbd_server_stop
     Stop the QEMU embedded NBD server.
mce cpu bank status mcgstatus addr misc
     Inject an MCE on the given CPU (x86 only).
getfd fdname
     If a file descriptor is passed alongside this command using the SCM_RIGHTS mechanism on unix sockets, it is
     stored using the name fdname for later use by other monitor commands.
closefd fdname
     Close the file descriptor previously assigned to fdname using the getfd command. This is only needed if the
     file descriptor was never used by another monitor command.
block_set_io_throttle device bps bps_rd bps_wr iops iops_rd iops_wr
     Change I/O throttle limits for a block drive to bps bps_rd bps_wr iops iops_rd iops_wr. device can be a block
     device name, a qdev ID or a QOM path.
set_password [ vnc | spice ] password [ -d display ] [ action-if-connected ]
     Change spice/vnc password. display can be used with ‘vnc’ to specify which display to set the password on.
     action-if-connected specifies what should happen in case a connection is established: fail makes the password
     change fail. disconnect changes the password and disconnects the client. keep changes the password and keeps
     the connection up. keep is the default.
expire_password [ vnc | spice ] expire-time [ -d display ]
     Specify when a password for spice/vnc becomes invalid. display behaves the same as in set_password. expire-
     time accepts:
      now
             Invalidate password instantly.
      never
          Password stays valid forever.
      +nsec
           Password stays valid for nsec seconds starting now.
      nsec
             Password is invalidated at the given time. nsec are the seconds passed since 1970, i.e. unix epoch.
chardev-add args
     chardev-add accepts the same parameters as the -chardev command line switch.




142                                                                                 Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


chardev-change args
     chardev-change accepts existing chardev id and then the same arguments as the -chardev command line switch
     (except for “id”).
chardev-remove id
     Removes the chardev id.
chardev-send-break id
     Send a break on the chardev id.
qemu-io device command
     Executes a qemu-io command on the given block device.
qom-list [path]
     Print QOM properties of object at location path
qom-get path property
     Print QOM property property of object at location path
qom-set path property value
     Set QOM property property of object at location path to value value
replay_break icount
     Set replay breakpoint at instruction count icount. Execution stops when the specified instruction is reached.
     There can be at most one breakpoint. When breakpoint is set, any prior one is removed. The breakpoint may
     be set only in replay mode and only “in the future”, i.e. at instruction counts greater than the current one. The
     current instruction count can be observed with info replay.
replay_delete_break
     Remove replay breakpoint which was previously set with replay_break. The command is ignored when there
     are no replay breakpoints.
replay_seek icount
     Automatically proceed to the instruction count icount, when replaying the execution. The command automati-
     cally loads nearest snapshot and replays the execution to find the desired instruction. When there is no preceding
     snapshot or the execution is not replayed, then the command fails. icount for the reference may be observed with
     info replay command.
calc_dirty_rate second
     Start a round of dirty rate measurement with the period specified in second. The result of the dirty rate measure-
     ment may be observed with info dirty_rate command.
set_vcpu_dirty_limit
     Set dirty page rate limit on virtual CPU, the information about all the virtual CPU dirty limit status can be
     observed with info vcpu_dirty_limit command.
cancel_vcpu_dirty_limit
     Cancel dirty page rate limit on virtual CPU, the information about all the virtual CPU dirty limit status can be
     observed with info vcpu_dirty_limit command.
dumpdtb filename
     Dump the FDT in dtb format to filename.
xen-event-inject port
     Notify guest via event channel on port port.
xen-event-list
     List event channels in the guest
info subcommand
     Show various information about the system state.



2.6. QEMU Monitor                                                                                                 143
QEMU Documentation, Release 8.1.5


      info version
          Show the version of QEMU.
      info network
          Show the network state.
      info chardev
          Show the character devices.
      info block
          Show info of one block device or all block devices.
      info blockstats
          Show block device statistics.
      info block-jobs
          Show progress of ongoing block device operations.
      info registers
          Show the cpu registers.
      info lapic
          Show local APIC state
      info cpus
          Show infos for each CPU.
      info history
          Show the command line history.
      info irq
          Show the interrupts statistics (if available).
      info pic
          Show PIC state.
      info rdma
          Show RDMA state.
      info pci
          Show PCI information.
      info tlb
          Show virtual to physical memory mappings.
      info mem
          Show the active virtual memory mappings.
      info mtree
          Show memory tree.
      info jit
          Show dynamic compiler info.
      info opcount
          Show dynamic compiler opcode counters
      info sync-profile [-m|-n] [max]
          Show synchronization profiling info, up to max entries (default: 10), sorted by total wait time.
           -m
                sort by mean wait time




144                                                                               Chapter 2. System Emulation
                                                                      QEMU Documentation, Release 8.1.5


         -n
              do not coalesce objects with the same call site
         When different objects that share the same call site are coalesced, the “Object” field shows—enclosed in
         brackets—the number of objects being coalesced.
    info kvm
        Show KVM information.
    info numa
        Show NUMA information.
    info usb
        Show guest USB devices.
    info usbhost
        Show host USB devices.
    info capture
        Show capture information.
    info snapshots
        Show the currently saved VM snapshots.
    info status
        Show the current VM status (running|paused).
    info mice
        Show which guest mouse is receiving events.
    info vnc
        Show the vnc server status.
    info spice
        Show the spice server status.
    info name
        Show the current VM name.
    info uuid
        Show the current VM UUID.
    info usernet
        Show user network stack connection states.
    info migrate
        Show migration status.
    info migrate_capabilities
        Show current migration capabilities.
    info migrate_parameters
        Show current migration parameters.
    info balloon
        Show balloon information.
    info qtree
        Show device tree.
    info qdm
        Show qdev device model list.




2.6. QEMU Monitor                                                                                           145
QEMU Documentation, Release 8.1.5


      info qom-tree
          Show QOM composition tree.
      info roms
          Show roms.
      info trace-events
          Show available trace-events & their state.
      info tpm
          Show the TPM device.
      info memdev
          Show memory backends
      info memory-devices
          Show memory devices.
      info iothreads
          Show iothread’s identifiers.
      info rocker name
          Show rocker switch.
      info rocker-ports name-ports
          Show rocker ports.
      info rocker-of-dpa-flows name [tbl_id]
          Show rocker OF-DPA flow tables.
      info rocker-of-dpa-groups name [type]
          Show rocker OF-DPA groups.
      info skeys address
          Display the value of a storage key (s390 only)
      info cmma address
          Display the values of the CMMA storage attributes for a range of pages (s390 only)
      info dump
          Display the latest dump status.
      info ramblock
          Dump all the ramblocks of the system.
      info hotpluggable-cpus
          Show information about hotpluggable CPUs
      info vm-generation-id
          Show Virtual Machine Generation ID
      info memory_size_summary
          Display the amount of initially allocated and present hotpluggable (if enabled) memory in bytes.
      info sev
          Show SEV information.
      info replay
          Display the record/replay information: mode and the current icount.
      info dirty_rate
          Display the vcpu dirty rate information.




146                                                                             Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


      info vcpu_dirty_limit
          Display the vcpu dirty page limit information.
      info sgx
          Show intel SGX information.
      info via
          Show guest mos6522 VIA devices.
      stats
          Show runtime-collected statistics
      info virtio
          List all available virtio devices
      info virtio-status path
          Display status of a given virtio device
      info virtio-queue-status path queue
          Display status of a given virtio queue
      info virtio-vhost-queue-status path queue
          Display status of a given vhost queue
      info virtio-queue-element path queue [index]
          Display element of a given virtio queue
      info cryptodev
          Show the crypto devices.


2.6.2 Integer expressions

The monitor understands integers expressions for every integer argument. You can use register names to get the value
of specifics CPU registers by prefixing them with $.


2.7 Disk Images

QEMU supports many disk image formats, including growable disk images (their size increase as non empty sectors
are written), compressed and encrypted disk images.


2.7.1 Quick start for disk image creation

You can create a disk image with the command:

qemu-img create myimage.img mysize

where myimage.img is the disk image filename and mysize is its size in kilobytes. You can add an M suffix to give the
size in megabytes and a G suffix for gigabytes.
See the qemu-img invocation documentation for more information.




2.7. Disk Images                                                                                                147
QEMU Documentation, Release 8.1.5


2.7.2 Snapshot mode

If you use the option -snapshot, all disk images are considered as read only. When sectors in written, they are written
in a temporary file created in /tmp. You can however force the write back to the raw disk images by using the commit
monitor command (or C-a s in the serial console).


2.7.3 VM snapshots

VM snapshots are snapshots of the complete virtual machine including CPU state, RAM, device state and the content
of all the writable disks. In order to use VM snapshots, you must have at least one non removable and writable block
device using the qcow2 disk image format. Normally this device is the first virtual hard drive.
Use the monitor command savevm to create a new VM snapshot or replace an existing one. A human readable name
can be assigned to each snapshot in addition to its numerical ID.
Use loadvm to restore a VM snapshot and delvm to remove a VM snapshot. info snapshots lists the available
snapshots with their associated information:

(qemu) info snapshots
Snapshot devices: hda
Snapshot list (from hda):
ID        TAG                            VM SIZE                DATE                 VM CLOCK
1         start                              41M 2006-08-06 12:38:02             00:00:14.954
2                                            40M 2006-08-06 12:43:29             00:00:18.633
3         msys                               40M 2006-08-06 12:44:04             00:00:23.514

A VM snapshot is made of a VM state info (its size is shown in info snapshots) and a snapshot of every writable
disk image. The VM state info is stored in the first qcow2 non removable and writable block device. The disk image
snapshots are stored in every disk image. The size of a snapshot in a disk image is difficult to evaluate and is not
shown by info snapshots because the associated disk sectors are shared among all the snapshots to save disk space
(otherwise each snapshot would need a full copy of all the disk images).
When using the (unrelated) -snapshot option (Snapshot mode), you can always make VM snapshots, but they are
deleted as soon as you exit QEMU.
VM snapshots currently have the following known limitations:
      • They cannot cope with removable devices if they are removed or inserted after a snapshot is done.
      • A few device drivers still have incomplete snapshot support so their state is not saved or restored properly (in
        particular USB).


2.7.4 Disk image file formats

QEMU supports many image file formats that can be used with VMs as well as with any of the tools (like qemu-img).
This includes the preferred formats raw and qcow2 as well as formats that are supported for compatibility with older
QEMU versions or other hypervisors.
Depending on the image format, different options can be passed to qemu-img create and qemu-img convert using
the -o option. This section describes each format and the options that are supported for it.
raw
        Raw disk image format. This format has the advantage of being simple and easily exportable to all other emula-
        tors. If your file system supports holes (for example in ext2 or ext3 on Linux or NTFS on Windows), then only
        the written sectors will reserve space. Use qemu-img info to know the real size used by the image or ls -ls
        on Unix/Linux.


148                                                                                 Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


    Supported options:
    preallocation
         Preallocation mode (allowed values: off, falloc, full). falloc mode preallocates space for image
         by calling posix_fallocate(). full mode preallocates space for image by writing data to underlying
         storage. This data may or may not be zero, depending on the storage location.
qcow2
    QEMU image format, the most versatile format. Use it to have smaller images (useful if your filesystem does not
    supports holes, for example on Windows), zlib based compression and support of multiple VM snapshots.
    Supported options:
    compat
         Determines the qcow2 version to use. compat=0.10 uses the traditional image format that can be read
         by any QEMU since 0.10. compat=1.1 enables image format extensions that only QEMU 1.1 and newer
         understand (this is the default). Amongst others, this includes zero clusters, which allow efficient copy-on-
         read for sparse images.
    backing_file
         File name of a base image (see create subcommand)
    backing_fmt
         Image format of the base image
    encryption
         This option is deprecated and equivalent to encrypt.format=aes
    encrypt.format
         If this is set to luks, it requests that the qcow2 payload (not qcow2 header) be encrypted using the LUKS
         format. The passphrase to use to unlock the LUKS key slot is given by the encrypt.key-secret param-
         eter. LUKS encryption parameters can be tuned with the other encrypt.* parameters.
         If this is set to aes, the image is encrypted with 128-bit AES-CBC. The encryption key is given by the
         encrypt.key-secret parameter. This encryption format is considered to be flawed by modern cryptog-
         raphy standards, suffering from a number of design problems:
             • The AES-CBC cipher is used with predictable initialization vectors based on the sector number. This
               makes it vulnerable to chosen plaintext attacks which can reveal the existence of encrypted data.
             • The user passphrase is directly used as the encryption key. A poorly chosen or short passphrase will
               compromise the security of the encryption.
             • In the event of the passphrase being compromised there is no way to change the passphrase to protect
               data in any qcow images. The files must be cloned, using a different encryption passphrase in the new
               file. The original file must then be securely erased using a program like shred, though even this is
               ineffective with many modern storage technologies.
         The use of this is no longer supported in system emulators. Support only remains in the command line
         utilities, for the purposes of data liberation and interoperability with old versions of QEMU. The luks
         format should be used instead.
    encrypt.key-secret
         Provides the ID of a secret object that contains the passphrase (encrypt.format=luks) or encryption
         key (encrypt.format=aes).
    encrypt.cipher-alg
         Name of the cipher algorithm and key length. Currently defaults to aes-256. Only used when encrypt.
         format=luks.


2.7. Disk Images                                                                                                 149
QEMU Documentation, Release 8.1.5


      encrypt.cipher-mode
          Name of the encryption mode to use. Currently defaults to xts. Only used when encrypt.format=luks.
      encrypt.ivgen-alg
          Name of the initialization vector generator algorithm. Currently defaults to plain64. Only used when
          encrypt.format=luks.
      encrypt.ivgen-hash-alg
          Name of the hash algorithm to use with the initialization vector generator (if required). Defaults to sha256.
          Only used when encrypt.format=luks.
      encrypt.hash-alg
          Name of the hash algorithm to use for PBKDF algorithm Defaults to sha256. Only used when encrypt.
          format=luks.
      encrypt.iter-time
          Amount of time, in milliseconds, to use for PBKDF algorithm per key slot. Defaults to 2000. Only used
          when encrypt.format=luks.
      cluster_size
          Changes the qcow2 cluster size (must be between 512 and 2M). Smaller cluster sizes can improve the image
          file size whereas larger cluster sizes generally provide better performance.
      preallocation
          Preallocation mode (allowed values: off, metadata, falloc, full). An image with preallocated meta-
          data is initially larger but can improve performance when the image needs to grow. falloc and full
          preallocations are like the same options of raw format, but sets up metadata also.
      lazy_refcounts
          If this option is set to on, reference count updates are postponed with the goal of avoiding metadata I/O and
          improving performance. This is particularly interesting with cache=writethrough which doesn’t batch
          metadata updates. The tradeoff is that after a host crash, the reference count tables must be rebuilt, i.e. on
          the next open an (automatic) qemu-img check -r all is required, which may take some time.
           This option can only be enabled if compat=1.1 is specified.
      nocow
          If this option is set to on, it will turn off COW of the file. It’s only valid on btrfs, no effect on other file
          systems.
           Btrfs has low performance when hosting a VM image file, even more when the guest on the VM also using
           btrfs as file system. Turning off COW is a way to mitigate this bad performance. Generally there are two
           ways to turn off COW on btrfs:
              • Disable it by mounting with nodatacow, then all newly created files will be NOCOW.
              • For an empty file, add the NOCOW file attribute. That’s what this option does.
           Note: this option is only valid to new or empty files. If there is an existing file which is COW and has data
           blocks already, it couldn’t be changed to NOCOW by setting nocow=on. One can issue lsattr filename
           to check if the NOCOW flag is set or not (Capital ‘C’ is NOCOW flag).
qed
      Old QEMU image format with support for backing files and compact image files (when your filesystem or trans-
      port medium does not support holes).
      When converting QED images to qcow2, you might want to consider using the lazy_refcounts=on option to
      get a more QED-like behaviour.
      Supported options:


150                                                                                  Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5


       backing_file
            File name of a base image (see create subcommand).
       backing_fmt
            Image file format of backing file (optional). Useful if the format cannot be autodetected because it has no
            header, like some vhd/vpc files.
       cluster_size
            Changes the cluster size (must be power-of-2 between 4K and 64K). Smaller cluster sizes can improve the
            image file size whereas larger cluster sizes generally provide better performance.
       table_size
            Changes the number of clusters per L1/L2 table (must be power-of-2 between 1 and 16). There is normally
            no need to change this value but this option can between used for performance benchmarking.
qcow
       Old QEMU image format with support for backing files, compact image files, encryption and compression.
       Supported options:
            backing_file
                File name of a base image (see create subcommand)
            encryption
                This option is deprecated and equivalent to encrypt.format=aes
            encrypt.format
                If this is set to aes, the image is encrypted with 128-bit AES-CBC. The encryption key is given
                by the encrypt.key-secret parameter. This encryption format is considered to be flawed
                by modern cryptography standards, suffering from a number of design problems enumerated
                previously against the qcow2 image format.
                The use of this is no longer supported in system emulators. Support only remains in the command
                line utilities, for the purposes of data liberation and interoperability with old versions of QEMU.
                Users requiring native encryption should use the qcow2 format instead with encrypt.
                format=luks.
            encrypt.key-secret
                Provides the ID of a secret object that contains the encryption key (encrypt.format=aes).
luks
       LUKS v1 encryption format, compatible with Linux dm-crypt/cryptsetup
       Supported options:
       key-secret
            Provides the ID of a secret object that contains the passphrase.
       cipher-alg
            Name of the cipher algorithm and key length. Currently defaults to aes-256.
       cipher-mode
            Name of the encryption mode to use. Currently defaults to xts.
       ivgen-alg
            Name of the initialization vector generator algorithm. Currently defaults to plain64.




2.7. Disk Images                                                                                                      151
QEMU Documentation, Release 8.1.5


       ivgen-hash-alg
            Name of the hash algorithm to use with the initialization vector generator (if required). Defaults to sha256.
       hash-alg
            Name of the hash algorithm to use for PBKDF algorithm Defaults to sha256.
       iter-time
            Amount of time, in milliseconds, to use for PBKDF algorithm per key slot. Defaults to 2000.
vdi
       VirtualBox 1.1 compatible image format.
       Supported options:
       static
            If this option is set to on, the image is created with metadata preallocation.
vmdk
       VMware 3 and 4 compatible image format.
       Supported options:
       backing_file
            File name of a base image (see create subcommand).
       compat6
            Create a VMDK version 6 image (instead of version 4)
       hwversion
            Specify vmdk virtual hardware version. Compat6 flag cannot be enabled if hwversion is specified.
       subformat
            Specifies which VMDK subformat to use.    Valid options are monolithicSparse (default),
            monolithicFlat, twoGbMaxExtentSparse, twoGbMaxExtentFlat and streamOptimized.
vpc
       VirtualPC compatible image format (VHD).
       Supported options:
       subformat
            Specifies which VHD subformat to use. Valid options are dynamic (default) and fixed.
VHDX
       Hyper-V compatible image format (VHDX).
       Supported options:
       subformat
            Specifies which VHDX subformat to use. Valid options are dynamic (default) and fixed.
            block_state_zero
                 Force use of payload blocks of type ‘ZERO’. Can be set to on (default) or off. When set to
                 off, new blocks will be created as PAYLOAD_BLOCK_NOT_PRESENT, which means parsers are free
                 to return arbitrary data for those blocks. Do not set to off when using qemu-img convert with
                 subformat=dynamic.
            block_size
                 Block size; min 1 MB, max 256 MB. 0 means auto-calculate based on image size.



152                                                                                  Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


           log_size
                Log size; min 1 MB.


2.7.5 Read-only formats

More disk image file formats are supported in a read-only mode.
bochs
      Bochs images of growing type.
cloop
      Linux Compressed Loop image, useful only to reuse directly compressed CD-ROM images present for example
      in the Knoppix CD-ROMs.
dmg
      Apple disk image.
parallels
      Parallels disk image format.


2.7.6 Using host drives

In addition to disk image files, QEMU can directly access host devices. We describe here the usage for QEMU version
>= 0.8.3.


Linux

On Linux, you can directly use the host device filename instead of a disk image filename provided you have enough
privileges to access it. For example, use /dev/cdrom to access to the CDROM.
CD
      You can specify a CDROM device even if no CDROM is loaded. QEMU has specific code to detect CDROM
      insertion or removal. CDROM ejection by the guest OS is supported. Currently only data CDs are supported.
Floppy
     You can specify a floppy device even if no floppy is loaded. Floppy removal is currently not detected accurately
     (if you change floppy without doing floppy access while the floppy is not loaded, the guest OS will think that
     the same floppy is loaded). Use of the host’s floppy device is deprecated, and support for it will be removed in a
     future release.
Hard disks
     Hard disks can be used. Normally you must specify the whole disk (/dev/hdb instead of /dev/hdb1) so that
     the guest OS can see it as a partitioned disk. WARNING: unless you know what you do, it is better to only make
     READ-ONLY accesses to the hard disk otherwise you may corrupt your host data (use the -snapshot command
     line option or modify the device permissions accordingly).
Zoned block devices
     Zoned block devices can be passed through to the guest if the emulated storage controller supports zoned stor-
     age. Use --blockdev host_device, node-name=drive0,filename=/dev/nullb0,cache.direct=on
     to pass through /dev/nullb0 as drive0.




2.7. Disk Images                                                                                                  153
QEMU Documentation, Release 8.1.5


Windows

CD
       The preferred syntax is the drive letter (e.g. d:). The alternate syntax \\.\d: is supported. /dev/cdrom is
       supported as an alias to the first CDROM drive.
       Currently there is no specific code to handle removable media, so it is better to use the change or eject monitor
       commands to change or eject media.
Hard disks
     Hard disks can be used with the syntax: \\.\PhysicalDriveN where N is the drive number (0 is the first hard
     disk).
       WARNING: unless you know what you do, it is better to only make READ-ONLY accesses to the hard disk
       otherwise you may corrupt your host data (use the -snapshot command line so that the modifications are written
       in a temporary file).


Mac OS X

/dev/cdrom is an alias to the first CDROM.
Currently there is no specific code to handle removable media, so it is better to use the change or eject monitor
commands to change or eject media.


2.7.7 Virtual FAT disk images

QEMU can automatically create a virtual FAT disk image from a directory tree. In order to use it, just type:
qemu-system-x86_64 linux.img -hdb fat:/my_directory
Then you access access to all the files in the /my_directory directory without having to copy them in a disk image
or to export them via SAMBA or NFS. The default access is read-only.
Floppies can be emulated with the :floppy: option:
qemu-system-x86_64 linux.img -fda fat:floppy:/my_directory
A read/write support is available for testing (beta stage) with the :rw: option:
qemu-system-x86_64 linux.img -fda fat:floppy:rw:/my_directory
What you should never do:
     • use non-ASCII filenames
     • use “-snapshot” together with “:rw:”
     • expect it to work when loadvm’ing
     • write to the FAT directory on the host system while accessing it with the guest system




154                                                                                Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


2.7.8 NBD access

QEMU can access directly to block device exported using the Network Block Device protocol.
qemu-system-x86_64 linux.img -hdb nbd://my_nbd_server.mydomain.org:1024/
If the NBD server is located on the same host, you can use an unix socket instead of an inet socket:
qemu-system-x86_64 linux.img -hdb nbd+unix://?socket=/tmp/my_socket
In this case, the block device must be exported using qemu-nbd:

qemu-nbd --socket=/tmp/my_socket my_disk.qcow2

The use of qemu-nbd allows sharing of a disk between several guests:

qemu-nbd --socket=/tmp/my_socket --share=2 my_disk.qcow2

and then you can use it with two guests:
qemu-system-x86_64 linux1.img -hdb nbd+unix://?socket=/tmp/my_socket
qemu-system-x86_64 linux2.img -hdb nbd+unix://?socket=/tmp/my_socket
If the nbd-server uses named exports (supported since NBD 2.9.18, or with QEMU’s own embedded NBD server),
you must specify an export name in the URI:
qemu-system-x86_64 -cdrom nbd://localhost/debian-500-ppc-netinst
qemu-system-x86_64 -cdrom nbd://localhost/openSUSE-11.1-ppc-netinst
The URI syntax for NBD is supported since QEMU 1.3. An alternative syntax is also available. Here are some example
of the older syntax:
qemu-system-x86_64 linux.img -hdb nbd:my_nbd_server.mydomain.org:1024
qemu-system-x86_64 linux2.img -hdb nbd:unix:/tmp/my_socket
qemu-system-x86_64 -cdrom nbd:localhost:10809:exportname=debian-500-ppc-netinst


2.7.9 iSCSI LUNs

iSCSI is a popular protocol used to access SCSI devices across a computer network.
There are two different ways iSCSI devices can be used by QEMU.
The first method is to mount the iSCSI LUN on the host, and make it appear as any other ordinary SCSI device on the
host and then to access this device as a /dev/sd device from QEMU. How to do this differs between host OSes.
The second method involves using the iSCSI initiator that is built into QEMU. This provides a mechanism that works
the same way regardless of which host OS you are running QEMU on. This section will describe this second method
of using iSCSI together with QEMU.
In QEMU, iSCSI devices are described using special iSCSI URLs. URL syntax:

iscsi://[<username>[%<password>]@]<host>[:<port>]/<target-iqn-name>/<lun>

Username and password are optional and only used if your target is set up using CHAP authentication for access control.
Alternatively the username and password can also be set via environment variables to have these not show up in the
process list:

export LIBISCSI_CHAP_USERNAME=<username>
export LIBISCSI_CHAP_PASSWORD=<password>
iscsi://<host>/<target-iqn-name>/<lun>


2.7. Disk Images                                                                                                  155
QEMU Documentation, Release 8.1.5


Various session related parameters can be set via special options, either in a configuration file provided via ‘-readconfig’
or directly on the command line.
If the initiator-name is not specified qemu will use a default name of ‘iqn.2008-11.org.linux-kvm[:<uuid>’] where
<uuid> is the UUID of the virtual machine. If the UUID is not specified qemu will use ‘iqn.2008-11.org.linux-
kvm[:<name>’] where <name> is the name of the virtual machine.
Setting a specific initiator name to use when logging in to the target:

-iscsi initiator-name=iqn.qemu.test:my-initiator

Controlling which type of header digest to negotiate with the target:

-iscsi header-digest=CRC32C|CRC32C-NONE|NONE-CRC32C|NONE

These can also be set via a configuration file:

[iscsi]
  user = "CHAP username"
  password = "CHAP password"
  initiator-name = "iqn.qemu.test:my-initiator"
  # header digest is one of CRC32C|CRC32C-NONE|NONE-CRC32C|NONE
  header-digest = "CRC32C"

Setting the target name allows different options for different targets:

[iscsi "iqn.target.name"]
  user = "CHAP username"
  password = "CHAP password"
  initiator-name = "iqn.qemu.test:my-initiator"
  # header digest is one of CRC32C|CRC32C-NONE|NONE-CRC32C|NONE
  header-digest = "CRC32C"

How to use a configuration file to set iSCSI configuration options:
cat >iscsi.conf <<EOF
[iscsi]
  user = "me"
  password = "my password"
  initiator-name = "iqn.qemu.test:my-initiator"
  header-digest = "CRC32C"
EOF

qemu-system-x86_64 -drive file=iscsi://127.0.0.1/iqn.qemu.test/1 \
  -readconfig iscsi.conf
How to set up a simple iSCSI target on loopback and access it via QEMU: this example shows how to set up an iSCSI
target with one CDROM and one DISK using the Linux STGT software target. This target is available on Red Hat
based systems as the package ‘scsi-target-utils’.
tgtd --iscsi portal=127.0.0.1:3260
tgtadm --lld iscsi --op new --mode target --tid 1 -T iqn.qemu.test
tgtadm --lld iscsi --mode logicalunit --op new --tid 1 --lun 1 \
    -b /IMAGES/disk.img --device-type=disk
tgtadm --lld iscsi --mode logicalunit --op new --tid 1 --lun 2 \
    -b /IMAGES/cd.iso --device-type=cd
tgtadm --lld iscsi --op bind --mode target --tid 1 -I ALL


156                                                                                   Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5




qemu-system-x86_64 -iscsi initiator-name=iqn.qemu.test:my-initiator \
  -boot d -drive file=iscsi://127.0.0.1/iqn.qemu.test/1 \
  -cdrom iscsi://127.0.0.1/iqn.qemu.test/2


2.7.10 GlusterFS disk images

GlusterFS is a user space distributed file system.
You can boot from the GlusterFS disk image with the command:
URI:
qemu-system-x86_64 -drive file=gluster[+TYPE]://[HOST}[:PORT]]/VOLUME/PATH
                             [?socket=...][,file.debug=9][,file.logfile=...]
JSON:
qemu-system-x86_64 'json:{"driver":"qcow2",
                         "file":{"driver":"gluster",
                                  "volume":"testvol","path":"a.img","debug":9,
 ˓→"logfile":"...",

                                  "server":[{"type":"tcp","host":"...","port":"..."},
                                            {"type":"unix","socket":"..."}]}}'
gluster is the protocol.
TYPE specifies the transport type used to connect to gluster management daemon (glusterd). Valid transport types are
tcp and unix. In the URI form, if a transport type isn’t specified, then tcp type is assumed.
HOST specifies the server where the volume file specification for the given volume resides. This can be either a
hostname or an ipv4 address. If transport type is unix, then HOST field should not be specified. Instead socket field
needs to be populated with the path to unix domain socket.
PORT is the port number on which glusterd is listening. This is optional and if not specified, it defaults to port 24007.
If the transport type is unix, then PORT should not be specified.
VOLUME is the name of the gluster volume which contains the disk image.
PATH is the path to the actual disk image that resides on gluster volume.
debug is the logging level of the gluster protocol driver. Debug levels are 0-9, with 9 being the most verbose, and 0
representing no debugging output. The default level is 4. The current logging levels defined in the gluster source are 0
- None, 1 - Emergency, 2 - Alert, 3 - Critical, 4 - Error, 5 - Warning, 6 - Notice, 7 - Info, 8 - Debug, 9 - Trace
logfile is a commandline option to mention log file path which helps in logging to the specified file and also help in
persisting the gfapi logs. The default is stderr.
You can create a GlusterFS disk image with the command:

qemu-img create gluster://HOST/VOLUME/PATH SIZE

Examples
qemu-system-x86_64 -drive file=gluster://1.2.3.4/testvol/a.img
qemu-system-x86_64 -drive file=gluster+tcp://1.2.3.4/testvol/a.img
qemu-system-x86_64 -drive file=gluster+tcp://1.2.3.4:24007/testvol/dir/a.img
qemu-system-x86_64 -drive file=gluster+tcp://[1:2:3:4:5:6:7:8]/testvol/dir/a.img
qemu-system-x86_64 -drive file=gluster+tcp://[1:2:3:4:5:6:7:8]:24007/testvol/dir/a.img
qemu-system-x86_64 -drive file=gluster+tcp://server.domain.com:24007/testvol/dir/a.img



2.7. Disk Images                                                                                                    157
QEMU Documentation, Release 8.1.5


qemu-system-x86_64 -drive file=gluster+unix:///testvol/dir/a.img?socket=/tmp/glusterd.
 ˓→socket

qemu-system-x86_64 -drive file=gluster+rdma://1.2.3.4:24007/testvol/a.img
qemu-system-x86_64 -drive file=gluster://1.2.3.4/testvol/a.img,file.debug=9,file.
 ˓→logfile=/var/log/qemu-gluster.log

qemu-system-x86_64 'json:{"driver":"qcow2",
                          "file":{"driver":"gluster",
                                   "volume":"testvol","path":"a.img",
                                   "debug":9,"logfile":"/var/log/qemu-gluster.log",
                                   "server":[{"type":"tcp","host":"1.2.3.4","port":24007},
                                             {"type":"unix","socket":"/var/run/glusterd.
 ˓→socket"}]}}'

qemu-system-x86_64 -drive driver=qcow2,file.driver=gluster,file.volume=testvol,file.
 ˓→path=/path/a.img,

                                      file.debug=9,file.logfile=/var/log/qemu-gluster.log,
                                      file.server.0.type=tcp,file.server.0.host=1.2.3.4,
 ˓→file.server.0.port=24007,

                                      file.server.1.type=unix,file.server.1.socket=/var/
 ˓→run/glusterd.socket




2.7.11 Secure Shell (ssh) disk images

You can access disk images located on a remote ssh server by using the ssh protocol:
qemu-system-x86_64 -drive file=ssh://[USER@]SERVER[:PORT]/PATH[?host_key_check=HOST_KEY_
 ˓→CHECK]


Alternative syntax using properties:
qemu-system-x86_64 -drive file.driver=ssh[,file.user=USER],file.host=SERVER[,file.
 ˓→port=PORT],file.path=PATH[,file.host_key_check=HOST_KEY_CHECK]


ssh is the protocol.
USER is the remote user. If not specified, then the local username is tried.
SERVER specifies the remote ssh server. Any ssh server can be used, but it must implement the sftp-server protocol.
Most Unix/Linux systems should work without requiring any extra configuration.
PORT is the port number on which sshd is listening. By default the standard ssh port (22) is used.
PATH is the path to the disk image.
The optional HOST_KEY_CHECK parameter controls how the remote host’s key is checked. The default is yes which
means to use the local .ssh/known_hosts file. Setting this to no turns off known-hosts checking. Or you can check
that the host key matches a specific fingerprint. The fingerprint can be provided in md5, sha1, or sha256 format,
however, it is strongly recommended to only use sha256, since the other options are considered insecure by modern
standards. The fingerprint value must be given as a hex encoded string:

host_key_check=sha256:04ce2ae89ff4295a6b9c4111640bdcb3297858ee55cb434d9dd88796e93aa795

The key string may optionally contain “:” separators between each pair of hex digits.
The $HOME/.ssh/known_hosts file contains the base64 encoded host keys. These can be converted into the format
needed for QEMU using a command such as:




158                                                                               Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5



$ for key in `grep 10.33.8.112 known_hosts | awk '{print $3}'`
  do
    echo $key | base64 -d | sha256sum
  done
  6c3aa525beda9dc83eadfbd7e5ba7d976ecb59575d1633c87cd06ed2ed6e366f                        -
  12214fd9ea5b408086f98ecccd9958609bd9ac7c0ea316734006bc7818b45dc8                        -
  d36420137bcbd101209ef70c3b15dc07362fbe0fa53c5b135eba6e6afa82f0ce                        -

Note that there can be multiple keys present per host, each with different key ciphers. Care is needed to pick the key
fingerprint that matches the cipher QEMU will negotiate with the remote server.
Currently authentication must be done using ssh-agent. Other authentication methods may be supported in future.
Note: Many ssh servers do not support an fsync-style operation. The ssh driver cannot guarantee that disk flush
requests are obeyed, and this causes a risk of disk corruption if the remote server or network goes down during writes.
The driver will print a warning when fsync is not supported:

warning: ssh server ssh.example.com:22 does not support fsync

With sufficiently new versions of libssh and OpenSSH, fsync is supported.


2.7.12 NVMe disk images

NVM Express (NVMe) storage controllers can be accessed directly by a userspace driver in QEMU. This bypasses
the host kernel file system and block layers while retaining QEMU block layer functionalities, such as block jobs, I/O
throttling, image formats, etc. Disk I/O performance is typically higher than with -drive file=/dev/sda using
either thread pool or linux-aio.
The controller will be exclusively used by the QEMU process once started. To be able to share storage between multiple
VMs and other applications on the host, please use the file based protocols.
Before starting QEMU, bind the host NVMe controller to the host vfio-pci driver. For example:
# modprobe vfio-pci
# lspci -n -s 0000:06:0d.0
06:0d.0 0401: 1102:0002 (rev 08)
# echo 0000:06:0d.0 > /sys/bus/pci/devices/0000:06:0d.0/driver/unbind
# echo 1102 0002 > /sys/bus/pci/drivers/vfio-pci/new_id

# qemu-system-x86_64 -drive file=nvme://HOST:BUS:SLOT.FUNC/NAMESPACE
Alternative syntax using properties:
qemu-system-x86_64 -drive file.driver=nvme,file.device=HOST:BUS:SLOT.FUNC,file.
 ˓→namespace=NAMESPACE


HOST :BUS:SLOT.FUNC is the NVMe controller’s PCI device address on the host.
NAMESPACE is the NVMe namespace number, starting from 1.




2.7. Disk Images                                                                                                  159
QEMU Documentation, Release 8.1.5


2.7.13 Disk image file locking

By default, QEMU tries to protect image files from unexpected concurrent access, as long as it’s supported by the block
protocol driver and host operating system. If multiple QEMU processes (including QEMU emulators and utilities) try
to open the same image with conflicting accessing modes, all but the first one will get an error.
This feature is currently supported by the file protocol on Linux with the Open File Descriptor (OFD) locking API, and
can be configured to fall back to POSIX locking if the POSIX host doesn’t support Linux OFD locking.
To explicitly enable image locking, specify “locking=on” in the file protocol driver options. If OFD locking is not
possible, a warning will be printed and the POSIX locking API will be used. In this case there is a risk that the lock
will get silently lost when doing hot plugging and block jobs, due to the shortcomings of the POSIX locking API.
QEMU transparently handles lock handover during shared storage migration. For shared virtual disk images between
multiple VMs, the “share-rw” device option should be used.
By default, the guest has exclusive write access to its disk image. If the guest can safely share the disk image with other
writers the -device ...,share-rw=on parameter can be used. This is only safe if the guest is running software,
such as a cluster file system, that coordinates disk accesses to avoid corruption.
Note that share-rw=on only declares the guest’s ability to share the disk. Some QEMU features, such as image file
formats, require exclusive write access to the disk image and this is unaffected by the share-rw=on option.
Alternatively, locking can be fully disabled by “locking=off” block device option. In the command line, the option is
usually in the form of “file.locking=off” as the protocol driver is normally placed as a “file” child under a format driver.
For example:

-blockdev driver=qcow2,file.filename=/path/to/image,file.locking=off,file.driver=file

To check if image locking is active, check the output of the “lslocks” command on host and see if there are locks held
by the QEMU process on the image file. More than one byte could be locked by the QEMU instance, each byte of
which reflects a particular permission that is acquired or protected by the running block driver.


2.7.14 Filter drivers

QEMU supports several filter drivers, which don’t store any data, but perform some additional tasks, hooking io re-
quests.
preallocate
      The preallocate filter driver is intended to be inserted between format and protocol nodes and preallocates some
      additional space (expanding the protocol file) when writing past the file’s end. This can be useful for file-systems
      with slow allocation.
      Supported options:
      prealloc-align
            On preallocation, align the file length to this value (in bytes), default 1M.
      prealloc-size
            How much to preallocate (in bytes), default 128M.




160                                                                                   Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5



2.8 QEMU virtio-net standby (net_failover)

This document explains the setup and usage of virtio-net standby feature which is used to create a net_failover pair of
devices.
The general idea is that we have a pair of devices, a (vfio-)pci and a virtio-net device. Before migration the vfio device
is unplugged and data flows through the virtio-net device, on the target side another vfio-pci device is plugged in to
take over the data-path. In the guest the net_failover kernel module will pair net devices with the same MAC address.
The two devices are called primary and standby device. The fast hardware based networking device is called the primary
device and the virtio-net device is the standby device.


2.8.1 Restrictions

Currently only PCIe devices are allowed as primary devices, this restriction can be lifted in the future with enhanced
QEMU support. Also, only networking devices are allowed as primary device. The user needs to ensure that primary
and standby devices are not plugged into the same PCIe slot.


2.8.2 Usecase

      Virtio-net standby allows easy migration while using a passed-through fast networking device by falling
      back to a virtio-net device for the duration of the migration. It is like a simple version of a bond, the
      difference is that it requires no configuration in the guest. When a guest is live-migrated to another host
      QEMU will unplug the primary device via the PCIe based hotplug handler and traffic will go through the
      virtio-net device. On the target system the primary device will be automatically plugged back and the
      net_failover module registers it again as the primary device.


2.8.3 Usage

      The primary device can be hotplugged or be part of the startup configuration
      -device virtio-net-pci,netdev=hostnet1,id=net1,mac=52:54:00:6f:55:cc,
           bus=root2,failover=on
      With the parameter failover=on the VIRTIO_NET_F_STANDBY feature will be enabled.
      -device vfio-pci,host=5e:00.2,id=hostdev0,bus=root1,failover_pair_id=net1
      failover_pair_id references the id of the virtio-net standby device. This is only for pairing the devices
      within QEMU. The guest kernel module net_failover will match devices with identical MAC addresses.


2.8.4 Hotplug

      Both primary and standby device can be hotplugged via the QEMU monitor. Note that if the virtio-net
      device is plugged first a warning will be issued that it couldn’t find the primary device.




2.8. QEMU virtio-net standby (net_failover)                                                                          161
QEMU Documentation, Release 8.1.5


2.8.5 Migration

      A new migration state wait-unplug was added for this feature. If failover primary devices are present in
      the configuration, migration will go into this state. It will wait until the device unplug is completed in
      the guest and then move into active state. On the target system the primary devices will be automatically
      hotplugged when the feature bit was negotiated for the virtio-net standby device.


2.9 Direct Linux Boot

This section explains how to launch a Linux kernel inside QEMU without having to make a full bootable image. It is
very useful for fast Linux kernel testing.
The syntax is:
qemu-system-x86_64 -kernel bzImage -hda rootdisk.img -append "root=/dev/hda"
Use -kernel to provide the Linux kernel image and -append to give the kernel command line arguments. The
-initrd option can be used to provide an INITRD image.
If you do not need graphical output, you can disable it and redirect the virtual serial port and the QEMU monitor to the
console with the -nographic option. The typical command line is:
qemu-system-x86_64 -kernel bzImage -hda rootdisk.img                                            -append "root=/dev/
 ˓→hda console=ttyS0" -nographic


Use Ctrl-a c to switch between the serial console and the monitor (see Keys in the graphical frontends).


2.10 Generic Loader

The ‘loader’ device allows the user to load multiple images or values into QEMU at startup.


2.10.1 Loading Data into Memory Values

The loader device allows memory values to be set from the command line. This can be done by following the syntax
below:

-device loader,addr=<addr>,data=<data>,data-len=<data-len> \
                [,data-be=<data-be>][,cpu-num=<cpu-num>]

<addr>
     The address to store the data in.
<data>
     The value to be written to the address. The maximum size of the data is 8 bytes.
<data-len>
     The length of the data in bytes. This argument must be included if the data argument is.
<data-be>
     Set to true if the data to be stored on the guest should be written as big endian data. The default is to write little
     endian data.
<cpu-num>
     The number of the CPU’s address space where the data should be loaded. If not specified the address space of
     the first CPU is used.


162                                                                                  Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


All values are parsed using the standard QemuOps parsing. This allows the user to specify any values in any format
supported. By default the values will be parsed as decimal. To use hex values the user should prefix the number with
a ‘0x’.
An example of loading value 0x8000000e to address 0xfd1a0104 is:
-device loader,addr=0xfd1a0104,data=0x8000000e,data-len=4



2.10.2 Setting a CPU’s Program Counter

The loader device allows the CPU’s PC to be set from the command line. This can be done by following the syntax
below:
-device loader,addr=<addr>,cpu-num=<cpu-num>

<addr>
     The value to use as the CPU’s PC.
<cpu-num>
     The number of the CPU whose PC should be set to the specified value.
All values are parsed using the standard QemuOpts parsing. This allows the user to specify any values in any format
supported. By default the values will be parsed as decimal. To use hex values the user should prefix the number with
a ‘0x’.
An example of setting CPU 0’s PC to 0x8000 is:
-device loader,addr=0x8000,cpu-num=0



2.10.3 Loading Files

The loader device also allows files to be loaded into memory. It can load ELF, U-Boot, and Intel HEX executable
formats as well as raw images. The syntax is shown below:
      -device loader,file=<file>[,addr=<addr>][,cpu-num=<cpu-num>][,force-raw=<raw>]
<file>
     A file to be loaded into memory
<addr>
     The memory address where the file should be loaded. This is required for raw images and ignored for non-raw
     files.
<cpu-num>
     This specifies the CPU that should be used. This is an optional argument and will cause the CPU’s PC to be set
     to the memory address where the raw file is loaded or the entry point specified in the executable format header.
     This option should only be used for the boot image. This will also cause the image to be written to the specified
     CPU’s address space. If not specified, the default is CPU 0.
<force-raw>
     Setting ‘force-raw=on’ forces the file to be treated as a raw image. This can be used to load supported executable
     formats as if they were raw.
All values are parsed using the standard QemuOpts parsing. This allows the user to specify any values in any format
supported. By default the values will be parsed as decimal. To use hex values the user should prefix the number with
a ‘0x’.
An example of loading an ELF file which CPU0 will boot is shown below:


2.10. Generic Loader                                                                                              163
QEMU Documentation, Release 8.1.5



-device loader,file=./images/boot.elf,cpu-num=0



2.10.4 Restrictions and ToDos

At the moment it is just assumed that if you specify a cpu-num then you want to set the PC as well. This might not
always be the case. In future the internal state ‘set_pc’ (which exists in the generic loader now) should be exposed to
the user so that they can choose if the PC is set or not.


2.11 Guest Loader

The guest loader is similar to the generic-loader although it is aimed at a particular use case of loading hypervisor
guests. This is useful for debugging hypervisors without having to jump through the hoops of firmware and boot-
loaders.
The guest loader does two things:
    • load blobs (kernels and initial ram disks) into memory
    • sets platform FDT data so hypervisors can find and boot them
This is what is typically done by a boot-loader like grub using its multi-boot capability. A typical example would look
like:
qemu-system-x86_64 -kernel ~/xen.git/xen/xen    -append "dom0_mem=1G,max:1G loglvl=all␣
 ˓→guest_loglvl=all"   -device guest-loader,addr=0x42000000,kernel=Image,bootargs="root=/
 ˓→dev/sda2 ro console=hvc0 earlyprintk=xen"   -device guest-loader,addr=0x47000000,
 ˓→initrd=rootfs.cpio


In the above example the Xen hypervisor is loaded by the -kernel parameter and passed its boot arguments via -append.
The Dom0 guest is loaded into the areas of memory. Each blob will get /chosen/module@<addr> entry in the FDT
to indicate its location and size. Additional information can be passed with by using additional arguments.
Currently the only supported machines which use FDT data to boot are the ARM and RiscV virt machines.


2.11.1 Arguments

The full syntax of the guest-loader is:

-device guest-loader,addr=<addr>[,kernel=<file>,[bootargs=<args>]][,initrd=<file>]

addr=<addr>
     This is mandatory and indicates the start address of the blob.
kernel|initrd=<file>
     Indicates the filename of the kernel or initrd blob. Both blobs will have the “multiboot,module” compatibility
     string as well as “multiboot,kernel” or “multiboot,ramdisk” as appropriate.
bootargs=<args>
     This is an optional field for kernel blobs which will pass command like via the /chosen/module@<addr>/
     bootargs node.




164                                                                                Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5



2.12 QEMU Barrier Client

Generally, mouse and keyboard are grabbed through the QEMU video interface emulation.
But when we want to use a video graphic adapter via a PCI passthrough there is no way to provide the keyboard and
mouse inputs to the VM except by plugging a second set of mouse and keyboard to the host or by installing a KVM
software in the guest OS.
The QEMU Barrier client avoids this by implementing directly the Barrier protocol into QEMU.
Barrier is a KVM (Keyboard-Video-Mouse) software forked from Symless’s synergy 1.9 codebase.
This protocol is enabled by adding an input-barrier object to QEMU.
Syntax:

input-barrier,id=<object-id>,name=<guest display name>
[,server=<barrier server address>][,port=<barrier server port>]
[,x-origin=<x-origin>][,y-origin=<y-origin>]
[,width=<width>][,height=<height>]

The object can be added on the QEMU command line, for instance with:

-object input-barrier,id=barrier0,name=VM-1

where VM-1 is the name the display configured in the Barrier server on the host providing the mouse and the keyboard
events.
by default <barrier server address> is localhost, <port> is 24800, <x-origin> and <y-origin> are set to
0, <width> and <height> to 1920 and 1080.
If the Barrier server is stopped QEMU needs to be reconnected manually, by removing and re-adding the input-barrier
object, for instance with the help of the HMP monitor:

(qemu) object_del barrier0
(qemu) object_add input-barrier,id=barrier0,name=VM-1



2.13 VNC security

The VNC server capability provides access to the graphical console of the guest VM across the network. This has a
number of security considerations depending on the deployment scenarios.


2.13.1 Without passwords

The simplest VNC server setup does not include any form of authentication. For this setup it is recommended to restrict
it to listen on a UNIX domain socket only. For example
qemu-system-x86_64 [...OPTIONS...] -vnc unix:/home/joebloggs/.qemu-myvm-vnc
This ensures that only users on local box with read/write access to that path can access the VNC server. To securely
access the VNC server from a remote machine, a combination of netcat+ssh can be used to provide a secure tunnel.




2.12. QEMU Barrier Client                                                                                         165
QEMU Documentation, Release 8.1.5


2.13.2 With passwords

The VNC protocol has limited support for password based authentication. Since the protocol limits passwords to 8
characters it should not be considered to provide high security. The password can be fairly easily brute-forced by a
client making repeat connections. For this reason, a VNC server using password authentication should be restricted to
only listen on the loopback interface or UNIX domain sockets. Password authentication is not supported when operating
in FIPS 140-2 compliance mode as it requires the use of the DES cipher. Password authentication is requested with the
password option, and then once QEMU is running the password is set with the monitor. Until the monitor is used to
set the password all clients will be rejected.
qemu-system-x86_64 [...OPTIONS...] -vnc :1,password=on -monitor stdio
(qemu) change vnc password
Password: ****
(qemu)


2.13.3 With x509 certificates

The QEMU VNC server also implements the VeNCrypt extension allowing use of TLS for encryption of the session,
and x509 certificates for authentication. The use of x509 certificates is strongly recommended, because TLS on its
own is susceptible to man-in-the-middle attacks. Basic x509 certificate support provides a secure session, but no
authentication. This allows any client to connect, and provides an encrypted session.
qemu-system-x86_64 [...OPTIONS...]              -object tls-creds-x509,id=tls0,dir=/etc/pki/qemu,
 ˓→endpoint=server,verify-peer=off             -vnc :1,tls-creds=tls0 -monitor stdio
In the above example /etc/pki/qemu should contain at least three files, ca-cert.pem, server-cert.pem and
server-key.pem. Unprivileged users will want to use a private directory, for example $HOME/.pki/qemu. NB
the server-key.pem file should be protected with file mode 0600 to only be readable by the user owning it.


2.13.4 With x509 certificates and client verification

Certificates can also provide a means to authenticate the client connecting. The server will request that the client
provide a certificate, which it will then validate against the CA certificate. This is a good choice if deploying in an
environment with a private internal certificate authority. It uses the same syntax as previously, but with verify-peer
set to on instead.
qemu-system-x86_64 [...OPTIONS...]   -object tls-creds-x509,id=tls0,dir=/etc/pki/qemu,
 ˓→endpoint=server,verify-peer=on  -vnc :1,tls-creds=tls0 -monitor stdio


2.13.5 With x509 certificates, client verification and passwords

Finally, the previous method can be combined with VNC password authentication to provide two layers of authentication
for clients.
qemu-system-x86_64 [...OPTIONS...]   -object tls-creds-x509,id=tls0,dir=/etc/pki/qemu,
 ˓→endpoint=server,verify-peer=on  -vnc :1,tls-creds=tls0,password=on -monitor stdio
(qemu) change vnc password
Password: ****
(qemu)




166                                                                                Chapter 2. System Emulation
                                                                                QEMU Documentation, Release 8.1.5


2.13.6 With SASL authentication

The SASL authentication method is a VNC extension, that provides an easily extendable, pluggable authentication
method. This allows for integration with a wide range of authentication mechanisms, such as PAM, GSSAPI/Kerberos,
LDAP, SQL databases, one-time keys and more. The strength of the authentication depends on the exact mechanism
configured. If the chosen mechanism also provides a SSF layer, then it will encrypt the datastream as well.
Refer to the later docs on how to choose the exact SASL mechanism used for authentication, but assuming use of one
supporting SSF, then QEMU can be launched with:
qemu-system-x86_64 [...OPTIONS...] -vnc :1,sasl=on -monitor stdio


2.13.7 With x509 certificates and SASL authentication

If the desired SASL authentication mechanism does not supported SSF layers, then it is strongly advised to run it in
combination with TLS and x509 certificates. This provides securely encrypted data stream, avoiding risk of compro-
mising of the security credentials. This can be enabled, by combining the ‘sasl’ option with the aforementioned TLS
+ x509 options:
qemu-system-x86_64 [...OPTIONS...]   -object tls-creds-x509,id=tls0,dir=/etc/pki/qemu,
 ˓→endpoint=server,verify-peer=on  -vnc :1,tls-creds=tls0,sasl=on -monitor stdio


2.13.8 Configuring SASL mechanisms

The following documentation assumes use of the Cyrus SASL implementation on a Linux host, but the principles
should apply to any other SASL implementation or host. When SASL is enabled, the mechanism configuration will be
loaded from system default SASL service config /etc/sasl2/qemu.conf. If running QEMU as an unprivileged user, an
environment variable SASL_CONF_PATH can be used to make it search alternate locations for the service config file.
If the TLS option is enabled for VNC, then it will provide session encryption, otherwise the SASL mechanism will have
to provide encryption. In the latter case the list of possible plugins that can be used is drastically reduced. In fact only the
GSSAPI SASL mechanism provides an acceptable level of security by modern standards. Previous versions of QEMU
referred to the DIGEST-MD5 mechanism, however, it has multiple serious flaws described in detail in RFC 6331 and
thus should never be used any more. The SCRAM-SHA-256 mechanism provides a simple username/password auth
facility similar to DIGEST-MD5, but does not support session encryption, so can only be used in combination with
TLS.
When not using TLS the recommended configuration is

mech_list: gssapi
keytab: /etc/qemu/krb5.tab

This says to use the ‘GSSAPI’ mechanism with the Kerberos v5 protocol, with the server principal stored in
/etc/qemu/krb5.tab. For this to work the administrator of your KDC must generate a Kerberos principal for the server,
with a name of ‘qemu/somehost.example.com@EXAMPLE.COM’ replacing ‘somehost.example.com’ with the fully
qualified host name of the machine running QEMU, and ‘EXAMPLE.COM’ with the Kerberos Realm.
When using TLS, if username+password authentication is desired, then a reasonable configuration is

mech_list: scram-sha-256
sasldb_path: /etc/qemu/passwd.db

The saslpasswd2 program can be used to populate the passwd.db file with accounts. Note that the passwd.db file
stores passwords in clear text.




2.13. VNC security                                                                                                         167
QEMU Documentation, Release 8.1.5


Other SASL configurations will be left as an exercise for the reader. Note that all mechanisms, except GSSAPI, should
be combined with use of TLS to ensure a secure data channel.


2.14 TLS setup for network services

Almost all network services in QEMU have the ability to use TLS for session data encryption, along with x509 certifi-
cates for simple client authentication. What follows is a description of how to generate certificates suitable for usage
with QEMU, and applies to the VNC server, character devices with the TCP backend, NBD server and client, and
migration server and client.
At a high level, QEMU requires certificates and private keys to be provided in PEM format. Aside from the core fields,
the certificates should include various extension data sets, including v3 basic constraints data, key purpose, key usage
and subject alt name.
The GnuTLS package includes a command called certtool which can be used to easily generate certificates and keys
in the required format with expected data present. Alternatively a certificate management service may be used.
At a minimum it is necessary to setup a certificate authority, and issue certificates to each server. If using x509 certifi-
cates for authentication, then each client will also need to be issued a certificate.
Assuming that the QEMU network services will only ever be exposed to clients on a private intranet, there is no need
to use a commercial certificate authority to create certificates. A self-signed CA is sufficient, and in fact likely to be
more secure since it removes the ability of malicious 3rd parties to trick the CA into mis-issuing certs for impersonating
your services. The only likely exception where a commercial CA might be desirable is if enabling the VNC websockets
server and exposing it directly to remote browser clients. In such a case it might be useful to use a commercial CA to
avoid needing to install custom CA certs in the web browsers.
The recommendation is for the server to keep its certificates in either /etc/pki/qemu or for unprivileged users in
$HOME/.pki/qemu.


2.14.1 Setup the Certificate Authority

This step only needs to be performed once per organization / organizational unit. First the CA needs a private key. This
key must be kept VERY secret and secure. If this key is compromised the entire trust chain of the certificates issued
with it is lost.

# certtool --generate-privkey > ca-key.pem

To generate a self-signed certificate requires one core piece of information, the name of the organization. A template
file ca.info should be populated with the desired data to avoid having to deal with interactive prompts from certtool:

# cat > ca.info <<EOF
cn = Name of your organization
ca
cert_signing_key
EOF
# certtool --generate-self-signed \
           --load-privkey ca-key.pem \
           --template ca.info \
           --outfile ca-cert.pem

The ca keyword in the template sets the v3 basic constraints extension to indicate this certificate is for a CA, while
cert_signing_key sets the key usage extension to indicate this will be used for signing other keys. The generated




168                                                                                   Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


ca-cert.pem file should be copied to all servers and clients wishing to utilize TLS support in the VNC server. The
ca-key.pem must not be disclosed/copied anywhere except the host responsible for issuing certificates.


2.14.2 Issuing server certificates

Each server (or host) needs to be issued with a key and certificate. When connecting the certificate is sent to the client
which validates it against the CA certificate. The core pieces of information for a server certificate are the hostnames
and/or IP addresses that will be used by clients when connecting. The hostname / IP address that the client specifies
when connecting will be validated against the hostname(s) and IP address(es) recorded in the server certificate, and if
no match is found the client will close the connection.
Thus it is recommended that the server certificate include both the fully qualified and unqualified hostnames. If the
server will have permanently assigned IP address(es), and clients are likely to use them when connecting, they may
also be included in the certificate. Both IPv4 and IPv6 addresses are supported. Historically certificates only included
1 hostname in the CN field, however, usage of this field for validation is now deprecated. Instead modern TLS clients
will validate against the Subject Alt Name extension data, which allows for multiple entries. In the future usage of the
CN field may be discontinued entirely, so providing SAN extension data is strongly recommended.
On the host holding the CA, create template files containing the information for each server, and use it to issue server
certificates.

# cat > server-hostNNN.info <<EOF
organization = Name of your organization
cn = hostNNN.foo.example.com
dns_name = hostNNN
dns_name = hostNNN.foo.example.com
ip_address = 10.0.1.87
ip_address = 192.8.0.92
ip_address = 2620:0:cafe::87
ip_address = 2001:24::92
tls_www_server
encryption_key
signing_key
EOF
# certtool --generate-privkey > server-hostNNN-key.pem
# certtool --generate-certificate \
           --load-ca-certificate ca-cert.pem \
           --load-ca-privkey ca-key.pem \
           --load-privkey server-hostNNN-key.pem \
           --template server-hostNNN.info \
           --outfile server-hostNNN-cert.pem

The dns_name and ip_address fields in the template are setting the subject alt name extension data. The
tls_www_server keyword is the key purpose extension to indicate this certificate is intended for usage in a web
server. Although QEMU network services are not in fact HTTP servers (except for VNC websockets), setting this
key purpose is still recommended. The encryption_key and signing_key keyword is the key usage extension to
indicate this certificate is intended for usage in the data session.
The server-hostNNN-key.pem and server-hostNNN-cert.pem files should now be securely copied to the server
for which they were generated, and renamed to server-key.pem and server-cert.pem when added to the /etc/
pki/qemu directory on the target host. The server-key.pem file is security sensitive and should be kept protected
with file mode 0600 to prevent disclosure.




2.14. TLS setup for network services                                                                                 169
QEMU Documentation, Release 8.1.5


2.14.3 Issuing client certificates

The QEMU x509 TLS credential setup defaults to enabling client verification using certificates, providing a simple
authentication mechanism. If this default is used, each client also needs to be issued a certificate. The client certificate
contains enough metadata to uniquely identify the client with the scope of the certificate authority. The client certificate
would typically include fields for organization, state, city, building, etc.
Once again on the host holding the CA, create template files containing the information for each client, and use it to
issue client certificates.

# cat > client-hostNNN.info <<EOF
country = GB
state = London
locality = City Of London
organization = Name of your organization
cn = hostNNN.foo.example.com
tls_www_client
encryption_key
signing_key
EOF
# certtool --generate-privkey > client-hostNNN-key.pem
# certtool --generate-certificate \
           --load-ca-certificate ca-cert.pem \
           --load-ca-privkey ca-key.pem \
           --load-privkey client-hostNNN-key.pem \
           --template client-hostNNN.info \
           --outfile client-hostNNN-cert.pem

The subject alt name extension data is not required for clients, so the dns_name and ip_address fields are not included.
The tls_www_client keyword is the key purpose extension to indicate this certificate is intended for usage in a web
client. Although QEMU network clients are not in fact HTTP clients, setting this key purpose is still recommended.
The encryption_key and signing_key keyword is the key usage extension to indicate this certificate is intended for
usage in the data session.
The client-hostNNN-key.pem and client-hostNNN-cert.pem files should now be securely copied to the client
for which they were generated, and renamed to client-key.pem and client-cert.pem when added to the /etc/
pki/qemu directory on the target host. The client-key.pem file is security sensitive and should be kept protected
with file mode 0600 to prevent disclosure.
If a single host is going to be using TLS in both a client and server role, it is possible to create a single certificate to
cover both roles. This would be quite common for the migration and NBD services, where a QEMU process will be
started by accepting a TLS protected incoming migration, and later itself be migrated out to another host. To generate
a single certificate, simply include the template data from both the client and server instructions in one.

# cat > both-hostNNN.info <<EOF
country = GB
state = London
locality = City Of London
organization = Name of your organization
cn = hostNNN.foo.example.com
dns_name = hostNNN
dns_name = hostNNN.foo.example.com
ip_address = 10.0.1.87
ip_address = 192.8.0.92
ip_address = 2620:0:cafe::87
                                                                                                       (continues on next page)



170                                                                                   Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


                                                                                               (continued from previous page)
ip_address = 2001:24::92
tls_www_server
tls_www_client
encryption_key
signing_key
EOF
# certtool --generate-privkey > both-hostNNN-key.pem
# certtool --generate-certificate \
           --load-ca-certificate ca-cert.pem \
           --load-ca-privkey ca-key.pem \
           --load-privkey both-hostNNN-key.pem \
           --template both-hostNNN.info \
           --outfile both-hostNNN-cert.pem

When copying the PEM files to the target host, save them twice, once as server-cert.pem and server-key.pem,
and again as client-cert.pem and client-key.pem.


2.14.4 TLS x509 credential configuration

QEMU has a standard mechanism for loading x509 credentials that will be used for network services and clients. It
requires specifying the tls-creds-x509 class name to the --object command line argument for the system emu-
lators. Each set of credentials loaded should be given a unique string identifier via the id parameter. A single set of
TLS credentials can be used for multiple network backends, so VNC, migration, NBD, character devices can all share
the same credentials. Note, however, that credentials for use in a client endpoint must be loaded separately from those
used in a server endpoint.
When specifying the object, the dir parameters specifies which directory contains the credential files. This
directory is expected to contain files with the names mentioned previously, ca-cert.pem, server-key.pem,
server-cert.pem, client-key.pem and client-cert.pem as appropriate. It is also possible to include a set
of pre-generated Diffie-Hellman (DH) parameters in a file dh-params.pem, which can be created using the certtool
--generate-dh-params command. If omitted, QEMU will dynamically generate DH parameters when loading the
credentials.
The endpoint parameter indicates whether the credentials will be used for a network client or server, and determines
which PEM files are loaded.
The verify parameter determines whether x509 certificate validation should be performed. This defaults to enabled,
meaning clients will always validate the server hostname against the certificate subject alt name fields and/or CN field.
It also means that servers will request that clients provide a certificate and validate them. Verification should never be
turned off for client endpoints, however, it may be turned off for server endpoints if an alternative mechanism is used
to authenticate clients. For example, the VNC server can use SASL to authenticate clients instead.
To load server credentials with client certificate validation enabled
qemu-system-x86_64 -object tls-creds-x509,id=tls0,dir=/etc/pki/qemu,endpoint=server
while to load client credentials use
qemu-system-x86_64 -object tls-creds-x509,id=tls0,dir=/etc/pki/qemu,endpoint=client
Network services which support TLS will all have a tls-creds parameter which expects the ID of the TLS credentials
object. For example with VNC:
qemu-system-x86_64 -vnc 0.0.0.0:0,tls-creds=tls0




2.14. TLS setup for network services                                                                                    171
QEMU Documentation, Release 8.1.5


2.14.5 TLS Pre-Shared Keys (PSK)

Instead of using certificates, you may also use TLS Pre-Shared Keys (TLS-PSK). This can be simpler to set up than
certificates but is less scalable.
Use the GnuTLS psktool program to generate a keys.psk file containing one or more usernames and random keys:

mkdir -m 0700 /tmp/keys
psktool -u rich -p /tmp/keys/keys.psk

TLS-enabled servers such as qemu-nbd can use this directory like so:

qemu-nbd \
  -t -x / \
  --object tls-creds-psk,id=tls0,endpoint=server,dir=/tmp/keys \
  --tls-creds tls0 \
  image.qcow2

When connecting from a qemu-based client you must specify the directory containing keys.psk and an optional
username (defaults to “qemu”):

qemu-img info \
  --object tls-creds-psk,id=tls0,dir=/tmp/keys,username=rich,endpoint=client \
  --image-opts \
  file.driver=nbd,file.host=localhost,file.port=10809,file.tls-creds=tls0,file.export=/



2.15 Providing secret data to QEMU

There are a variety of objects in QEMU which require secret data to be provided by the administrator or management
application. For example, network block devices often require a password, LUKS block devices require a passphrase
to unlock key material, remote desktop services require an access password. QEMU has a general purpose mechanism
for providing secret data to QEMU in a secure manner, using the secret object type.
At startup this can be done using the -object secret,... command line argument. At runtime this can be done
using the object_add QMP / HMP monitor commands. The examples that follow will illustrate use of -object
command lines, but they all apply equivalentely in QMP / HMP. When creating a secret object it must be given a
unique ID string. This ID is then used to identify the object when configuring the thing which need the data.


2.15.1 INSECURE: Passing secrets as clear text inline

The following should never be done in a production environment or on a multi-user host. Command line argu-
ments are usually visible in the process listings and are often collected in log files by system monitoring agents
or bug reporting tools. QMP/HMP commands and their arguments are also often logged and attached to bug
reports. This all risks compromising secrets that are passed inline.
For the convenience of people debugging / developing with QEMU, it is possible to pass secret data inline on the
command line.

-object secret,id=secvnc0,data=87539319

Again it is possible to provide the data in base64 encoded format, which is particularly useful if the data contains binary
characters that would clash with argument parsing.



172                                                                                  Chapter 2. System Emulation
                                                                              QEMU Documentation, Release 8.1.5



-object secret,id=secvnc0,data=ODc1MzkzMTk=,format=base64

Note: base64 encoding does not provide any security benefit.


2.15.2 Passing secrets as clear text via a file

The simplest approach to providing data securely is to use a file to store the secret:

-object secret,id=secvnc0,file=vnc-password.txt

In this example the file vnc-password.txt contains the plain text secret data. It is important to note that the contents
of the file are treated as an opaque blob. The entire raw file contents is used as the value, thus it is important not to
mistakenly add any trailing newline character in the file if this newline is not intended to be part of the secret data.
In some cases it might be more convenient to pass the secret data in base64 format and have QEMU decode to get the
raw bytes before use:

-object secret,id=sec0,file=vnc-password.txt,format=base64

The file should generally be given mode 0600 or 0400 permissions, and have its user/group ownership set to the same
account that the QEMU process will be launched under. If using mandatory access control such as SELinux, then the
file should be labelled to only grant access to the specific QEMU process that needs access. This will prevent other
processes/users from compromising the secret data.


2.15.3 Passing secrets as cipher text inline

To address the insecurity of passing secrets inline as clear text, it is possible to configure a second secret as an AES
key to use for decrypting the data.
The secret used as the AES key must always be configured using the file based storage mechanism:

-object secret,id=secmaster,file=masterkey.data,format=base64

In this case the masterkey.data file would be initialized with 32 cryptographically secure random bytes, which are
then base64 encoded. The contents of this file will by used as an AES-256 key to encrypt the real secret that can now
be safely passed to QEMU inline as cipher text

-object secret,id=secvnc0,keyid=secmaster,data=BASE64-CIPHERTEXT,iv=BASE64-IV,
 ˓→format=base64



In this example BASE64-CIPHERTEXT is the result of AES-256-CBC encrypting the secret with masterkey.data and
then base64 encoding the ciphertext. The BASE64-IV data is 16 random bytes which have been base64 encrypted.
These bytes are used as the initialization vector for the AES-256-CBC value.
A single master key can be used to encrypt all subsequent secrets, but it is critical that a different initialization vector
is used for every secret.




2.15. Providing secret data to QEMU                                                                                    173
QEMU Documentation, Release 8.1.5


2.15.4 Passing secrets via the Linux keyring

The earlier mechanisms described are platform agnostic. If using QEMU on a Linux host, it is further possible to pass
secrets to QEMU using the Linux keyring:

-object secret_keyring,id=secvnc0,serial=1729

This instructs QEMU to load data from the Linux keyring secret identified by the serial number 1729. It is possible to
combine use of the keyring with other features mentioned earlier such as base64 encoding:

-object secret_keyring,id=secvnc0,serial=1729,format=base64

and also encryption with a master key:

-object secret_keyring,id=secvnc0,keyid=secmaster,serial=1729,iv=BASE64-IV



2.15.5 Best practice

It is recommended for production deployments to use a master key secret, and then pass all subsequent inline secrets
encrypted with the master key.
Each QEMU instance must have a distinct master key, and that must be generated from a cryptographically secure
random data source. The master key should be deleted immediately upon QEMU shutdown. If passing the master key
as a file, the key file must have access control rules applied that restrict access to just the one QEMU process that is
intended to use it. Alternatively the Linux keyring can be used to pass the master key to QEMU.
The secrets for individual QEMU device backends must all then be encrypted with this master key.
This procedure helps ensure that the individual secrets for QEMU backends will not be compromised, even if -object
CLI args or object_add monitor commands are collected in log files and attached to public bug support tickets. The
only item that needs strongly protecting is the master key file.


2.16 Client authorization

When configuring a QEMU network backend with either TLS certificates or SASL authentication, access will be
granted if the client successfully proves their identity. If the authorization identity database is scoped to the QEMU
client this may be sufficient. It is common, however, for the identity database to be much broader and thus authentication
alone does not enable sufficient access control. In this case QEMU provides a flexible system for enforcing finer grained
authorization on clients post-authentication.


2.16.1 Identity providers

At the time of writing there are two authentication frameworks used by QEMU that emit an identity upon completion.
    • TLS x509 certificate distinguished name.
      When configuring the QEMU backend as a network server with TLS, there are a choice of credentials to use. The
      most common scenario is to utilize x509 certificates. The simplest configuration only involves issuing certificates
      to the servers, allowing the client to avoid a MITM attack against their intended server.
      It is possible, however, to enable mutual verification by requiring that the client provide a certificate to the server
      to prove its own identity. This is done by setting the property verify-peer=yes on the tls-creds-x509
      object, which is in fact the default.



174                                                                                    Chapter 2. System Emulation
                                                                               QEMU Documentation, Release 8.1.5


       When peer verification is enabled, client will need to be issued with a certificate by the same certificate authority
       as the server. If this is still not sufficiently strong access control the Distinguished Name of the certificate can be
       used as an identity in the QEMU authorization framework.
     • SASL username.
       When configuring the QEMU backend as a network server with SASL, upon completion of the SASL authenti-
       cation mechanism, a username will be provided. The format of this username will vary depending on the choice
       of mechanism configured for SASL. It might be a simple UNIX style user joebloggs, while if using Ker-
       beros/GSSAPI it can have a realm attached joebloggs@QEMU.ORG. Whatever format the username is presented
       in, it can be used with the QEMU authorization framework.


2.16.2 Authorization drivers

The QEMU authorization framework is a general purpose design with choice of user customizable drivers. These are
provided as objects that can be created at startup using the -object argument, or at runtime using the object_add
monitor command.


Simple

This authorization driver provides a simple mechanism for granting access based on an exact match against a single
identity. This is useful when it is known that only a single client is to be allowed access.
A possible use case would be when configuring QEMU for an incoming live migration. It is known exactly which
source QEMU the migration is expected to arrive from. The x509 certificate associated with this source QEMU would
thus be used as the identity to match against. Alternatively if the virtual machine is dedicated to a specific tenant, then
the VNC server would be configured with SASL and the username of only that tenant listed.
To create an instance of this driver via QMP:

{
    "execute": "object-add",
    "arguments": {
      "qom-type": "authz-simple",
      "id": "authz0",
      "identity": "fred"
    }
}

Or via the command line

-object authz-simple,id=authz0,identity=fred


List

In some network backends it will be desirable to grant access to a range of clients. This authorization driver provides
a list mechanism for granting access by matching identities against a list of permitted one. Each match rule has an
associated policy and a catch all policy applies if no rule matches. The match can either be done as an exact string
comparison, or can use the shell-like glob syntax, which allows for use of wildcards.
To create an instance of this class via QMP:




2.16. Client authorization                                                                                               175
QEMU Documentation, Release 8.1.5



{
    "execute": "object-add",
    "arguments": {
      "qom-type": "authz-list",
      "id": "authz0",
      "rules": [
         { "match": "fred", "policy": "allow", "format": "exact" },
         { "match": "bob", "policy": "allow", "format": "exact" },
         { "match": "danb", "policy": "deny", "format": "exact" },
         { "match": "dan*", "policy": "allow", "format": "glob" }
      ],
      "policy": "deny"
    }
}

Due to the way this driver requires setting nested properties, creating it on the command line will require use of the
JSON syntax for -object. In most cases, however, the next driver will be more suitable.


List file

This is a variant on the previous driver that allows for a more dynamic access control policy by storing the match rules
in a standalone file that can be reloaded automatically upon change.
To create an instance of this class via QMP:

{
    "execute": "object-add",
    "arguments": {
      "qom-type": "authz-list-file",
      "id": "authz0",
      "filename": "/etc/qemu/myvm-vnc.acl",
      "refresh": true
    }
}

If refresh is yes, inotify is used to monitor for changes to the file and auto-reload the rules.
The myvm-vnc.acl file should contain the match rules in a format that closely matches the previous driver:

{
    "rules": [
      { "match": "fred", "policy": "allow", "format": "exact" },
      { "match": "bob", "policy": "allow", "format": "exact" },
      { "match": "danb", "policy": "deny", "format": "exact" },
      { "match": "dan*", "policy": "allow", "format": "glob" }
    ],
    "policy": "deny"
}

The object can be created on the command line using

-object authz-list-file,id=authz0,\
        filename=/etc/qemu/myvm-vnc.acl,refresh=on




176                                                                                 Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


PAM

In some scenarios it might be desirable to integrate with authorization mechanisms that are implemented outside of
QEMU. In order to allow maximum flexibility, QEMU provides a driver that uses the PAM framework.
To create an instance of this class via QMP:

{
    "execute": "object-add",
    "arguments": {
      "qom-type": "authz-pam",
      "id": "authz0",
      "parameters": {
        "service": "qemu-vnc-tls"
      }
    }
}

The driver only uses the PAM “account” verification subsystem. The above config would require a config file
/etc/pam.d/qemu-vnc-tls. For a simple file lookup it would contain

account requisite pam_listfile.so item=user sense=allow \
        file=/etc/qemu/vnc.allow

The external file would then contain a list of usernames. If x509 cert was being used as the username, a suitable entry
would match the distinguished name:

CN=laptop.berrange.com,O=Berrange Home,L=London,ST=London,C=GB

On the command line it can be created using

-object authz-pam,id=authz0,service=qemu-vnc-tls

There are a variety of PAM plugins that can be used which are not illustrated here, and it is possible to implement brand
new plugins using the PAM API.


2.16.3 Connecting backends

The authorization driver is created using the -object argument and then needs to be associated with a network service.
The authorization driver object will be given a unique ID that needs to be referenced.
The property to set in the network service will vary depending on the type of identity to verify. By convention, any
network server backend that uses TLS will provide tls-authz property, while any server using SASL will provide a
sasl-authz property.
Thus an example using SASL and authorization for the VNC server would look like:

$QEMU --object authz-simple,id=authz0,identity=fred \
      --vnc 0.0.0.0:1,sasl,sasl-authz=authz0

While to validate both the x509 certificate and SASL username:

echo "CN=laptop.qemu.org,O=QEMU Project,L=London,ST=London,C=GB" >> tls.acl
$QEMU --object authz-simple,id=authz0,identity=fred \
      --object authz-list-file,id=authz1,filename=tls.acl \
                                                                                                    (continues on next page)


2.16. Client authorization                                                                                            177
QEMU Documentation, Release 8.1.5


                                                                                            (continued from previous page)
       --object tls-creds-x509,id=tls0,dir=/etc/qemu/tls,verify-peer=yes \
       --vnc 0.0.0.0:1,sasl,sasl-authz=auth0,tls-creds=tls0,tls-authz=authz1



2.17 GDB usage

QEMU supports working with gdb via gdb’s remote-connection facility (the “gdbstub”). This allows you to debug
guest code in the same way that you might with a low-level debug facility like JTAG on real hardware. You can stop
and start the virtual machine, examine state like registers and memory, and set breakpoints and watchpoints.
In order to use gdb, launch QEMU with the -s and -S options. The -s option will make QEMU listen for an incoming
connection from gdb on TCP port 1234, and -S will make QEMU not start the guest until you tell it to from gdb. (If
you want to specify which TCP port to use or to use something other than TCP for the gdbstub connection, use the
-gdb dev option instead of -s. See Using unix sockets for an example.)
qemu-system-x86_64 -s -S -kernel bzImage -hda rootdisk.img -append "root=/dev/hda"
QEMU will launch but will silently wait for gdb to connect.
Then launch gdb on the ‘vmlinux’ executable:

> gdb vmlinux

In gdb, connect to QEMU:

(gdb) target remote localhost:1234

Then you can use gdb normally. For example, type ‘c’ to launch the kernel:

(gdb) c

Here are some useful tips in order to use gdb on system code:
   1. Use info reg to display all the CPU registers.
   2. Use x/10i $eip to display the code at the PC position.
   3. Use set architecture i8086 to dump 16 bit code. Then use x/10i $cs*16+$eip to dump the code at the
      PC position.


2.17.1 Breakpoint and Watchpoint support

While GDB can always fall back to inserting breakpoints into memory (if writable) other features are very much
dependent on support of the accelerator. For TCG system emulation we advertise an infinite number of hardware
assisted breakpoints and watchpoints. For other accelerators it will depend on if support has been added (see sup-
ports_guest_debug and related hooks in AccelOpsClass).
As TCG cannot track all memory accesses in user-mode there is no support for watchpoints.




178                                                                            Chapter 2. System Emulation
                                                                             QEMU Documentation, Release 8.1.5



2.18 Relocating code

On modern kernels confusion can be caused by code being relocated by features such as address space layout randomi-
sation. To avoid confusion when debugging such things you either need to update gdb’s view of where things are in
memory or perhaps more trivially disable ASLR when booting the system.


2.18.1 Debugging multicore machines

GDB’s abstraction for debugging targets with multiple possible parallel flows of execution is a two layer one: it supports
multiple “inferiors”, each of which can have multiple “threads”. When the QEMU machine has more than one CPU,
QEMU exposes each CPU cluster as a separate “inferior”, where each CPU within the cluster is a separate “thread”.
Most QEMU machine types have identical CPUs, so there is a single cluster which has all the CPUs in it. A few
machine types are heterogeneous and have multiple clusters: for example the sifive_u machine has a cluster with one
E51 core and a second cluster with four U54 cores. Here the E51 is the only thread in the first inferior, and the U54
cores are all threads in the second inferior.
When you connect gdb to the gdbstub, it will automatically connect to the first inferior; you can display the CPUs in
this cluster using the gdb info thread command, and switch between them using gdb’s usual thread-management
commands.
For multi-cluster machines, unfortunately gdb does not by default handle multiple inferiors, and so you have to explicitly
connect to them. First, you must connect with the extended-remote protocol, not remote:

(gdb) target extended-remote localhost:1234

Once connected, gdb will have a single inferior, for the first cluster. You need to create inferiors for the other clusters
and attach to them, like this:

(gdb) add-inferior
Added inferior 2
(gdb) inferior 2
[Switching to inferior 2 [<null>] (<noexec>)]
(gdb) attach 2
Attaching to process 2
warning: No executable has been specified and target does not support
determining executable automatically. Try using the "file" command.
0x00000000 in ?? ()

Once you’ve done this, info threads will show CPUs in all the clusters you have attached to:

(gdb) info threads
  Id   Target Id        Frame
  1.1 Thread 1.1 (cortex-m33-arm-cpu cpu [running]) 0x00000000 in ?? ()
* 2.1 Thread 2.2 (cortex-m33-arm-cpu cpu [halted ]) 0x00000000 in ?? ()

You probably also want to set gdb to schedule-multiple mode, so that when you tell gdb to continue it resumes
all CPUs, not just those in the cluster you are currently working on:

(gdb) set schedule-multiple on




2.18. Relocating code                                                                                                 179
QEMU Documentation, Release 8.1.5


2.18.2 Using unix sockets

An alternate method for connecting gdb to the QEMU gdbstub is to use a unix socket (if supported by your operating
system). This is useful when running several tests in parallel, or if you do not have a known free TCP port (e.g. when
running automated tests).
First create a chardev with the appropriate options, then instruct the gdbserver to use that device:
qemu-system-x86_64 -chardev socket,path=/tmp/gdb-socket,server=on,wait=off,id=gdb0 -gdb␣
 ˓→chardev:gdb0 -S ...


Start gdb as before, but this time connect using the path to the socket:

(gdb) target remote /tmp/gdb-socket

Note that to use a unix socket for the connection you will need gdb version 9.0 or newer.


2.18.3 Advanced debugging options

Changing single-stepping behaviour

The default single stepping behavior is step with the IRQs and timer service routines off. It is set this way because
when gdb executes a single step it expects to advance beyond the current instruction. With the IRQs and timer service
routines on, a single step might jump into the one of the interrupt or exception vectors instead of executing the current
instruction. This means you may hit the same breakpoint a number of times before executing the instruction gdb wants
to have executed. Because there are rare circumstances where you want to single step into an interrupt vector the
behavior can be controlled from GDB. There are three commands you can query and set the single step behavior:
maintenance packet qqemu.sstepbits
     This will display the MASK bits used to control the single stepping IE:

      (gdb) maintenance packet qqemu.sstepbits
      sending: "qqemu.sstepbits"
      received: "ENABLE=1,NOIRQ=2,NOTIMER=4"

maintenance packet qqemu.sstep
     This will display the current value of the mask used when single stepping IE:

      (gdb) maintenance packet qqemu.sstep
      sending: "qqemu.sstep"
      received: "0x7"

maintenance packet Qqemu.sstep=HEX_VALUE
     This will change the single step mask, so if wanted to enable IRQs on the single step, but not timers, you would
     use:

      (gdb) maintenance packet Qqemu.sstep=0x5
      sending: "qemu.sstep=0x5"
      received: "OK"




180                                                                                 Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


Examining physical memory

Another feature that QEMU gdbstub provides is to toggle the memory GDB works with, by default GDB will show the
current process memory respecting the virtual address translation.
If you want to examine/change the physical memory you can set the gdbstub to work with the physical memory rather
with the virtual one.
The memory mode can be checked by sending the following command:
maintenance packet qqemu.PhyMemMode
     This will return either 0 or 1, 1 indicates you are currently in the physical memory mode.
maintenance packet Qqemu.PhyMemMode:1
     This will change the memory mode to physical memory.
maintenance packet Qqemu.PhyMemMode:0
     This will change it back to normal memory mode.


2.18.4 Security considerations

Connecting to the GDB socket allows running arbitrary code inside the guest; in case of the TCG emulation, which is
not considered a security boundary, this also means running arbitrary code on the host. Additionally, when debugging
qemu-user, it allows directly downloading any file readable by QEMU from the host.
The GDB socket is not protected by authentication, authorization or encryption. It is therefore a responsibility of the
user to make sure that only authorized clients can connect to it, e.g., by using a unix socket with proper permissions,
or by opening a TCP socket only on interfaces that are not reachable by potential attackers.


2.19 Record/replay

Record/replay functions are used for the deterministic replay of qemu execution. Execution recording writes a non-
deterministic events log, which can be later used for replaying the execution anywhere and for unlimited number of
times. It also supports checkpointing for faster rewind to the specific replay moment. Execution replaying reads the
log and replays all non-deterministic events including external input, hardware clocks, and interrupts.
Deterministic replay has the following features:
    • Deterministically replays whole system execution and all contents of the memory, state of the hardware devices,
      clocks, and screen of the VM.
    • Writes execution log into the file for later replaying for multiple times on different machines.
    • Supports i386, x86_64, ARM, AArch64, Risc-V, MIPS, MIPS64, S390X, Alpha, PowerPC, PowerPC64,
      M68000, Microblaze, OpenRISC, Nios II, SPARC, and Xtensa hardware platforms.
    • Performs deterministic replay of all operations with keyboard and mouse input devices, serial ports, and network.
Usage of the record/replay:
    • First, record the execution with the following command line:
           qemu-system-x86_64 \
           -icount shift=auto,rr=record,rrfile=replay.bin \
           -drive file=disk.qcow2,if=none,snapshot,id=img-direct \
           -drive driver=blkreplay,if=none,image=img-direct,id=img-blkreplay \
           -device ide-hd,drive=img-blkreplay \
           -netdev user,id=net1 -device rtl8139,netdev=net1 \


2.19. Record/replay                                                                                               181
QEMU Documentation, Release 8.1.5


           -object filter-replay,id=replay,netdev=net1
    • After recording, you can replay it by using another command line:
           qemu-system-x86_64 \
           -icount shift=auto,rr=replay,rrfile=replay.bin \
           -drive file=disk.qcow2,if=none,snapshot,id=img-direct \
           -drive driver=blkreplay,if=none,image=img-direct,id=img-blkreplay \
           -device ide-hd,drive=img-blkreplay \
           -netdev user,id=net1 -device rtl8139,netdev=net1 \
           -object filter-replay,id=replay,netdev=net1
      The only difference with recording is changing the rr option from record to replay.
    • Block device images are not actually changed in the recording mode, because all of the changes are written to the
      temporary overlay file. This behavior is enabled by using blkreplay driver. It should be used for every enabled
      block device, as described in Block devices section.
    • -net none option should be specified when network is not used, because QEMU adds network card by default.
      When network is needed, it should be configured explicitly with replay filter, as described in Network devices
      section.
    • Interaction with audio devices and serial ports are recorded and replayed automatically when such devices are
      enabled.


2.19.1 Core idea

Record/replay system is based on saving and replaying non-deterministic events (e.g. keyboard input) and simulating
deterministic ones (e.g. reading from HDD or memory of the VM). Saving only non-deterministic events makes log
file smaller and simulation faster.
The following non-deterministic data from peripheral devices is saved into the log: mouse and keyboard input, network
packets, audio controller input, serial port input, and hardware clocks (they are non-deterministic too, because their
values are taken from the host machine). Inputs from simulated hardware, memory of VM, software interrupts, and
execution of instructions are not saved into the log, because they are deterministic and can be replayed by simulating
the behavior of virtual machine starting from initial state.


2.19.2 Instruction counting

QEMU should work in icount mode to use record/replay feature. icount was designed to allow deterministic execution
in absence of external inputs of the virtual machine. Record/replay feature is enabled through -icount command-line
option, making possible deterministic execution of the machine, interacting with user or network.


2.19.3 Block devices

Block devices record/replay module intercepts calls of bdrv coroutine functions at the top of block drivers stack. To
record and replay block operations the drive must be configured as following:

-drive file=disk.qcow2,if=none,snapshot,id=img-direct
-drive driver=blkreplay,if=none,image=img-direct,id=img-blkreplay
-device ide-hd,drive=img-blkreplay

blkreplay driver should be inserted between disk image and virtual driver controller. Therefore all disk requests may
be recorded and replayed.



182                                                                                Chapter 2. System Emulation
                                                                             QEMU Documentation, Release 8.1.5


2.19.4 Snapshotting

New VM snapshots may be created in replay mode. They can be used later to recover the desired VM state. All VM
states created in replay mode are associated with the moment of time in the replay scenario. After recovering the VM
state replay will start from that position.
Default starting snapshot name may be specified with icount field rrsnapshot as follows:

-icount shift=auto,rr=record,rrfile=replay.bin,rrsnapshot=snapshot_name

This snapshot is created at start of recording and restored at start of replaying. It also can be loaded while replaying to
roll back the execution.
snapshot flag of the disk image must be removed to save the snapshots in the overlay (or original image) instead of
using the temporary overlay.

-drive file=disk.ovl,if=none,id=img-direct
-drive driver=blkreplay,if=none,image=img-direct,id=img-blkreplay
-device ide-hd,drive=img-blkreplay

Use QEMU monitor to create additional snapshots. savevm <name> command created the snapshot and loadvm
<name> restores it. To prevent corruption of the original disk image, use overlay files linked to the original images.
Therefore all new snapshots (including the starting one) will be saved in overlays and the original image remains
unchanged.
When you need to use snapshots with diskless virtual machine, it must be started with “orphan” qcow2 image. This
image will be used for storing VM snapshots. Here is the example of the command line for this:
qemu-system-x86_64 \
  -icount shift=auto,rr=replay,rrfile=record.bin,rrsnapshot=init \
  -net none -drive file=empty.qcow2,if=none,id=rr
empty.qcow2 drive does not connected to any virtual block device and used for VM snapshots only.


2.19.5 Network devices

Record and replay for network interactions is performed with the network filter. Each backend must have its own
instance of the replay filter as follows:

-netdev user,id=net1 -device rtl8139,netdev=net1
-object filter-replay,id=replay,netdev=net1

Replay network filter is used to record and replay network packets. While recording the virtual machine this filter puts
all packets coming from the outer world into the log. In replay mode packets from the log are injected into the network
device. All interactions with network backend in replay mode are disabled.




2.19. Record/replay                                                                                                   183
QEMU Documentation, Release 8.1.5


2.19.6 Audio devices

Audio data is recorded and replay automatically. The command line for recording and replaying must contain identical
specifications of audio hardware, e.g.:

-soundhw ac97



2.19.7 Serial ports

Serial ports input is recorded and replay automatically. The command lines for recording and replaying must contain
identical number of ports in record and replay modes, but their backends may differ. E.g., -serial stdio in record
mode, and -serial null in replay mode.


2.19.8 Reverse debugging

Reverse debugging allows “executing” the program in reverse direction. GDB remote protocol supports “reverse step”
and “reverse continue” commands. The first one steps single instruction backwards in time, and the second one finds
the last breakpoint in the past.
Recorded executions may be used to enable reverse debugging. QEMU can’t execute the code in backwards direction,
but can load a snapshot and replay forward to find the desired position or breakpoint.
The following GDB commands are supported:
    • reverse-stepi (or rsi) - step one instruction backwards
    • reverse-continue (or rc) - find last breakpoint in the past
Reverse step loads the nearest snapshot and replays the execution until the required instruction is met.
Reverse continue may include several passes of examining the execution between the snapshots. Each of the passes
include the following steps:
   1. loading the snapshot
   2. replaying to examine the breakpoints
   3. if breakpoint or watchpoint was met
         • loading the snapshot again
         • replaying to the required breakpoint
   4. else
         • proceeding to the p.1 with the earlier snapshot
Therefore usage of the reverse debugging requires at least one snapshot created. This can be done by omitting snapshot
option for the block drives and adding rrsnapshot for both record and replay command lines. See the Snapshotting
section to learn more about running record/replay and creating the snapshot in these modes.
When rrsnapshot is not used, then snapshot named start_debugging created in temporary overlay. This allows
using reverse debugging, but with temporary snapshots (existing within the session).




184                                                                                 Chapter 2. System Emulation
                                                                             QEMU Documentation, Release 8.1.5



2.20 Managed start up options

In system mode emulation, it’s possible to create a VM in a paused state using the -S command line option. In this
state the machine is completely initialized according to command line options and ready to execute VM code but VCPU
threads are not executing any code. The VM state in this paused state depends on the way QEMU was started. It could
be in:
    • initial state (after reset/power on state)
    • with direct kernel loading, the initial state could be amended to execute code loaded by QEMU in the VM’s RAM
      and with incoming migration
    • with incoming migration, initial state will be amended with the migrated machine state after migration completes
This paused state is typically used by users to query machine state and/or additionally configure the machine (by
hotplugging devices) in runtime before allowing VM code to run.
However, at the -S pause point, it’s impossible to configure options that affect initial VM creation (like: -smp/-m/-numa
. . . ) or cold plug devices. The experimental --preconfig command line option allows pausing QEMU before the
initial VM creation, in a “preconfig” state, where additional queries and configuration can be performed via QMP
before moving on to the resulting configuration startup. In the preconfig state, QEMU only allows a limited set of
commands over the QMP monitor, where the commands do not depend on an initialized machine, including but not
limited to:
    • qmp_capabilities
    • query-qmp-schema
    • query-commands
    • query-status
    • x-exit-preconfig


2.21 Managing device boot order with bootindex properties

QEMU can tell QEMU-aware guest firmware (like the x86 PC BIOS) which order it should look for a bootable OS
on which devices. A simple way to set this order is to use the -boot order= option, but you can also do this more
flexibly, by setting a bootindex property on the individual block or net devices you specify on the QEMU command
line.
The bootindex properties are used to determine the order in which firmware will consider devices for booting the
guest OS. If the bootindex property is not set for a device, it gets the lowest boot priority. There is no particular order
in which devices with no bootindex property set will be considered for booting, but they will still be bootable.
Some guest machine types (for instance the s390x machines) do not support -boot order=; on those machines you
must always use bootindex properties.
There is no way to set a bootindex property if you are using a short-form option like -hda or -cdrom, so to use
bootindex properties you will need to expand out those options into long-form -drive and -device option pairs.




2.20. Managed start up options                                                                                        185
QEMU Documentation, Release 8.1.5


2.21.1 Example

Let’s assume we have a QEMU machine with two NICs (virtio, e1000) and two disks (IDE, virtio):
qemu-system-x86_64 -drive file=disk1.img,if=none,id=disk1 \
              -device ide-hd,drive=disk1,bootindex=4 \
              -drive file=disk2.img,if=none,id=disk2 \
              -device virtio-blk-pci,drive=disk2,bootindex=3 \
              -netdev type=user,id=net0 \
              -device virtio-net-pci,netdev=net0,bootindex=2 \
              -netdev type=user,id=net1 \
              -device e1000,netdev=net1,bootindex=1
Given the command above, firmware should try to boot from the e1000 NIC first. If this fails, it should try the virtio
NIC next; if this fails too, it should try the virtio disk, and then the IDE disk.


2.21.2 Limitations

Some firmware has limitations on which devices can be considered for booting. For instance, the PC BIOS boot
specification allows only one disk to be bootable. If boot from disk fails for some reason, the BIOS won’t retry booting
from other disk. It can still try to boot from floppy or net, though.
Sometimes, firmware cannot map the device path QEMU wants firmware to boot from to a boot method. It doesn’t
happen for devices the firmware can natively boot from, but if firmware relies on an option ROM for booting, and the
same option ROM is used for booting from more then one device, the firmware may not be able to ask the option ROM
to boot from a particular device reliably. For instance with the PC BIOS, if a SCSI HBA has three bootable devices
target1, target3, target5 connected to it, the option ROM will have a boot method for each of them, but it is not possible
to map from boot method back to a specific target. This is a shortcoming of the PC BIOS boot specification.


2.21.3 Mixing bootindex and boot order parameters

Note that it does not make sense to use the bootindex property together with the -boot order=... (or -boot once=.
..) parameter. The guest firmware implementations normally either support the one or the other, but not both param-
eters at the same time. Mixing them will result in undefined behavior, and thus the guest firmware will likely not boot
from the expected devices.


2.22 Virtual CPU hotplug

A complete example of vCPU hotplug (and hot-unplug) using QMP device_add and device_del.


2.22.1 vCPU hotplug

  (1) Launch QEMU as follows (note that the “maxcpus” is mandatory to allow vCPU hotplug):

      $ qemu-system-x86_64 -display none -no-user-config -m 2048 \
          -nodefaults -monitor stdio -machine pc,accel=kvm,usb=off \
          -smp 1,maxcpus=2 -cpu IvyBridge-IBRS \
          -qmp unix:/tmp/qmp-sock,server=on,wait=off

  (2) Run ‘qmp-shell’ (located in the source tree, under: “scripts/qmp/) to connect to the just-launched QEMU:




186                                                                                  Chapter 2. System Emulation
                                                                     QEMU Documentation, Release 8.1.5



     $> ./qmp-shell -p -v /tmp/qmp-sock
     [...]
     (QEMU)

 (3) Find out which CPU types could be plugged, and into which sockets:

     (QEMU) query-hotpluggable-cpus
     {
         "execute": "query-hotpluggable-cpus",
         "arguments": {}
     }
     {
         "return": [
             {
                 "type": "IvyBridge-IBRS-x86_64-cpu",
                 "vcpus-count": 1,
                 "props": {
                     "socket-id": 1,
                     "core-id": 0,
                     "thread-id": 0
                 }
             },
             {
                 "qom-path": "/machine/unattached/device[0]",
                 "type": "IvyBridge-IBRS-x86_64-cpu",
                 "vcpus-count": 1,
                 "props": {
                     "socket-id": 0,
                     "core-id": 0,
                     "thread-id": 0
                 }
             }
         ]
     }
     (QEMU)

 (4) The query-hotpluggable-cpus command returns an object for CPUs that are present (containing a “qom-
     path” member) or which may be hot-plugged (no “qom-path” member). From its output in step (3), we can see
     that IvyBridge-IBRS-x86_64-cpu is present in socket 0, while hot-plugging a CPU into socket 1 requires
     passing the listed properties to QMP device_add:

     (QEMU) device_add id=cpu-2 driver=IvyBridge-IBRS-x86_64-cpu socket-id=1 core-id=0␣
      ˓→thread-id=0

     {
          "execute": "device_add",
          "arguments": {
              "socket-id": 1,
              "driver": "IvyBridge-IBRS-x86_64-cpu",
              "id": "cpu-2",
              "core-id": 0,
              "thread-id": 0
          }
     }
                                                                                           (continues on next page)



2.22. Virtual CPU hotplug                                                                                    187
QEMU Documentation, Release 8.1.5


                                                                                (continued from previous page)
      {
          "return": {}
      }
      (QEMU)

 (5) Optionally, run QMP query-cpus-fast for some details about the vCPUs:

      (QEMU) query-cpus-fast
      {
          "execute": "query-cpus-fast",
          "arguments": {}
      }
      {
          "return": [
              {
                  "qom-path": "/machine/unattached/device[0]",
                  "target": "x86_64",
                  "thread-id": 11534,
                  "cpu-index": 0,
                  "props": {
                      "socket-id": 0,
                      "core-id": 0,
                      "thread-id": 0
                  },
                  "arch": "x86"
              },
              {
                  "qom-path": "/machine/peripheral/cpu-2",
                  "target": "x86_64",
                  "thread-id": 12106,
                  "cpu-index": 1,
                  "props": {
                      "socket-id": 1,
                      "core-id": 0,
                      "thread-id": 0
                  },
                  "arch": "x86"
              }
          ]
      }
      (QEMU)




188                                                                      Chapter 2. System Emulation
                                                                             QEMU Documentation, Release 8.1.5


2.22.2 vCPU hot-unplug

From the ‘qmp-shell’, invoke the QMP device_del command:

(QEMU) device_del id=cpu-2
{
    "execute": "device_del",
    "arguments": {
        "id": "cpu-2"
    }
}
{
    "return": {}
}
(QEMU)


Note: vCPU hot-unplug requires guest cooperation; so the device_del command above does not guarantee vCPU
removal – it’s a “request to unplug”. At this point, the guest will get a System Control Interrupt (SCI) and calls the ACPI
handler for the affected vCPU device. Then the guest kernel will bring the vCPU offline and tell QEMU to unplug it.




2.23 Persistent reservation managers

SCSI persistent reservations allow restricting access to block devices to specific initiators in a shared storage setup.
When implementing clustering of virtual machines, it is a common requirement for virtual machines to send persis-
tent reservation SCSI commands. However, the operating system restricts sending these commands to unprivileged
programs because incorrect usage can disrupt regular operation of the storage fabric.
For this reason, QEMU’s SCSI passthrough devices, scsi-block and scsi-generic (both are only available on
Linux) can delegate implementation of persistent reservations to a separate object, the “persistent reservation man-
ager”. Only PERSISTENT RESERVE OUT and PERSISTENT RESERVE IN commands are passed to the persistent
reservation manager object; other commands are processed by QEMU as usual.


2.23.1 Defining a persistent reservation manager

A persistent reservation manager is an instance of a subclass of the “pr-manager” QOM class.
Right now only one subclass is defined, pr-manager-helper, which forwards the commands to an external privileged
helper program over Unix sockets. The helper program only allows sending persistent reservation commands to devices
for which QEMU has a file descriptor, so that QEMU will not be able to effect persistent reservations unless it has access
to both the socket and the device.
pr-manager-helper has a single string property, path, which accepts the path to the helper program’s Unix socket.
For example, the following command line defines a pr-manager-helper object and attaches it to a SCSI passthrough
device:

$ qemu-system-x86_64
    -device virtio-scsi \
    -object pr-manager-helper,id=helper0,path=/var/run/qemu-pr-helper.sock
    -drive if=none,id=hd,driver=raw,file.filename=/dev/sdb,file.pr-manager=helper0
    -device scsi-block,drive=hd




2.23. Persistent reservation managers                                                                                 189
QEMU Documentation, Release 8.1.5


Alternatively, using -blockdev:

$ qemu-system-x86_64
     -device virtio-scsi \
     -object pr-manager-helper,id=helper0,path=/var/run/qemu-pr-helper.sock
     -blockdev node-name=hd,driver=raw,file.driver=host_device,file.filename=/dev/sdb,
 ˓→file.pr-manager=helper0

     -device scsi-block,drive=hd

You will also need to ensure that the helper program qemu-pr-helper is running, and that it has been set up to use
the same socket filename as your QEMU commandline specifies. See the qemu-pr-helper documentation or manpage
for further details.


2.23.2 Multipath devices and persistent reservations

Proper support of persistent reservation for multipath devices requires communication with the multipath dae-
mon, so that the reservation is registered and applied when a path is newly discovered or becomes online again.
qemu-pr-helper can do this if the libmpathpersist library was available on the system at build time.
As of August 2017, a reservation key must be specified in multipath.conf for multipathd to check for persistent
reservation for newly discovered paths or reinstated paths. The attribute can be added to the defaults section or the
multipaths section; for example:

multipaths {
    multipath {
        wwid    XXXXXXXXXXXXXXXX
        alias       yellow
        reservation_key 0x123abc
    }
}

Linking qemu-pr-helper to libmpathpersist does not impede its usage on regular SCSI devices.


2.24 QEMU System Emulator Targets

QEMU is a generic emulator and it emulates many machines. Most of the options are similar for all machines. Specific
information about the various targets are mentioned in the following sections.
Contents:


2.24.1 Arm System emulator

QEMU can emulate both 32-bit and 64-bit Arm CPUs. Use the qemu-system-aarch64 executable to simulate
a 64-bit Arm machine. You can use either qemu-system-arm or qemu-system-aarch64 to simulate a 32-bit
Arm machine: in general, command lines that work for qemu-system-arm will behave the same when used with
qemu-system-aarch64.
QEMU has generally good support for Arm guests. It has support for nearly fifty different machines. The reason we
support so many is that Arm hardware is much more widely varying than x86 hardware. Arm CPUs are generally built
into “system-on-chip” (SoC) designs created by many different companies with different devices, and these SoCs are
then built into machines which can vary still further even if they use the same SoC. Even with fifty boards QEMU does
not cover more than a small fraction of the Arm hardware ecosystem.


190                                                                              Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


The situation for 64-bit Arm is fairly similar, except that we don’t implement so many different machines.
As well as the more common “A-profile” CPUs (which have MMUs and will run Linux) QEMU also supports “M-
profile” CPUs such as the Cortex-M0, Cortex-M4 and Cortex-M33 (which are microcontrollers used in very embedded
boards). For most boards the CPU type is fixed (matching what the hardware has), so typically you don’t need to specify
the CPU type by hand, except for special cases like the virt board.


Choosing a board model

For QEMU’s Arm system emulation, you must specify which board model you want to use with the -M or --machine
option; there is no default.
Because Arm systems differ so much and in fundamental ways, typically operating system or firmware images intended
to run on one machine will not run at all on any other. This is often surprising for new users who are used to the x86
world where every system looks like a standard PC. (Once the kernel has booted, most userspace software cares much
less about the detail of the hardware.)
If you already have a system image or a kernel that works on hardware and you want to boot with QEMU, check whether
QEMU lists that machine in its -machine help output. If it is listed, then you can probably use that board model. If
it is not listed, then unfortunately your image will almost certainly not boot on QEMU. (You might be able to extract
the filesystem and use that with a different kernel which boots on a system that QEMU does emulate.)
If you don’t care about reproducing the idiosyncrasies of a particular bit of hardware, such as small amount of RAM,
no PCI or other hard disk, etc., and just want to run Linux, the best option is to use the virt board. This is a platform
which doesn’t correspond to any real hardware and is designed for use in virtual machines. You’ll need to compile
Linux with a suitable configuration for running on the virt board. virt supports PCI, virtio, recent CPUs and large
amounts of RAM. It also supports 64-bit CPUs.


Board-specific documentation

Unfortunately many of the Arm boards QEMU supports are currently undocumented; you can get a complete list by
running qemu-system-aarch64 --machine help.


Arm Integrator/CP (integratorcp)

The Arm Integrator/CP board is emulated with the following devices:
    • ARM926E, ARM1026E, ARM946E, ARM1136 or Cortex-A8 CPU
    • Two PL011 UARTs
    • SMC 91c111 Ethernet adapter
    • PL110 LCD controller
    • PL050 KMI with PS/2 keyboard and mouse.
    • PL181 MultiMedia Card Interface with SD card.




2.24. QEMU System Emulator Targets                                                                                  191
QEMU Documentation, Release 8.1.5


Arm MPS2 and MPS3 boards (mps2-an385, mps2-an386, mps2-an500, mps2-an505, mps2-an511,
mps2-an521, mps3-an524, mps3-an547)

These board models all use Arm M-profile CPUs.
The Arm MPS2, MPS2+ and MPS3 dev boards are FPGA based (the 2+ has a bigger FPGA but is otherwise the same
as the 2; the 3 has a bigger FPGA again, can handle 4GB of RAM and has a USB controller and QSPI flash).
Since the CPU itself and most of the devices are in the FPGA, the details of the board as seen by the guest depend
significantly on the FPGA image.
QEMU models the following FPGA images:
mps2-an385
     Cortex-M3 as documented in Arm Application Note AN385
mps2-an386
     Cortex-M4 as documented in Arm Application Note AN386
mps2-an500
     Cortex-M7 as documented in Arm Application Note AN500
mps2-an505
     Cortex-M33 as documented in Arm Application Note AN505
mps2-an511
     Cortex-M3 ‘DesignStart’ as documented in Arm Application Note AN511
mps2-an521
     Dual Cortex-M33 as documented in Arm Application Note AN521
mps3-an524
     Dual Cortex-M33 on an MPS3, as documented in Arm Application Note AN524
mps3-an547
     Cortex-M55 on an MPS3, as documented in Arm Application Note AN547
Differences between QEMU and real hardware:
    • AN385/AN386 remapping of low 16K of memory to either ZBT SSRAM1 or to block RAM is unimplemented
      (QEMU always maps this to ZBT SSRAM1, as if zbt_boot_ctrl is always zero)
    • AN524 remapping of low memory to either BRAM or to QSPI flash is unimplemented (QEMU always maps
      this to BRAM, ignoring the SCC CFG_REG0 memory-remap bit)
    • QEMU provides a LAN9118 ethernet rather than LAN9220; the only guest visible difference is that the LAN9118
      doesn’t support checksum offloading
    • QEMU does not model the QSPI flash in MPS3 boards as real QSPI flash, but only as simple ROM, so attempting
      to rewrite the flash from the guest will fail
    • QEMU does not model the USB controller in MPS3 boards




192                                                                            Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


Machine-specific options

The following machine-specific options are supported:
remap
     Supported for mps3-an524 only. Set BRAM/QSPI to select the initial memory mapping. The default is BRAM.


Arm Musca boards (musca-a, musca-b1)

The Arm Musca development boards are a reference implementation of a system using the SSE-200 Subsystem for
Embedded. They are dual Cortex-M33 systems.
QEMU provides models of the A and B1 variants of this board.
Unimplemented devices:
    • SPI
    • I2 C
    • I2 S
    • PWM
    • QSPI
    • Timer
    • SCC
    • GPIO
    • eFlash
    • MHU
    • PVT
    • SDIO
    • CryptoCell
Note that (like the real hardware) the Musca-A machine is asymmetric: CPU 0 does not have the FPU or DSP extensions,
but CPU 1 does. Also like the real hardware, the memory maps for the A and B1 variants differ significantly, so guest
software must be built for the right variant.


Arm Realview boards (realview-eb, realview-eb-mpcore, realview-pb-a8, realview-pbx-a9)

Several variants of the Arm RealView baseboard are emulated, including the EB, PB-A8 and PBX-A9. Due to inter-
actions with the bootloader, only certain Linux kernel configurations work out of the box on these boards.
Kernels for the PB-A8 board should have CONFIG_REALVIEW_HIGH_PHYS_OFFSET enabled in the kernel,
and expect 512M RAM. Kernels for The PBX-A9 board should have CONFIG_SPARSEMEM enabled, CON-
FIG_REALVIEW_HIGH_PHYS_OFFSET disabled and expect 1024M RAM.
The following devices are emulated:
    • ARM926E, ARM1136, ARM11MPCore, Cortex-A8 or Cortex-A9 MPCore CPU
    • Arm AMBA Generic/Distributed Interrupt Controller
    • Four PL011 UARTs



2.24. QEMU System Emulator Targets                                                                              193
QEMU Documentation, Release 8.1.5


    • SMC 91c111 or SMSC LAN9118 Ethernet adapter
    • PL110 LCD controller
    • PL050 KMI with PS/2 keyboard and mouse
    • PCI host bridge
    • PCI OHCI USB controller
    • LSI53C895A PCI SCSI Host Bus Adapter with hard disk and CD-ROM devices
    • PL181 MultiMedia Card Interface with SD card.


Arm Server Base System Architecture Reference board (sbsa-ref)

While the virt board is a generic board platform that doesn’t match any real hardware the sbsa-ref board intends to
look like real hardware. The Server Base System Architecture defines a minimum base line of hardware support and
importantly how the firmware reports that to any operating system.
It is intended to be a machine for developing firmware and testing standards compliance with operating systems.


Supported devices

The sbsa-ref board supports:
    • A configurable number of AArch64 CPUs
    • GIC version 3
    • System bus AHCI controller
    • System bus XHCI controller
    • CDROM and hard disc on AHCI bus
    • E1000E ethernet card on PCIe bus
    • Bochs display adapter on PCIe bus
    • A generic SBSA watchdog device


Board to firmware interface

sbsa-ref is a static system that reports a very minimal devicetree to the firmware for non-discoverable information
about system components. This includes both internal hardware and parts affected by the qemu command line (i.e.
CPUs and memory). As a result it must have a firmware specifically built to expect a certain hardware layout (as you
would in a real machine).




194                                                                              Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


DeviceTree information

The devicetree provided by the board model to the firmware is not intended to be a complete compliant DT. It currently
reports:
      • CPUs
      • memory
      • platform version
      • GIC addresses


Platform version

The platform version is only for informing platform firmware about what kind of sbsa-ref board it is running on. It is
neither a QEMU versioned machine type nor a reflection of the level of the SBSA/SystemReady SR support provided.
The machine-version-major value is updated when changes breaking fw compatibility are introduced. The
machine-version-minor value is updated when features are added that don’t break fw compatibility.
Platform version changes:
0.0
        Devicetree holds information about CPUs, memory and platform version.
0.1
        GIC information is present in devicetree.
0.2
        GIC ITS information is present in devicetree.
0.3
        The USB controller is an XHCI device, not EHCI


Arm Versatile boards (versatileab, versatilepb)

The Arm Versatile baseboard is emulated with the following devices:
      • ARM926E, ARM1136 or Cortex-A8 CPU
      • PL190 Vectored Interrupt Controller
      • Four PL011 UARTs
      • SMC 91c111 Ethernet adapter
      • PL110 LCD controller
      • PL050 KMI with PS/2 keyboard and mouse.
      • PCI host bridge. Note the emulated PCI bridge only provides access to PCI memory space. It does not provide
        access to PCI IO space. This means some devices (eg. ne2k_pci NIC) are not usable, and others (eg. rtl8139
        NIC) are only usable when the guest drivers use the memory mapped control registers.
      • PCI OHCI USB controller.
      • LSI53C895A PCI SCSI Host Bus Adapter with hard disk and CD-ROM devices.
      • PL181 MultiMedia Card Interface with SD card.




2.24. QEMU System Emulator Targets                                                                               195
QEMU Documentation, Release 8.1.5


Booting a Linux kernel

Building a current Linux kernel with versatile_defconfig should be enough to get something running. Nowadays
an out-of-tree build is recommended (and also useful if you build a lot of different targets). In the following example
$BLD points to the build directory and $SRC points to the root of the Linux source tree. You can drop $SRC if you
are running from there.

$ make O=$BLD -C $SRC ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- versatile_defconfig
$ make O=$BLD -C $SRC ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

You may want to enable some additional modules if you want to boot something from the SCSI interface:

CONFIG_PCI=y
CONFIG_PCI_VERSATILE=y
CONFIG_SCSI=y
CONFIG_SCSI_SYM53C8XX_2=y

You can then boot with a command line like:

$ qemu-system-arm -machine type=versatilepb \
    -serial mon:stdio \
    -drive if=scsi,driver=file,filename=debian-buster-armel-rootfs.ext4 \
    -kernel zImage \
    -dtb versatile-pb.dtb \
    -append "console=ttyAMA0 ro root=/dev/sda"


Arm Versatile Express boards (vexpress-a9, vexpress-a15)

QEMU models two variants of the Arm Versatile Express development board family:
    • vexpress-a9 models the combination of the Versatile Express motherboard and the CoreTile Express A9x4
      daughterboard
    • vexpress-a15 models the combination of the Versatile Express motherboard and the CoreTile Express A15x2
      daughterboard
Note that as this hardware does not have PCI, IDE or SCSI, the only available storage option is emulated SD card.
Implemented devices:
    • PL041 audio
    • PL181 SD controller
    • PL050 keyboard and mouse
    • PL011 UARTs
    • SP804 timers
    • I2C controller
    • PL031 RTC
    • PL111 LCD display controller
    • Flash memory
    • LAN9118 ethernet



196                                                                                Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5


Unimplemented devices:
    • SP810 system control block
    • PCI-express
    • USB controller (Philips ISP1761)
    • Local DAP ROM
    • CoreSight interfaces
    • PL301 AXI interconnect
    • SCC
    • System counter
    • HDLCD controller (vexpress-a15)
    • SP805 watchdog
    • PL341 dynamic memory controller
    • DMA330 DMA controller
    • PL354 static memory controller
    • BP147 TrustZone Protection Controller
    • TrustZone Address Space Controller
Other differences between the hardware and the QEMU model:
    • QEMU will default to creating one CPU unless you pass a different -smp argument
    • QEMU allows the amount of RAM provided to be specified with the -m argument
    • QEMU defaults to providing a CPU which does not provide either TrustZone or the Virtualization Extensions:
      if you want these you must enable them with -machine secure=on and -machine virtualization=on
    • QEMU provides 4 virtio-mmio virtio transports; these start at address 0x10013000 for vexpress-a9 and at
      0x1c130000 for vexpress-a15, and have IRQs from 40 upwards. If a dtb is provided on the command line
      then QEMU will edit it to include suitable entries describing these transports for the guest.


Booting a Linux kernel

Building a current Linux kernel with multi_v7_defconfig should be enough to get something running. Nowadays
an out-of-tree build is recommended (and also useful if you build a lot of different targets). In the following example
$BLD points to the build directory and $SRC points to the root of the Linux source tree. You can drop $SRC if you
are running from there.

$ make O=$BLD -C $SRC ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- multi_v7_defconfig
$ make O=$BLD -C $SRC ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

By default you will want to boot your rootfs off the sdcard interface. Your rootfs will need to be padded to the right
size. With a suitable DTB you could also add devices to the virtio-mmio bus.

$ qemu-system-arm -cpu cortex-a15 -smp 4 -m 4096 \
    -machine type=vexpress-a15 -serial mon:stdio \
    -drive if=sd,driver=file,filename=armel-rootfs.ext4 \
    -kernel zImage \
                                                                                                   (continues on next page)



2.24. QEMU System Emulator Targets                                                                                   197
QEMU Documentation, Release 8.1.5


                                                                                    (continued from previous page)
      -dtb vexpress-v2p-ca15-tc1.dtb \
      -append "console=ttyAMA0 root=/dev/mmcblk0 ro"


Aspeed family boards (*-bmc, ast2500-evb, ast2600-evb)

The QEMU Aspeed machines model BMCs of various OpenPOWER systems and Aspeed evaluation boards. They
are based on different releases of the Aspeed SoC : the AST2400 integrating an ARM926EJ-S CPU (400MHz), the
AST2500 with an ARM1176JZS CPU (800MHz) and more recently the AST2600 with dual cores ARM Cortex-A7
CPUs (1.2GHz).
The SoC comes with RAM, Gigabit ethernet, USB, SD/MMC, USB, SPI, I2C, etc.
AST2400 SoC based machines :
   • palmetto-bmc OpenPOWER Palmetto POWER8 BMC
   • quanta-q71l-bmc OpenBMC Quanta BMC
   • supermicrox11-bmc Supermicro X11 BMC
AST2500 SoC based machines :
   • ast2500-evb Aspeed AST2500 Evaluation board
   • romulus-bmc OpenPOWER Romulus POWER9 BMC
   • witherspoon-bmc OpenPOWER Witherspoon POWER9 BMC
   • sonorapass-bmc OCP SonoraPass BMC
   • fp5280g2-bmc Inspur FP5280G2 BMC
   • g220a-bmc Bytedance G220A BMC
   • yosemitev2-bmc Facebook YosemiteV2 BMC
   • tiogapass-bmc Facebook Tiogapass BMC
AST2600 SoC based machines :
   • ast2600-evb Aspeed AST2600 Evaluation board (Cortex-A7)
   • tacoma-bmc OpenPOWER Witherspoon POWER9 AST2600 BMC
   • rainier-bmc IBM Rainier POWER10 BMC
   • fuji-bmc Facebook Fuji BMC
   • bletchley-bmc Facebook Bletchley BMC
   • fby35-bmc Facebook fby35 BMC
   • qcom-dc-scm-v1-bmc Qualcomm DC-SCM V1 BMC
   • qcom-firework-bmc Qualcomm Firework BMC




198                                                                       Chapter 2. System Emulation
                                                                        QEMU Documentation, Release 8.1.5


Supported devices

   • SMP (for the AST2600 Cortex-A7)
   • Interrupt Controller (VIC)
   • Timer Controller
   • RTC Controller
   • I2C Controller, including the new register interface of the AST2600
   • System Control Unit (SCU)
   • SRAM mapping
   • X-DMA Controller (basic interface)
   • Static Memory Controller (SMC or FMC) - Only SPI Flash support
   • SPI Memory Controller
   • USB 2.0 Controller
   • SD/MMC storage controllers
   • SDRAM controller (dummy interface for basic settings and training)
   • Watchdog Controller
   • GPIO Controller (Master only)
   • UART
   • Ethernet controllers
   • Front LEDs (PCA9552 on I2C bus)
   • LPC Peripheral Controller (a subset of subdevices are supported)
   • Hash/Crypto Engine (HACE) - Hash support only. TODO: HMAC and RSA
   • ADC
   • Secure Boot Controller (AST2600)
   • eMMC Boot Controller (dummy)
   • PECI Controller (minimal)
   • I3C Controller


Missing devices

   • Coprocessor support
   • PWM and Fan Controller
   • Slave GPIO Controller
   • Super I/O Controller
   • PCI-Express 1 Controller
   • Graphic Display Controller
   • MCTP Controller



2.24. QEMU System Emulator Targets                                                                   199
QEMU Documentation, Release 8.1.5


    • Mailbox Controller
    • Virtual UART
    • eSPI Controller


Boot options

The Aspeed machines can be started using the -kernel and -dtb options to load a Linux kernel or from a firmware.
Images can be downloaded from the OpenBMC jenkins :
      https://jenkins.openbmc.org/job/ci-openbmc/lastSuccessfulBuild/
or directly from the OpenBMC GitHub release repository :
      https://github.com/openbmc/openbmc/releases
To boot a kernel directly from a Linux build tree:

$ qemu-system-arm -M ast2600-evb -nographic \
      -kernel arch/arm/boot/zImage \
      -dtb arch/arm/boot/dts/aspeed-ast2600-evb.dtb \
      -initrd rootfs.cpio

The image should be attached as an MTD drive. Run :

$ qemu-system-arm -M romulus-bmc -nic user \
      -drive file=obmc-phosphor-image-romulus.static.mtd,format=raw,if=mtd -nographic

Options specific to Aspeed machines are :
    • execute-in-place which emulates the boot from the CE0 flash device by using the FMC controller to load
      the instructions, and not simply from RAM. This takes a little longer.
    • fmc-model to change the FMC Flash model. FW needs support for the chip model to boot.
    • spi-model to change the SPI Flash model.
    • bmc-console to change the default console device. Most of the machines use the UART5 device for a boot
      console, which is mapped on /dev/ttyS4 under Linux, but it is not always the case.
For instance, to start the ast2500-evb machine with a different FMC chip and a bigger (64M) SPI chip, use :

-M ast2500-evb,fmc-model=mx25l25635e,spi-model=mx66u51235f

To change the boot console and use device UART3 (/dev/ttyS2 under Linux), use :

-M ast2500-evb,bmc-console=uart3




200                                                                            Chapter 2. System Emulation
                                                                        QEMU Documentation, Release 8.1.5


Aspeed minibmc family boards (ast1030-evb)

The QEMU Aspeed machines model mini BMCs of various Aspeed evaluation boards. They are based on different
releases of the Aspeed SoC : the AST1030 integrating an ARM Cortex M4F CPU (200MHz).
The SoC comes with SRAM, SPI, I2C, etc.
AST1030 SoC based machines :
   • ast1030-evb Aspeed AST1030 Evaluation board (Cortex-M4F)


Supported devices

   • SMP (for the AST1030 Cortex-M4F)
   • Interrupt Controller (VIC)
   • Timer Controller
   • I2C Controller
   • System Control Unit (SCU)
   • SRAM mapping
   • Static Memory Controller (SMC or FMC) - Only SPI Flash support
   • SPI Memory Controller
   • USB 2.0 Controller
   • Watchdog Controller
   • GPIO Controller (Master only)
   • UART
   • LPC Peripheral Controller (a subset of subdevices are supported)
   • Hash/Crypto Engine (HACE) - Hash support only. TODO: HMAC and RSA
   • ADC
   • Secure Boot Controller
   • PECI Controller (minimal)


Missing devices

   • PWM and Fan Controller
   • Slave GPIO Controller
   • Mailbox Controller
   • Virtual UART
   • eSPI Controller
   • I3C Controller




2.24. QEMU System Emulator Targets                                                                   201
QEMU Documentation, Release 8.1.5


Boot options

The Aspeed machines can be started using the -kernel to load a Zephyr OS or from a firmware. Images can be
downloaded from the ASPEED GitHub release repository :
      https://github.com/AspeedTech-BMC/zephyr/releases
To boot a kernel directly from a Zephyr build tree:

$ qemu-system-arm -M ast1030-evb -nographic \
      -kernel zephyr.elf


Facebook Yosemite v3.5 Platform and CraterLake Server (fby35)

Facebook has a series of multi-node compute server designs named Yosemite. The most recent version released was
Yosemite v3.
Yosemite v3.5 is an iteration on this design, and is very similar: there’s a baseboard with a BMC, and 4 server slots. The
new server board design termed “CraterLake” includes a Bridge IC (BIC), with room for expansion boards to include
various compute accelerators (video, inferencing, etc). At the moment, only the first server slot’s BIC is included.
Yosemite v3.5 is itself a sled which fits into a 40U chassis, and 3 sleds can be fit into a chassis. See here for an example.
In this generation, the BMC is an AST2600 and each BIC is an AST1030. The BMC runs OpenBMC, and the BIC
runs OpenBIC.
Firmware images can be retrieved from the Github releases or built from the source code, see the README’s for
instructions on that. This image uses the “fby35” machine recipe from OpenBMC, and the “yv35-cl” target from
OpenBIC. Some reference images can also be found here:

$ wget https://github.com/facebook/openbmc/releases/download/openbmc-e2294ff5d31d/fby35.
 ˓→mtd

$ wget https://github.com/peterdelevoryas/OpenBIC/releases/download/oby35-cl-2022.13.01/
 ˓→Y35BCL.elf




Since this machine has multiple SoC’s, each with their own serial console, the recommended way to run it is to allocate
a pseudoterminal for each serial console and let the monitor use stdio. Also, starting in a paused state is useful because
it allows you to attach to the pseudoterminals before the boot process starts.

$ qemu-system-arm -machine fby35 \
    -drive file=fby35.mtd,format=raw,if=mtd \
    -device loader,file=Y35BCL.elf,addr=0,cpu-num=2 \
    -serial pty -serial pty -serial mon:stdio \
    -display none -S
$ screen /dev/tty0 # In a separate TMUX pane, terminal window, etc.
$ screen /dev/tty1
$ (qemu) c             # Start the boot process once screen is setup.




202                                                                                    Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


Banana Pi BPI-M2U (bpim2u)

Banana Pi BPI-M2 Ultra is a quad-core mini single board computer built with Allwinner A40i/R40/V40 SoC. It features
2GB of RAM and 8GB eMMC. It also has onboard WiFi and BT. On the ports side, the BPI-M2 Ultra has 2 USB A
2.0 ports, 1 USB OTG port, 1 HDMI port, 1 audio jack, a DC power port, and last but not least, a SATA port.


Supported devices

The Banana Pi M2U machine supports the following devices:
    • SMP (Quad Core Cortex-A7)
    • Generic Interrupt Controller configuration
    • SRAM mappings
    • SDRAM controller
    • Timer device (re-used from Allwinner A10)
    • UART
    • SD/MMC storage controller
    • EMAC ethernet
    • GMAC ethernet
    • Clock Control Unit
    • TWI (I2C)


Limitations

Currently, Banana Pi M2U does not support the following features:
    • Graphical output via HDMI, GPU and/or the Display Engine
    • Audio output
    • Hardware Watchdog
    • Real Time Clock
    • USB 2.0 interfaces
Also see the ‘unimplemented’ array in the Allwinner R40 SoC module for a complete list of unimplemented I/O devices:
./hw/arm/allwinner-r40.c


Boot options

The Banana Pi M2U machine can start using the standard -kernel functionality for loading a Linux kernel or ELF
executable. Additionally, the Banana Pi M2U machine can also emulate the BootROM which is present on an actual
Allwinner R40 based SoC, which loads the bootloader from a SD card, specified via the -sd argument to qemu-system-
arm.




2.24. QEMU System Emulator Targets                                                                             203
QEMU Documentation, Release 8.1.5


Running mainline Linux

To build a Linux mainline kernel that can be booted by the Banana Pi M2U machine, simply configure the kernel using
the sunxi_defconfig configuration:

$ ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- make mrproper
$ ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- make sunxi_defconfig

To boot the newly build linux kernel in QEMU with the Banana Pi M2U machine, use:

$ qemu-system-arm -M bpim2u -nographic \
    -kernel /path/to/linux/arch/arm/boot/zImage \
    -append 'console=ttyS0,115200' \
    -dtb /path/to/linux/arch/arm/boot/dts/sun8i-r40-bananapi-m2-ultra.dtb


Banana Pi M2U images

Note that the mainline kernel does not have a root filesystem. You can choose to build you own image with buildroot
using the bananapi_m2_ultra_defconfig. Also see https://buildroot.org for more information.
Another possibility is to run an OpenWrt image for Banana Pi M2U which can be downloaded from:
      https://downloads.openwrt.org/releases/22.03.3/targets/sunxi/cortexa7/
When using an image as an SD card, it must be resized to a power of two. This can be done with the qemu-img
command. It is recommended to only increase the image size instead of shrinking it to a power of two, to avoid loss of
data. For example, to prepare a downloaded Armbian image, first extract it and then increase its size to one gigabyte
as follows:

$ qemu-img resize \
  openwrt-22.03.3-sunxi-cortexa7-sinovoip_bananapi-m2-ultra-ext4-sdcard.img \
  1G

Instead of providing a custom Linux kernel via the -kernel command you may also choose to let the Banana Pi M2U
machine load the bootloader from SD card, just like a real board would do using the BootROM. Simply pass the selected
image via the -sd argument and remove the -kernel, -append, -dbt and -initrd arguments:

$ qemu-system-arm -M bpim2u -nic user -nographic \
  -sd openwrt-22.03.3-sunxi-cortexa7-sinovoip_bananapi-m2-ultra-ext4-sdcard.img


Running U-Boot

U-Boot mainline can be build and configured using the Bananapi_M2_Ultra_defconfig using similar commands as
describe above for Linux. Note that it is recommended for development/testing to select the following configuration
setting in U-Boot:
      Device Tree Control > Provider for DTB for DT Control > Embedded DTB
The BootROM of allwinner R40 loading u-boot from the 8KiB offset of sdcard. Let’s create an bootable disk image:

$ dd if=/dev/zero of=sd.img bs=32M count=1
$ dd if=u-boot-sunxi-with-spl.bin of=sd.img bs=1k seek=8 conv=notrunc

And then boot it.


204                                                                               Chapter 2. System Emulation
                                                                          QEMU Documentation, Release 8.1.5



$ qemu-system-arm -M bpim2u -nographic -sd sd.img


Banana Pi M2U integration tests

The Banana Pi M2U machine has several integration tests included. To run the whole set of tests, build QEMU from
source and simply provide the following command:

$ cd qemu-build-dir
$ AVOCADO_ALLOW_LARGE_STORAGE=yes tests/venv/bin/avocado \
  --verbose --show=app,console run -t machine:bpim2u \
  ../tests/avocado/boot_linux_console.py


Boundary Devices SABRE Lite (sabrelite)

Boundary Devices SABRE Lite i.MX6 Development Board is a low-cost development platform featuring the powerful
Freescale / NXP Semiconductor’s i.MX 6 Quad Applications Processor.


Supported devices

The SABRE Lite machine supports the following devices:
    • Up to 4 Cortex-A9 cores
    • Generic Interrupt Controller
    • 1 Clock Controller Module
    • 1 System Reset Controller
    • 5 UARTs
    • 2 EPIC timers
    • 1 GPT timer
    • 2 Watchdog timers
    • 1 FEC Ethernet controller
    • 3 I2C controllers
    • 7 GPIO controllers
    • 4 SDHC storage controllers
    • 4 USB 2.0 host controllers
    • 5 ECSPI controllers
    • 1 SST 25VF016B flash
Please note above list is a complete superset the QEMU SABRE Lite machine can support. For a normal use case, a
device tree blob that represents a real world SABRE Lite board, only exposes a subset of devices to the guest software.




2.24. QEMU System Emulator Targets                                                                                205
QEMU Documentation, Release 8.1.5


Boot options

The SABRE Lite machine can start using the standard -kernel functionality for loading a Linux kernel, U-Boot boot-
loader or ELF executable.


Running Linux kernel

Linux mainline v5.10 release is tested at the time of writing. To build a Linux mainline kernel that can be booted by
the SABRE Lite machine, simply configure the kernel using the imx_v6_v7_defconfig configuration:

$ export ARCH=arm
$ export CROSS_COMPILE=arm-linux-gnueabihf-
$ make imx_v6_v7_defconfig
$ make

To boot the newly built Linux kernel in QEMU with the SABRE Lite machine, use:

$ qemu-system-arm -M sabrelite -smp 4 -m 1G \
    -display none -serial null -serial stdio \
    -kernel arch/arm/boot/zImage \
    -dtb arch/arm/boot/dts/imx6q-sabrelite.dtb \
    -initrd /path/to/rootfs.ext4 \
    -append "root=/dev/ram"


Running U-Boot

U-Boot mainline v2020.10 release is tested at the time of writing. To build a U-Boot mainline bootloader that can be
booted by the SABRE Lite machine, use the mx6qsabrelite_defconfig with similar commands as described above for
Linux:

$ export CROSS_COMPILE=arm-linux-gnueabihf-
$ make mx6qsabrelite_defconfig

Note we need to adjust settings by:

$ make menuconfig

then manually select the following configuration in U-Boot:
      Device Tree Control > Provider of DTB for DT Control > Embedded DTB
To start U-Boot using the SABRE Lite machine, provide the u-boot binary to the -kernel argument, along with an SD
card image with rootfs:

$ qemu-system-arm -M sabrelite -smp 4 -m 1G \
    -display none -serial null -serial stdio \
    -kernel u-boot

The following example shows booting Linux kernel from dhcp, and uses the rootfs on an SD card. This requires some
additional command line parameters for QEMU:

-nic user,tftp=/path/to/kernel/zImage \
-drive file=sdcard.img,id=rootfs -device sd-card,drive=rootfs


206                                                                              Chapter 2. System Emulation
                                                                           QEMU Documentation, Release 8.1.5


The directory for the built-in TFTP server should also contain the device tree blob of the SABRE Lite board. The
sample SD card image was populated with the root file system with one single partition. You may adjust the kernel
“root=” boot parameter accordingly.
After U-Boot boots, type the following commands in the U-Boot command shell to boot the Linux kernel:

=> setenv ethaddr 00:11:22:33:44:55
=> setenv bootfile zImage
=> dhcp
=> tftpboot 14000000 imx6q-sabrelite.dtb
=> setenv bootargs root=/dev/mmcblk3p1
=> bootz 12000000 - 14000000


Canon A1100 (canon-a1100)

This machine is a model of the Canon PowerShot A1100 camera, which uses the DIGIC SoC. This model is based on
reverse engineering efforts by the contributors to the CHDK and Magic Lantern projects.
The emulation is incomplete. In particular it can’t be used to run the original camera firmware, but it can successfully
run an experimental version of the barebox bootloader.


Cubietech Cubieboard (cubieboard)

The cubieboard model emulates the Cubietech Cubieboard, which is a Cortex-A8 based single-board computer using
the AllWinner A10 SoC.
Emulated devices:
    • Timer
    • UART
    • RTC
    • EMAC
    • SDHCI
    • USB controller
    • SATA controller
    • TWI (I2C) controller
    • Watchdog timer


Emcraft SmartFusion2 SOM kit (emcraft-sf2)

The emcraft-sf2 board emulates the SmartFusion2 SOM kit from Emcraft (M2S010). This is a System-on-Module
from EmCraft systems, based on the SmartFusion2 SoC FPGA from Microsemi Corporation. The SoC is based on a
Cortex-M4 processor.
Emulated devices:
    • System timer
    • System registers
    • SPI controller


2.24. QEMU System Emulator Targets                                                                                 207
QEMU Documentation, Release 8.1.5


   • UART
   • EMAC


Calxeda Highbank and Midway (highbank, midway)

highbank is a model of the Calxeda Highbank (ECX-1000) system, which has four Cortex-A9 cores.
midway is a model of the Calxeda Midway (ECX-2000) system, which has four Cortex-A15 cores.
Emulated devices:
   • L2x0 cache controller
   • SP804 dual timer
   • PL011 UART
   • PL061 GPIOs
   • PL031 RTC
   • PL022 synchronous serial port controller
   • AHCI
   • XGMAC ethernet controllers


Freecom MusicPal (musicpal)

The Freecom MusicPal internet radio emulation includes the following elements:
   • Marvell MV88W8618 Arm core.
   • 32 MB RAM, 256 KB SRAM, 8 MB flash.
   • Up to 2 16550 UARTs
   • MV88W8xx8 Ethernet controller
   • MV88W8618 audio controller, WM8750 CODEC and mixer
   • 128x64 display with brightness control
   • 2 buttons, 2 navigation wheels with button function


Gumstix Connex and Verdex (connex, verdex)

These machines model the Gumstix Connex and Verdex boards. The Connex has a PXA255 CPU and the Verdex has
a PXA270.
Implemented devices:
   • NOR flash
   • SMC91C111 ethernet
   • Interrupt controller
   • DMA
   • Timer



208                                                                              Chapter 2. System Emulation
                                                                   QEMU Documentation, Release 8.1.5


   • GPIO
   • MMC/SD card
   • Fast infra-red communications port (FIR)
   • LCD controller
   • Synchronous serial ports (SPI)
   • PCMCIA interface
   • I2C
   • I2S


Intel Mainstone II board (mainstone)

The mainstone board emulates the Intel Mainstone II development board, which uses a PXA270 CPU.
Emulated devices:
   • Flash memory
   • Keypad
   • MMC controller
   • 91C111 ethernet
   • PIC
   • Timer
   • DMA
   • GPIO
   • FIR
   • Serial
   • LCD controller
   • SSP
   • USB controller
   • RTC
   • PCMCIA
   • I2C
   • I2S




2.24. QEMU System Emulator Targets                                                                209
QEMU Documentation, Release 8.1.5


Kyoto Microcomputer KZM-ARM11-01 (kzm)

The kzm board emulates the Kyoto Microcomputer KZM-ARM11-01 evaluation board, which is based on an NXP
i.MX32 SoC which uses an ARM1136 CPU.
Emulated devices:
    • UARTs
    • LAN9118 ethernet
    • AVIC
    • CCM
    • GPT
    • EPIT timers
    • I2C
    • GPIO controllers
    • Watchdog timer


Nordic nRF boards (microbit)

The Nordic nRF chips are a family of ARM-based System-on-Chip that are designed to be used for low-power and
short-range wireless solutions.
The nRF51 series is the first series for short range wireless applications. It is superseded by the nRF52 series. The
following machines are based on this chip :
    • microbit BBC micro:bit board with nRF51822 SoC
There are other series such as nRF52, nRF53 and nRF91 which are currently not supported by QEMU.


Supported devices

    • ARM Cortex-M0 (ARMv6-M)
    • Serial ports (UART)
    • Clock controller
    • Timers
    • Random Number Generator (RNG)
    • GPIO controller
    • NVMC
    • SWI




210                                                                              Chapter 2. System Emulation
                                                                       QEMU Documentation, Release 8.1.5


Missing devices

   • Watchdog
   • Real-Time Clock (RTC) controller
   • TWI (i2c)
   • SPI controller
   • Analog to Digital Converter (ADC)
   • Quadrature decoder
   • Radio


Boot options

The Micro:bit machine can be started using the -device option to load a firmware in ihex format. Example:

$ qemu-system-arm -M microbit -device loader,file=test.hex


Nokia N800 and N810 tablets (n800, n810)

Nokia N800 and N810 internet tablets (known also as RX-34 and RX-44 / 48) emulation supports the following ele-
ments:
   • Texas Instruments OMAP2420 System-on-chip (ARM1136 core)
   • RAM and non-volatile OneNAND Flash memories
   • Display connected to EPSON remote framebuffer chip and OMAP on-chip display controller and a LS041y3
     MIPI DBI-C controller
   • TI TSC2301 (in N800) and TI TSC2005 (in N810) touchscreen controllers driven through SPI bus
   • National Semiconductor LM8323-controlled qwerty keyboard driven through I2 C bus
   • Secure Digital card connected to OMAP MMC/SD host
   • Three OMAP on-chip UARTs and on-chip STI debugging console
   • Mentor Graphics "Inventra" dual-role USB controller embedded in a TI TUSB6010 chip - only USB host mode
     is supported
   • TI TMP105 temperature sensor driven through I2 C bus
   • TI TWL92230C power management companion with an RTC on I2 C bus
   • Nokia RETU and TAHVO multi-purpose chips with an RTC, connected through CBUS




2.24. QEMU System Emulator Targets                                                                          211
QEMU Documentation, Release 8.1.5


Nuvoton iBMC boards (*-bmc, npcm750-evb, quanta-gsj)

The Nuvoton iBMC chips (NPCM7xx) are a family of ARM-based SoCs that are designed to be used as Baseboard
Management Controllers (BMCs) in various servers. They all feature one or two ARM Cortex-A9 CPU cores, as well
as an assortment of peripherals targeted for either Enterprise or Data Center / Hyperscale applications. The former is
a superset of the latter, so NPCM750 has all the peripherals of NPCM730 and more.
The NPCM750 SoC has two Cortex-A9 cores and is targeted for the Enterprise segment. The following machines are
based on this chip :
    • npcm750-evb Nuvoton NPCM750 Evaluation board
The NPCM730 SoC has two Cortex-A9 cores and is targeted for Data Center and Hyperscale applications. The fol-
lowing machines are based on this chip :
    • quanta-gbs-bmc Quanta GBS server BMC
    • quanta-gsj Quanta GSJ server BMC
    • kudo-bmc Fii USA Kudo server BMC
    • mori-bmc Fii USA Mori server BMC
There are also two more SoCs, NPCM710 and NPCM705, which are single-core variants of NPCM750 and NPCM730,
respectively. These are currently not supported by QEMU.


Supported devices

    • SMP (Dual Core Cortex-A9)
    • Cortex-A9MPCore built-in peripherals: SCU, GIC, Global Timer, Private Timer and Watchdog.
    • SRAM, ROM and DRAM mappings
    • System Global Control Registers (GCR)
    • Clock and reset controller (CLK)
    • Timer controller (TIM)
    • Serial ports (16550-based)
    • DDR4 memory controller (dummy interface indicating memory training is done)
    • OTP controllers (no protection features)
    • Flash Interface Unit (FIU; no protection features)
    • Random Number Generator (RNG)
    • USB host (USBH)
    • GPIO controller
    • Analog to Digital Converter (ADC)
    • Pulse Width Modulation (PWM)
    • SMBus controller (SMBF)
    • Ethernet controller (EMC)
    • Tachometer
    • Peripheral SPI controller (PSPI)



212                                                                               Chapter 2. System Emulation
                                                                       QEMU Documentation, Release 8.1.5


Missing devices

    • LPC/eSPI host-to-BMC interface, including
         – Keyboard and mouse controller interface (KBCI)
         – Keyboard Controller Style (KCS) channels
         – BIOS POST code FIFO
         – System Wake-up Control (SWC)
         – Shared memory (SHM)
         – eSPI slave interface
    • Ethernet controller (GMAC)
    • USB device (USBD)
    • SD/MMC host
    • PECI interface
    • PCI and PCIe root complex and bridges
    • VDM and MCTP support
    • Serial I/O expansion
    • LPC/eSPI host
    • Coprocessor
    • Graphics
    • Video capture
    • Encoding compression engine
    • Security features


Boot options

The Nuvoton machines can boot from an OpenBMC firmware image, or directly into a kernel using the -kernel option.
OpenBMC images for quanta-gsj and possibly others can be downloaded from the OpenBMC jenkins :
      https://jenkins.openbmc.org/
The firmware image should be attached as an MTD drive. Example :

$ qemu-system-arm -machine quanta-gsj -nographic \
    -drive file=image-bmc,if=mtd,bus=0,unit=0,format=raw

The default root password for test images is usually 0penBmc.




2.24. QEMU System Emulator Targets                                                                          213
QEMU Documentation, Release 8.1.5


NXP i.MX25 PDK board (imx25-pdk)

The imx25-pdk board emulates the NXP i.MX25 Product Development Kit board, which is based on an i.MX25 SoC
which uses an ARM926 CPU.
Emulated devices:
   • SD controller
   • AVIC
   • CCM
   • GPT
   • EPIT timers
   • FEC
   • RNGC
   • I2C
   • GPIO controllers
   • Watchdog timer
   • USB controllers


Orange Pi PC (orangepi-pc)

The Xunlong Orange Pi PC is an Allwinner H3 System on Chip based embedded computer with mainline support in
both U-Boot and Linux. The board comes with a Quad Core Cortex-A7 @ 1.3GHz, 1GiB RAM, 100Mbit ethernet,
USB, SD/MMC, USB, HDMI and various other I/O.


Supported devices

The Orange Pi PC machine supports the following devices:
   • SMP (Quad Core Cortex-A7)
   • Generic Interrupt Controller configuration
   • SRAM mappings
   • SDRAM controller
   • Real Time Clock
   • Timer device (re-used from Allwinner A10)
   • UART
   • SD/MMC storage controller
   • EMAC ethernet
   • USB 2.0 interfaces
   • Clock Control Unit
   • System Control module
   • Security Identifier device


214                                                                       Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


    • TWI (I2C)
    • Watchdog timer


Limitations

Currently, Orange Pi PC does not support the following features:
    • Graphical output via HDMI, GPU and/or the Display Engine
    • Audio output
    • Hardware Watchdog
Also see the ‘unimplemented’ array in the Allwinner H3 SoC module for a complete list of unimplemented I/O devices:
./hw/arm/allwinner-h3.c


Boot options

The Orange Pi PC machine can start using the standard -kernel functionality for loading a Linux kernel or ELF exe-
cutable. Additionally, the Orange Pi PC machine can also emulate the BootROM which is present on an actual Allwin-
ner H3 based SoC, which loads the bootloader from a SD card, specified via the -sd argument to qemu-system-arm.


Machine-specific options

The following machine-specific options are supported:
    • allwinner-rtc.base-year=YYYY
      The Allwinner RTC device is automatically created by the Orange Pi PC machine and uses a default base year
      value which can be overridden using the ‘base-year’ property. The base year is the actual represented year when
      the RTC year value is zero. This option can be used in case the target operating system driver uses a different
      base year value. The minimum value for the base year is 1900.
    • allwinner-sid.identifier=abcd1122-a000-b000-c000-12345678ffff
      The Security Identifier value can be read by the guest. For example, U-Boot uses it to determine a unique MAC
      address.
The above machine-specific options can be specified in qemu-system-arm via the ‘-global’ argument, for example:

$ qemu-system-arm -M orangepi-pc -sd mycard.img \
     -global allwinner-rtc.base-year=2000


Running mainline Linux

Mainline Linux kernels from 4.19 up to latest master are known to work. To build a Linux mainline kernel that can be
booted by the Orange Pi PC machine, simply configure the kernel using the sunxi_defconfig configuration:

$ ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- make mrproper
$ ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- make sunxi_defconfig

To be able to use USB storage, you need to manually enable the corresponding configuration item. Start the kconfig
configuration tool:



2.24. QEMU System Emulator Targets                                                                              215
QEMU Documentation, Release 8.1.5



$ ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- make menuconfig

Navigate to the following item, enable it and save your configuration:
      Device Drivers > USB support > USB Mass Storage support
Build the Linux kernel with:

$ ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- make

To boot the newly build linux kernel in QEMU with the Orange Pi PC machine, use:

$ qemu-system-arm -M orangepi-pc -nic user -nographic \
    -kernel /path/to/linux/arch/arm/boot/zImage \
    -append 'console=ttyS0,115200' \
    -dtb /path/to/linux/arch/arm/boot/dts/sun8i-h3-orangepi-pc.dtb


Orange Pi PC images

Note that the mainline kernel does not have a root filesystem. You may provide it with an official Orange Pi PC image
from the official website:
      http://www.orangepi.org/downloadresources/
Another possibility is to run an Armbian image for Orange Pi PC which can be downloaded from:
      https://www.armbian.com/orange-pi-pc/
Alternatively, you can also choose to build you own image with buildroot using the orangepi_pc_defconfig. Also see
https://buildroot.org for more information.
When using an image as an SD card, it must be resized to a power of two. This can be done with the qemu-img
command. It is recommended to only increase the image size instead of shrinking it to a power of two, to avoid loss of
data. For example, to prepare a downloaded Armbian image, first extract it and then increase its size to one gigabyte
as follows:

$ qemu-img resize Armbian_19.11.3_Orangepipc_bionic_current_5.3.9.img 1G

You can choose to attach the selected image either as an SD card or as USB mass storage. For example, to boot using the
Orange Pi PC Debian image on SD card, simply add the -sd argument and provide the proper root= kernel parameter:

$ qemu-system-arm -M orangepi-pc -nic user -nographic \
    -kernel /path/to/linux/arch/arm/boot/zImage \
    -append 'console=ttyS0,115200 root=/dev/mmcblk0p2' \
    -dtb /path/to/linux/arch/arm/boot/dts/sun8i-h3-orangepi-pc.dtb \
    -sd OrangePi_pc_debian_stretch_server_linux5.3.5_v1.0.img

To attach the image as an USB mass storage device to the machine, simply append to the command:

-drive if=none,id=stick,file=myimage.img \
-device usb-storage,bus=usb-bus.0,drive=stick

Instead of providing a custom Linux kernel via the -kernel command you may also choose to let the Orange Pi PC
machine load the bootloader from SD card, just like a real board would do using the BootROM. Simply pass the
selected image via the -sd argument and remove the -kernel, -append, -dbt and -initrd arguments:




216                                                                                Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5



$ qemu-system-arm -M orangepi-pc -nic user -nographic \
     -sd Armbian_19.11.3_Orangepipc_buster_current_5.3.9.img

Note that both the official Orange Pi PC images and Armbian images start a lot of userland programs via systemd.
Depending on the host hardware and OS, they may be slow to emulate, especially due to emulating the 4 cores. To
help reduce the performance slow down due to emulating the 4 cores, you can give the following kernel parameters via
U-Boot (or via -append):

=> setenv extraargs 'systemd.default_timeout_start_sec=9000 loglevel=7 nosmp␣
 ˓→console=ttyS0,115200'




Running U-Boot

U-Boot mainline can be build and configured using the orangepi_pc_defconfig using similar commands as describe
above for Linux. Note that it is recommended for development/testing to select the following configuration setting in
U-Boot:
      Device Tree Control > Provider for DTB for DT Control > Embedded DTB
To start U-Boot using the Orange Pi PC machine, provide the u-boot binary to the -kernel argument:

$ qemu-system-arm -M orangepi-pc -nic user -nographic \
    -kernel /path/to/uboot/u-boot -sd disk.img

Use the following U-boot commands to load and boot a Linux kernel from SD card:

=> setenv bootargs console=ttyS0,115200
=> ext2load mmc 0 0x42000000 zImage
=> ext2load mmc 0 0x43000000 sun8i-h3-orangepi-pc.dtb
=> bootz 0x42000000 - 0x43000000


Running NetBSD

The NetBSD operating system also includes support for Allwinner H3 based boards, including the Orange Pi PC.
NetBSD 9.0 is known to work best for the Orange Pi PC board and provides a fully working system with serial console,
networking and storage. For the Orange Pi PC machine, get the ‘evbarm-earmv7hf’ based image from:
      https://cdn.netbsd.org/pub/NetBSD/NetBSD-9.0/evbarm-earmv7hf/binary/gzimg/armv7.img.gz
The image requires manually installing U-Boot in the image. Build U-Boot with the orangepi_pc_defconfig configu-
ration as described in the previous section. Next, unzip the NetBSD image and write the U-Boot binary including SPL
using:

$ gunzip armv7.img.gz
$ dd if=/path/to/u-boot-sunxi-with-spl.bin of=armv7.img bs=1024 seek=8 conv=notrunc

Finally, before starting the machine the SD image must be extended such that the size of the SD image is a power of
two and that the NetBSD kernel will not conclude the NetBSD partition is larger than the emulated SD card:

$ qemu-img resize armv7.img 2G

Start the machine using the following command:




2.24. QEMU System Emulator Targets                                                                              217
QEMU Documentation, Release 8.1.5



$ qemu-system-arm -M orangepi-pc -nic user -nographic \
      -sd armv7.img -global allwinner-rtc.base-year=2000

At the U-Boot stage, interrupt the automatic boot process by pressing a key and set the following environment variables
before booting:

=> setenv bootargs root=ld0a
=> setenv kernel netbsd-GENERIC.ub
=> setenv fdtfile dtb/sun8i-h3-orangepi-pc.dtb
=> setenv bootcmd 'fatload mmc 0:1 ${kernel_addr_r} ${kernel}; fatload mmc 0:1 ${fdt_
 ˓→addr_r} ${fdtfile}; fdt addr ${fdt_addr_r}; bootm ${kernel_addr_r} - ${fdt_addr_r}'



Optionally you may save the environment variables to SD card with ‘saveenv’. To continue booting simply give the
‘boot’ command and NetBSD boots.


Orange Pi PC integration tests

The Orange Pi PC machine has several integration tests included. To run the whole set of tests, build QEMU from
source and simply provide the following command:

$ AVOCADO_ALLOW_LARGE_STORAGE=yes avocado --show=app,console run \
   -t machine:orangepi-pc tests/avocado/boot_linux_console.py


Palm Tungsten|E PDA (cheetah)

The Palm Tungsten|E PDA (codename "Cheetah") emulation includes the following elements:
    • Texas Instruments OMAP310 System-on-chip (ARM925T core)
    • ROM and RAM memories (ROM firmware image can be loaded with -option-rom)
    • On-chip LCD controller
    • On-chip Real Time Clock
    • TI TSC2102i touchscreen controller / analog-digital converter / Audio CODEC, connected through MicroWire
      and I2 S busses
    • GPIO-connected matrix keypad
    • Secure Digital card connected to OMAP MMC/SD host
    • Three on-chip UARTs


Raspberry Pi boards (raspi0, raspi1ap, raspi2b, raspi3ap, raspi3b)

QEMU provides models of the following Raspberry Pi boards:
raspi0 and raspi1ap
     ARM1176JZF-S core, 512 MiB of RAM
raspi2b
     Cortex-A7 (4 cores), 1 GiB of RAM
raspi3ap
     Cortex-A53 (4 cores), 512 MiB of RAM


218                                                                                Chapter 2. System Emulation
                                                                         QEMU Documentation, Release 8.1.5


raspi3b
     Cortex-A53 (4 cores), 1 GiB of RAM


Implemented devices

   • ARM1176JZF-S, Cortex-A7 or Cortex-A53 CPU
   • Interrupt controller
   • DMA controller
   • Clock and reset controller (CPRMAN)
   • System Timer
   • GPIO controller
   • Serial ports (BCM2835 AUX - 16550 based - and PL011)
   • Random Number Generator (RNG)
   • Frame Buffer
   • USB host (USBH)
   • GPIO controller
   • SD/MMC host controller
   • SoC thermal sensor
   • USB2 host controller (DWC2 and MPHI)
   • MailBox controller (MBOX)
   • VideoCore firmware (property)


Missing devices

   • Peripheral SPI controller (SPI)
   • Analog to Digital Converter (ADC)
   • Pulse Width Modulation (PWM)


Sharp XScale-based PDA models (akita, borzoi, spitz, terrier, tosa)

The Sharp Zaurus are PDAs based on XScale, able to run Linux (‘SL series’).
The SL-6000 ("Tosa"), released in 2005, uses a PXA255 System-on-chip.
The SL-C3000 ("Spitz"), SL-C1000 ("Akita"), SL-C3100 ("Borzoi") and SL-C3200 ("Terrier") use a PXA270.
The clamshell PDA models emulation includes the following peripherals:
   • Intel PXA255/PXA270 System-on-chip (ARMv5TE core)
   • NAND Flash memory - not in "Tosa"
   • IBM/Hitachi DSCM microdrive in a PXA PCMCIA slot - not in "Akita"
   • On-chip OHCI USB controller - not in "Tosa"



2.24. QEMU System Emulator Targets                                                                       219
QEMU Documentation, Release 8.1.5


   • On-chip LCD controller
   • On-chip Real Time Clock
   • TI ADS7846 touchscreen controller on SSP bus
   • Maxim MAX1111 analog-digital converter on I2 C bus
   • GPIO-connected keyboard controller and LEDs
   • Secure Digital card connected to PXA MMC/SD host
   • Three on-chip UARTs
   • WM8750 audio CODEC on I2 C and I2 S busses


Sharp Zaurus SL-5500 (collie)

This machine is a model of the Sharp Zaurus SL-5500, which was a 1990s PDA based on the StrongARM SA1110.
Implemented devices:
   • NOR flash
   • Interrupt controller
   • Timer
   • RTC
   • GPIO
   • Peripheral Pin Controller (PPC)
   • UARTs
   • Synchronous Serial Ports (SSP)


Siemens SX1 (sx1, sx1-v1)

The Siemens SX1 models v1 and v2 (default) basic emulation. The emulation includes the following elements:
   • Texas Instruments OMAP310 System-on-chip (ARM925T core)
   • ROM and RAM memories (ROM firmware image can be loaded with -pflash) V1 1 Flash of 16MB and 1 Flash
     of 8MB V2 1 Flash of 32MB
   • On-chip LCD controller
   • On-chip Real Time Clock
   • Secure Digital card connected to OMAP MMC/SD host
   • Three on-chip UARTs




220                                                                           Chapter 2. System Emulation
                                                                     QEMU Documentation, Release 8.1.5


Stellaris boards (lm3s6965evb, lm3s811evb)

The Luminary Micro Stellaris LM3S811EVB emulation includes the following devices:
   • Cortex-M3 CPU core.
   • 64k Flash and 8k SRAM.
   • Timers, UARTs, ADC and I2 C interface.
   • OSRAM Pictiva 96x16 OLED with SSD0303 controller on I2 C bus.
The Luminary Micro Stellaris LM3S6965EVB emulation includes the following devices:
   • Cortex-M3 CPU core.
   • 256k Flash and 64k SRAM.
   • Timers, UARTs, ADC, I2 C and SSI interfaces.
   • OSRAM Pictiva 128x64 OLED with SSD0323 controller connected via SSI.


STMicroelectronics STM32 boards (netduino2, netduinoplus2, stm32vldiscovery)

The STM32 chips are a family of 32-bit ARM-based microcontroller by STMicroelectronics.
The STM32F1 series is based on ARM Cortex-M3 core. The following machines are based on this chip :
   • stm32vldiscovery STM32VLDISCOVERY board with STM32F100RBT6 microcontroller
The STM32F2 series is based on ARM Cortex-M3 core. The following machines are based on this chip :
   • netduino2 Netduino 2 board with STM32F205RFT6 microcontroller
The STM32F4 series is based on ARM Cortex-M4F core. This series is pin-to-pin compatible with STM32F2 series.
The following machines are based on this chip :
   • netduinoplus2 Netduino Plus 2 board with STM32F405RGT6 microcontroller
   • olimex-stm32-h405 Olimex STM32 H405 board with STM32F405RGT6 microcontroller
There are many other STM32 series that are currently not supported by QEMU.


Supported devices

   • ARM Cortex-M3, Cortex M4F
   • Analog to Digital Converter (ADC)
   • EXTI interrupt
   • Serial ports (USART)
   • SPI controller
   • System configuration (SYSCFG)
   • Timer controller (TIMER)




2.24. QEMU System Emulator Targets                                                                       221
QEMU Documentation, Release 8.1.5


Missing devices

    • Camera interface (DCMI)
    • Controller Area Network (CAN)
    • Cycle Redundancy Check (CRC) calculation unit
    • Digital to Analog Converter (DAC)
    • DMA controller
    • Ethernet controller
    • Flash Interface Unit
    • GPIO controller
    • I2C controller
    • Inter-Integrated Sound (I2S) controller
    • Power supply configuration (PWR)
    • Random Number Generator (RNG)
    • Real-Time Clock (RTC) controller
    • Reset and Clock Controller (RCC)
    • Secure Digital Input/Output (SDIO) interface
    • USB OTG
    • Watchdog controller (IWDG, WWDG)


Boot options

The STM32 machines can be started using the -kernel option to load a firmware. Example:

$ qemu-system-arm -M stm32vldiscovery -kernel firmware.bin


‘virt’ generic virtual platform (virt)

The virt board is a platform which does not correspond to any real hardware; it is designed for use in virtual machines.
It is the recommended board type if you simply want to run a guest such as Linux and do not care about reproducing
the idiosyncrasies and limitations of a particular bit of real-world hardware.
This is a “versioned” board model, so as well as the virt machine type itself (which may have improvements, bugfixes
and other minor changes between QEMU versions) a version is provided that guarantees to have the same behaviour as
that of previous QEMU releases, so that VM migration will work between QEMU versions. For instance the virt-5.
0 machine type will behave like the virt machine from the QEMU 5.0 release, and migration should work between
virt-5.0 of the 5.0 release and virt-5.0 of the 5.1 release. Migration is not guaranteed to work between different
QEMU releases for the non-versioned virt machine type.




222                                                                                Chapter 2. System Emulation
                                                                       QEMU Documentation, Release 8.1.5


Supported devices

The virt board supports:
    • PCI/PCIe devices
    • Flash memory
    • One PL011 UART
    • An RTC
    • The fw_cfg device that allows a guest to obtain data from QEMU
    • A PL061 GPIO controller
    • An optional SMMUv3 IOMMU
    • hotpluggable DIMMs
    • hotpluggable NVDIMMs
    • An MSI controller (GICv2M or ITS). GICv2M is selected by default along with GICv2. ITS is selected by
      default with GICv3 (>= virt-2.7). Note that ITS is not modeled in TCG mode.
    • 32 virtio-mmio transport devices
    • running guests using the KVM accelerator on aarch64 hardware
    • large amounts of RAM (at least 255GB, and more if using highmem)
    • many CPUs (up to 512 if using a GICv3 and highmem)
    • Secure-World-only devices if the CPU has TrustZone:
         – A second PL011 UART
         – A second PL061 GPIO controller, with GPIO lines for triggering a system reset or system poweroff
         – A secure flash memory
         – 16MB of secure RAM
Supported guest CPU types:
    • cortex-a7 (32-bit)
    • cortex-a15 (32-bit; the default)
    • cortex-a35 (64-bit)
    • cortex-a53 (64-bit)
    • cortex-a55 (64-bit)
    • cortex-a57 (64-bit)
    • cortex-a72 (64-bit)
    • cortex-a76 (64-bit)
    • a64fx (64-bit)
    • host (with KVM only)
    • neoverse-n1 (64-bit)
    • neoverse-v1 (64-bit)
    • max (same as host for KVM; best possible emulation with TCG)


2.24. QEMU System Emulator Targets                                                                            223
QEMU Documentation, Release 8.1.5


Note that the default is cortex-a15, so for an AArch64 guest you must specify a CPU type.
Graphics output is available, but unlike the x86 PC machine types there is no default display device enabled: you should
select one from the Display devices section of “-device help”. The recommended option is virtio-gpu-pci; this is
the only one which will work correctly with KVM. You may also need to ensure your guest kernel is configured with
support for this; see below.


Machine-specific options

The following machine-specific options are supported:
secure
      Set on/off to enable/disable emulating a guest CPU which implements the Arm Security Extensions (Trust-
      Zone). The default is off.
virtualization
      Set on/off to enable/disable emulating a guest CPU which implements the Arm Virtualization Extensions. The
      default is off.
mte
      Set on/off to enable/disable emulating a guest CPU which implements the Arm Memory Tagging Extensions.
      The default is off.
highmem
     Set on/off to enable/disable placing devices and RAM in physical address space above 32 bits. The default is
     on for machine types later than virt-2.12.
compact-highmem
    Set on/off to enable/disable the compact layout for high memory regions. The default is on for machine types
    later than virt-7.2.
highmem-redists
     Set on/off to enable/disable the high memory region for GICv3 or GICv4 redistributor. The default is on.
     Setting this to off will limit the maximum number of CPUs when GICv3 or GICv4 is used.
highmem-ecam
     Set on/off to enable/disable the high memory region for PCI ECAM. The default is on for machine types later
     than virt-3.0.
highmem-mmio
     Set on/off to enable/disable the high memory region for PCI MMIO. The default is on.
gic-version
      Specify the version of the Generic Interrupt Controller (GIC) to provide. Valid values are:
      2
             GICv2. Note that this limits the number of CPUs to 8.
      3
             GICv3. This allows up to 512 CPUs.
      4
             GICv4. Requires virtualization to be on; allows up to 317 CPUs.
      host
             Use the same GIC version the host provides, when using KVM
      max
             Use the best GIC version possible (same as host when using KVM; with TCG this is currently 3 if
             virtualization is off and 4 if virtualization is on, but this may change in future)



224                                                                                Chapter 2. System Emulation
                                                                            QEMU Documentation, Release 8.1.5


its
       Set on/off to enable/disable ITS instantiation. The default is on for machine types later than virt-2.7.
iommu
    Set the IOMMU type to create for the guest. Valid values are:
       none
              Don’t create an IOMMU (the default)
       smmuv3
           Create an SMMUv3
ras
       Set on/off to enable/disable reporting host memory errors to a guest using ACPI and guest external abort ex-
       ceptions. The default is off.
dtb-randomness
      Set on/off to pass random seeds via the guest DTB rng-seed and kaslr-seed nodes (in both “/chosen” and
      “/secure-chosen”) to use for features like the random number generator and address space randomisation. The
      default is on. You will want to disable it if your trusted boot chain will verify the DTB it is passed, since this
      option causes the DTB to be non-deterministic. It would be the responsibility of the firmware to come up with a
      seed and pass it on if it wants to.
dtb-kaslr-seed
     A deprecated synonym for dtb-randomness.


Linux guest kernel configuration

The ‘defconfig’ for Linux arm and arm64 kernels should include the right device drivers for virtio and the PCI controller;
however some older kernel versions, especially for 32-bit Arm, did not have everything enabled by default. If you’re
not seeing PCI devices that you expect, then check that your guest config has:

CONFIG_PCI=y
CONFIG_VIRTIO_PCI=y
CONFIG_PCI_HOST_GENERIC=y

If you want to use the virtio-gpu-pci graphics device you will also need:

CONFIG_DRM=y
CONFIG_DRM_VIRTIO_GPU=y


Hardware configuration information for bare-metal programming

The virt board automatically generates a device tree blob (“dtb”) which it passes to the guest. This provides informa-
tion about the addresses, interrupt lines and other configuration of the various devices in the system. Guest code can
rely on and hard-code the following addresses:
      • Flash memory starts at address 0x0000_0000
      • RAM starts at 0x4000_0000
All other information about device locations may change between QEMU versions, so guest code must look in the
DTB.
QEMU supports two types of guest image boot for virt, and the way for the guest code to locate the dtb binary differs:




2.24. QEMU System Emulator Targets                                                                                   225
QEMU Documentation, Release 8.1.5


    • For guests using the Linux kernel boot protocol (this means any non-ELF file passed to the QEMU -kernel
      option) the address of the DTB is passed in a register (r2 for 32-bit guests, or x0 for 64-bit guests)
    • For guests booting as “bare-metal” (any other kind of boot), the DTB is at the start of RAM (0x4000_0000)


Xilinx Versal Virt (xlnx-versal-virt)

Xilinx Versal is a family of heterogeneous multi-core SoCs (System on Chip) that combine traditional hardened CPUs
and I/O peripherals in a Processing System (PS) with runtime programmable FPGA logic (PL) and an Artificial Intel-
ligence Engine (AIE).
More details here: https://www.xilinx.com/products/silicon-devices/acap/versal.html
The family of Versal SoCs share a single architecture but come in different parts with different speed grades, amounts
of PL and other differences.
The Xilinx Versal Virt board in QEMU is a model of a virtual board (does not exist in reality) with a virtual Versal
SoC without I/O limitations. Currently, we support the following cores and devices:
Implemented CPU cores:
    • 2 ACPUs (ARM Cortex-A72)
Implemented devices:
    • Interrupt controller (ARM GICv3)
    • 2 UARTs (ARM PL011)
    • An RTC (Versal built-in)
    • 2 GEMs (Cadence MACB Ethernet MACs)
    • 8 ADMA (Xilinx zDMA) channels
    • 2 SD Controllers
    • OCM (256KB of On Chip Memory)
    • XRAM (4MB of on chip Accelerator RAM)
    • DDR memory
    • BBRAM (36 bytes of Battery-backed RAM)
    • eFUSE (3072 bytes of one-time field-programmable bit array)
    • 2 CANFDs
QEMU does not yet model any other devices, including the PL and the AI Engine.
Other differences between the hardware and the QEMU model:
    • QEMU allows the amount of DDR memory provided to be specified with the -m argument. If a DTB is provided
      on the command line then QEMU will edit it to include suitable entries describing the Versal DDR memory
      ranges.
    • QEMU provides 8 virtio-mmio virtio transports; these start at address 0xa0000000 and have IRQs from 111
      and upwards.




226                                                                               Chapter 2. System Emulation

```
