Goals
    * Make a toy operating system
    * Has a windows 95-like user interface
    * But has no notion of drives. We use unix filesystem semantics

Methodology
    * Only state goals, do not make assumptions on what the architecture will look like
    * Do not refactor code too early. Only pull out functions when it is needed

Toolset:
    * clang 3.8
    * gdb
    * qemu

Constraints:
    * Bootloader: grub2
    * Gfx: vesa (but can be limited to text-mode in the beginning)

Things to implement:
    * Kernel boots in qemu and writes message to serial port [OK]
    * Load multiboot module
    * Parse multiboot memory layout
    * Kernel heap
    * Symbol loading and backtrace support
    * Load files from an initrd
    * Initialize protected-mode (gdt & idt)
    * PIC
    * System timer (PIT)
    * Physical memory manager (page frame allocator)
    * Virtual memory manager (page allocator)
    * Syscall handler
        * mmap
        * fork
        * exec
    * Process manager
        * Scheduler
        * Context switching
        * Message-passing
    * Usermode libc
    * Graphical usermode program
    * Text-mode usermode program
        * Writes text to screen
        * execs another child program
    * RTC driver so we can have current date in logs




