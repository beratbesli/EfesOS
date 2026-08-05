; BeerOS kernel giriş noktası (real mode)
; Bootloader bu ikiliyi 1000:0000 adresine yükler ve DL'de boot sürücüsünü verir.

[bits 16]
[org 0x0000]

KERNEL_STACK_SEGMENT equ 0x9000
KERNEL_STACK_OFFSET  equ 0xFFFE

kernel_entry:
    cli
    cld

    ; Kernel, bootloader'ın segment kayıtlarına bağımlı kalmaz.
    mov ax, cs
    mov ds, ax
    mov es, ax

    ; Sonraki disk işlemleri için BIOS'un verdiği boot sürücüsünü koru.
    mov [boot_drive], dl

    ; Stack'i video belleğinin (0xA0000) hemen altındaki güvenli alana taşı.
    mov ax, KERNEL_STACK_SEGMENT
    mov ss, ax
    mov sp, KERNEL_STACK_OFFSET
    sti

    call kernel_main

.halt:
    hlt
    jmp .halt

; İleride C tabanlı kernel_main bu katmanın yerini alacak.
kernel_main:
    mov si, entry_message
    call print_string
    ret

; DS:SI ile sonlanan bir metni BIOS teletype servisiyle ekrana yazar.
print_string:
.next_character:
    lodsb
    test al, al
    jz .done

    mov ah, 0x0E
    mov bh, 0x00
    mov bl, 0x0A
    push ds
    push si
    int 0x10
    pop si
    pop ds
    jmp .next_character
.done:
    ret

entry_message:
    db 'BeerOS kernel: entry point reached.', 13, 10, 0

boot_drive:
    db 0

; Stage-1 loader şu an bir sektör yükler; ikili bu nedenle tam 512 bayttır.
times 512 - ($ - $$) db 0

