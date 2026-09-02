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
BOOT_KERNEL_INTEGRITY_VERIFIED equ 2
E820_ENTRY_SIZE       equ 24
E820_MAX_ENTRIES      equ 32

%ifndef STAGE2_SECTORS
%define STAGE2_SECTORS 12
%endif

%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 1
%endif

KERNEL_LENGTH_BITS equ KERNEL_SECTORS * 4096
KERNEL_LENGTH_BE equ ((KERNEL_LENGTH_BITS & 0xFF) << 24) | (((KERNEL_LENGTH_BITS >> 8) & 0xFF) << 16) | (((KERNEL_LENGTH_BITS >> 16) & 0xFF) << 8) | ((KERNEL_LENGTH_BITS >> 24) & 0xFF)

%ifndef KERNEL_SHA256_0
%define KERNEL_SHA256_0 0
%define KERNEL_SHA256_1 0
%define KERNEL_SHA256_2 0
%define KERNEL_SHA256_3 0
%define KERNEL_SHA256_4 0
%define KERNEL_SHA256_5 0
%define KERNEL_SHA256_6 0
%define KERNEL_SHA256_7 0
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
    call verify_kernel_sha256
    test ax, ax
    jz checksum_error
    or dword [BOOT_INFO_ADDRESS + 20], BOOT_KERNEL_INTEGRITY_VERIFIED

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

; SHA-256 implementation for the loaded kernel. The message is sector padded
; by the image builder, so one additional 64-byte length block is sufficient.
verify_kernel_sha256:
    xor ax, ax
    mov ds, ax
    call sha256_init
    mov ax, KERNEL_LOAD_SEGMENT
    mov es, ax
    xor esi, esi
    mov ecx, (KERNEL_SECTORS * 512) / 64
.sha_data_block:
    push ecx
    call sha256_process_block
    pop ecx
    add esi, 64
    cmp esi, 0x10000
    jb .sha_data_no_segment_wrap
    mov ax, es
    add ax, 0x1000
    mov es, ax
    xor esi, esi
.sha_data_no_segment_wrap:
    dec ecx
    jnz .sha_data_block

    mov dword [sha_block + 0], 0x00000080
    mov dword [sha_block + 4], 0
    mov dword [sha_block + 8], 0
    mov dword [sha_block + 12], 0
    mov dword [sha_block + 16], 0
    mov dword [sha_block + 20], 0
    mov dword [sha_block + 24], 0
    mov dword [sha_block + 28], 0
    mov dword [sha_block + 32], 0
    mov dword [sha_block + 36], 0
    mov dword [sha_block + 40], 0
    mov dword [sha_block + 44], 0
    mov dword [sha_block + 48], 0
    mov dword [sha_block + 52], 0
    mov dword [sha_block + 56], 0
    mov dword [sha_block + 60], KERNEL_LENGTH_BE
    xor ax, ax
    mov es, ax
    mov esi, sha_block
    call sha256_process_block
    mov eax, [sha_h0]
    cmp eax, KERNEL_SHA256_0
    jne .sha_failed
    mov eax, [sha_h1]
    cmp eax, KERNEL_SHA256_1
    jne .sha_failed
    mov eax, [sha_h2]
    cmp eax, KERNEL_SHA256_2
    jne .sha_failed
    mov eax, [sha_h3]
    cmp eax, KERNEL_SHA256_3
    jne .sha_failed
    mov eax, [sha_h4]
    cmp eax, KERNEL_SHA256_4
    jne .sha_failed
    mov eax, [sha_h5]
    cmp eax, KERNEL_SHA256_5
    jne .sha_failed
    mov eax, [sha_h6]
    cmp eax, KERNEL_SHA256_6
    jne .sha_failed
    mov eax, [sha_h7]
    cmp eax, KERNEL_SHA256_7
    jne .sha_failed
    mov ax, 1
    ret
.sha_failed:
    xor ax, ax
    ret

sha256_init:
    mov dword [sha_h0], 0x6A09E667
    mov dword [sha_h1], 0xBB67AE85
    mov dword [sha_h2], 0x3C6EF372
    mov dword [sha_h3], 0xA54FF53A
    mov dword [sha_h4], 0x510E527F
    mov dword [sha_h5], 0x9B05688C
    mov dword [sha_h6], 0x1F83D9AB
    mov dword [sha_h7], 0x5BE0CD19
    ret

sha256_process_block:
    push esi
    xor ecx, ecx
.sha_copy:
    mov eax, [es:esi + ecx * 4]
    bswap eax
    mov [sha_w + ecx * 4], eax
    inc ecx
    cmp ecx, 16
    jb .sha_copy

    mov ecx, 16
.sha_expand:
    mov edi, ecx
    sub edi, 15
    mov eax, [sha_w + edi * 4]
    ror eax, 7
    mov ebx, [sha_w + edi * 4]
    ror ebx, 18
    xor eax, ebx
    mov ebx, [sha_w + edi * 4]
    shr ebx, 3
    xor eax, ebx
    mov [sha_s0], eax

    mov edi, ecx
    sub edi, 2
    mov eax, [sha_w + edi * 4]
    ror eax, 17
    mov ebx, [sha_w + edi * 4]
    ror ebx, 19
    xor eax, ebx
    mov ebx, [sha_w + edi * 4]
    shr ebx, 10
    xor eax, ebx
    mov [sha_s1], eax
    mov edi, ecx
    sub edi, 16
    mov eax, [sha_w + edi * 4]
    add eax, [sha_s0]
    mov edi, ecx
    sub edi, 7
    add eax, [sha_w + edi * 4]
    add eax, [sha_s1]
    mov [sha_w + ecx * 4], eax
    inc ecx
    cmp ecx, 64
    jb .sha_expand

    mov eax, [sha_h0]
    mov [sha_a], eax
    mov eax, [sha_h1]
    mov [sha_b], eax
    mov eax, [sha_h2]
    mov [sha_c], eax
    mov eax, [sha_h3]
    mov [sha_d], eax
    mov eax, [sha_h4]
    mov [sha_e], eax
    mov eax, [sha_h5]
    mov [sha_f], eax
    mov eax, [sha_h6]
    mov [sha_g], eax
    mov eax, [sha_h7]
    mov [sha_h], eax
    xor ecx, ecx
.sha_round:
    mov eax, [sha_e]
    ror eax, 6
    mov ebx, [sha_e]
    ror ebx, 11
    xor eax, ebx
    mov ebx, [sha_e]
    ror ebx, 25
    xor eax, ebx
    mov [sha_s1], eax
    mov ebx, [sha_e]
    and ebx, [sha_f]
    mov edx, [sha_e]
    not edx
    and edx, [sha_g]
    xor ebx, edx
    add eax, ebx
    add eax, [sha_h]
    add eax, [sha_k + ecx * 4]
    add eax, [sha_w + ecx * 4]
    mov [sha_t1], eax

    mov eax, [sha_a]
    ror eax, 2
    mov ebx, [sha_a]
    ror ebx, 13
    xor eax, ebx
    mov ebx, [sha_a]
    ror ebx, 22
    xor eax, ebx
    mov [sha_s0], eax
    mov ebx, [sha_a]
    and ebx, [sha_b]
    mov edx, [sha_a]
    and edx, [sha_c]
    xor ebx, edx
    mov edx, [sha_b]
    and edx, [sha_c]
    xor ebx, edx
    add eax, ebx
    mov [sha_t2], eax

    mov eax, [sha_g]
    mov [sha_h], eax
    mov eax, [sha_f]
    mov [sha_g], eax
    mov eax, [sha_e]
    mov [sha_f], eax
    mov eax, [sha_d]
    add eax, [sha_t1]
    mov [sha_e], eax
    mov eax, [sha_c]
    mov [sha_d], eax
    mov eax, [sha_b]
    mov [sha_c], eax
    mov eax, [sha_a]
    mov [sha_b], eax
    mov eax, [sha_t1]
    add eax, [sha_t2]
    mov [sha_a], eax
    inc ecx
    cmp ecx, 64
    jb .sha_round

    mov eax, [sha_h0]
    add eax, [sha_a]
    mov [sha_h0], eax
    mov eax, [sha_h1]
    add eax, [sha_b]
    mov [sha_h1], eax
    mov eax, [sha_h2]
    add eax, [sha_c]
    mov [sha_h2], eax
    mov eax, [sha_h3]
    add eax, [sha_d]
    mov [sha_h3], eax
    mov eax, [sha_h4]
    add eax, [sha_e]
    mov [sha_h4], eax
    mov eax, [sha_h5]
    add eax, [sha_f]
    mov [sha_h5], eax
    mov eax, [sha_h6]
    add eax, [sha_g]
    mov [sha_h6], eax
    mov eax, [sha_h7]
    add eax, [sha_h]
    mov [sha_h7], eax
    pop esi
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
    db 'EfesOS: kernel SHA-256 integrity check failed.', 13, 10, 0

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

sha_h0: dd 0
sha_h1: dd 0
sha_h2: dd 0
sha_h3: dd 0
sha_h4: dd 0
sha_h5: dd 0
sha_h6: dd 0
sha_h7: dd 0
sha_a: dd 0
sha_b: dd 0
sha_c: dd 0
sha_d: dd 0
sha_e: dd 0
sha_f: dd 0
sha_g: dd 0
sha_h: dd 0
sha_s0: dd 0
sha_s1: dd 0
sha_t1: dd 0
sha_t2: dd 0
sha_block: times 16 dd 0
sha_w: times 64 dd 0

sha_k:
    dd 0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5
    dd 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5
    dd 0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3
    dd 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174
    dd 0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC
    dd 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA
    dd 0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7
    dd 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967
    dd 0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13
    dd 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85
    dd 0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3
    dd 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070
    dd 0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5
    dd 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3
    dd 0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208
    dd 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2

times (STAGE2_SECTORS * 512) - ($ - $$) db 0
