; EfesOS stage-2 BIOS loader
; Collects boot metadata, enables A20 and loads the kernel with retried CHS reads.

[bits 16]
[org 0x8000]

KERNEL_LOAD_SEGMENT   equ 0x1000
KERNEL_LOAD_OFFSET    equ 0x0000
SECTORS_PER_TRACK     equ 18
HEADS_PER_CYLINDER    equ 2
BOOT_INFO_ADDRESS     equ 0x5000
BOOT_INFO_MAGIC       equ 0x534F4645 ; "EFOS" in little endian
BOOT_INFO_HEADER_SIZE equ 24
BOOT_KERNEL_CHECKSUM_VERIFIED equ 2
E820_ENTRY_SIZE       equ 24
E820_MAX_ENTRIES      equ 32

%ifndef STAGE2_SECTORS
%define STAGE2_SECTORS 8
%endif

%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 1
%endif

%ifndef KERNEL_CHECKSUM
%define KERNEL_CHECKSUM 0
%endif

KERNEL_START_LBA equ 1 + STAGE2_SECTORS

stage2_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    cld

    mov [boot_drive], dl
    call initialize_boot_info
    call collect_e820_map
    call enable_a20
    test ax, ax
    jz a20_error

    call capture_vga_font
    call load_kernel
    call verify_kernel_checksum
    test ax, ax
    jz checksum_error
    or dword [BOOT_INFO_ADDRESS + 20], BOOT_KERNEL_CHECKSUM_VERIFIED

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov dl, [boot_drive]
    mov esi, BOOT_INFO_ADDRESS
    jmp KERNEL_LOAD_SEGMENT:KERNEL_LOAD_OFFSET

initialize_boot_info:
    mov dword [BOOT_INFO_ADDRESS + 0], BOOT_INFO_MAGIC
    xor eax, eax
    mov al, [boot_drive]
    mov dword [BOOT_INFO_ADDRESS + 4], eax
    mov dword [BOOT_INFO_ADDRESS + 8], 0
    mov dword [BOOT_INFO_ADDRESS + 12], E820_ENTRY_SIZE
    mov dword [BOOT_INFO_ADDRESS + 16], 0
    mov dword [BOOT_INFO_ADDRESS + 20], 0
    ret

collect_e820_map:
    xor ebx, ebx
    mov di, BOOT_INFO_ADDRESS + BOOT_INFO_HEADER_SIZE
.next_entry:
    xor ax, ax
    mov ds, ax
    mov es, ax
    cmp dword [BOOT_INFO_ADDRESS + 8], E820_MAX_ENTRIES
    jae .done

    mov dword [es:di + 20], 1
    mov eax, 0xE820
    mov edx, 0x534D4150
    mov ecx, E820_ENTRY_SIZE
    push di
    int 0x15
    pop di
    jc .done
    cmp eax, 0x534D4150
    jne .failed
    cmp ecx, 20
    jb .failed

    xor ax, ax
    mov ds, ax
    mov es, ax

    mov eax, [es:di + 8]
    or eax, [es:di + 12]
    jz .skip_entry
    inc dword [BOOT_INFO_ADDRESS + 8]
    add di, E820_ENTRY_SIZE
.skip_entry:
    test ebx, ebx
    jnz .next_entry
.done:
    xor ax, ax
    mov ds, ax
    cmp dword [BOOT_INFO_ADDRESS + 8], 0
    je .failed
    ret
.failed:
    xor ax, ax
    mov ds, ax
    mov dword [BOOT_INFO_ADDRESS + 8], 0
    ret

enable_a20:
    call check_a20
    test ax, ax
    jnz .done

    mov ax, 0x2401
    int 0x15
    xor bx, bx
    mov ds, bx
    mov es, bx
    call check_a20
    test ax, ax
    jnz .done

    in al, 0x92
    or al, 0x02
    and al, 0xFE
    out 0x92, al
    call check_a20
    test ax, ax
    jnz .done

    call enable_a20_keyboard_controller
    call check_a20
.done:
    ret

verify_kernel_checksum:
    xor ax, ax
    mov ds, ax
    mov esi, KERNEL_LOAD_SEGMENT
    shl esi, 4
    mov eax, 0xFFFFFFFF
    mov ecx, KERNEL_SECTORS * 512
.checksum_byte:
    xor edx, edx
    mov dl, [ds:esi]
    xor eax, edx
    mov ebx, 8
.checksum_bit:
    shr eax, 1
    jnc .checksum_no_polynomial
    xor eax, 0xEDB88320
.checksum_no_polynomial:
    dec ebx
    jnz .checksum_bit
    inc esi
    dec ecx
    jnz .checksum_byte
    not eax
    cmp eax, KERNEL_CHECKSUM
    jne .checksum_failed
    mov ax, 1
    ret
.checksum_failed:
    xor ax, ax
    ret

enable_a20_keyboard_controller:
    pushf
    cli
    call wait_keyboard_input_empty
    jc .restore
    mov al, 0xAD
    out 0x64, al

    call wait_keyboard_input_empty
    jc .enable_keyboard
    mov al, 0xD0
    out 0x64, al
    call wait_keyboard_output_full
    jc .enable_keyboard
    in al, 0x60
    push ax

    call wait_keyboard_input_empty
    jc .discard_saved_output
    mov al, 0xD1
    out 0x64, al
    call wait_keyboard_input_empty
    jc .discard_saved_output
    pop ax
    or al, 0x02
    out 0x60, al
    call wait_keyboard_input_empty
    jmp .enable_keyboard

.discard_saved_output:
    pop ax
.enable_keyboard:
    call wait_keyboard_input_empty
    jc .restore
    mov al, 0xAE
    out 0x64, al
.restore:
    popf
    ret

wait_keyboard_input_empty:
    mov cx, 0xFFFF
.wait:
    in al, 0x64
    test al, 0x02
    jz .ready
    loop .wait
    stc
    ret
.ready:
    clc
    ret

wait_keyboard_output_full:
    mov cx, 0xFFFF
.wait:
    in al, 0x64
    test al, 0x01
    jnz .ready
    loop .wait
    stc
    ret
.ready:
    clc
    ret

check_a20:
    pushf
    cli
    push ds
    push es
    push si
    push di

    xor ax, ax
    mov ds, ax
    mov si, 0x0500
    mov ax, 0xFFFF
    mov es, ax
    mov di, 0x0510

    mov al, [ds:si]
    push ax
    mov al, [es:di]
    push ax
    mov byte [ds:si], 0x00
    mov byte [es:di], 0xFF
    cmp byte [ds:si], 0xFF

    pop ax
    mov [es:di], al
    pop ax
    mov [ds:si], al
    mov ax, 0
    je .restore
    mov ax, 1
.restore:
    pop di
    pop si
    pop es
    pop ds
    popf
    ret

capture_vga_font:
    mov ax, 0x1130
    mov bh, 0x06
    int 0x10
    jc .unavailable
    xor eax, eax
    mov ax, es
    shl eax, 4
    movzx edx, bp
    add eax, edx
    jc .unavailable
    cmp eax, 0x1000
    jb .unavailable
    cmp eax, 0x003FF000
    jae .unavailable
    xor dx, dx
    mov ds, dx
    mov [BOOT_INFO_ADDRESS + 16], eax
    mov dword [BOOT_INFO_ADDRESS + 20], 1
    ret
.unavailable:
    xor dx, dx
    mov ds, dx
    mov dword [BOOT_INFO_ADDRESS + 16], 0
    mov dword [BOOT_INFO_ADDRESS + 20], 0
    ret

load_kernel:
    mov word [current_lba], KERNEL_START_LBA
    mov word [remaining_sectors], KERNEL_SECTORS
    mov ax, KERNEL_LOAD_SEGMENT
    mov es, ax
    xor bx, bx
.next_sector:
    cmp word [remaining_sectors], 0
    je .done

    push bx
    mov ax, [current_lba]
    xor dx, dx
    mov cx, SECTORS_PER_TRACK * HEADS_PER_CYLINDER
    div cx
    mov [read_cylinder], al
    mov ax, dx
    xor dx, dx
    mov cx, SECTORS_PER_TRACK
    div cx
    mov [read_head], al
    inc dl
    mov [read_sector], dl
    pop bx

    mov byte [retry_count], 3
.read_retry:
    mov ah, 0x02
    mov al, 1
    mov ch, [read_cylinder]
    mov cl, [read_sector]
    mov dh, [read_head]
    mov dl, [boot_drive]
    int 0x13
    jnc .read_ok

    xor ax, ax
    mov ds, ax
    mov dl, [boot_drive]
    int 0x13
    xor ax, ax
    mov ds, ax
    dec byte [retry_count]
    jnz .read_retry
    jmp disk_error

.read_ok:
    xor ax, ax
    mov ds, ax
    inc word [current_lba]
    dec word [remaining_sectors]
    add bx, 512
    jnc .next_sector
    mov ax, es
    add ax, 0x1000
    mov es, ax
    jmp .next_sector
.done:
    ret

print_string:
    cld
.next_character:
    lodsb
    test al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    mov bl, 0x07
    int 0x10
    jmp .next_character
.done:
    ret

a20_error:
    mov si, a20_error_message
    call print_string
    jmp halt

checksum_error:
    mov si, checksum_error_message
    call print_string
    jmp halt

disk_error:
    xor ax, ax
    mov ds, ax
    mov si, disk_error_message
    call print_string

halt:
    cli
    hlt
    jmp halt

a20_error_message:
    db 'EfesOS: A20 could not be enabled.', 13, 10, 0
disk_error_message:
    db 'EfesOS: kernel disk read failed.', 13, 10, 0
checksum_error_message:
    db 'EfesOS: kernel integrity check failed.', 13, 10, 0

boot_drive:
    db 0
current_lba:
    dw 0
remaining_sectors:
    dw 0
read_cylinder:
    db 0
read_head:
    db 0
read_sector:
    db 0
retry_count:
    db 0

times (STAGE2_SECTORS * 512) - ($ - $$) db 0
