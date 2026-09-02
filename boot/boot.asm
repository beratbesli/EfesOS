; EfesOS stage-1 bootloader
; BIOS loads this sector at physical 0000:7C00 and passes the boot drive in DL.

[bits 16]
[org 0x7C00]

STAGE2_SEGMENT equ 0x0000
STAGE2_OFFSET  equ 0x8000

%ifndef STAGE2_SECTORS
%define STAGE2_SECTORS 12
%endif

%if STAGE2_SECTORS < 1 || STAGE2_SECTORS > 17
%error "stage-2 must fit in the remainder of the first floppy track"
%endif

    jmp 0x0000:boot_start

boot_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    cld

    mov [boot_drive], dl
    mov si, loading_message
    call print_string

    mov byte [retry_count], 3
.read_retry:
    xor ax, ax
    mov es, ax
    mov bx, STAGE2_OFFSET
    mov ah, 0x02
    mov al, STAGE2_SECTORS
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jnc .stage2_ready

    xor ax, ax
    mov ds, ax
    mov dl, [boot_drive]
    int 0x13
    xor ax, ax
    mov ds, ax
    dec byte [retry_count]
    jnz .read_retry
    jmp disk_error

.stage2_ready:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov dl, [boot_drive]
    jmp STAGE2_SEGMENT:STAGE2_OFFSET

disk_error:
    xor ax, ax
    mov ds, ax
    mov si, disk_error_message
    call print_string

halt:
    cli
    hlt
    jmp halt

print_string:
    cld
.next_character:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    int 0x10
    jmp .next_character
.done:
    ret

loading_message:
    db 'EfesOS: loading stage-2...', 13, 10, 0

disk_error_message:
    db 'EfesOS: stage-2 disk read failed.', 13, 10, 0

boot_drive:
    db 0
retry_count:
    db 0

times 510 - ($ - $$) db 0
dw 0xAA55
