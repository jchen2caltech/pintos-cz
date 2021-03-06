#include "threads/loader.h"

#### Kernel loader.

#### This code should be stored in the first sector of a hard disk.
#### When the BIOS runs, it loads this code at physical address
#### 0x7c00-0x7e00 (512 bytes) and jumps to the beginning of it,
#### in real mode.  The loader loads the kernel into memory and jumps
#### to its entry point, which is the start function in start.S.
####
#### The BIOS passes in the drive that the loader was read from as
#### DL, with floppy drives numbered 0x00, 0x01, ... and hard drives
#### numbered 0x80, 0x81, ...  We want to support booting a kernel on
#### a different drive from the loader, so we don't take advantage of
#### this.

# Runs in real mode, which is a 16-bit segment.
  .code16

# Set up segment registers.
# Set stack to grow downward from 60 kB (after boot, the kernel
# continues to use this stack for its initial thread).

  sub %ax, %ax
  mov %ax, %ds
  mov %ax, %ss
  mov $0xf000, %esp

# Configure serial port so we can report progress without connected VGA.
# See [IntrList] for details.
  sub %dx, %dx      # Serial port 0.
  mov $0xe3, %al    # 9600 bps, N-8-1.
          # AH is already 0 (Initialize Port).
  int $0x14      # Destroys AX.

  call puts
  .string "PiLo"

  
  mov $0x7F, %dl      # Start with Hard Drive A; set to 0x79 to compress code
  
hard_disk_loop:

  inc %dl           
  xor %ebx, %ebx      # Read partition table at sector 0
  mov $0x2000, %ax      # Set up memory buffer
  mov %ax, %es
  
  call read_sector  
  jc drive_notfound   # Error occured in 0x13
  
print_hdindex:

  call puts
  .string " hd"
  mov %dl, %al
  sub $0x80, %al
  add $'a', %al
  call putc
  
check_signature:

  cmpw $0xaa55, %es:510
  jne hard_disk_loop
  
  mov $'0', %al       # Partition 1; trick to compress code
  mov $430, %si       # Partition 1 entry; trick to compress code
  
check_partition:
  
  inc %al
  call putc           # print the partition being checked

  add $16, %si        # Increment partition
  cmpw $510, %si      # If all 4 partitions have been checked
  je  hard_disk_loop  # go to the next hard drive
  
  cmpb $0x80, %es:(%si) # Is the partition bootable?
  jne check_partition
  
  cmpb $0x20, %es:4(%si)  # Is this partition kernel-type?
  jne check_partition
 
load_kernel:
  
  mov %es:8(%si), %ebx  # first sector in partition
  mov $0x2000, %ax        # buffer start address
  
  mov %es:12(%si), %ecx # size of the partition
  cmp $1024, %ecx       # maximum set at 1024 sectors (512 kByte)
  jbe load_kernel_loop
  mov $1024, %ecx
  
load_kernel_loop:

  mov %ax, %es        # load address is es:00
  call read_sector
  jc  read_failure
  
  test $0x7, %bx       # print 1 progress indicator every 16 sectors
  jnz load_next
  call puts
  .string "."
  
load_next:
  
  inc %bx             # next sector
  add $32, %ax        # move buffer head (512 = 32 * 16) bytes forward
  loop load_kernel_loop
  
load_finish:
  
  call puts
  .string "\r"
  jmp boot_begin
  
drive_notfound:
  call puts
  .string "\rNo bootable drive\r"
  int $0x18
  
read_failure:
  call puts
  .string "\rRead Failure\r"
  int $0x18

#### Transfer control to the kernel that we loaded.  We read the start
#### address out of the ELF header (see [ELF1]) and convert it from a
#### 32-bit linear address into a 16:16 segment:offset address for
#### real mode, then jump to the converted address.  The 80x86 doesn't
#### have an instruction to jump to an absolute segment:offset kept in
#### registers, so in fact we store the address in a temporary memory
#### location, then jump indirectly through that location.  To save 4
#### bytes in the loader, we reuse 4 bytes of the loader's code for
#### this temporary pointer.

boot_begin:
  mov $0x2000, %ax
  mov %ax, %es
  mov %es:0x18, %dx
  mov %dx, start
  movw $0x2000, start + 2
  ljmp *start

#### The following label is a placeholder for the preceding code to use
#### to set up for the jump to the Pintos kernel.  Of course, the start
#### label also points to the puts function; when the previous code runs,
#### it will clobber a few of the bytes at the start of the puts function,
#### but hey we're done bootloading, so we don't need it anymore.

start:

#### Print string subroutine.  To save space in the loader, this
#### subroutine takes its null-terminated string argument from the
#### code stream just after the call, and then returns to the byte
#### just after the terminating null.  This subroutine preserves all
#### general-purpose registers.

puts:  xchg %si, %ss:(%esp)
  push %ax
next_char:
  mov %cs:(%si), %al
  inc %si
  test %al, %al
  jz 1f
  call putc
  jmp next_char
1:  pop %ax
  xchg %si, %ss:(%esp)
  ret

#### Character output subroutine.  Prints the character in AL to the
#### VGA display and serial port 0, using BIOS services (see
#### [IntrList]).  Preserves all general-purpose registers.
####
#### If called upon to output a carriage return, this subroutine
#### automatically supplies the following line feed.

putc:  pusha

1:  sub %bh, %bh      # Page 0.
  mov $0x0e, %ah      # Teletype output service.
  int $0x10

  mov $0x01, %ah      # Serial port output service.
  sub %dx, %dx      # Serial port 0.
2:  int $0x14       # Destroys AH.
  test $0x80, %ah     # Output timed out?
  jz 3f
  movw $0x9090, 2b    # Turn "int $0x14" above into NOPs.

3:
  cmp $'\r', %al
  jne popa_ret
  mov $'\n', %al
  jmp 1b

popa_ret:
  popa
  ret        # Error code still in CF


#### For reading sector. Sector number is set in %ebx, drive number in %dl.
#### buffer segment

read_sector:

  pusha           # Pushing all registers on stack

  
                  # Set up the DAP in the stack:
                  
  sub %ax, %ax    # Set %ax to 0 for LBA sector number
  push %ax        # Push LBA sector number (8 bytes)
  push %ax        # offset by two 2-byte 0s
  push %ebx       # Push the final four bytes
  push %es        # Push buffer segment
  push %ax        # Push buffer offset
  push $1         # Read one sector
  push $16        # Size of DAP
  mov %sp, %si    # Make DS:SI point at out DAP
 
  mov $0x42, %ah  # Set up extended read
  int $0x13       # Call BIOS, error flag in CF
  popa            # Pop the DAP back
  
  popa            # Pop previously-stored registers back
  ret

#### Command-line arguments and their count.
#### This is written by the `pintos' utility and read by the kernel.
#### The loader itself does not do anything with the command line.
  .org LOADER_ARG_CNT - LOADER_BASE
  .fill LOADER_ARG_CNT_LEN, 1, 0

  .org LOADER_ARGS - LOADER_BASE
  .fill LOADER_ARGS_LEN, 1, 0

#### Partition table.
  .org LOADER_PARTS - LOADER_BASE
  .fill LOADER_PARTS_LEN, 1, 0

#### Boot-sector signature for BIOS inspection.
  .org LOADER_SIG - LOADER_BASE
  .word 0xaa55

