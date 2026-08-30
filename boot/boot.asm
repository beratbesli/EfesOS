; EfesOS stage-1 bootloader
; BIOS bu sektörü fiziksel 0x7C00 adresine yükler ve DL'de önyükleme diskini verir.

[bits 16]
[org 0x7C00]

KERNEL_LOAD_SEGMENT equ 0x1000
KERNEL_LOAD_OFFSET  equ 0x0000
KERNEL_START_SECTOR equ 2
%ifndef KERNEL_SECTORS
%define KERNEL_SECTORS 1
%endif

%if KERNEL_SECTORS > 17
%define FIRST_LOAD_SECTORS 17
%else
%define FIRST_LOAD_SECTORS KERNEL_SECTORS
%endif

    ; CS'yi kanonik 0000h değerine getir. Böylece tüm sabit adresler nettir.
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

    ; BIOS'un verdiği sürücü numarasını, BIOS çağrılarından önce sakla.
    mov [boot_drive], dl

    mov word [0x04F0], 0
    mov ax, 0x1130
    mov bh, 0x06
    int 0x10
    mov [0x04F2], es
    mov [0x04F4], bp
    mov word [0x04F0], 0xB33F

    xor ax, ax
    mov ds, ax

    mov si, loading_message
    call print_string

    ; Ekran BIOS'u sonrasında veri segmentini yeniden kanonikleştir.
    xor ax, ax
    mov ds, ax

    ; İmajın ikinci sektörünü 1000:0000'a (fiziksel 0x10000) yükle.
    mov ax, KERNEL_LOAD_SEGMENT
    mov es, ax
    mov bx, KERNEL_LOAD_OFFSET
    mov ah, 0x02                ; INT 13h, fonksiyon 02h: sektör oku
    mov al, FIRST_LOAD_SECTORS
    mov ch, 0                   ; silindir 0
    mov cl, KERNEL_START_SECTOR ; BIOS sektörleri 1'den başlar
    mov dh, 0                   ; kafa 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

%if KERNEL_SECTORS > 17
%if KERNEL_SECTORS > 35
%define SECOND_LOAD_SECTORS 18
%else
%define SECOND_LOAD_SECTORS KERNEL_SECTORS - 17
%endif
    mov ax, KERNEL_LOAD_SEGMENT + (17 * 32)
    mov es, ax
    mov bx, KERNEL_LOAD_OFFSET
    mov ah, 0x02
    mov al, SECOND_LOAD_SECTORS
    mov ch, 0
    mov cl, 1
    mov dh, 1
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
%endif

%if KERNEL_SECTORS > 35
    mov ax, KERNEL_LOAD_SEGMENT + (35 * 32)
    mov es, ax
    mov bx, KERNEL_LOAD_OFFSET
    mov ah, 0x02
    mov al, KERNEL_SECTORS - 35
    mov ch, 1
    mov cl, 1
    mov dh, 0
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
%endif

    ; BIOS çağrısının segment kayıtlarını veya DF'yi koruduğunu varsayma.
    xor ax, ax
    mov ds, ax
    mov ax, KERNEL_LOAD_SEGMENT
    mov es, ax
    cld
    mov dl, [boot_drive]

    ; Kernel'in gerçek giriş noktasına kontrolü devret.
    jmp KERNEL_LOAD_SEGMENT:KERNEL_LOAD_OFFSET

disk_error:
    xor ax, ax
    mov ds, ax
    mov si, disk_error_message
    call print_string

halt:
    hlt
    jmp halt

; DS:SI ile sonlanan bir metni BIOS teletype servisiyle ekrana yazar.
print_string:
.next_character:
    lodsb
    test al, al
    jz .done

    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x07
    push ds
    push si
    int 0x10
    pop si
    pop ds
    jmp .next_character
.done:
    ret

loading_message:
    db 'EfesOS: loading kernel...', 13, 10, 0

disk_error_message:
    db 'EfesOS: disk read error.', 13, 10, 0

boot_drive:
    db 0

; BIOS boot imzası son iki baytta olmak zorunda: 55 AA.
times 510 - ($ - $$) db 0
dw 0xAA55
