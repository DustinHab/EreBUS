; made by the compiler; the source lies beside this
section code

private phys_to_virt
section code
phys_to_virt:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov rax, -140737488355328
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    jmp .Lret1
.Lret1:
    mov rsp, rbp
    pop rbp
    ret

private virt_to_phys
section code
virt_to_phys:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, -140737488355328
    mov rdi, rax
    pop rax
    sub rax, rdi
    jmp .Lret2
.Lret2:
    mov rsp, rbp
    pop rbp
    ret

private kernel_virt_to_phys
section code
kernel_virt_to_phys:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, -2147483648
    mov rdi, rax
    pop rax
    sub rax, rdi
    jmp .Lret3
.Lret3:
    mov rsp, rbp
    pop rbp
    ret

private slot_lba
section code
slot_lba:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov dword [rbp - 4], edi
    mov rax, 1024
    push rax
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    mov rax, 2048
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov eax, eax
    jmp .Lret4
.Lret4:
    mov rsp, rbp
    pop rbp
    ret

private checksum
section code
checksum:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 24]
    push rax
    mov rax, -3750763034362895579
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 32]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L1:
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L3
    lea rax, [rbp - 24]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    xor rax, rdi
    mov rdi, r8
    mov [rdi], rax
    lea rax, [rbp - 24]
    push rax
    mov rax, 1099511628211
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    imul rax, rdi
    mov rdi, r8
    mov [rdi], rax
.L2:
    lea rax, [rbp - 32]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L1
.L3:
    lea rax, [rbp - 24]
    mov rax, [rax]
    jmp .Lret5
.Lret5:
    mov rsp, rbp
    pop rbp
    ret

private ensure_buffer
section code
ensure_buffer:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rax, [buffer]
    mov rax, [rax]
    test rax, rax
    je .L4
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret6
    jmp .L5
.L4:
.L5:
    lea rax, [rbp - 8]
    push rax
    mov rax, 2048
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 4096
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 4096
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    not rax
    mov rdi, rax
    pop rax
    and rax, rdi
    push rax
    mov rax, 4096
    mov rdi, rax
    pop rax
    xor edx, edx
    div rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 16]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call pmm_alloc_contig
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L6
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret6
    jmp .L7
.L6:
.L7:
    lea rax, [buffer]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call phys_to_virt
    pop rdi
    mov [rdi], rax
    lea rax, [buffer_bytes]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 4096
    mov rdi, rax
    pop rax
    imul rax, rdi
    pop rdi
    mov [rdi], rax
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret6
.Lret6:
    mov rsp, rbp
    pop rbp
    ret

private align8
section code
align8:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 7
    not rax
    mov rdi, rax
    pop rax
    and rax, rdi
    jmp .Lret7
.Lret7:
    mov rsp, rbp
    pop rbp
    ret

private same32
section code
same32:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 20]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L8:
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L10
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L11
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret8
    jmp .L12
.L11:
.L12:
.L9:
    lea rax, [rbp - 20]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L8
.L10:
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret8
.Lret8:
    mov rsp, rbp
    pop rbp
    ret

private payload_bytes
section code
payload_bytes:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    mov eax, dword [rax]
    push rax
    mov rax, 2147483648
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    je .L13
    mov rax, 40
    jmp .L14
.L13:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    push rax
    pop rdi
    call align8
.L14:
    jmp .Lret9
.Lret9:
    mov rsp, rbp
    pop rbp
    ret

private collect
section code
collect:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L17
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_marked
    test rax, rax
    jne .L17
    mov rax, 0
    jmp .L18
.L17:
    mov rax, 1
.L18:
    test rax, rax
    je .L15
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret10
    jmp .L16
.L15:
.L16:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_is_transient
    test rax, rax
    je .L19
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret10
    jmp .L20
.L19:
.L20:
    mov rax, 4096
    push rax
    lea rax, [collected_count]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L21
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret10
    jmp .L22
.L21:
.L22:
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_set_mark
    lea rax, [collected]
    push rax
    lea rax, [collected_count]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 16]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L23:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_slots
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L25
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    push rax
    pop rdi
    call collect
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L26
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret10
    jmp .L27
.L26:
.L27:
.L24:
    lea rax, [rbp - 16]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L23
.L25:
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret10
.Lret10:
    mov rsp, rbp
    pop rbp
    ret

private clear_marks
section code
clear_marks:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rax, [rbp - 4]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L28:
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    lea rax, [collected_count]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L30
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    push rax
    lea rax, [collected]
    push rax
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_set_mark
.L29:
    lea rax, [rbp - 4]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L28
.L30:
.Lret11:
    mov rsp, rbp
    pop rbp
    ret

private index_of
section code
index_of:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L31
    mov rax, 1
    neg rax
    jmp .Lret12
    jmp .L32
.L31:
.L32:
    lea rax, [rbp - 12]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L33:
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [collected_count]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L35
    lea rax, [collected]
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L36
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    jmp .Lret12
    jmp .L37
.L36:
.L37:
.L34:
    lea rax, [rbp - 12]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L33
.L35:
    mov rax, 1
    neg rax
    jmp .Lret12
.Lret12:
    mov rsp, rbp
    pop rbp
    ret

private copy_name
section code
copy_name:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 20]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L38:
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L40
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L39:
    lea rax, [rbp - 20]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L38
.L40:
    lea rax, [rbp - 16]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L41
    jmp .Lret13
    jmp .L42
.L41:
.L42:
    lea rax, [rbp - 24]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L43:
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    push rax
    mov rax, 32
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L46
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L46
    mov rax, 1
    jmp .L47
.L46:
    mov rax, 0
.L47:
    test rax, rax
    je .L45
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L44:
    lea rax, [rbp - 24]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L43
.L45:
.Lret13:
    mov rsp, rbp
    pop rbp
    ret

private big_worthy
section code
big_worthy:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    mov rax, 4096
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    jne .L50
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_is_fleeting
    test rax, rax
    jne .L50
    mov rax, 0
    jmp .L51
.L50:
    mov rax, 1
.L51:
    test rax, rax
    je .L48
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret14
    jmp .L49
.L48:
.L49:
    lea rax, [rbp - 12]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L52
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L52
    mov rax, 0
    jmp .L53
.L52:
    mov rax, 1
.L53:
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret14
.Lret14:
    mov rsp, rbp
    pop rbp
    ret

private read_header
section code
read_header:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov dword [rbp - 4], edi
    mov [rbp - 16], rsi
    lea rax, [sector.1]
    push rax
    mov rax, 1
    push rax
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    pop rdi
    call slot_lba
    mov eax, eax
    push rax
    pop rdi
    pop rsi
    pop rdx
    call blk_read
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L54
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret15
    jmp .L55
.L54:
.L55:
    lea rax, [rbp - 24]
    push rax
    lea rax, [sector.1]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    mov rax, [rax]
    push rax
    mov rax, 5782989516021518917
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L56
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret15
    jmp .L57
.L56:
.L57:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    jne .L60
    mov rax, 5
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    jne .L60
    mov rax, 0
    jmp .L61
.L60:
    mov rax, 1
.L61:
    test rax, rax
    je .L58
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret15
    jmp .L59
.L58:
.L59:
    mov rax, 2048
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 40
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L62
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret15
    jmp .L63
.L62:
.L63:
    mov rax, 4096
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 24
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L64
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret15
    jmp .L65
.L64:
.L65:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    pop rdi
    mov rcx, [rax + 0]
    mov [rdi + 0], rcx
    mov rcx, [rax + 8]
    mov [rdi + 8], rcx
    mov rcx, [rax + 16]
    mov [rdi + 16], rcx
    mov rcx, [rax + 24]
    mov [rdi + 24], rcx
    mov rcx, [rax + 32]
    mov [rdi + 32], rcx
    mov rcx, [rax + 40]
    mov [rdi + 40], rcx
    mov rcx, [rax + 48]
    mov [rdi + 48], rcx
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret15
.Lret15:
    mov rsp, rbp
    pop rbp
    ret

private ensure_live
section code
ensure_live:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rax, [live]
    mov rax, [rax]
    test rax, rax
    je .L66
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret16
    jmp .L67
.L66:
.L67:
    lea rax, [rbp - 8]
    push rax
    mov rax, 8192
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 4096
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 4096
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    not rax
    mov rdi, rax
    pop rax
    and rax, rdi
    push rax
    mov rax, 4096
    mov rdi, rax
    pop rax
    xor edx, edx
    div rdi
    push rax
    pop rdi
    call pmm_alloc_contig
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L68
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret16
    jmp .L69
.L68:
.L69:
    lea rax, [live]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call phys_to_virt
    pop rdi
    mov [rdi], rax
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret16
.Lret16:
    mov rsp, rbp
    pop rbp
    ret

private live_add
section code
live_add:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 12]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L70:
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [live_count]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L72
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [live]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    call same32
    test rax, rax
    je .L73
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret17
    jmp .L74
.L73:
.L74:
.L71:
    lea rax, [rbp - 12]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L70
.L72:
    mov rax, 8192
    push rax
    lea rax, [live_count]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L75
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret17
    jmp .L76
.L75:
.L76:
    lea rax, [rbp - 16]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L77:
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L79
    lea rax, [live]
    mov rax, [rax]
    push rax
    lea rax, [live_count]
    mov eax, dword [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L78:
    lea rax, [rbp - 16]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L77
.L79:
    lea rax, [live_count]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret17
.Lret17:
    mov rsp, rbp
    pop rbp
    ret

private live_from_slot
section code
live_from_slot:
    push rbp
    mov rbp, rsp
    sub rsp, 112
    mov dword [rbp - 4], edi
    lea rax, [rbp - 64]
    push rax
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    pop rdi
    pop rsi
    call read_header
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L80
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret18
    jmp .L81
.L80:
.L81:
    lea rax, [rbp - 64]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 5
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L82
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret18
    jmp .L83
.L82:
.L83:
    lea rax, [rbp - 68]
    push rax
    lea rax, [rbp - 64]
    add rax, 40
    mov rax, [rax]
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    xor edx, edx
    div rdi
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    test rax, rax
    je .L86
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    pop rdi
    call slot_lba
    mov eax, eax
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call blk_read
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L86
    mov rax, 1
    jmp .L87
.L86:
    mov rax, 0
.L87:
    test rax, rax
    je .L84
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret18
    jmp .L85
.L84:
.L85:
    lea rax, [rbp - 64]
    add rax, 40
    mov rax, [rax]
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call checksum
    push rax
    lea rax, [rbp - 64]
    add rax, 48
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L88
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret18
    jmp .L89
.L88:
.L89:
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 64]
    add rax, 32
    mov rax, [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 88]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L90:
    lea rax, [rbp - 88]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 64]
    add rax, 24
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L92
    lea rax, [rbp - 64]
    add rax, 40
    mov rax, [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L93
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret18
    jmp .L94
.L93:
.L94:
    lea rax, [rbp - 96]
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 80]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
    lea rax, [rbp - 104]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    call payload_bytes
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 64]
    add rax, 40
    mov rax, [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    add rax, 4
    mov eax, dword [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L95
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret18
    jmp .L96
.L95:
.L96:
    lea rax, [rbp - 96]
    mov rax, [rax]
    mov eax, dword [rax]
    push rax
    mov rax, 2147483648
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    je .L97
    lea rax, [rbp - 112]
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    pop rdi
    call live_add
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L99
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret18
    jmp .L100
.L99:
.L100:
    jmp .L98
.L97:
.L98:
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    add rax, 4
    mov eax, dword [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
.L91:
    lea rax, [rbp - 88]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L90
.L92:
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret18
.Lret18:
    mov rsp, rbp
    pop rbp
    ret

private drop_oldest
section code
drop_oldest:
    push rbp
    mov rbp, rsp
    sub rsp, 80
    lea rax, [rbp - 8]
    push rax
    mov rax, 0
    not rax
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 12]
    push rax
    mov rax, 1
    neg rax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 16]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 20]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L101:
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L103
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    pop rdi
    pop rsi
    call read_header
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L104
    jmp .L102
    jmp .L105
.L104:
.L105:
    lea rax, [rbp - 16]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    lea rax, [rbp - 80]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L106
    lea rax, [rbp - 8]
    push rax
    lea rax, [rbp - 80]
    add rax, 16
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 12]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    jmp .L107
.L106:
.L107:
.L102:
    lea rax, [rbp - 20]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L101
.L103:
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    jne .L110
    lea rax, [rbp - 12]
    movsxd rax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L110
    mov rax, 0
    jmp .L111
.L110:
    mov rax, 1
.L111:
    test rax, rax
    je .L108
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret19
    jmp .L109
.L108:
.L109:
    lea rax, [zero.2]
    push rax
    mov rax, 1
    push rax
    lea rax, [rbp - 12]
    movsxd rax, dword [rax]
    mov eax, eax
    push rax
    pop rdi
    call slot_lba
    mov eax, eax
    push rax
    pop rdi
    pop rsi
    pop rdx
    call blk_write
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L112
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret19
    jmp .L113
.L112:
.L113:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [.Ls0]
    push rax
    call kprintf
    add rsp, 16
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret19
.Lret19:
    mov rsp, rbp
    pop rbp
    ret

private make_room
section code
make_room:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
.L114:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    call blob_free_sectors
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L117
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret20
    jmp .L118
.L117:
.L118:
    call ensure_live
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L119
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret20
    jmp .L120
.L119:
.L120:
    lea rax, [live_count]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 12]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L121:
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [collected_count]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L123
    lea rax, [is_big]
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    test rax, rax
    je .L126
    lea rax, [big_hash]
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    call live_add
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L126
    mov rax, 1
    jmp .L127
.L126:
    mov rax, 0
.L127:
    test rax, rax
    je .L124
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret20
    jmp .L125
.L124:
.L125:
.L122:
    lea rax, [rbp - 12]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L121
.L123:
    lea rax, [rbp - 16]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L128:
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L130
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    push rax
    pop rdi
    call live_from_slot
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L131
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret20
    jmp .L132
.L131:
.L132:
.L129:
    lea rax, [rbp - 16]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L128
.L130:
    lea rax, [live_count]
    mov eax, dword [rax]
    push rax
    lea rax, [live]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call blob_compact
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L133
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret20
    jmp .L134
.L133:
.L134:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    call blob_free_sectors
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L135
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret20
    jmp .L136
.L135:
.L136:
    call drop_oldest
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L137
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret20
    jmp .L138
.L137:
.L138:
.L115:
    jmp .L114
.L116:
.Lret20:
    mov rsp, rbp
    pop rbp
    ret

section code
snap_save:
    push rbp
    mov rbp, rsp
    sub rsp, 272
    mov [rbp - 8], rdi
    mov dword [rbp - 12], esi
    call blk_present
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L141
    call ensure_buffer
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L141
    mov rax, 0
    jmp .L142
.L141:
    mov rax, 1
.L142:
    test rax, rax
    je .L139
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret21
    jmp .L140
.L139:
.L140:
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L143
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret21
    jmp .L144
.L143:
.L144:
    lea rax, [collected_count]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 16]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L145:
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L147
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    pop rdi
    call collect
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L148
    call clear_marks
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret21
    jmp .L149
.L148:
.L149:
.L146:
    lea rax, [rbp - 16]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L145
.L147:
    lea rax, [rbp - 24]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 28]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L150:
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [collected_count]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L152
    lea rax, [rbp - 40]
    push rax
    lea rax, [collected]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [is_big]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call big_worthy
    test rax, rax
    je .L153
    mov rax, 1
    jmp .L154
.L153:
    mov rax, 0
.L154:
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [is_big]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L155
    jmp .L151
    jmp .L156
.L155:
.L156:
    lea rax, [big_hash]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    push rax
    pop rdi
    pop rsi
    pop rdx
    call sha256
    lea rax, [rbp - 56]
    push rax
    lea rax, [rbp - 48]
    push rax
    lea rax, [big_hash]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call blob_find
    test rax, rax
    je .L159
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L159
    mov rax, 1
    jmp .L160
.L159:
    mov rax, 0
.L160:
    test rax, rax
    je .L157
    lea rax, [big_lba]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    jmp .L158
.L157:
    lea rax, [big_lba]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 24]
    push rax
    mov rax, 1
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    xor edx, edx
    div rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
.L158:
.L151:
    lea rax, [rbp - 28]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L150
.L152:
    lea rax, [rbp - 24]
    mov rax, [rax]
    test rax, rax
    je .L163
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    call make_room
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L163
    mov rax, 1
    jmp .L164
.L163:
    mov rax, 0
.L164:
    test rax, rax
    je .L161
    call clear_marks
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    xor edx, edx
    div rdi
    push rax
    lea rax, [.Ls1]
    push rax
    call kprintf
    add rsp, 16
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret21
    jmp .L162
.L161:
.L162:
    lea rax, [rbp - 60]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L165:
    lea rax, [rbp - 60]
    mov eax, dword [rax]
    push rax
    lea rax, [collected_count]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L167
    lea rax, [is_big]
    push rax
    lea rax, [rbp - 60]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L170
    lea rax, [big_lba]
    push rax
    lea rax, [rbp - 60]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    test rax, rax
    jne .L170
    mov rax, 0
    jmp .L171
.L170:
    mov rax, 1
.L171:
    test rax, rax
    je .L168
    jmp .L166
    jmp .L169
.L168:
.L169:
    lea rax, [rbp - 72]
    push rax
    lea rax, [collected]
    push rax
    lea rax, [rbp - 60]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [big_lba]
    push rax
    lea rax, [rbp - 60]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    push rax
    lea rax, [big_hash]
    push rax
    lea rax, [rbp - 60]
    mov eax, dword [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call blob_store
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L172
    call clear_marks
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [.Ls2]
    push rax
    call kprintf
    add rsp, 16
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret21
    jmp .L173
.L172:
.L173:
.L166:
    lea rax, [rbp - 60]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L165
.L167:
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    pop rdi
    mov [rdi], rax
    mov rax, 2048
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L174
    call clear_marks
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret21
    jmp .L175
.L174:
.L175:
    lea rax, [rbp - 84]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L176:
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L178
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    pop rdi
    call index_of
    pop rdi
    mov [rdi], rax
.L177:
    lea rax, [rbp - 84]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L176
.L178:
    lea rax, [rbp - 88]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L179:
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    push rax
    lea rax, [collected_count]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L181
    lea rax, [rbp - 96]
    push rax
    lea rax, [collected]
    push rax
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 104]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 112]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_slots
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 120]
    push rax
    lea rax, [is_big]
    push rax
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    test rax, rax
    je .L182
    mov rax, 40
    jmp .L183
.L182:
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    pop rdi
    call align8
.L183:
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 128]
    push rax
    mov rax, 48
    push rax
    lea rax, [rbp - 120]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    mov rax, 2048
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 128]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L184
    call clear_marks
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret21
    jmp .L185
.L184:
.L185:
    lea rax, [rbp - 136]
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 136]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    lea rax, [is_big]
    push rax
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    test rax, rax
    je .L186
    mov rax, 2147483648
    jmp .L187
.L186:
    mov rax, 0
.L187:
    mov rdi, rax
    pop rax
    or rax, rdi
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 136]
    mov rax, [rax]
    add rax, 4
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 136]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_name
    push rax
    lea rax, [rbp - 136]
    mov rax, [rax]
    add rax, 16
    push rax
    pop rdi
    pop rsi
    call copy_name
    lea rax, [rbp - 80]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
    lea rax, [is_big]
    push rax
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    test rax, rax
    je .L188
    lea rax, [rbp - 144]
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 148]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L190:
    lea rax, [rbp - 148]
    mov eax, dword [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L192
    lea rax, [rbp - 144]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 148]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [big_hash]
    push rax
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 148]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L191:
    lea rax, [rbp - 148]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L190
.L192:
    lea rax, [rbp - 144]
    mov rax, [rax]
    add rax, 32
    push rax
    lea rax, [big_lba]
    push rax
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    jmp .L189
.L188:
    lea rax, [rbp - 160]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 168]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L193:
    lea rax, [rbp - 168]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L195
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 168]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 160]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 168]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L194:
    lea rax, [rbp - 168]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L193
.L195:
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
.L196:
    lea rax, [rbp - 176]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    pop rdi
    call align8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L198
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 176]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L197:
    lea rax, [rbp - 176]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L196
.L198:
.L189:
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 120]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
    lea rax, [rbp - 184]
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 192]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L199:
    lea rax, [rbp - 192]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L201
    lea rax, [rbp - 184]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 192]
    mov rax, [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 192]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    push rax
    pop rdi
    call index_of
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 184]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 192]
    mov rax, [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 8
    push rax
    lea rax, [rbp - 192]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_slot_rights
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 184]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 192]
    mov rax, [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 12
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 192]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_slot_name
    push rax
    lea rax, [rbp - 184]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 192]
    mov rax, [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 16
    push rax
    pop rdi
    pop rsi
    call copy_name
.L200:
    lea rax, [rbp - 192]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L199
.L201:
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
.L180:
    lea rax, [rbp - 88]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L179
.L181:
    call clear_marks
    lea rax, [rbp - 248]
    mov rdi, rax
    lea rsi, [.Li0]
    mov rcx, 56
.L202:
    mov al, byte [rsi]
    mov byte [rdi], al
    inc rsi
    inc rdi
    dec rcx
    jne .L202
    lea rax, [rbp - 248]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [current_generation]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 248]
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [collected_count]
    mov eax, dword [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 248]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 248]
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 248]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call checksum
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 252]
    push rax
    lea rax, [rbp - 248]
    add rax, 16
    mov rax, [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    xor edx, edx
    div rdi
    mov rax, rdx
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 256]
    push rax
    lea rax, [rbp - 252]
    mov eax, dword [rax]
    push rax
    pop rdi
    call slot_lba
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 260]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    xor edx, edx
    div rdi
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    mov rax, 2048
    push rax
    lea rax, [rbp - 260]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L203
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret21
    jmp .L204
.L203:
.L204:
    lea rax, [rbp - 260]
    mov eax, dword [rax]
    test rax, rax
    je .L207
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 260]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 256]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call blk_write
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L207
    mov rax, 1
    jmp .L208
.L207:
    mov rax, 0
.L208:
    test rax, rax
    je .L205
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret21
    jmp .L206
.L205:
.L206:
    lea rax, [rbp - 264]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L209:
    lea rax, [rbp - 264]
    mov eax, dword [rax]
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L211
    lea rax, [header_sector.3]
    push rax
    lea rax, [rbp - 264]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L210:
    lea rax, [rbp - 264]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L209
.L211:
    lea rax, [rbp - 268]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L212:
    lea rax, [rbp - 268]
    mov eax, dword [rax]
    push rax
    mov rax, 56
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L214
    lea rax, [header_sector.3]
    push rax
    lea rax, [rbp - 268]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 248]
    push rax
    lea rax, [rbp - 268]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L213:
    lea rax, [rbp - 268]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L212
.L214:
    lea rax, [header_sector.3]
    push rax
    mov rax, 1
    push rax
    lea rax, [rbp - 256]
    mov eax, dword [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call blk_write
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L215
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret21
    jmp .L216
.L215:
.L216:
    lea rax, [current_generation]
    push rax
    lea rax, [rbp - 248]
    add rax, 16
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [have_snapshot]
    push rax
    mov rax, 1
    pop rdi
    test rax, rax
    setne al
    movzx rax, al
    mov byte [rdi], al
    lea rax, [last_bytes]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [last_objects]
    push rax
    lea rax, [collected_count]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    jmp .Lret21
.Lret21:
    mov rsp, rbp
    pop rbp
    ret

section code
snap_history:
    push rbp
    mov rbp, rsp
    sub rsp, 96
    mov [rbp - 8], rdi
    mov dword [rbp - 12], esi
    call blk_present
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L217
    mov rax, 0
    mov eax, eax
    jmp .Lret22
    jmp .L218
.L217:
.L218:
    lea rax, [rbp - 16]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 20]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L219:
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L222
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L222
    mov rax, 1
    jmp .L223
.L222:
    mov rax, 0
.L223:
    test rax, rax
    je .L221
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    pop rdi
    pop rsi
    call read_header
    test rax, rax
    je .L224
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 80]
    add rax, 16
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    jmp .L225
.L224:
.L225:
.L220:
    lea rax, [rbp - 20]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L219
.L221:
    lea rax, [rbp - 84]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L226:
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L228
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L229:
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L231
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L232
    lea rax, [rbp - 96]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    jmp .L233
.L232:
.L233:
.L230:
    lea rax, [rbp - 88]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L229
.L231:
.L227:
    lea rax, [rbp - 84]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L226
.L228:
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    mov eax, eax
    jmp .Lret22
.Lret22:
    mov rsp, rbp
    pop rbp
    ret

private rebuild
section code
rebuild:
    push rbp
    mov rbp, rsp
    sub rsp, 160
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov dword [rbp - 20], edx
    lea rax, [collected_count]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 32
    mov rax, [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 48]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L234:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 24
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L236
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 40
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L237
    jmp .Lg23_refuse
    jmp .L238
.L237:
.L238:
    lea rax, [rbp - 56]
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 40]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
    lea rax, [rbp - 64]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    pop rdi
    call payload_bytes
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 40
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    add rax, 4
    mov eax, dword [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L239
    jmp .Lg23_refuse
    jmp .L240
.L239:
.L240:
    lea rax, [rbp - 72]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    add rax, 4
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    mov eax, dword [rax]
    push rax
    mov rax, 2147483648
    not rax
    mov rdi, rax
    pop rax
    and rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_create
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 72]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L241
    jmp .Lg23_refuse
    jmp .L242
.L241:
.L242:
    lea rax, [collected]
    push rax
    lea rax, [collected_count]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 72]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    mov eax, dword [rax]
    push rax
    mov rax, 2147483648
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    je .L243
    lea rax, [rbp - 88]
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    push rax
    lea rax, [rbp - 88]
    mov rax, [rax]
    add rax, 32
    mov rax, [rax]
    push rax
    lea rax, [rbp - 88]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call blob_read
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L245
    lea rax, [rbp - 56]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [.Ls3]
    push rax
    call kprintf
    add rsp, 24
    jmp .Lg23_refuse
    jmp .L246
.L245:
.L246:
    jmp .L244
.L243:
    lea rax, [rbp - 96]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L247:
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L249
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L248:
    lea rax, [rbp - 96]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L247
.L249:
.L244:
    lea rax, [rbp - 56]
    mov rax, [rax]
    add rax, 16
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L250
    lea rax, [rbp - 56]
    mov rax, [rax]
    add rax, 16
    jmp .L251
.L250:
    mov rax, 0
.L251:
    push rax
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_set_name
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    add rax, 4
    mov eax, dword [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
.L235:
    lea rax, [rbp - 48]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L234
.L236:
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 104]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L252:
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 24
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L254
    lea rax, [rbp - 112]
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 40]
    push rax
    mov rax, 48
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    pop rdi
    call payload_bytes
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
    lea rax, [rbp - 120]
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 124]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L255:
    lea rax, [rbp - 124]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    add rax, 4
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L257
    lea rax, [rbp - 136]
    push rax
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 124]
    mov eax, dword [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    mov rax, 0
    push rax
    lea rax, [rbp - 136]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L260
    lea rax, [rbp - 136]
    mov rax, [rax]
    push rax
    lea rax, [collected_count]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L260
    mov rax, 1
    jmp .L261
.L260:
    mov rax, 0
.L261:
    test rax, rax
    je .L258
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 124]
    mov eax, dword [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 8
    mov eax, dword [rax]
    push rax
    lea rax, [collected]
    push rax
    lea rax, [rbp - 136]
    mov rax, [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    lea rax, [rbp - 124]
    mov eax, dword [rax]
    push rax
    lea rax, [collected]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call obj_set_slot
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 124]
    mov eax, dword [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 16
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L262
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 124]
    mov eax, dword [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 16
    jmp .L263
.L262:
    mov rax, 0
.L263:
    push rax
    lea rax, [rbp - 124]
    mov eax, dword [rax]
    push rax
    lea rax, [collected]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_set_slot_name
    jmp .L259
.L258:
.L259:
.L256:
    lea rax, [rbp - 124]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L255
.L257:
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    add rax, 4
    mov eax, dword [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
.L253:
    lea rax, [rbp - 104]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L252
.L254:
    lea rax, [rbp - 140]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 32
    mov rax, [rax]
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 140]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L264
    lea rax, [rbp - 140]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L265
.L264:
.L265:
    lea rax, [rbp - 144]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L266:
    lea rax, [rbp - 144]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 140]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L268
    lea rax, [rbp - 152]
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 144]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [collected_count]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 152]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L269
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 144]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    jmp .L267
    jmp .L270
.L269:
.L270:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 144]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [collected]
    push rax
    lea rax, [rbp - 152]
    mov rax, [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 144]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    pop rdi
    call obj_retain
.L267:
    lea rax, [rbp - 144]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L266
.L268:
    lea rax, [rbp - 156]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L271:
    lea rax, [rbp - 156]
    mov eax, dword [rax]
    push rax
    lea rax, [collected_count]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L273
    lea rax, [collected]
    push rax
    lea rax, [rbp - 156]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    pop rdi
    call obj_release
.L272:
    lea rax, [rbp - 156]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L271
.L273:
    lea rax, [rbp - 140]
    mov eax, dword [rax]
    mov eax, eax
    jmp .Lret23
.Lg23_refuse:
    lea rax, [rbp - 160]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L274:
    lea rax, [rbp - 160]
    mov eax, dword [rax]
    push rax
    lea rax, [collected_count]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L276
    lea rax, [collected]
    push rax
    lea rax, [rbp - 160]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    pop rdi
    call obj_release
.L275:
    lea rax, [rbp - 160]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L274
.L276:
    lea rax, [collected_count]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    mov rax, 0
    mov eax, eax
    jmp .Lret23
.Lret23:
    mov rsp, rbp
    pop rbp
    ret

private load_slot
section code
load_slot:
    push rbp
    mov rbp, rsp
    sub rsp, 96
    mov dword [rbp - 4], edi
    mov [rbp - 16], rsi
    mov dword [rbp - 20], edx
    mov byte [rbp - 21], cl
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    pop rdi
    pop rsi
    call read_header
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L277
    mov rax, 0
    mov eax, eax
    jmp .Lret24
    jmp .L278
.L277:
.L278:
    lea rax, [rbp - 84]
    push rax
    lea rax, [rbp - 80]
    add rax, 40
    mov rax, [rax]
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 512
    mov rdi, rax
    pop rax
    xor edx, edx
    div rdi
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    test rax, rax
    je .L281
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    pop rdi
    call slot_lba
    mov eax, eax
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call blk_read
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L281
    mov rax, 1
    jmp .L282
.L281:
    mov rax, 0
.L282:
    test rax, rax
    je .L279
    mov rax, 0
    mov eax, eax
    jmp .Lret24
    jmp .L280
.L279:
.L280:
    lea rax, [rbp - 80]
    add rax, 40
    mov rax, [rax]
    push rax
    lea rax, [buffer]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call checksum
    push rax
    lea rax, [rbp - 80]
    add rax, 48
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L283
    lea rax, [rbp - 80]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [.Ls4]
    push rax
    call kprintf
    add rsp, 16
    mov rax, 0
    mov eax, eax
    jmp .Lret24
    jmp .L284
.L283:
.L284:
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 80]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call rebuild
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    test rax, rax
    je .L287
    lea rax, [rbp - 21]
    movzx rax, byte [rax]
    test rax, rax
    je .L287
    mov rax, 1
    jmp .L288
.L287:
    mov rax, 0
.L288:
    test rax, rax
    je .L285
    lea rax, [current_generation]
    push rax
    lea rax, [rbp - 80]
    add rax, 16
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [have_snapshot]
    push rax
    mov rax, 1
    pop rdi
    test rax, rax
    setne al
    movzx rax, al
    mov byte [rdi], al
    lea rax, [last_bytes]
    push rax
    lea rax, [rbp - 80]
    add rax, 40
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [last_objects]
    push rax
    lea rax, [rbp - 80]
    add rax, 24
    mov rax, [rax]
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L286
.L285:
.L286:
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    mov eax, eax
    jmp .Lret24
.Lret24:
    mov rsp, rbp
    pop rbp
    ret

section code
snap_load:
    push rbp
    mov rbp, rsp
    sub rsp, 96
    mov [rbp - 8], rdi
    mov dword [rbp - 12], esi
    call blk_present
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L291
    call ensure_buffer
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L291
    mov rax, 0
    jmp .L292
.L291:
    mov rax, 1
.L292:
    test rax, rax
    je .L289
    mov rax, 0
    mov eax, eax
    jmp .Lret25
    jmp .L290
.L289:
.L290:
.L293:
    lea rax, [rbp - 24]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 28]
    push rax
    mov rax, 1
    neg rax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 32]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L296:
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L298
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    pop rdi
    pop rsi
    call read_header
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L299
    jmp .L297
    jmp .L300
.L299:
.L300:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 88]
    add rax, 16
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L301
    lea rax, [rbp - 24]
    push rax
    lea rax, [rbp - 88]
    add rax, 16
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 28]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    movsxd rax, eax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    jmp .L302
.L301:
.L302:
.L297:
    lea rax, [rbp - 32]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L296
.L298:
    lea rax, [rbp - 28]
    movsxd rax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L303
    mov rax, 0
    mov eax, eax
    jmp .Lret25
    jmp .L304
.L303:
.L304:
    lea rax, [rbp - 92]
    push rax
    mov rax, 1
    test rax, rax
    setne al
    movzx rax, al
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    movsxd rax, dword [rax]
    mov eax, eax
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call load_slot
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    test rax, rax
    je .L305
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov eax, eax
    jmp .Lret25
    jmp .L306
.L305:
.L306:
    lea rax, [zero.4]
    push rax
    mov rax, 1
    push rax
    lea rax, [rbp - 28]
    movsxd rax, dword [rax]
    mov eax, eax
    push rax
    pop rdi
    call slot_lba
    mov eax, eax
    push rax
    pop rdi
    pop rsi
    pop rdx
    call blk_write
.L294:
    jmp .L293
.L295:
.Lret25:
    mov rsp, rbp
    pop rbp
    ret

section code
snap_load_generation:
    push rbp
    mov rbp, rsp
    sub rsp, 80
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov dword [rbp - 20], edx
    call blk_present
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L309
    call ensure_buffer
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L309
    mov rax, 0
    jmp .L310
.L309:
    mov rax, 1
.L310:
    test rax, rax
    je .L307
    mov rax, 0
    mov eax, eax
    jmp .Lret26
    jmp .L308
.L307:
.L308:
    lea rax, [rbp - 24]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L311:
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L313
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    push rax
    pop rdi
    pop rsi
    call read_header
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L314
    jmp .L312
    jmp .L315
.L314:
.L315:
    lea rax, [rbp - 80]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L316
    jmp .L312
    jmp .L317
.L316:
.L317:
    mov rax, 0
    test rax, rax
    setne al
    movzx rax, al
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call load_slot
    mov eax, eax
    mov eax, eax
    jmp .Lret26
.L312:
    lea rax, [rbp - 24]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L311
.L313:
    mov rax, 0
    mov eax, eax
    jmp .Lret26
.Lret26:
    mov rsp, rbp
    pop rbp
    ret

section code
snap_present:
    push rbp
    mov rbp, rsp
    lea rax, [have_snapshot]
    movzx rax, byte [rax]
    jmp .Lret27
.Lret27:
    mov rsp, rbp
    pop rbp
    ret

section code
snap_generation:
    push rbp
    mov rbp, rsp
    lea rax, [current_generation]
    mov rax, [rax]
    jmp .Lret28
.Lret28:
    mov rsp, rbp
    pop rbp
    ret

section code
snap_bytes:
    push rbp
    mov rbp, rsp
    lea rax, [last_bytes]
    mov rax, [rax]
    jmp .Lret29
.Lret29:
    mov rsp, rbp
    pop rbp
    ret

section code
snap_object_count:
    push rbp
    mov rbp, rsp
    lea rax, [last_objects]
    mov eax, dword [rax]
    mov eax, eax
    jmp .Lret30
.Lret30:
    mov rsp, rbp
    pop rbp
    ret

section code
snap_slot_count:
    push rbp
    mov rbp, rsp
    mov rax, 16
    mov eax, eax
    jmp .Lret31
.Lret31:
    mov rsp, rbp
    pop rbp
    ret

section code
main:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov rax, 0
    jmp .Lret32
.Lret32:
    mov rsp, rbp
    pop rbp
    ret

section code
_start:
    mov rbp, rsp
    call main
    mov rdi, rax
    mov rax, 0
    syscall

section data
    align 8
.Li0:
    db 69, 82, 69, 66, 83, 78, 65, 80, 5, 0, 0, 0, 0, 0, 0, 0
    db 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    db 0, 0, 0, 0, 0, 0, 0, 0
.Ls0: db 115, 110, 97, 112, 58, 32, 103, 101, 110, 101, 114, 97, 116, 105, 111, 110, 32, 37, 108, 108, 117, 32, 108, 101, 116, 32, 103, 111, 44, 32, 116, 111, 32, 109, 97, 107, 101, 32, 114, 111, 111, 109, 32, 105, 110, 32, 116, 104, 101, 32, 108, 111, 103, 10, 0
.Ls1: db 115, 110, 97, 112, 58, 32, 110, 111, 32, 114, 111, 111, 109, 32, 105, 110, 32, 116, 104, 101, 32, 108, 111, 103, 32, 102, 111, 114, 32, 37, 108, 108, 117, 32, 75, 105, 66, 32, 111, 102, 32, 98, 105, 103, 32, 111, 98, 106, 101, 99, 116, 115, 10, 0
.Ls2: db 115, 110, 97, 112, 58, 32, 116, 104, 101, 32, 108, 111, 103, 32, 119, 111, 117, 108, 100, 32, 110, 111, 116, 32, 116, 97, 107, 101, 32, 37, 108, 108, 117, 32, 98, 121, 116, 101, 115, 10, 0
.Ls3: db 115, 110, 97, 112, 58, 32, 103, 101, 110, 101, 114, 97, 116, 105, 111, 110, 32, 37, 108, 108, 117, 32, 110, 97, 109, 101, 115, 32, 97, 32, 98, 105, 103, 32, 111, 98, 106, 101, 99, 116, 32, 111, 102, 32, 37, 108, 108, 117, 32, 98, 121, 116, 101, 115, 32, 116, 104, 101, 32, 108, 111, 103, 32, 110, 111, 32, 108, 111, 110, 103, 101, 114, 32, 104, 97, 115, 10, 0
.Ls4: db 115, 110, 97, 112, 58, 32, 103, 101, 110, 101, 114, 97, 116, 105, 111, 110, 32, 37, 108, 108, 117, 32, 102, 97, 105, 108, 115, 32, 105, 116, 115, 32, 99, 104, 101, 99, 107, 115, 117, 109, 44, 32, 115, 107, 105, 112, 112, 105, 110, 103, 10, 0

section bss
private have_snapshot
    align 1
have_snapshot:
    res 1
private current_generation
    align 8
current_generation:
    res 8
private last_bytes
    align 8
last_bytes:
    res 8
private last_objects
    align 4
last_objects:
    res 4
private buffer
    align 8
buffer:
    res 8
private buffer_bytes
    align 8
buffer_bytes:
    res 8
private collected
    align 8
collected:
    res 32768
private collected_count
    align 4
collected_count:
    res 4
private big_hash
    align 1
big_hash:
    res 131072
private big_lba
    align 8
big_lba:
    res 32768
private is_big
    align 1
is_big:
    res 4096
private live
    align 8
live:
    res 8
private live_count
    align 4
live_count:
    res 4
private sector.1
    align 1
sector.1:
    res 512
private zero.2
    align 1
zero.2:
    res 512
private header_sector.3
    align 1
header_sector.3:
    res 512
private zero.4
    align 1
zero.4:
    res 512
