# XeonPhi  
Xeon phi KNL modules 4.4.1, libscif 4.4.1 and micmpssd 4.4.1 for linux kernel and tested with kernel 5.6rc1 (ubuntu 20.04)  
My test is to run a xeon phi card (KNC and KNL) on a computer with no option in bios with "4G Above Decoding".  
  
I started from the work i saw at the two links address below  
 ->https://egpu.io/forums/thunderbolt-enclosures/pdf-guide-and-patches-for-making-linux-v5-3-kernel-to-work-with-thunderbolt-3-add-in-card/ 
 ->inspired by the guy "Quant.Geek" at the link https://software.intel.com/en-us/forums/intel-many-integrated-core/topic/719921
 ->my post on intel forum : https://community.intel.com/t5/Server-Products/Intel-Xeon-Phi-Coprocessor-with-eGPU-over-Thunderbolt-3/td-p/1271859
  
# What i use   
 * An AKiTiO Node Thunderbolt 3   
 * A macbook air mid 2015 i7 and 8 gigs of ram  
 * An adapter Thunderbolt 3 to Thunderbolt 2 
 * Ubuntu 20.04 kernel 5.6rc1 on a extern ssd with uefi boot  
 * A xeon phi KNL pci card (7220A)  
 * You can see differents images from my post on intel forum with a test on chuwi box pro but i have to made some change in futur because the card is not seeing from a Thunderbolt port.  
   
# My setting on ubuntu  
 * In /etc/default/grub file : GRUB_CMDLINE_LINUX="hpbussize=0x33,realloc,hpmemsize=2M,hpmmioprefsize=64G,nocrs pcie_ports=native"  
   
 Result :   
 * I can hotplug the card  
   
 * I can see the card information with lspci and see below (sudo lspci -vs xx:xx.x)  
    03:00.0 Co-processor: Intel Corporation Xeon Phi coprocessor 7220 (rev ca)  
    Subsystem: Intel Corporation Xeon Phi coprocessor 7220  
    Physical Slot: 6  
    Flags: bus master, fast devsel, latency 0, IRQ 44, NUMA node 0  
    Memory at fb200000 (32-bit, non-prefetchable) [size=256K]  
    Memory at 383000000000 (64-bit, prefetchable) [size=32G]  
    Capabilities: [40] Power Management version 3  
    Capabilities: [48] MSI: Enable+ Count=1/4 Maskable+ 64bit+  
    Capabilities: [68] Express Endpoint, MSI 00  
    Capabilities: [c8] Vendor Specific Information: Len=00 <?>  
    Capabilities: [100] Device Serial Number ca-xx-xx-xx-xx-xx-xx-xx  
    Capabilities: [fb4] Advanced Error Reporting  
    Capabilities: [138] Power Budgeting <?>  
    Capabilities: [10c] Secondary PCI Express  
    Capabilities: [148] Virtual Channel  
    Capabilities: [c34] Vendor Specific Information: ID=0003 Rev=0 Len=078 <?>  
    Capabilities: [b70] Vendor Specific Information: ID=0001 Rev=0 Len=010 <?>  
    Kernel driver in use: mic_x200  
      
* For information without the grub command line parameters, i have this    
   Region 0: Memory at c1700000 (32-bit, non-prefetchable) [disabled] [size=256K]   
   Region 2: Memory at <unassigned> (64-bit,    prefetchable) [disabled]  
  
* I can verify some inforamtion about modules load and mic card detected with "miccheck" and i have to resolve the issue with   
 mpssdaemon(mpssdaemon 3.8.6 works but it's not for this card as i understand)  
  
* I can verify mic card statut with "micctrl" (micctrl -s") and i can shutdown too (micctrl --shutdown)  

* I can start mpssdaemon service with (systemctl start mpss.service") and i can stop too  
  
* I can see some information of the card with the program "micinfo" (sudo micinfo)  
   micinfo Utility Log  
   Created On Mon May 18 02:46:30 2020  
  
   System Info:  
       Host OS                        : Linux  
       OS Version                     : 5.6.0-050600rc1-generic  
       MPSS Version                   : package mpss-release is not installed  
  
       Host Physical Memory           : 7864 MB  
  
   Device No: 0, Device Name: mic0 [x200]  
  
   Version:  
       SMC Firmware Version           : 121.32.10634  
       Coprocessor OS Version         : Not Available  
       Device Serial Number           : QSKLxxxxxx  
       BIOS Version                   : GVPRCRB8.86B.0015.D07.1705120331  
       BIOS Build date                : 05/12/2017  
       ME Version                     : 3.2.2.8  
  
   Board:  
       Vendor ID                      : 0x8086  
       Device ID                      : 0x2262  
       Subsystem ID                   : 0x7504  
       Coprocessor Stepping ID        : 0x01  
       UUID                           : 9152E853-9710-E711-A5AF-001E67FC1849  
       PCIe Width                     : x4 <----( not well as x16) :)  
       PCIe Speed                     : 8.00 GT/s  
       PCIe Ext Tag Field             : Disabled  
       PCIe No Snoop                  : Enabled  
       PCIe Relaxed Ordering          : Enabled  
       PCIe Max payload size          : 128 bytes  
       PCIe Max read request size     : 128 bytes  
       Coprocessor Model              : 0x57  
       Coprocessor Type               : 0x00  
       Coprocessor Family             : 0x06  
       Coprocessor Stepping           : B0  
       Board SKU                      : B0 SKU 7220A  
       ECC Mode                       : Disabled  
       PCIe Bus Information           : 0000:0a:00.0  
       Coprocessor SMBus Address      : Not Available  
       Coprocessor Brand              : Intel(R) Corporation  
       Coprocessor Board Type         : 0x0a  
       Coprocessor TDP                : Not Available  
  
   Core:  
       Total No. of Active Cores      : Not Available  
       Threads per Core               : Not Available  
       Voltage                        : Not Available  
       Frequency                      : Not Available  
  
   Thermal:  
       Thermal Dissipation            : Not Available  
       Fan RPM                        : Not Available  
       Fan PWM                        : Not Available  
       Die Temp                       : Not Available  
  
   Memory:  
       Vendor                         : Not Available  
       Size                           : Not Available  
       Technology                     : Not Available  
       Speed                          : Not Available  
       Frequency                      : Not Available  
   
  
* The last missing step is booting the card os.

Anothers failed tests  
* I try to port the code of mpss-module 3.8.6 and use a 7210A/3120A xeon phi card but the system is freezing (bad pointer deference -->xxxflush fonction failed from vhost_dev_init).  
* With the port of the version of 3.6.1, i have the same resultat above with the 7220A but the system is freezing too (bad pointer deference-->xxxflush fonction failed vhost_dev_init)  
