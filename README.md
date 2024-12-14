Minix Device Driver: Secret Keeper

This project involved creating a custom character-special device driver in Minix, called /dev/Secret, to  store and manage secrets between processes. It explored several key operating systems concepts and device driver implementations, including:
- Device Driver Architecture: Building and integrating a custom Minix device driver, following the Minix kernel’s architecture and system-level configurations.
- File Operations: Managing file access and permissions to ensure proper ownership and exclusivity for reading and writing operations.
- Inter-Process Communication: Implementing safe communication mechanisms using system calls like ioctl() to transfer ownership of secrets between processes.
- Memory Safety: Using sys_safecopyfrom and sys_safecopyto to handle data transfers between user and kernel space securely.
- State Management: Ensuring that /dev/Secret preserves its state across live updates.
This project showcased an in-depth understanding of how device drivers work in an operating system, emphasizing secure data handling, user-kernel interactions, and system-level debugging. The driver provided a practical implementation of OS-level file operations while maintaining security and resource management principles.
