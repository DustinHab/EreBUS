; made by the compiler; the source lies beside this
section code

section code
term_out:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    test rax, rax
    je .L1
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17232
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    jmp .L2
.L1:
.L2:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 844
    jmp .Lret1
.Lret1:
    mov rsp, rbp
    pop rbp
    ret

section code
term_total:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17240
    mov rax, [rax]
    jmp .Lret2
.Lret2:
    mov rsp, rbp
    pop rbp
    ret

section code
term_sequence:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17248
    mov rax, [rax]
    jmp .Lret3
.Lret3:
    mov rsp, rbp
    pop rbp
    ret

private t_putc
section code
t_putc:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov rax, 16384
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17232
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L3
    lea rax, [rbp - 24]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17232
    mov rax, [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    xor edx, edx
    div rdi
    pop rdi
    mov [rdi], rax
.L5:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17232
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L7
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 844
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L7
    mov rax, 1
    jmp .L8
.L7:
    mov rax, 0
.L8:
    test rax, rax
    je .L6
    lea rax, [rbp - 24]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L5
.L6:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17232
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L9
    lea rax, [rbp - 24]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L10
.L9:
.L10:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17232
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 844
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 844
    push rax
    pop rdi
    pop rsi
    pop rdx
    call memmove
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17232
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    sub rax, rdi
    mov rdi, r8
    mov [rdi], rax
    jmp .L4
.L3:
.L4:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 844
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17232
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 9]
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17240
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
.Lret4:
    mov rsp, rbp
    pop rbp
    ret

private t_puts
section code
t_puts:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
.L11:
    lea rax, [rbp - 16]
    mov rax, [rax]
    movsx rax, byte [rax]
    test rax, rax
    je .L12
    lea rax, [rbp - 16]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    movsx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
    jmp .L11
.L12:
.Lret5:
    mov rsp, rbp
    pop rbp
    ret

private t_dec
section code
t_dec:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 44]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
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
    je .L13
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 44]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 48
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    jmp .L14
.L13:
.L14:
.L15:
    lea rax, [rbp - 16]
    mov rax, [rax]
    test rax, rax
    je .L16
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 44]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 48
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    xor edx, edx
    div rdi
    mov rax, rdx
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, al
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 16]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    xor edx, edx
    div rdi
    mov rdi, r8
    mov [rdi], rax
    jmp .L15
.L16:
.L17:
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    test rax, rax
    je .L18
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 44]
    mov rdi, rax
    mov eax, dword [rax]
    add rax, -1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
    jmp .L17
.L18:
.Lret6:
    mov rsp, rbp
    pop rbp
    ret

private t_end
section code
t_end:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov rax, 10
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17248
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
.Lret7:
    mov rsp, rbp
    pop rbp
    ret

private t_say
section code
t_say:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call t_end
.Lret8:
    mov rsp, rbp
    pop rbp
    ret

private focus
section code
focus:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    jmp .Lret9
.Lret9:
    mov rsp, rbp
    pop rbp
    ret

private focus_rights
section code
focus_rights:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 136
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov eax, dword [rax]
    mov eax, eax
    jmp .Lret10
.Lret10:
    mov rsp, rbp
    pop rbp
    ret

private session_begin
section code
session_begin:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov rax, 17664
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call memset
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [troot]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_retain
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
    push rax
    mov rax, 0
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [troot]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 136
    push rax
    mov rax, 0
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [troot_rights]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 16]
    push rax
    lea rax, [.Ls0]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 20]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L19:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L20
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 200
    push rax
    mov rax, 0
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
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
    jmp .L19
.L20:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 200
    push rax
    mov rax, 0
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
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
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    push rax
    mov rax, 1
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [.Ls1]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret11:
    mov rsp, rbp
    pop rbp
    ret

section code
term_init:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov dword [rbp - 12], esi
    lea rax, [rbp - 8]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L21
    jmp .Lret12
    jmp .L22
.L21:
.L22:
    lea rax, [troot]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [troot_rights]
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [sessions]
    push rax
    mov rax, 0
    push rax
    mov rax, 17664
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    call session_begin
.Lret12:
    mov rsp, rbp
    pop rbp
    ret

section code
term_screen:
    push rbp
    mov rbp, rsp
    lea rax, [sessions]
    push rax
    mov rax, 0
    push rax
    mov rax, 17664
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    jmp .Lret13
.Lret13:
    mov rsp, rbp
    pop rbp
    ret

section code
term_open:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    lea rax, [troot]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L23
    mov rax, 0
    jmp .Lret14
    jmp .L24
.L23:
.L24:
    lea rax, [rbp - 4]
    push rax
    mov rax, 1
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L25:
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L27
    lea rax, [sessions]
    push rax
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    mov rax, 17664
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L28
    lea rax, [sessions]
    push rax
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    mov rax, 17664
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    call session_begin
    lea rax, [sessions]
    push rax
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    mov rax, 17664
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    jmp .Lret14
    jmp .L29
.L28:
.L29:
.L26:
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
    jmp .L25
.L27:
    mov rax, 0
    jmp .Lret14
.Lret14:
    mov rsp, rbp
    pop rbp
    ret

section code
term_close:
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
    jne .L34
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [sessions]
    push rax
    mov rax, 0
    push rax
    mov rax, 17664
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L34
    mov rax, 0
    jmp .L35
.L34:
    mov rax, 1
.L35:
    test rax, rax
    jne .L32
    lea rax, [rbp - 8]
    mov rax, [rax]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L32
    mov rax, 0
    jmp .L33
.L32:
    mov rax, 1
.L33:
    test rax, rax
    je .L30
    jmp .Lret15
    jmp .L31
.L30:
.L31:
.L36:
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L37
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, -1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
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
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
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
    jmp .L36
.L37:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.Lret15:
    mov rsp, rbp
    pop rbp
    ret

private kind_word
section code
kind_word:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov dword [rbp - 4], edi
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    mov rcx, 8
    cmp rax, rcx
    je .L42
    mov rcx, 7
    cmp rax, rcx
    je .L41
    mov rcx, 4
    cmp rax, rcx
    je .L40
    mov rcx, 2
    cmp rax, rcx
    je .L39
    mov rcx, 3
    cmp rax, rcx
    je .L38
    jmp .L43
.L38:
    lea rax, [.Ls2]
    jmp .Lret16
.L39:
    lea rax, [.Ls3]
    jmp .Lret16
.L40:
    lea rax, [.Ls4]
    jmp .Lret16
.L41:
    lea rax, [.Ls5]
    jmp .Lret16
.L42:
    lea rax, [.Ls6]
    jmp .Lret16
.L43:
    lea rax, [.Ls7]
    jmp .Lret16
.L44:
.Lret16:
    mov rsp, rbp
    pop rbp
    ret

private rights_word
section code
rights_word:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov dword [rbp - 4], edi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    je .L45
    mov rax, 114
    jmp .L46
.L45:
    mov rax, 45
.L46:
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    je .L47
    mov rax, 119
    jmp .L48
.L47:
    mov rax, 45
.L48:
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 4]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    je .L49
    mov rax, 103
    jmp .L50
.L49:
    mov rax, 45
.L50:
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.Lret17:
    mov rsp, rbp
    pop rbp
    ret

private shown_name
section code
shown_name:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 24]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_slot_name
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    test rax, rax
    je .L53
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L53
    mov rax, 1
    jmp .L54
.L53:
    mov rax, 0
.L54:
    test rax, rax
    je .L51
    lea rax, [rbp - 24]
    mov rax, [rax]
    jmp .Lret18
    jmp .L52
.L51:
.L52:
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    test rax, rax
    je .L55
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_name
    jmp .L56
.L55:
    mov rax, 0
.L56:
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    test rax, rax
    je .L59
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L59
    mov rax, 1
    jmp .L60
.L59:
    mov rax, 0
.L60:
    test rax, rax
    je .L57
    lea rax, [rbp - 40]
    mov rax, [rax]
    jmp .L58
.L57:
    lea rax, [.Ls8]
.L58:
    jmp .Lret18
.Lret18:
    mov rsp, rbp
    pop rbp
    ret

private text_len
section code
text_len:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 24]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L61:
    lea rax, [rbp - 24]
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
    je .L63
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    test rax, rax
    je .L63
    mov rax, 1
    jmp .L64
.L63:
    mov rax, 0
.L64:
    test rax, rax
    je .L62
    lea rax, [rbp - 24]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L61
.L62:
    lea rax, [rbp - 24]
    mov rax, [rax]
    jmp .Lret19
.Lret19:
    mov rsp, rbp
    pop rbp
    ret

private low
section code
low:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov byte [rbp - 1], dil
    mov rax, 65
    push rax
    lea rax, [rbp - 1]
    movsx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L67
    lea rax, [rbp - 1]
    movsx rax, byte [rax]
    push rax
    mov rax, 90
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L67
    mov rax, 1
    jmp .L68
.L67:
    mov rax, 0
.L68:
    test rax, rax
    je .L65
    lea rax, [rbp - 1]
    movsx rax, byte [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, al
    jmp .L66
.L65:
    lea rax, [rbp - 1]
    movsx rax, byte [rax]
.L66:
    movsx rax, al
    jmp .Lret20
.Lret20:
    mov rsp, rbp
    pop rbp
    ret

private word_starts
section code
word_starts:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    lea rax, [rbp - 28]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L69:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L71
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L71
    mov rax, 1
    jmp .L72
.L71:
    mov rax, 0
.L72:
    test rax, rax
    je .L70
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
    jmp .L69
.L70:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L73
    mov rax, 0
    movzx rax, al
    jmp .Lret21
    jmp .L74
.L73:
.L74:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L77
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L77
    mov rax, 1
    jmp .L78
.L77:
    mov rax, 0
.L78:
    test rax, rax
    je .L75
    mov rax, 0
    movzx rax, al
    jmp .Lret21
    jmp .L76
.L75:
.L76:
.L79:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L80
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
    jmp .L79
.L80:
    lea rax, [rbp - 24]
    mov rax, [rax]
    test rax, rax
    je .L81
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    jmp .L82
.L81:
.L82:
    mov rax, 1
    movzx rax, al
    jmp .Lret21
.Lret21:
    mov rsp, rbp
    pop rbp
    ret

private slot_named
section code
slot_named:
    push rbp
    mov rbp, rsp
    sub rsp, 80
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 24]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_slots
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 33]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 40]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L83:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L85
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L88
    mov rax, 57
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L88
    mov rax, 0
    jmp .L89
.L88:
    mov rax, 1
.L89:
    test rax, rax
    je .L86
    lea rax, [rbp - 33]
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L85
    jmp .L87
.L86:
.L87:
.L84:
    lea rax, [rbp - 40]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L83
.L85:
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    test rax, rax
    je .L90
    lea rax, [rbp - 48]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 52]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L92:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L94
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
.L93:
    lea rax, [rbp - 52]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L92
.L94:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L97
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    test rax, rax
    je .L97
    mov rax, 1
    jmp .L98
.L97:
    mov rax, 0
.L98:
    test rax, rax
    je .L95
    lea rax, [rbp - 48]
    mov rax, [rax]
    jmp .Lret22
    jmp .L96
.L95:
.L96:
    mov rax, 1
    neg rax
    jmp .Lret22
    jmp .L91
.L90:
.L91:
    lea rax, [rbp - 64]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L99:
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L101
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L102
    jmp .L100
    jmp .L103
.L102:
.L103:
    lea rax, [rbp - 72]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call shown_name
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 76]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L104:
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L108
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L108
    mov rax, 1
    jmp .L109
.L108:
    mov rax, 0
.L109:
    test rax, rax
    je .L106
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    pop rdi
    call low
    movsx rax, al
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    pop rdi
    call low
    movsx rax, al
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L106
    mov rax, 1
    jmp .L107
.L106:
    mov rax, 0
.L107:
    test rax, rax
    je .L105
    lea rax, [rbp - 76]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L104
.L105:
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L112
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L112
    mov rax, 1
    jmp .L113
.L112:
    mov rax, 0
.L113:
    test rax, rax
    je .L110
    lea rax, [rbp - 64]
    mov rax, [rax]
    jmp .Lret22
    jmp .L111
.L110:
.L111:
.L100:
    lea rax, [rbp - 64]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L99
.L101:
    mov rax, 1
    neg rax
    jmp .Lret22
.Lret22:
    mov rsp, rbp
    pop rbp
    ret

private resolve
section code
resolve:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L114
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus_rights
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 16
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 200
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 24
    push rax
    mov rax, 1
    neg rax
    pop rdi
    mov [rdi], rax
    mov rax, 1
    movzx rax, al
    jmp .Lret23
    jmp .L115
.L114:
.L115:
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call slot_named
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L116
    lea rax, [.Ls9]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls10]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    mov rax, 0
    movzx rax, al
    jmp .Lret23
    jmp .L117
.L116:
.L117:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus_rights
    mov eax, eax
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    push rax
    pop rdi
    pop rsi
    call obj_slot_rights
    mov eax, eax
    mov rdi, rax
    pop rax
    and rax, rdi
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 16
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    push rax
    pop rdi
    pop rsi
    call shown_name
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 24
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L118
    lea rax, [.Ls11]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    mov rax, 0
    movzx rax, al
    jmp .Lret23
    jmp .L119
.L118:
.L119:
    mov rax, 1
    movzx rax, al
    jmp .Lret23
.Lret23:
    mov rsp, rbp
    pop rbp
    ret

private split_at
section code
split_at:
    push rbp
    mov rbp, rsp
    sub rsp, 80
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    mov [rbp - 32], rcx
    lea rax, [rbp - 40]
    push rax
    mov rax, 1
    neg rax
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 44]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L120:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L121
    lea rax, [rbp - 44]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L120
.L121:
    lea rax, [rbp - 48]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L122:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L124
    lea rax, [rbp - 52]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L125:
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L127
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov eax, eax
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L127
    mov rax, 1
    jmp .L128
.L127:
    mov rax, 0
.L128:
    test rax, rax
    je .L126
    lea rax, [rbp - 52]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L125
.L126:
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L129
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 48]
    mov eax, dword [rax]
    pop rdi
    mov [rdi], rax
    jmp .L130
.L129:
.L130:
.L123:
    lea rax, [rbp - 48]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L122
.L124:
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L131
    mov rax, 0
    movzx rax, al
    jmp .Lret24
    jmp .L132
.L131:
.L132:
    lea rax, [rbp - 56]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 64]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L133:
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L136
    lea rax, [rbp - 56]
    mov eax, dword [rax]
    push rax
    mov rax, 200
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
    je .L136
    mov rax, 1
    jmp .L137
.L136:
    mov rax, 0
.L137:
    test rax, rax
    je .L135
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L134:
    lea rax, [rbp - 64]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L133
.L135:
.L138:
    lea rax, [rbp - 56]
    mov eax, dword [rax]
    test rax, rax
    je .L140
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L140
    mov rax, 1
    jmp .L141
.L140:
    mov rax, 0
.L141:
    test rax, rax
    je .L139
    lea rax, [rbp - 56]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, -1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L138
.L139:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 56]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 68]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    mov eax, eax
    push rax
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L142:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L145
    lea rax, [rbp - 56]
    mov eax, dword [rax]
    push rax
    mov rax, 200
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
    je .L145
    mov rax, 1
    jmp .L146
.L145:
    mov rax, 0
.L146:
    test rax, rax
    je .L144
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L143:
    lea rax, [rbp - 68]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L142
.L144:
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L147
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L147
    mov rax, 1
    jmp .L148
.L147:
    mov rax, 0
.L148:
    movzx rax, al
    jmp .Lret24
.Lret24:
    mov rsp, rbp
    pop rbp
    ret

private lay_here
section code
lay_here:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov dword [rbp - 20], edx
    mov [rbp - 32], rcx
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_slots
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 56]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 64]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L149:
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L151
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L152
    lea rax, [rbp - 56]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    jmp .L151
    jmp .L153
.L152:
.L153:
.L150:
    lea rax, [rbp - 64]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L149
.L151:
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L156
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_grow_slots
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L156
    mov rax, 1
    jmp .L157
.L156:
    mov rax, 0
.L157:
    test rax, rax
    je .L154
    mov rax, 1
    neg rax
    jmp .Lret25
    jmp .L155
.L154:
.L155:
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call obj_set_slot
    movzx rax, al
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_set_slot_name
    movzx rax, al
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_touch
    lea rax, [rbp - 56]
    mov rax, [rax]
    jmp .Lret25
.Lret25:
    mov rsp, rbp
    pop rbp
    ret

private describe
section code
describe:
    push rbp
    mov rbp, rsp
    sub rsp, 112
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov dword [rbp - 20], edx
    mov [rbp - 32], rcx
    lea rax, [rbp - 36]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    pop rdi
    pop rsi
    call rights_word
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L158
    lea rax, [rbp - 32]
    mov rax, [rax]
    jmp .L159
.L158:
    lea rax, [.Ls12]
.L159:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls13]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    pop rdi
    call kind_word
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls14]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 36]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls15]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L160
    lea rax, [rbp - 48]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 56]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_slots
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 64]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L162:
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L164
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    test rax, rax
    je .L165
    lea rax, [rbp - 48]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L166
.L165:
.L166:
.L163:
    lea rax, [rbp - 64]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L162
.L164:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L167
    lea rax, [.Ls16]
    jmp .L168
.L167:
    lea rax, [.Ls17]
.L168:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    jmp .L161
.L160:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L169
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    push rax
    pop rdi
    pop rsi
    call text_len
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls18]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    jmp .L170
.L169:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L171
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call proc_is_running
    movzx rax, al
    test rax, rax
    je .L173
    lea rax, [.Ls19]
    jmp .L174
.L173:
    lea rax, [.Ls20]
.L174:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    jmp .L172
.L171:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls21]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
.L172:
.L170:
.L161:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call t_end
    lea rax, [rbp - 72]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_slots
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 80]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L175:
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 72]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L177
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 88]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L178
    jmp .L176
    jmp .L179
.L178:
.L179:
    lea rax, [rbp - 92]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_slot_rights
    mov eax, eax
    mov rdi, rax
    pop rax
    and rax, rdi
    mov eax, eax
    push rax
    pop rdi
    pop rsi
    call rights_word
    lea rax, [.Ls22]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls23]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 92]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls24]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 104]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call shown_name
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L180
    lea rax, [rbp - 104]
    mov rax, [rax]
    jmp .L181
.L180:
    lea rax, [.Ls25]
.L181:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls26]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 88]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    pop rdi
    call kind_word
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call t_end
.L176:
    lea rax, [rbp - 80]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L175
.L177:
.Lret26:
    mov rsp, rbp
    pop rbp
    ret

private cmd_look
section code
cmd_look:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L182
    jmp .Lret27
    jmp .L183
.L182:
.L183:
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call describe
.Lret27:
    mov rsp, rbp
    pop rbp
    ret

private cmd_where
section code
cmd_where:
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
.L184:
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L186
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    test rax, rax
    je .L187
    lea rax, [.Ls27]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    jmp .L188
.L187:
.L188:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 200
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    mov rax, 40
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
    pop rdi
    pop rsi
    call t_puts
.L185:
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
    jmp .L184
.L186:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call t_end
.Lret28:
    mov rsp, rbp
    pop rbp
    ret

private cmd_go
section code
cmd_go:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L189
    lea rax, [.Ls28]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret29
    jmp .L190
.L189:
.L190:
    mov rax, 16
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L191
    lea rax, [.Ls29]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret29
    jmp .L192
.L191:
.L192:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L193
    jmp .Lret29
    jmp .L194
.L193:
.L194:
    lea rax, [rbp - 48]
    add rax, 24
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L195
    jmp .Lret29
    jmp .L196
.L195:
.L196:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_retain
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
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
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 136
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 52]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L197:
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L199
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 40
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
    je .L199
    mov rax, 1
    jmp .L200
.L199:
    mov rax, 0
.L200:
    test rax, rax
    je .L198
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 200
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov eax, dword [rax]
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 52]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L197
.L198:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 200
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov eax, dword [rax]
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    lea rax, [.Ls30]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_look
.Lret29:
    mov rsp, rbp
    pop rbp
    ret

private cmd_back
section code
cmd_back:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L201
    lea rax, [.Ls31]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret30
    jmp .L202
.L201:
.L202:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, -1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
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
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
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
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call cmd_where
.Lret30:
    mov rsp, rbp
    pop rbp
    ret

private cmd_home
section code
cmd_home:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
.L203:
    mov rax, 1
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L204
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, -1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
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
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
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
    jmp .L203
.L204:
    lea rax, [.Ls32]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret31:
    mov rsp, rbp
    pop rbp
    ret

private cmd_read
section code
cmd_read:
    push rbp
    mov rbp, rsp
    sub rsp, 160
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L205
    jmp .Lret32
    jmp .L206
.L205:
.L206:
    lea rax, [rbp - 56]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 60]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 60]
    mov eax, dword [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L207
    lea rax, [rbp - 72]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call text_len
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 88]
    push rax
    mov rax, 2000
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L209
    mov rax, 2000
    jmp .L210
.L209:
    lea rax, [rbp - 80]
    mov rax, [rax]
.L210:
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 96]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L211:
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 88]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L213
    mov rax, 32
    push rax
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    jne .L216
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L216
    mov rax, 0
    jmp .L217
.L216:
    mov rax, 1
.L217:
    test rax, rax
    je .L214
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    movsx rax, al
    jmp .L215
.L214:
    mov rax, 32
.L215:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
.L212:
    lea rax, [rbp - 96]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L211
.L213:
    lea rax, [rbp - 88]
    mov rax, [rax]
    test rax, rax
    je .L220
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 844
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17232
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L220
    mov rax, 1
    jmp .L221
.L220:
    mov rax, 0
.L221:
    test rax, rax
    je .L218
    mov rax, 10
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
    jmp .L219
.L218:
.L219:
    lea rax, [rbp - 88]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L222
    lea rax, [.Ls33]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 88]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls34]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .L223
.L222:
.L223:
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L224
    lea rax, [.Ls35]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .L225
.L224:
.L225:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17248
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .Lret32
    jmp .L208
.L207:
.L208:
    lea rax, [rbp - 60]
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L226
    lea rax, [rbp - 104]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 112]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls36]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [rbp - 120]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L228:
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L231
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L231
    mov rax, 1
    jmp .L232
.L231:
    mov rax, 0
.L232:
    test rax, rax
    je .L230
    lea rax, [.Ls38]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 128]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L233:
    lea rax, [rbp - 128]
    mov rax, [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L236
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 128]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L236
    mov rax, 1
    jmp .L237
.L236:
    mov rax, 0
.L237:
    test rax, rax
    je .L235
    lea rax, [rbp - 129]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 128]
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
    lea rax, [hx.1]
    push rax
    lea rax, [rbp - 129]
    movzx rax, byte [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shr rax, cl
    mov eax, eax
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
    lea rax, [hx.1]
    push rax
    lea rax, [rbp - 129]
    movzx rax, byte [rax]
    push rax
    mov rax, 15
    mov rdi, rax
    pop rax
    and rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
    mov rax, 32
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
.L234:
    lea rax, [rbp - 128]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L233
.L235:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call t_end
.L229:
    lea rax, [rbp - 120]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L228
.L230:
    jmp .Lret32
    jmp .L227
.L226:
.L227:
    lea rax, [rbp - 60]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L238
    lea rax, [rbp - 144]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 148]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 152]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 156]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L240:
    lea rax, [rbp - 156]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L242
    lea rax, [rbp - 148]
    push rax
    lea rax, [rbp - 144]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 156]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    mov eax, eax
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
    mov rcx, rdi
    shl rax, cl
    mov eax, eax
    mov rdi, rax
    pop rax
    mov r8, rax
    mov eax, dword [rax]
    or rax, rdi
    mov rdi, r8
    mov eax, eax
    mov dword [rdi], eax
.L241:
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
    jmp .L240
.L242:
    lea rax, [rbp - 160]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L243:
    lea rax, [rbp - 160]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L245
    lea rax, [rbp - 152]
    push rax
    lea rax, [rbp - 144]
    mov rax, [rax]
    push rax
    mov rax, 4
    push rax
    lea rax, [rbp - 160]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    mov eax, eax
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
    mov rcx, rdi
    shl rax, cl
    mov eax, eax
    mov rdi, rax
    pop rax
    mov r8, rax
    mov eax, dword [rax]
    or rax, rdi
    mov rdi, r8
    mov eax, eax
    mov dword [rdi], eax
.L244:
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
    jmp .L243
.L245:
    lea rax, [.Ls39]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 148]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls40]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 152]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls41]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret32
    jmp .L239
.L238:
.L239:
    lea rax, [rbp - 60]
    mov eax, dword [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L246
    lea rax, [.Ls42]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    pop rdi
    call proc_is_running
    movzx rax, al
    test rax, rax
    je .L248
    lea rax, [.Ls43]
    jmp .L249
.L248:
    lea rax, [.Ls44]
.L249:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls45]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret32
    jmp .L247
.L246:
.L247:
    lea rax, [.Ls46]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret32:
    mov rsp, rbp
    pop rbp
    ret

private cmd_write
section code
cmd_write:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 24]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L250
    lea rax, [.Ls47]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret33
    jmp .L251
.L250:
.L251:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus_rights
    mov eax, eax
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L252
    lea rax, [.Ls48]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret33
    jmp .L253
.L252:
.L253:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L254
    lea rax, [.Ls49]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret33
    jmp .L255
.L254:
.L255:
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call text_len
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 56]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L256:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L257
    lea rax, [rbp - 56]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L256
.L257:
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L258
    lea rax, [.Ls50]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret33
    jmp .L259
.L258:
.L259:
    lea rax, [rbp - 48]
    mov rax, [rax]
    test rax, rax
    je .L262
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L262
    mov rax, 1
    jmp .L263
.L262:
    mov rax, 0
.L263:
    test rax, rax
    je .L260
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 10
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L261
.L260:
.L261:
    lea rax, [rbp - 64]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L264:
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L266
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L265:
    lea rax, [rbp - 64]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L264
.L266:
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 10
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_touch
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    call settings_object
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L267
    call settings_apply
    jmp .L268
.L267:
.L268:
    lea rax, [.Ls51]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret33:
    mov rsp, rbp
    pop rbp
    ret

private cmd_make
section code
cmd_make:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls52]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L269
    lea rax, [rbp - 28]
    push rax
    mov rax, 3
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L270
.L269:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls53]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L271
    lea rax, [rbp - 28]
    push rax
    mov rax, 4
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L272
.L271:
    lea rax, [.Ls54]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret34
.L272:
.L270:
    lea rax, [rbp - 24]
    mov rax, [rax]
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L273
    lea rax, [.Ls55]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret34
    jmp .L274
.L273:
.L274:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus_rights
    mov eax, eax
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L275
    lea rax, [.Ls56]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret34
    jmp .L276
.L275:
.L276:
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L277
    mov rax, 4
    jmp .L278
.L277:
    mov rax, 0
.L278:
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L279
    mov rax, 3000
    jmp .L280
.L279:
    mov rax, 0
.L280:
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_create
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L281
    lea rax, [.Ls57]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret34
    jmp .L282
.L281:
.L282:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    mov rax, 1
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call lay_here
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_release
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L283
    lea rax, [.Ls58]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret34
    jmp .L284
.L283:
.L284:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls59]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call t_end
.Lret34:
    mov rsp, rbp
    pop rbp
    ret

private cmd_copy
section code
cmd_copy:
    push rbp
    mov rbp, rsp
    sub rsp, 160
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L285
    lea rax, [.Ls60]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret35
    jmp .L286
.L285:
.L286:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L287
    jmp .Lret35
    jmp .L288
.L287:
.L288:
    lea rax, [rbp - 48]
    add rax, 24
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L289
    jmp .Lret35
    jmp .L290
.L289:
.L290:
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L291
    lea rax, [.Ls61]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret35
    jmp .L292
.L291:
.L292:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus_rights
    mov eax, eax
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L293
    lea rax, [.Ls62]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret35
    jmp .L294
.L293:
.L294:
    lea rax, [rbp - 52]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 64]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L295
    lea rax, [rbp - 72]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_slots
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 64]
    push rax
    lea rax, [rbp - 72]
    mov rax, [rax]
    test rax, rax
    je .L297
    lea rax, [rbp - 72]
    mov rax, [rax]
    jmp .L298
.L297:
    mov rax, 4
.L298:
    push rax
    mov rax, 0
    push rax
    mov rax, 4
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_create
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    test rax, rax
    je .L299
    lea rax, [rbp - 80]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L301:
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 72]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L303
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 88]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L304
    jmp .L302
    jmp .L305
.L304:
.L305:
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_slot_rights
    mov eax, eax
    push rax
    lea rax, [rbp - 88]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call obj_set_slot
    movzx rax, al
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_slot_name
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_set_slot_name
    movzx rax, al
.L302:
    lea rax, [rbp - 80]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L301
.L303:
    jmp .L300
.L299:
.L300:
    jmp .L296
.L295:
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L310
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L310
    mov rax, 0
    jmp .L311
.L310:
    mov rax, 1
.L311:
    test rax, rax
    jne .L308
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L308
    mov rax, 0
    jmp .L309
.L308:
    mov rax, 1
.L309:
    test rax, rax
    je .L306
    lea rax, [rbp - 64]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_create
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    test rax, rax
    je .L314
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    test rax, rax
    je .L314
    mov rax, 1
    jmp .L315
.L314:
    mov rax, 0
.L315:
    test rax, rax
    je .L312
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    push rax
    pop rdi
    pop rsi
    pop rdx
    call memcpy
    jmp .L313
.L312:
.L313:
    jmp .L307
.L306:
    lea rax, [.Ls63]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret35
.L307:
.L296:
    lea rax, [rbp - 64]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L316
    lea rax, [.Ls64]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret35
    jmp .L317
.L316:
.L317:
    lea rax, [rbp - 132]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L318:
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 132]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L320
    lea rax, [rbp - 132]
    mov eax, dword [rax]
    push rax
    mov rax, 19
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L320
    mov rax, 1
    jmp .L321
.L320:
    mov rax, 0
.L321:
    test rax, rax
    je .L319
    lea rax, [rbp - 128]
    push rax
    lea rax, [rbp - 132]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 132]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 132]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L318
.L319:
    lea rax, [rbp - 144]
    push rax
    lea rax, [.Ls65]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 148]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L322:
    lea rax, [rbp - 144]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 148]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L324
    lea rax, [rbp - 128]
    push rax
    lea rax, [rbp - 132]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 144]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 148]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L323:
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
    jmp .L322
.L324:
    lea rax, [rbp - 128]
    push rax
    lea rax, [rbp - 132]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 160]
    push rax
    lea rax, [rbp - 128]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    mov rax, 1
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call lay_here
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_release
    lea rax, [rbp - 160]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L325
    lea rax, [.Ls66]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret35
    jmp .L326
.L325:
.L326:
    lea rax, [rbp - 128]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls67]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret35:
    mov rsp, rbp
    pop rbp
    ret

private cmd_rename
section code
cmd_rename:
    push rbp
    mov rbp, rsp
    sub rsp, 448
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 416]
    push rax
    lea rax, [rbp - 216]
    push rax
    lea rax, [.Ls68]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call split_at
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L327
    lea rax, [.Ls69]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret36
    jmp .L328
.L327:
.L328:
    lea rax, [rbp - 448]
    push rax
    lea rax, [rbp - 216]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L329
    jmp .Lret36
    jmp .L330
.L329:
.L330:
    lea rax, [rbp - 448]
    add rax, 24
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L331
    jmp .Lret36
    jmp .L332
.L331:
.L332:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus_rights
    mov eax, eax
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L333
    lea rax, [.Ls70]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret36
    jmp .L334
.L333:
.L334:
    lea rax, [rbp - 416]
    push rax
    lea rax, [rbp - 448]
    add rax, 24
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_set_slot_name
    movzx rax, al
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    push rax
    pop rdi
    call obj_touch
    lea rax, [rbp - 216]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls71]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 416]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret36:
    mov rsp, rbp
    pop rbp
    ret

private cmd_letgo
section code
cmd_letgo:
    push rbp
    mov rbp, rsp
    sub rsp, 208
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L335
    lea rax, [.Ls72]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret37
    jmp .L336
.L335:
.L336:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L337
    jmp .Lret37
    jmp .L338
.L337:
.L338:
    lea rax, [rbp - 48]
    add rax, 24
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L339
    lea rax, [.Ls73]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret37
    jmp .L340
.L339:
.L340:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus_rights
    mov eax, eax
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L341
    lea rax, [.Ls74]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret37
    jmp .L342
.L341:
.L342:
    lea rax, [rbp - 92]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L343:
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L345
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    push rax
    mov rax, 40
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
    je .L345
    mov rax, 1
    jmp .L346
.L345:
    mov rax, 0
.L346:
    test rax, rax
    je .L344
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 92]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L343
.L344:
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 96]
    push rax
    lea rax, [rbp - 48]
    add rax, 24
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    push rax
    pop rdi
    pop rsi
    call obj_slot_rights
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L347
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    push rax
    pop rdi
    pop rsi
    call proc_revoke
    movzx rax, al
    jmp .L348
.L347:
.L348:
    lea rax, [rbp - 104]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
    push rax
    mov rax, 0
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
    lea rax, [rbp - 112]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 136
    push rax
    mov rax, 0
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    je .L349
    lea rax, [rbp - 120]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L351:
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 104]
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
    je .L353
    lea rax, [rbp - 128]
    push rax
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 136]
    push rax
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_slot_name
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 128]
    mov rax, [rax]
    test rax, rax
    je .L360
    lea rax, [rbp - 136]
    mov rax, [rax]
    test rax, rax
    je .L360
    mov rax, 1
    jmp .L361
.L360:
    mov rax, 0
.L361:
    test rax, rax
    je .L358
    lea rax, [.Ls75]
    push rax
    lea rax, [rbp - 136]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call strcmp
    movsxd rax, eax
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L358
    mov rax, 1
    jmp .L359
.L358:
    mov rax, 0
.L359:
    test rax, rax
    je .L356
    lea rax, [rbp - 128]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L356
    mov rax, 1
    jmp .L357
.L356:
    mov rax, 0
.L357:
    test rax, rax
    je .L354
    lea rax, [rbp - 112]
    push rax
    lea rax, [rbp - 128]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    jmp .L353
    jmp .L355
.L354:
.L355:
.L352:
    lea rax, [rbp - 120]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L351
.L353:
    jmp .L350
.L349:
.L350:
    lea rax, [rbp - 137]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L364
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L364
    mov rax, 0
    jmp .L365
.L364:
    mov rax, 1
.L365:
    test rax, rax
    jne .L362
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 136
    push rax
    mov rax, 0
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L362
    mov rax, 0
    jmp .L363
.L362:
    mov rax, 1
.L363:
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 137]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L368
    lea rax, [rbp - 112]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L368
    mov rax, 1
    jmp .L369
.L368:
    mov rax, 0
.L369:
    test rax, rax
    je .L366
    lea rax, [rbp - 152]
    push rax
    mov rax, 4
    push rax
    mov rax, 0
    push rax
    mov rax, 4
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_create
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 152]
    mov rax, [rax]
    test rax, rax
    je .L370
    lea rax, [.Ls76]
    push rax
    lea rax, [rbp - 152]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_set_name
    lea rax, [rbp - 160]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_slots
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 168]
    push rax
    lea rax, [rbp - 160]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 176]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L372:
    lea rax, [rbp - 176]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 160]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L374
    lea rax, [rbp - 176]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L375
    lea rax, [rbp - 168]
    push rax
    lea rax, [rbp - 176]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    jmp .L374
    jmp .L376
.L375:
.L376:
.L373:
    lea rax, [rbp - 176]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L372
.L374:
    lea rax, [rbp - 168]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 160]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    jne .L379
    lea rax, [rbp - 160]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_grow_slots
    movzx rax, al
    test rax, rax
    jne .L379
    mov rax, 0
    jmp .L380
.L379:
    mov rax, 1
.L380:
    test rax, rax
    je .L377
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    lea rax, [rbp - 152]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 168]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call obj_set_slot
    movzx rax, al
    lea rax, [.Ls77]
    push rax
    lea rax, [rbp - 168]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_set_slot_name
    movzx rax, al
    lea rax, [rbp - 112]
    push rax
    lea rax, [rbp - 152]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    jmp .L378
.L377:
.L378:
    lea rax, [rbp - 152]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_release
    jmp .L371
.L370:
.L371:
    jmp .L367
.L366:
.L367:
    lea rax, [rbp - 137]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L383
    lea rax, [rbp - 112]
    mov rax, [rax]
    test rax, rax
    je .L383
    mov rax, 1
    jmp .L384
.L383:
    mov rax, 0
.L384:
    test rax, rax
    je .L381
    lea rax, [rbp - 184]
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_slots
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 192]
    push rax
    lea rax, [rbp - 184]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 200]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L385:
    lea rax, [rbp - 200]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 184]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L387
    lea rax, [rbp - 200]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L388
    lea rax, [rbp - 192]
    push rax
    lea rax, [rbp - 200]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    jmp .L387
    jmp .L389
.L388:
.L389:
.L386:
    lea rax, [rbp - 200]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L385
.L387:
    lea rax, [rbp - 192]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 184]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L392
    lea rax, [rbp - 184]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_grow_slots
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L392
    mov rax, 1
    jmp .L393
.L392:
    mov rax, 0
.L393:
    test rax, rax
    je .L390
    lea rax, [rbp - 137]
    push rax
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L391
.L390:
    lea rax, [rbp - 96]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 192]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call obj_set_slot
    movzx rax, al
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 192]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_set_slot_name
    movzx rax, al
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_touch
.L391:
    jmp .L382
.L381:
.L382:
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 48]
    add rax, 24
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call obj_set_slot
    movzx rax, al
    mov rax, 0
    push rax
    lea rax, [rbp - 48]
    add rax, 24
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_set_slot_name
    movzx rax, al
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    push rax
    pop rdi
    call obj_touch
    lea rax, [rbp - 137]
    movzx rax, byte [rax]
    test rax, rax
    je .L394
    lea rax, [.Ls78]
    jmp .L395
.L394:
    lea rax, [.Ls79]
.L395:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret37:
    mov rsp, rbp
    pop rbp
    ret

private gnu_named
section code
gnu_named:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 12]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call strlen
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    mov rax, 2
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L398
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 46
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L398
    mov rax, 1
    jmp .L399
.L398:
    mov rax, 0
.L399:
    test rax, rax
    je .L396
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 83
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L396
    mov rax, 1
    jmp .L397
.L396:
    mov rax, 0
.L397:
    movzx rax, al
    jmp .Lret38
.Lret38:
    mov rsp, rbp
    pop rbp
    ret

private lay_made
section code
lay_made:
    push rbp
    mov rbp, rsp
    sub rsp, 256
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    mov [rbp - 32], rcx
    mov dword [rbp - 36], r8d
    lea rax, [rbp - 48]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 2
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_create
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L400
    lea rax, [.Ls80]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret39
    jmp .L401
.L400:
.L401:
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    push rax
    pop rdi
    pop rsi
    pop rdx
    call memcpy
    lea rax, [rbp - 100]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L402:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L404
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    push rax
    mov rax, 19
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L404
    mov rax, 1
    jmp .L405
.L404:
    mov rax, 0
.L405:
    test rax, rax
    je .L403
    lea rax, [rbp - 96]
    push rax
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 100]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L402
.L403:
    lea rax, [rbp - 112]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L406
    lea rax, [.Ls81]
    jmp .L407
.L406:
    lea rax, [.Ls82]
.L407:
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 116]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L408:
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 116]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L410
    lea rax, [rbp - 96]
    push rax
    lea rax, [rbp - 100]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 116]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L409:
    lea rax, [rbp - 116]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L408
.L410:
    lea rax, [rbp - 96]
    push rax
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 128]
    push rax
    lea rax, [rbp - 96]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    mov rax, 1
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call lay_here
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_release
    lea rax, [rbp - 128]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L411
    lea rax, [.Ls83]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret39
    jmp .L412
.L411:
.L412:
    lea rax, [rbp - 96]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls84]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L413
    lea rax, [.Ls85]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret39
    jmp .L414
.L413:
.L414:
    mov rax, 120
    push rax
    lea rax, [rbp - 248]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call ld_object_wants
    mov eax, eax
    lea rax, [.Ls86]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls87]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 248]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls88]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret39:
    mov rsp, rbp
    pop rbp
    ret

private cmd_assemble
section code
cmd_assemble:
    push rbp
    mov rbp, rsp
    sub rsp, 208
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L415
    lea rax, [.Ls89]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret40
    jmp .L416
.L415:
.L416:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L417
    jmp .Lret40
    jmp .L418
.L417:
.L418:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L419
    lea rax, [.Ls90]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret40
    jmp .L420
.L419:
.L420:
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L421
    lea rax, [.Ls91]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret40
    jmp .L422
.L421:
.L422:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus_rights
    mov eax, eax
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L423
    lea rax, [.Ls92]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret40
    jmp .L424
.L423:
.L424:
    lea rax, [rbp - 56]
    push rax
    call lang_out_buffer
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L425
    lea rax, [.Ls93]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret40
    jmp .L426
.L425:
.L426:
    lea rax, [rbp - 180]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 192]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 200]
    push rax
    mov rax, 120
    push rax
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 180]
    push rax
    mov rax, 8
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    pop rdi
    call gnu_named
    movzx rax, al
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 192]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call text_len
    push rax
    lea rax, [rbp - 192]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call lang_build_text
    add rsp, 16
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 200]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L427
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret40
    jmp .L428
.L427:
.L428:
    lea rax, [rbp - 180]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 200]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call lay_made
.Lret40:
    mov rsp, rbp
    pop rbp
    ret

private find_beside
section code
find_beside:
    push rbp
    mov rbp, rsp
    sub rsp, 96
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    mov [rbp - 32], rcx
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_slots
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 56]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L429:
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L431
    lea rax, [rbp - 64]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L434
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L434
    mov rax, 0
    jmp .L435
.L434:
    mov rax, 1
.L435:
    test rax, rax
    je .L432
    jmp .L430
    jmp .L433
.L432:
.L433:
    lea rax, [rbp - 72]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call shown_name
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
.L436:
    lea rax, [rbp - 88]
    mov rax, [rax]
    movsx rax, byte [rax]
    test rax, rax
    je .L438
    lea rax, [rbp - 88]
    mov rax, [rax]
    movsx rax, byte [rax]
    push rax
    mov rax, 47
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L439
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 88]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    jmp .L440
.L439:
.L440:
.L437:
    lea rax, [rbp - 88]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L436
.L438:
    lea rax, [rbp - 92]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L441:
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L445
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L445
    mov rax, 1
    jmp .L446
.L445:
    mov rax, 0
.L446:
    test rax, rax
    je .L443
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    pop rdi
    call low
    movsx rax, al
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    pop rdi
    call low
    movsx rax, al
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L443
    mov rax, 1
    jmp .L444
.L443:
    mov rax, 0
.L444:
    test rax, rax
    je .L442
    lea rax, [rbp - 92]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L441
.L442:
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    jne .L449
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    jne .L449
    mov rax, 0
    jmp .L450
.L449:
    mov rax, 1
.L450:
    test rax, rax
    je .L447
    jmp .L430
    jmp .L448
.L447:
.L448:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call text_len
    pop rdi
    mov [rdi], rax
    mov rax, 1
    movzx rax, al
    jmp .Lret41
.L430:
    lea rax, [rbp - 56]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L429
.L431:
    mov rax, 0
    movzx rax, al
    jmp .Lret41
.Lret41:
    mov rsp, rbp
    pop rbp
    ret

private cmd_compile
section code
cmd_compile:
    push rbp
    mov rbp, rsp
    sub rsp, 336
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L451
    lea rax, [.Ls94]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret42
    jmp .L452
.L451:
.L452:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L453
    jmp .Lret42
    jmp .L454
.L453:
.L454:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L455
    lea rax, [.Ls95]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret42
    jmp .L456
.L455:
.L456:
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L457
    lea rax, [.Ls96]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret42
    jmp .L458
.L457:
.L458:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus_rights
    mov eax, eax
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L459
    lea rax, [.Ls97]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret42
    jmp .L460
.L459:
.L460:
    lea rax, [rbp - 92]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L461:
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L463
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    push rax
    mov rax, 19
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L463
    mov rax, 1
    jmp .L464
.L463:
    mov rax, 0
.L464:
    test rax, rax
    je .L462
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 92]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L461
.L462:
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 104]
    push rax
    call lang_text_buffer
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 112]
    push rax
    call lang_out_buffer
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L467
    lea rax, [rbp - 112]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L467
    mov rax, 0
    jmp .L468
.L467:
    mov rax, 1
.L468:
    test rax, rax
    je .L465
    lea rax, [.Ls98]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret42
    jmp .L466
.L465:
.L466:
    lea rax, [rbp - 240]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 248]
    push rax
    mov rax, 120
    push rax
    lea rax, [rbp - 232]
    push rax
    mov rax, 4
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus
    push rax
    lea rax, [find_beside]
    push rax
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 240]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call text_len
    push rax
    lea rax, [rbp - 240]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call cc_compile
    add rsp, 24
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 248]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L469
    lea rax, [rbp - 232]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret42
    jmp .L470
.L469:
.L470:
    lea rax, [rbp - 256]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 248]
    mov rax, [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 3
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_create
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 256]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L471
    lea rax, [.Ls99]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret42
    jmp .L472
.L471:
.L472:
    lea rax, [rbp - 248]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 256]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    push rax
    pop rdi
    pop rsi
    pop rdx
    call memcpy
    lea rax, [rbp - 92]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L473:
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L474
    lea rax, [rbp - 296]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 92]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L473
.L474:
    lea rax, [rbp - 304]
    push rax
    lea rax, [.Ls100]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 308]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L475:
    lea rax, [rbp - 304]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 308]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L477
    lea rax, [rbp - 296]
    push rax
    lea rax, [rbp - 92]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 304]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 308]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L476:
    lea rax, [rbp - 308]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L475
.L477:
    lea rax, [rbp - 296]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 320]
    push rax
    lea rax, [rbp - 296]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    mov rax, 1
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    lea rax, [rbp - 256]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call lay_here
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 256]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_release
    lea rax, [rbp - 320]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L478
    lea rax, [.Ls101]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret42
    jmp .L479
.L478:
.L479:
    lea rax, [rbp - 296]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls102]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 248]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls103]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [rbp - 324]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 336]
    push rax
    mov rax, 120
    push rax
    lea rax, [rbp - 232]
    push rax
    lea rax, [rbp - 324]
    push rax
    mov rax, 8
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 248]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call lang_build_text
    add rsp, 16
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 336]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L480
    lea rax, [.Ls104]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [rbp - 232]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret42
    jmp .L481
.L480:
.L481:
    lea rax, [rbp - 324]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 336]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call lay_made
.Lret42:
    mov rsp, rbp
    pop rbp
    ret

private lay_into
section code
lay_into:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_slots
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
.L482:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L484
    lea rax, [rbp - 56]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_slot_name
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    test rax, rax
    je .L489
    lea rax, [rbp - 56]
    mov rax, [rax]
    test rax, rax
    je .L489
    mov rax, 1
    jmp .L490
.L489:
    mov rax, 0
.L490:
    test rax, rax
    je .L487
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call strcmp
    movsxd rax, eax
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L487
    mov rax, 1
    jmp .L488
.L487:
    mov rax, 0
.L488:
    test rax, rax
    je .L485
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    jmp .L484
    jmp .L486
.L485:
.L486:
.L483:
    lea rax, [rbp - 48]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L482
.L484:
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L491
    lea rax, [rbp - 64]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L493:
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L495
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L496
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    jmp .L495
    jmp .L497
.L496:
.L497:
.L494:
    lea rax, [rbp - 64]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L493
.L495:
    jmp .L492
.L491:
.L492:
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L500
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_grow_slots
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L500
    mov rax, 1
    jmp .L501
.L500:
    mov rax, 0
.L501:
    test rax, rax
    je .L498
    mov rax, 0
    movzx rax, al
    jmp .Lret43
    jmp .L499
.L498:
.L499:
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    mov rax, 1
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call obj_set_slot
    movzx rax, al
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_set_slot_name
    movzx rax, al
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_touch
    mov rax, 1
    movzx rax, al
    jmp .Lret43
.Lret43:
    mov rsp, rbp
    pop rbp
    ret

private link_units
section code
link_units:
    push rbp
    mov rbp, rsp
    sub rsp, 320
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    mov dword [rbp - 28], ecx
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L502
    lea rax, [.Ls105]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret44
    jmp .L503
.L502:
.L503:
    lea rax, [rbp - 29]
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 36]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L504:
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L506
    lea rax, [.Ls106]
    push rax
    lea rax, [units]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 8
    mov rax, [rax]
    push rax
    lea rax, [units]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    mov rax, 24
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
    call ld_object_defines
    movzx rax, al
    test rax, rax
    je .L507
    lea rax, [rbp - 29]
    push rax
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L508
.L507:
.L508:
.L505:
    lea rax, [rbp - 36]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L504
.L506:
    lea rax, [rbp - 48]
    push rax
    call lang_out_buffer
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L509
    lea rax, [.Ls107]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret44
    jmp .L510
.L509:
.L510:
    lea rax, [rbp - 216]
    push rax
    mov rax, 160
    push rax
    lea rax, [rbp - 208]
    push rax
    mov rax, 8
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 29]
    movzx rax, byte [rax]
    test rax, rax
    je .L511
    mov rax, 1
    jmp .L512
.L511:
    mov rax, 0
.L512:
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [units]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call ld_link
    add rsp, 8
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 216]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L513
    lea rax, [rbp - 208]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret44
    jmp .L514
.L513:
.L514:
    lea rax, [rbp - 29]
    movzx rax, byte [rax]
    test rax, rax
    je .L515
    lea rax, [rbp - 272]
    push rax
    lea rax, [.Ls108]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 276]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L517:
    lea rax, [rbp - 272]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 276]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L518
    lea rax, [rbp - 264]
    push rax
    lea rax, [rbp - 276]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 272]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 276]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 276]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L517
.L518:
    lea rax, [rbp - 264]
    push rax
    lea rax, [rbp - 276]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    jmp .L516
.L515:
    lea rax, [rbp - 280]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L519:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 280]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L521
    lea rax, [rbp - 280]
    mov eax, dword [rax]
    push rax
    mov rax, 19
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L521
    mov rax, 1
    jmp .L522
.L521:
    mov rax, 0
.L522:
    test rax, rax
    je .L520
    lea rax, [rbp - 264]
    push rax
    lea rax, [rbp - 280]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 280]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 280]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L519
.L520:
    lea rax, [rbp - 288]
    push rax
    lea rax, [.Ls109]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 292]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L523:
    lea rax, [rbp - 288]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 292]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L525
    lea rax, [rbp - 264]
    push rax
    lea rax, [rbp - 280]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 288]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 292]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L524:
    lea rax, [rbp - 292]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L523
.L525:
    lea rax, [rbp - 264]
    push rax
    lea rax, [rbp - 280]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L516:
    lea rax, [rbp - 304]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 216]
    mov rax, [rax]
    push rax
    mov rax, 2
    push rax
    pop rdi
    pop rsi
    pop rdx
    call obj_create
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 304]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L526
    lea rax, [.Ls110]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret44
    jmp .L527
.L526:
.L527:
    lea rax, [rbp - 216]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 304]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    push rax
    pop rdi
    pop rsi
    pop rdx
    call memcpy
    lea rax, [rbp - 305]
    push rax
    lea rax, [rbp - 264]
    push rax
    lea rax, [rbp - 304]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call lay_into
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 304]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_release
    lea rax, [rbp - 305]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L528
    lea rax, [.Ls111]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret44
    jmp .L529
.L528:
.L529:
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 216]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 264]
    push rax
    lea rax, [.Ls112]
    push rax
    call kprintf
    add rsp, 32
    lea rax, [rbp - 264]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls113]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 216]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls114]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [rbp - 29]
    movzx rax, byte [rax]
    test rax, rax
    je .L530
    lea rax, [.Ls115]
    jmp .L531
.L530:
    lea rax, [.Ls116]
.L531:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret44:
    mov rsp, rbp
    pop rbp
    ret

private cmd_link
section code
cmd_link:
    push rbp
    mov rbp, rsp
    sub rsp, 96
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L532
    lea rax, [.Ls117]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret45
    jmp .L533
.L532:
.L533:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L534
    jmp .Lret45
    jmp .L535
.L534:
.L535:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L536
    lea rax, [.Ls118]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret45
    jmp .L537
.L536:
.L537:
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L538
    lea rax, [.Ls119]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret45
    jmp .L539
.L538:
.L539:
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L540
    lea rax, [.Ls120]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret45
    jmp .L541
.L540:
.L541:
    lea rax, [rbp - 52]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 64]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L542:
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
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
    je .L545
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 128
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L545
    mov rax, 1
    jmp .L546
.L545:
    mov rax, 0
.L546:
    test rax, rax
    je .L544
    lea rax, [rbp - 72]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 72]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L549
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L549
    mov rax, 0
    jmp .L550
.L549:
    mov rax, 1
.L550:
    test rax, rax
    je .L547
    jmp .L543
    jmp .L548
.L547:
.L548:
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_slot_rights
    mov eax, eax
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L551
    jmp .L543
    jmp .L552
.L551:
.L552:
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
    pop rdi
    pop rsi
    call ld_object_ok
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L553
    jmp .L543
    jmp .L554
.L553:
.L554:
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call shown_name
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 84]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L555:
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L557
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    mov rax, 40
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
    je .L557
    mov rax, 1
    jmp .L558
.L557:
    mov rax, 0
.L558:
    test rax, rax
    je .L556
    lea rax, [unit_names]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
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
    jmp .L555
.L556:
    lea rax, [unit_names]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [units]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 24
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
    call obj_data
    pop rdi
    mov [rdi], rax
    lea rax, [units]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 8
    push rax
    lea rax, [rbp - 72]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    pop rdi
    mov [rdi], rax
    lea rax, [units]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 16
    push rax
    lea rax, [unit_names]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 52]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
.L543:
    lea rax, [rbp - 64]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L542
.L544:
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call link_units
.Lret45:
    mov rsp, rbp
    pop rbp
    ret

private cmd_build
section code
cmd_build:
    push rbp
    mov rbp, rsp
    sub rsp, 336
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L559
    lea rax, [.Ls121]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret46
    jmp .L560
.L559:
.L560:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L561
    jmp .Lret46
    jmp .L562
.L561:
.L562:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L563
    lea rax, [.Ls122]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret46
    jmp .L564
.L563:
.L564:
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L565
    lea rax, [.Ls123]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret46
    jmp .L566
.L565:
.L566:
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L567
    lea rax, [.Ls124]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret46
    jmp .L568
.L567:
.L568:
    lea rax, [arena]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L569
    lea rax, [arena]
    push rax
    mov rax, 24
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    pop rdi
    call lang_big_alloc
    pop rdi
    mov [rdi], rax
    jmp .L570
.L569:
.L570:
    lea rax, [rbp - 56]
    push rax
    call lang_text_buffer
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 64]
    push rax
    call lang_out_buffer
    pop rdi
    mov [rdi], rax
    lea rax, [arena]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L575
    lea rax, [rbp - 56]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L575
    mov rax, 0
    jmp .L576
.L575:
    mov rax, 1
.L576:
    test rax, rax
    jne .L573
    lea rax, [rbp - 64]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L573
    mov rax, 0
    jmp .L574
.L573:
    mov rax, 1
.L574:
    test rax, rax
    je .L571
    lea rax, [.Ls125]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret46
    jmp .L572
.L571:
.L572:
    lea rax, [rbp - 68]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 72]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 76]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 248]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L577:
    lea rax, [rbp - 248]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
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
    je .L580
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    push rax
    mov rax, 128
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L580
    mov rax, 1
    jmp .L581
.L580:
    mov rax, 0
.L581:
    test rax, rax
    je .L579
    lea rax, [rbp - 256]
    push rax
    lea rax, [rbp - 248]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 256]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L584
    lea rax, [rbp - 256]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L584
    mov rax, 0
    jmp .L585
.L584:
    mov rax, 1
.L585:
    test rax, rax
    je .L582
    jmp .L578
    jmp .L583
.L582:
.L583:
    lea rax, [rbp - 248]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_slot_rights
    mov eax, eax
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L586
    jmp .L578
    jmp .L587
.L586:
.L587:
    lea rax, [rbp - 264]
    push rax
    lea rax, [rbp - 248]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call shown_name
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 268]
    push rax
    lea rax, [rbp - 264]
    mov rax, [rax]
    push rax
    pop rdi
    call strlen
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 269]
    push rax
    mov rax, 2
    push rax
    lea rax, [rbp - 268]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L590
    lea rax, [rbp - 264]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 268]
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 46
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L590
    mov rax, 1
    jmp .L591
.L590:
    mov rax, 0
.L591:
    test rax, rax
    je .L588
    lea rax, [rbp - 264]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 268]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 99
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L588
    mov rax, 1
    jmp .L589
.L588:
    mov rax, 0
.L589:
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 270]
    push rax
    mov rax, 2
    push rax
    lea rax, [rbp - 268]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L594
    lea rax, [rbp - 264]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 268]
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 46
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L594
    mov rax, 1
    jmp .L595
.L594:
    mov rax, 0
.L595:
    test rax, rax
    je .L592
    lea rax, [rbp - 264]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 268]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 83
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L596
    lea rax, [rbp - 264]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 268]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 115
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L596
    mov rax, 0
    jmp .L597
.L596:
    mov rax, 1
.L597:
    test rax, rax
    je .L592
    mov rax, 1
    jmp .L593
.L592:
    mov rax, 0
.L593:
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 269]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L600
    lea rax, [rbp - 270]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L600
    mov rax, 1
    jmp .L601
.L600:
    mov rax, 0
.L601:
    test rax, rax
    je .L598
    jmp .L578
    jmp .L599
.L598:
.L599:
    lea rax, [rbp - 76]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    lea rax, [rbp - 280]
    push rax
    lea rax, [rbp - 256]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 288]
    push rax
    lea rax, [rbp - 256]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 280]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call text_len
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 296]
    push rax
    lea rax, [arena]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 72]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 304]
    push rax
    mov rax, 24
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 72]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 269]
    movzx rax, byte [rax]
    test rax, rax
    je .L602
    lea rax, [rbp - 320]
    push rax
    mov rax, 160
    push rax
    lea rax, [rbp - 236]
    push rax
    mov rax, 4
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [find_beside]
    push rax
    lea rax, [rbp - 264]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 288]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 280]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call cc_compile
    add rsp, 24
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 320]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L604
    lea rax, [rbp - 264]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls126]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 236]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret46
    jmp .L605
.L604:
.L605:
    lea rax, [rbp - 312]
    push rax
    mov rax, 160
    push rax
    lea rax, [rbp - 236]
    push rax
    lea rax, [rbp - 304]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 296]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 320]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call asm_assemble
    pop rdi
    mov [rdi], rax
    jmp .L603
.L602:
    lea rax, [rbp - 312]
    push rax
    lea rax, [rbp - 264]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 268]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 83
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L606
    mov rax, 160
    push rax
    lea rax, [rbp - 236]
    push rax
    lea rax, [rbp - 304]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 296]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 288]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 280]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call asm_assemble_gnu
    jmp .L607
.L606:
    mov rax, 160
    push rax
    lea rax, [rbp - 236]
    push rax
    lea rax, [rbp - 304]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 296]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 288]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 280]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call asm_assemble
.L607:
    pop rdi
    mov [rdi], rax
.L603:
    lea rax, [rbp - 312]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L608
    lea rax, [rbp - 264]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls127]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 236]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret46
    jmp .L609
.L608:
.L609:
    lea rax, [rbp - 324]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L610:
    lea rax, [rbp - 264]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 324]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L612
    lea rax, [rbp - 324]
    mov eax, dword [rax]
    push rax
    mov rax, 40
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
    je .L612
    mov rax, 1
    jmp .L613
.L612:
    mov rax, 0
.L613:
    test rax, rax
    je .L611
    lea rax, [unit_names]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 324]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 264]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 324]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 324]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L610
.L611:
    lea rax, [unit_names]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 324]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [units]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 296]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [units]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 8
    push rax
    lea rax, [rbp - 312]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [units]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 16
    push rax
    lea rax, [unit_names]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 68]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    lea rax, [rbp - 72]
    push rax
    lea rax, [rbp - 312]
    mov rax, [rax]
    push rax
    mov rax, 15
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 15
    not rax
    mov rdi, rax
    pop rax
    and rax, rdi
    mov eax, eax
    mov rdi, rax
    pop rax
    mov r8, rax
    mov eax, dword [rax]
    add rax, rdi
    mov rdi, r8
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [.Ls128]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 264]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls129]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 312]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls130]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.L578:
    lea rax, [rbp - 248]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L577
.L579:
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L614
    lea rax, [.Ls131]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret46
    jmp .L615
.L614:
.L615:
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call link_units
.Lret46:
    mov rsp, rbp
    pop rbp
    ret

private cmd_take_in
section code
cmd_take_in:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L616
    lea rax, [.Ls132]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret47
    jmp .L617
.L616:
.L617:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L618
    jmp .Lret47
    jmp .L619
.L618:
.L619:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L622
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L622
    mov rax, 0
    jmp .L623
.L622:
    mov rax, 1
.L623:
    test rax, rax
    je .L620
    lea rax, [.Ls133]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret47
    jmp .L621
.L620:
.L621:
    call fat_present
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L624
    lea rax, [.Ls134]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret47
    jmp .L625
.L624:
.L625:
    lea rax, [rbp - 52]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call fat_take_in
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls135]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret47:
    mov rsp, rbp
    pop rbp
    ret

private cmd_write_out
section code
cmd_write_out:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L626
    lea rax, [.Ls136]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret48
    jmp .L627
.L626:
.L627:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L628
    jmp .Lret48
    jmp .L629
.L628:
.L629:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L632
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L632
    mov rax, 0
    jmp .L633
.L632:
    mov rax, 1
.L633:
    test rax, rax
    je .L630
    lea rax, [.Ls137]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret48
    jmp .L631
.L630:
.L631:
    call fat_present
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L634
    lea rax, [.Ls138]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret48
    jmp .L635
.L634:
.L635:
    lea rax, [rbp - 52]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call fat_write_out
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls139]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret48:
    mov rsp, rbp
    pop rbp
    ret

private cmd_run
section code
cmd_run:
    push rbp
    mov rbp, rsp
    sub rsp, 176
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L636
    lea rax, [.Ls140]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret49
    jmp .L637
.L636:
.L637:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L638
    jmp .Lret49
    jmp .L639
.L638:
.L639:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call focus_rights
    mov eax, eax
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L640
    lea rax, [.Ls141]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret49
    jmp .L641
.L640:
.L641:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L642
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call code_image_ok
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L644
    lea rax, [.Ls142]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret49
    jmp .L645
.L644:
.L645:
    lea rax, [rbp - 92]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L646:
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L648
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    push rax
    mov rax, 40
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
    je .L648
    mov rax, 1
    jmp .L649
.L648:
    mov rax, 0
.L649:
    test rax, rax
    je .L647
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 92]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L646
.L647:
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 104]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call code_launch
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L650
    lea rax, [.Ls143]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret49
    jmp .L651
.L650:
.L651:
    lea rax, [rbp - 112]
    push rax
    lea rax, [rbp - 88]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    push rax
    mov rax, 1
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call lay_here
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L652
    lea rax, [.Ls144]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret49
    jmp .L653
.L652:
.L653:
    lea rax, [.Ls145]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret49
    jmp .L643
.L642:
.L643:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L654
    lea rax, [.Ls146]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret49
    jmp .L655
.L654:
.L655:
    lea rax, [rbp - 156]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L656:
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 156]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L658
    lea rax, [rbp - 156]
    mov eax, dword [rax]
    push rax
    mov rax, 40
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
    je .L658
    mov rax, 1
    jmp .L659
.L658:
    mov rax, 0
.L659:
    test rax, rax
    je .L657
    lea rax, [rbp - 152]
    push rax
    lea rax, [rbp - 156]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 48]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 156]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
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
    jmp .L656
.L657:
    lea rax, [rbp - 152]
    push rax
    lea rax, [rbp - 156]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 168]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call runner_launch
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 168]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L660
    lea rax, [.Ls147]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret49
    jmp .L661
.L660:
.L661:
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 152]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    push rax
    mov rax, 1
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    lea rax, [rbp - 168]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call lay_here
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 176]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L662
    lea rax, [.Ls148]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret49
    jmp .L663
.L662:
.L663:
    lea rax, [.Ls149]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret49:
    mov rsp, rbp
    pop rbp
    ret

private cmd_give
section code
cmd_give:
    push rbp
    mov rbp, rsp
    sub rsp, 480
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 416]
    push rax
    lea rax, [rbp - 216]
    push rax
    lea rax, [.Ls150]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call split_at
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L664
    lea rax, [.Ls151]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret50
    jmp .L665
.L664:
.L665:
    lea rax, [rbp - 448]
    push rax
    lea rax, [rbp - 216]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L668
    lea rax, [rbp - 480]
    push rax
    lea rax, [rbp - 416]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L668
    mov rax, 0
    jmp .L669
.L668:
    mov rax, 1
.L669:
    test rax, rax
    je .L666
    jmp .Lret50
    jmp .L667
.L666:
.L667:
    lea rax, [rbp - 480]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L672
    lea rax, [rbp - 480]
    mov rax, [rax]
    push rax
    pop rdi
    call proc_is_running
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L672
    mov rax, 0
    jmp .L673
.L672:
    mov rax, 1
.L673:
    test rax, rax
    je .L670
    lea rax, [.Ls152]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret50
    jmp .L671
.L670:
.L671:
    lea rax, [rbp - 480]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L674
    lea rax, [.Ls153]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret50
    jmp .L675
.L674:
.L675:
    lea rax, [rbp - 448]
    add rax, 8
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 448]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 480]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call proc_grant
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L676
    lea rax, [.Ls154]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret50
    jmp .L677
.L676:
.L677:
    lea rax, [rbp - 480]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls155]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret50:
    mov rsp, rbp
    pop rbp
    ret

private cmd_end
section code
cmd_end:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L678
    lea rax, [.Ls156]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret51
    jmp .L679
.L678:
.L679:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L680
    jmp .Lret51
    jmp .L681
.L680:
.L681:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L682
    lea rax, [.Ls157]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret51
    jmp .L683
.L682:
.L683:
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L684
    lea rax, [.Ls158]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret51
    jmp .L685
.L684:
.L685:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call proc_end
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L686
    lea rax, [.Ls159]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret51
    jmp .L687
.L686:
.L687:
    lea rax, [.Ls161]
    push rax
    lea rax, [.Ls160]
    push rax
    pop rdi
    pop rsi
    call journal_says
    lea rax, [.Ls162]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret51:
    mov rsp, rbp
    pop rbp
    ret

private cmd_send
section code
cmd_send:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L688
    lea rax, [.Ls163]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret52
    jmp .L689
.L688:
.L689:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L690
    jmp .Lret52
    jmp .L691
.L690:
.L691:
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L692
    lea rax, [.Ls164]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret52
    jmp .L693
.L692:
.L693:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call pipe_post
    movzx rax, al
    test rax, rax
    je .L694
    lea rax, [.Ls165]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .L695
.L694:
    lea rax, [.Ls166]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.L695:
.Lret52:
    mov rsp, rbp
    pop rbp
    ret

private cmd_ask
section code
cmd_ask:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L696
    lea rax, [.Ls167]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret53
    jmp .L697
.L696:
.L697:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call resolve
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L698
    jmp .Lret53
    jmp .L699
.L698:
.L699:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L700
    lea rax, [.Ls168]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret53
    jmp .L701
.L700:
.L701:
    lea rax, [rbp - 48]
    add rax, 8
    mov eax, dword [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    and rax, rdi
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call pipe_ask
    movzx rax, al
    test rax, rax
    je .L702
    lea rax, [.Ls169]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .L703
.L702:
    lea rax, [.Ls170]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.L703:
.Lret53:
    mov rsp, rbp
    pop rbp
    ret

private cmd_say
section code
cmd_say:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L704
    lea rax, [.Ls171]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret54
    jmp .L705
.L704:
.L705:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call pipe_say
    movzx rax, al
    test rax, rax
    je .L706
    lea rax, [.Ls172]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .L707
.L706:
    lea rax, [.Ls173]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.L707:
.Lret54:
    mov rsp, rbp
    pop rbp
    ret

private cmd_scan
section code
cmd_scan:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    call pipe_scan
    lea rax, [.Ls174]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret55:
    mov rsp, rbp
    pop rbp
    ret

private put_ip
section code
put_ip:
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
.L708:
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L710
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    test rax, rax
    je .L711
    mov rax, 46
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
    jmp .L712
.L711:
.L712:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
.L709:
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
    jmp .L708
.L710:
.Lret56:
    mov rsp, rbp
    pop rbp
    ret

private cmd_found
section code
cmd_found:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    lea rax, [rbp - 12]
    push rax
    call pipe_found_count
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
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
    je .L713
    call pipe_scanning
    movzx rax, al
    test rax, rax
    je .L715
    lea rax, [.Ls175]
    jmp .L716
.L715:
    lea rax, [.Ls176]
.L716:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret57
    jmp .L714
.L713:
.L714:
    lea rax, [rbp - 16]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L717:
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
    je .L719
    lea rax, [rbp - 52]
    push rax
    lea rax, [rbp - 45]
    push rax
    lea rax, [rbp - 44]
    push rax
    lea rax, [rbp - 20]
    push rax
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call pipe_found_at
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L720
    jmp .L718
    jmp .L721
.L720:
.L721:
    lea rax, [.Ls177]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 20]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call put_ip
    lea rax, [.Ls178]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 44]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L722
    lea rax, [rbp - 44]
    jmp .L723
.L722:
    lea rax, [.Ls179]
.L723:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 45]
    movzx rax, byte [rax]
    test rax, rax
    je .L724
    lea rax, [.Ls180]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls181]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    jmp .L725
.L724:
.L725:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call t_end
.L718:
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
    jmp .L717
.L719:
.Lret57:
    mov rsp, rbp
    pop rbp
    ret

private cmd_point
section code
cmd_point:
    push rbp
    mov rbp, rsp
    sub rsp, 240
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L726
    lea rax, [.Ls182]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret58
    jmp .L727
.L726:
.L727:
    lea rax, [rbp - 20]
    mov rdi, rax
    mov rcx, 4
.L728:
    mov byte [rdi], 0
    inc rdi
    dec rcx
    jne .L728
    lea rax, [rbp - 24]
    push rax
    mov rax, 7800
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 25]
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    push rax
    call pipe_found_count
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 36]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L729:
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L732
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L732
    mov rax, 1
    jmp .L733
.L732:
    mov rax, 0
.L733:
    test rax, rax
    je .L731
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 64]
    push rax
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call pipe_found_at
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L734
    jmp .L730
    jmp .L735
.L734:
.L735:
    lea rax, [rbp - 68]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L736:
    lea rax, [rbp - 64]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L740
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L740
    mov rax, 1
    jmp .L741
.L740:
    mov rax, 0
.L741:
    test rax, rax
    je .L738
    lea rax, [rbp - 64]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    pop rdi
    call low
    movsx rax, al
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    pop rdi
    call low
    movsx rax, al
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L738
    mov rax, 1
    jmp .L739
.L738:
    mov rax, 0
.L739:
    test rax, rax
    je .L737
    lea rax, [rbp - 68]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L736
.L737:
    lea rax, [rbp - 64]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L744
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L744
    mov rax, 1
    jmp .L745
.L744:
    mov rax, 0
.L745:
    test rax, rax
    je .L742
    lea rax, [rbp - 72]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L746:
    lea rax, [rbp - 72]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L748
    lea rax, [rbp - 20]
    push rax
    lea rax, [rbp - 72]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 72]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L747:
    lea rax, [rbp - 72]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L746
.L748:
    lea rax, [rbp - 25]
    push rax
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L743
.L742:
.L743:
.L730:
    lea rax, [rbp - 36]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L729
.L731:
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L749
    lea rax, [rbp - 76]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 80]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 84]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 85]
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L751:
    lea rax, [rbp - 86]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    mov rax, 48
    push rax
    lea rax, [rbp - 86]
    movsx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L756
    lea rax, [rbp - 86]
    movsx rax, byte [rax]
    push rax
    mov rax, 57
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L756
    mov rax, 1
    jmp .L757
.L756:
    mov rax, 0
.L757:
    test rax, rax
    je .L754
    lea rax, [rbp - 76]
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 86]
    movsx rax, byte [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov eax, eax
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 85]
    push rax
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L752
    jmp .L755
.L754:
.L755:
    lea rax, [rbp - 80]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L758
    lea rax, [rbp - 85]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L762
    mov rax, 255
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    jne .L762
    mov rax, 0
    jmp .L763
.L762:
    mov rax, 1
.L763:
    test rax, rax
    je .L760
    jmp .L753
    jmp .L761
.L760:
.L761:
    lea rax, [rbp - 20]
    push rax
    lea rax, [rbp - 80]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L759
.L758:
    lea rax, [rbp - 85]
    movzx rax, byte [rax]
    test rax, rax
    je .L768
    mov rax, 1
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L768
    mov rax, 1
    jmp .L769
.L768:
    mov rax, 0
.L769:
    test rax, rax
    je .L766
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    push rax
    mov rax, 65535
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L766
    mov rax, 1
    jmp .L767
.L766:
    mov rax, 0
.L767:
    test rax, rax
    je .L764
    lea rax, [rbp - 24]
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L765
.L764:
.L765:
    jmp .L753
.L759:
    lea rax, [rbp - 76]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 85]
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 86]
    movsx rax, byte [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L770
    jmp .L753
    jmp .L771
.L770:
.L771:
    lea rax, [rbp - 86]
    movsx rax, byte [rax]
    push rax
    mov rax, 46
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L774
    lea rax, [rbp - 86]
    movsx rax, byte [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L774
    mov rax, 1
    jmp .L775
.L774:
    mov rax, 0
.L775:
    test rax, rax
    je .L772
    jmp .L753
    jmp .L773
.L772:
.L773:
.L752:
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
    jmp .L751
.L753:
    lea rax, [rbp - 25]
    push rax
    lea rax, [rbp - 80]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L750
.L749:
.L750:
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L776
    lea rax, [.Ls183]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret58
    jmp .L777
.L776:
.L777:
    lea rax, [rbp - 96]
    push rax
    call settings_object
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L778
    lea rax, [.Ls184]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret58
    jmp .L779
.L778:
.L779:
    lea rax, [rbp - 104]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 112]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 120]
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call text_len
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 188]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 200]
    push rax
    lea rax, [.Ls185]
    pop rdi
    mov [rdi], rax
.L780:
    lea rax, [rbp - 200]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 188]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L781
    lea rax, [rbp - 184]
    push rax
    lea rax, [rbp - 188]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 200]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 188]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 188]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L780
.L781:
    lea rax, [rbp - 204]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L782:
    lea rax, [rbp - 204]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L784
    lea rax, [rbp - 208]
    push rax
    lea rax, [rbp - 20]
    push rax
    lea rax, [rbp - 204]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 216]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 208]
    mov eax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L785
    lea rax, [rbp - 212]
    push rax
    lea rax, [rbp - 216]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 48
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    jmp .L786
.L785:
.L786:
.L787:
    lea rax, [rbp - 208]
    mov eax, dword [rax]
    test rax, rax
    je .L788
    lea rax, [rbp - 212]
    push rax
    lea rax, [rbp - 216]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 48
    push rax
    lea rax, [rbp - 208]
    mov eax, dword [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cqo
    idiv rdi
    mov rax, rdx
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, al
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 208]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    mov r8, rax
    mov eax, dword [rax]
    xor edx, edx
    div rdi
    mov rdi, r8
    mov eax, eax
    mov dword [rdi], eax
    jmp .L787
.L788:
.L789:
    lea rax, [rbp - 216]
    mov eax, dword [rax]
    test rax, rax
    je .L790
    lea rax, [rbp - 184]
    push rax
    lea rax, [rbp - 188]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 212]
    push rax
    lea rax, [rbp - 216]
    mov rdi, rax
    mov eax, dword [rax]
    add rax, -1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    jmp .L789
.L790:
    lea rax, [rbp - 184]
    push rax
    lea rax, [rbp - 188]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 204]
    mov eax, dword [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L791
    mov rax, 46
    jmp .L792
.L791:
    mov rax, 32
.L792:
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L783:
    lea rax, [rbp - 204]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L782
.L784:
    lea rax, [rbp - 220]
    push rax
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 232]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L793:
    lea rax, [rbp - 220]
    mov eax, dword [rax]
    test rax, rax
    je .L794
    lea rax, [rbp - 228]
    push rax
    lea rax, [rbp - 232]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 48
    push rax
    lea rax, [rbp - 220]
    mov eax, dword [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cqo
    idiv rdi
    mov rax, rdx
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, al
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 220]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    mov r8, rax
    mov eax, dword [rax]
    xor edx, edx
    div rdi
    mov rdi, r8
    mov eax, eax
    mov dword [rdi], eax
    jmp .L793
.L794:
.L795:
    lea rax, [rbp - 232]
    mov eax, dword [rax]
    test rax, rax
    je .L796
    lea rax, [rbp - 184]
    push rax
    lea rax, [rbp - 188]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 228]
    push rax
    lea rax, [rbp - 232]
    mov rdi, rax
    mov eax, dword [rax]
    add rax, -1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    jmp .L795
.L796:
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 188]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L797
    lea rax, [.Ls186]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret58
    jmp .L798
.L797:
.L798:
    lea rax, [rbp - 120]
    mov rax, [rax]
    test rax, rax
    je .L801
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L801
    mov rax, 1
    jmp .L802
.L801:
    mov rax, 0
.L802:
    test rax, rax
    je .L799
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 120]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 10
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L800
.L799:
.L800:
    lea rax, [rbp - 236]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L803:
    lea rax, [rbp - 236]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 188]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L805
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 120]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 184]
    push rax
    lea rax, [rbp - 236]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L804:
    lea rax, [rbp - 236]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L803
.L805:
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 120]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 10
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 120]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_touch
    call settings_apply
    lea rax, [.Ls187]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 20]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call put_ip
    lea rax, [.Ls188]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret58:
    mov rsp, rbp
    pop rbp
    ret

private contains_ci
section code
contains_ci:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    lea rax, [rbp - 28]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L806:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L807
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
    jmp .L806
.L807:
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L810
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    jne .L810
    mov rax, 0
    jmp .L811
.L810:
    mov rax, 1
.L811:
    test rax, rax
    je .L808
    mov rax, 0
    movzx rax, al
    jmp .Lret59
    jmp .L809
.L808:
.L809:
    lea rax, [rbp - 40]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L812:
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L814
    lea rax, [rbp - 44]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L815:
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L817
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    movsx rax, al
    push rax
    pop rdi
    call low
    movsx rax, al
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    pop rdi
    call low
    movsx rax, al
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L817
    mov rax, 1
    jmp .L818
.L817:
    mov rax, 0
.L818:
    test rax, rax
    je .L816
    lea rax, [rbp - 44]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L815
.L816:
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L819
    mov rax, 1
    movzx rax, al
    jmp .Lret59
    jmp .L820
.L819:
.L820:
.L813:
    lea rax, [rbp - 40]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L812
.L814:
    mov rax, 0
    movzx rax, al
    jmp .Lret59
.Lret59:
    mov rsp, rbp
    pop rbp
    ret

private cmd_find
section code
cmd_find:
    push rbp
    mov rbp, rsp
    sub rsp, 192
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L821
    lea rax, [.Ls189]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret60
    jmp .L822
.L821:
.L822:
    lea rax, [rbp - 20]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 24]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 25]
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [seen.2]
    push rax
    mov rax, 0
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
    add rax, 8
    push rax
    mov rax, 0
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
    lea rax, [parent.3]
    push rax
    mov rax, 0
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [label.4]
    push rax
    mov rax, 0
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 20]
    push rax
    mov rax, 1
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 32]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L823:
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L825
    lea rax, [rbp - 40]
    push rax
    lea rax, [seen.2]
    push rax
    lea rax, [rbp - 32]
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
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_slots
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 56]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L826:
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L828
    lea rax, [rbp - 64]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call obj_get_slot
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L829
    jmp .L827
    jmp .L830
.L829:
.L830:
    lea rax, [rbp - 65]
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 72]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L831:
    lea rax, [rbp - 72]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L833
    lea rax, [seen.2]
    push rax
    lea rax, [rbp - 72]
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
    lea rax, [rbp - 64]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L834
    lea rax, [rbp - 65]
    push rax
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L833
    jmp .L835
.L834:
.L835:
.L832:
    lea rax, [rbp - 72]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L831
.L833:
    lea rax, [rbp - 65]
    movzx rax, byte [rax]
    test rax, rax
    je .L836
    jmp .L827
    jmp .L837
.L836:
.L837:
    mov rax, 256
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L838
    lea rax, [rbp - 25]
    push rax
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L828
    jmp .L839
.L838:
.L839:
    lea rax, [rbp - 76]
    push rax
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
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [seen.2]
    push rax
    lea rax, [rbp - 76]
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
    lea rax, [rbp - 64]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [parent.3]
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call shown_name
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 92]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L840:
    lea rax, [rbp - 88]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L842
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    push rax
    mov rax, 23
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L842
    mov rax, 1
    jmp .L843
.L842:
    mov rax, 0
.L843:
    test rax, rax
    je .L841
    lea rax, [label.4]
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 88]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 92]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L840
.L841:
    lea rax, [label.4]
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 93]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    push rax
    lea rax, [label.4]
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    push rax
    mov rax, 24
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
    call contains_ci
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 93]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L846
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_type
    mov eax, eax
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L846
    mov rax, 1
    jmp .L847
.L846:
    mov rax, 0
.L847:
    test rax, rax
    je .L844
    lea rax, [rbp - 104]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 112]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call text_len
    pop rdi
    mov [rdi], rax
    mov rax, 4096
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L848
    lea rax, [rbp - 112]
    push rax
    mov rax, 4096
    pop rdi
    mov [rdi], rax
    jmp .L849
.L848:
.L849:
    lea rax, [rbp - 93]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 104]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call contains_ci
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L845
.L844:
.L845:
    lea rax, [rbp - 93]
    movzx rax, byte [rax]
    test rax, rax
    je .L852
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
    je .L852
    mov rax, 1
    jmp .L853
.L852:
    mov rax, 0
.L853:
    test rax, rax
    je .L850
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
    lea rax, [rbp - 180]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 184]
    push rax
    lea rax, [rbp - 76]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L854:
    lea rax, [rbp - 184]
    mov eax, dword [rax]
    test rax, rax
    je .L856
    lea rax, [rbp - 180]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L856
    mov rax, 1
    jmp .L857
.L856:
    mov rax, 0
.L857:
    test rax, rax
    je .L855
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 180]
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
    mov rax, 4
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 184]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 184]
    push rax
    lea rax, [parent.3]
    push rax
    lea rax, [rbp - 184]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L854
.L855:
    lea rax, [.Ls190]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
.L858:
    lea rax, [rbp - 180]
    mov eax, dword [rax]
    test rax, rax
    je .L859
    lea rax, [.Ls191]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [label.4]
    push rax
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 180]
    mov rdi, rax
    mov eax, dword [rax]
    add rax, -1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov eax, dword [rax]
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L860
    lea rax, [label.4]
    push rax
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 180]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov eax, dword [rax]
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    jmp .L861
.L860:
    lea rax, [.Ls192]
.L861:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    jmp .L858
.L859:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call t_end
    jmp .L851
.L850:
.L851:
.L827:
    lea rax, [rbp - 56]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L826
.L828:
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    test rax, rax
    je .L862
    jmp .L825
    jmp .L863
.L862:
.L863:
.L824:
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
    jmp .L823
.L825:
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L864
    lea rax, [.Ls193]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .L865
.L864:
.L865:
    mov rax, 16
    push rax
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L866
    lea rax, [.Ls194]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .L867
.L866:
.L867:
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    test rax, rax
    je .L868
    lea rax, [.Ls195]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .L869
.L868:
.L869:
.Lret60:
    mov rsp, rbp
    pop rbp
    ret

private cmd_journal
section code
cmd_journal:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    lea rax, [rbp - 16]
    push rax
    call journal_object
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 24]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    test rax, rax
    je .L870
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_data
    jmp .L871
.L870:
    mov rax, 0
.L871:
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L872
    lea rax, [.Ls196]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret61
    jmp .L873
.L872:
.L873:
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    call obj_size
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call text_len
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L874
    lea rax, [.Ls197]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    jmp .Lret61
    jmp .L875
.L874:
.L875:
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
.L876:
    mov rax, 0
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L878
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    mov rax, 12
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L878
    mov rax, 1
    jmp .L879
.L878:
    mov rax, 0
.L879:
    test rax, rax
    je .L877
    lea rax, [rbp - 40]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, -1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    lea rax, [rbp - 40]
    mov rax, [rax]
    test rax, rax
    je .L882
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L882
    mov rax, 1
    jmp .L883
.L882:
    mov rax, 0
.L883:
    test rax, rax
    je .L880
    lea rax, [rbp - 48]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L881
.L880:
.L881:
    jmp .L876
.L877:
    lea rax, [rbp - 56]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
.L884:
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L886
    mov rax, 32
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    jne .L889
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L889
    mov rax, 0
    jmp .L890
.L889:
    mov rax, 1
.L890:
    test rax, rax
    je .L887
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    movsx rax, al
    jmp .L888
.L887:
    mov rax, 32
.L888:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
.L885:
    lea rax, [rbp - 56]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L884
.L886:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17232
    mov rax, [rax]
    test rax, rax
    je .L893
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 844
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17232
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L893
    mov rax, 1
    jmp .L894
.L893:
    mov rax, 0
.L894:
    test rax, rax
    je .L891
    mov rax, 10
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
    jmp .L892
.L891:
.L892:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17248
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
.Lret61:
    mov rsp, rbp
    pop rbp
    ret

private cmd_time
section code
cmd_time:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    lea rax, [rbp - 20]
    push rax
    lea rax, [rbp - 16]
    push rax
    lea rax, [rbp - 12]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call time_wall
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L895
    mov rax, 48
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
    jmp .L896
.L895:
.L896:
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    mov rax, 58
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L897
    mov rax, 48
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
    jmp .L898
.L897:
.L898:
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    mov rax, 58
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L899
    mov rax, 48
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_putc
    jmp .L900
.L899:
.L900:
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls198]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    call time_ns
    push rax
    mov rax, 1000000000
    mov rdi, rax
    pop rax
    xor edx, edx
    div rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_dec
    lea rax, [.Ls199]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret62:
    mov rsp, rbp
    pop rbp
    ret

private cmd_help
section code
cmd_help:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [.Ls200]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls201]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call t_end
    lea rax, [.Ls202]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls203]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls204]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls205]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls206]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls207]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call t_end
    lea rax, [.Ls208]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls209]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls210]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls211]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls212]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls213]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls214]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls215]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call t_end
    lea rax, [.Ls216]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls217]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls218]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls219]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls220]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls221]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls222]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls223]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls224]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls225]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call t_end
    lea rax, [.Ls226]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls227]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls228]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls229]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls230]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls231]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls232]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call t_end
    lea rax, [.Ls233]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls234]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [.Ls235]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.Lret63:
    mov rsp, rbp
    pop rbp
    ret

section code
term_line:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 8]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L905
    lea rax, [rbp - 8]
    mov rax, [rax]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L905
    mov rax, 0
    jmp .L906
.L905:
    mov rax, 1
.L906:
    test rax, rax
    jne .L903
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 840
    mov eax, dword [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L903
    mov rax, 0
    jmp .L904
.L903:
    mov rax, 1
.L904:
    test rax, rax
    je .L901
    jmp .Lret64
    jmp .L902
.L901:
.L902:
.L907:
    lea rax, [rbp - 16]
    mov rax, [rax]
    movsx rax, byte [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L908
    lea rax, [rbp - 16]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L907
.L908:
    lea rax, [.Ls236]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
    lea rax, [rbp - 16]
    mov rax, [rax]
    movsx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L909
    jmp .Lret64
    jmp .L910
.L909:
.L910:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls237]
    pop rdi
    mov [rdi], rax
    mov rax, 0
    push rax
    lea rax, [.Ls238]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L911
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call cmd_help
    jmp .L912
.L911:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls239]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L913
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_look
    jmp .L914
.L913:
    mov rax, 0
    push rax
    lea rax, [.Ls240]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L915
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call cmd_where
    jmp .L916
.L915:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls241]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L917
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_go
    jmp .L918
.L917:
    mov rax, 0
    push rax
    lea rax, [.Ls242]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L919
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call cmd_back
    jmp .L920
.L919:
    mov rax, 0
    push rax
    lea rax, [.Ls243]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L921
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call cmd_home
    jmp .L922
.L921:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls244]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L923
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_find
    jmp .L924
.L923:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls245]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L925
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_read
    jmp .L926
.L925:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls246]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L927
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_write_out
    jmp .L928
.L927:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls247]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L929
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_take_in
    jmp .L930
.L929:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls248]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L931
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_write
    jmp .L932
.L931:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls249]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L933
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_make
    jmp .L934
.L933:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls250]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L935
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_copy
    jmp .L936
.L935:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls251]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L937
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_rename
    jmp .L938
.L937:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls252]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L939
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_letgo
    jmp .L940
.L939:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls253]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L941
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_run
    jmp .L942
.L941:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls254]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L943
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_assemble
    jmp .L944
.L943:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls255]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L945
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_compile
    jmp .L946
.L945:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls256]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L947
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_link
    jmp .L948
.L947:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls257]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L949
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_build
    jmp .L950
.L949:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls258]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L951
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_give
    jmp .L952
.L951:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls259]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L953
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_end
    jmp .L954
.L953:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls260]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L955
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_send
    jmp .L956
.L955:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls261]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L957
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_ask
    jmp .L958
.L957:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls262]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L959
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_say
    jmp .L960
.L959:
    mov rax, 0
    push rax
    lea rax, [.Ls263]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L961
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call cmd_scan
    jmp .L962
.L961:
    mov rax, 0
    push rax
    lea rax, [.Ls264]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L963
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call cmd_found
    jmp .L964
.L963:
    lea rax, [rbp - 24]
    push rax
    lea rax, [.Ls265]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L965
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call cmd_point
    jmp .L966
.L965:
    mov rax, 0
    push rax
    lea rax, [.Ls266]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L967
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call cmd_journal
    jmp .L968
.L967:
    mov rax, 0
    push rax
    lea rax, [.Ls267]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call word_starts
    movzx rax, al
    test rax, rax
    je .L969
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call cmd_time
    jmp .L970
.L969:
    lea rax, [.Ls268]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_puts
    lea rax, [.Ls269]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call t_say
.L970:
.L968:
.L966:
.L964:
.L962:
.L960:
.L958:
.L956:
.L954:
.L952:
.L950:
.L948:
.L946:
.L944:
.L942:
.L940:
.L938:
.L936:
.L934:
.L932:
.L930:
.L928:
.L926:
.L924:
.L922:
.L920:
.L918:
.L916:
.L914:
.L912:
.Lret64:
    mov rsp, rbp
    pop rbp
    ret

section code
term_gather:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 16]
    mov rax, [rax]
    test rax, rax
    je .L971
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17456
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L972
.L971:
.L972:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17256
    jmp .Lret65
.Lret65:
    mov rsp, rbp
    pop rbp
    ret

section code
term_key:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    lea rax, [rbp - 9]
    movsx rax, byte [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L975
    mov rax, 126
    push rax
    lea rax, [rbp - 9]
    movsx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L975
    mov rax, 0
    jmp .L976
.L975:
    mov rax, 1
.L976:
    test rax, rax
    je .L973
    jmp .Lret66
    jmp .L974
.L973:
.L974:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17456
    mov eax, dword [rax]
    push rax
    mov rax, 200
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
    je .L977
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17256
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17456
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 9]
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    jmp .L978
.L977:
.L978:
.Lret66:
    mov rsp, rbp
    pop rbp
    ret

section code
term_rub:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17456
    mov eax, dword [rax]
    test rax, rax
    je .L979
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17456
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, -1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L980
.L979:
.L980:
.Lret67:
    mov rsp, rbp
    pop rbp
    ret

section code
term_clear_line:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17456
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.Lret68:
    mov rsp, rbp
    pop rbp
    ret

section code
term_recall:
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
.L981:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17460
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L983
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    mov rax, 200
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
    je .L983
    mov rax, 1
    jmp .L984
.L983:
    mov rax, 0
.L984:
    test rax, rax
    je .L982
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17256
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17460
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
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
    jmp .L981
.L982:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17456
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.Lret69:
    mov rsp, rbp
    pop rbp
    ret

section code
term_enter:
    push rbp
    mov rbp, rsp
    sub rsp, 224
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17256
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17456
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 212]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L985:
    lea rax, [rbp - 212]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17456
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L987
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17460
    push rax
    lea rax, [rbp - 212]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17256
    push rax
    lea rax, [rbp - 212]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 208]
    push rax
    lea rax, [rbp - 212]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17256
    push rax
    lea rax, [rbp - 212]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L986:
    lea rax, [rbp - 212]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L985
.L987:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 17456
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 208]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call term_line
.Lret70:
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
    jmp .Lret71
.Lret71:
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
private hx.1
    align 1
hx.1:
    db 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102
    db 0
.Ls0: db 104, 111, 109, 101, 0
.Ls1: db 116, 104, 101, 32, 116, 101, 114, 109, 105, 110, 97, 108, 46, 32, 32, 39, 104, 101, 108, 112, 39, 32, 110, 97, 109, 101, 115, 32, 116, 104, 101, 32, 119, 111, 114, 100, 115, 32, 105, 116, 32, 107, 110, 111, 119, 115, 46, 0
.Ls2: db 116, 101, 120, 116, 0
.Ls3: db 98, 121, 116, 101, 115, 0
.Ls4: db 108, 105, 115, 116, 0
.Ls5: db 112, 114, 111, 103, 114, 97, 109, 0
.Ls6: db 112, 105, 99, 116, 117, 114, 101, 0
.Ls7: db 116, 104, 105, 110, 103, 0
.Ls8: db 0
.Ls9: db 110, 111, 116, 104, 105, 110, 103, 32, 104, 101, 114, 101, 32, 105, 115, 32, 99, 97, 108, 108, 101, 100, 32, 0
.Ls10: db 46, 32, 32, 39, 108, 111, 111, 107, 39, 32, 115, 104, 111, 119, 115, 32, 116, 104, 101, 32, 110, 97, 109, 101, 115, 46, 0
.Ls11: db 116, 104, 97, 116, 32, 114, 101, 102, 101, 114, 101, 110, 99, 101, 32, 103, 114, 97, 110, 116, 115, 32, 110, 111, 116, 104, 105, 110, 103, 32, 105, 110, 32, 121, 111, 117, 114, 32, 104, 97, 110, 100, 46, 0
.Ls12: db 40, 117, 110, 110, 97, 109, 101, 100, 41, 0
.Ls13: db 32, 32, 0
.Ls14: db 32, 32, 0
.Ls15: db 32, 32, 0
.Ls16: db 32, 116, 104, 105, 110, 103, 0
.Ls17: db 32, 116, 104, 105, 110, 103, 115, 0
.Ls18: db 32, 108, 101, 116, 116, 101, 114, 115, 0
.Ls19: db 114, 117, 110, 110, 105, 110, 103, 0
.Ls20: db 101, 110, 100, 101, 100, 0
.Ls21: db 32, 98, 121, 116, 101, 115, 0
.Ls22: db 32, 32, 0
.Ls23: db 32, 32, 0
.Ls24: db 32, 32, 0
.Ls25: db 40, 117, 110, 110, 97, 109, 101, 100, 41, 0
.Ls26: db 32, 32, 0
.Ls27: db 32, 62, 32, 0
.Ls28: db 103, 111, 32, 119, 104, 101, 114, 101, 63, 32, 32, 39, 108, 111, 111, 107, 39, 32, 115, 104, 111, 119, 115, 32, 116, 104, 101, 32, 110, 97, 109, 101, 115, 46, 0
.Ls29: db 100, 101, 101, 112, 32, 101, 110, 111, 117, 103, 104, 59, 32, 39, 98, 97, 99, 107, 39, 32, 102, 105, 114, 115, 116, 46, 0
.Ls30: db 0
.Ls31: db 116, 104, 105, 115, 32, 105, 115, 32, 116, 104, 101, 32, 98, 101, 103, 105, 110, 110, 105, 110, 103, 46, 0
.Ls32: db 104, 111, 109, 101, 46, 0
.Ls33: db 46, 46, 46, 97, 110, 100, 32, 0
.Ls34: db 32, 109, 111, 114, 101, 32, 108, 101, 116, 116, 101, 114, 115, 0
.Ls35: db 40, 101, 109, 112, 116, 121, 41, 0
.Ls36: db 32, 98, 121, 116, 101, 115, 59, 32, 116, 104, 101, 32, 102, 105, 114, 115, 116, 32, 114, 111, 119, 115, 32, 111, 102, 32, 116, 104, 101, 109, 58, 0
.Ls37: db 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 97, 98, 99, 100, 101, 102, 0
.Ls38: db 32, 32, 0
.Ls39: db 97, 32, 112, 105, 99, 116, 117, 114, 101, 44, 32, 0
.Ls40: db 32, 98, 121, 32, 0
.Ls41: db 46, 32, 32, 116, 104, 101, 32, 115, 99, 114, 101, 101, 110, 39, 115, 32, 112, 105, 99, 116, 117, 114, 101, 32, 108, 101, 110, 115, 32, 115, 104, 111, 119, 115, 32, 105, 116, 46, 0
.Ls42: db 97, 32, 112, 114, 111, 103, 114, 97, 109, 44, 32, 0
.Ls43: db 114, 117, 110, 110, 105, 110, 103, 0
.Ls44: db 101, 110, 100, 101, 100, 0
.Ls45: db 46, 32, 32, 39, 108, 111, 111, 107, 39, 32, 115, 104, 111, 119, 115, 32, 119, 104, 97, 116, 32, 105, 116, 32, 104, 111, 108, 100, 115, 46, 0
.Ls46: db 39, 108, 111, 111, 107, 39, 32, 105, 115, 32, 116, 104, 101, 32, 119, 97, 121, 32, 116, 111, 32, 114, 101, 97, 100, 32, 116, 104, 105, 115, 32, 107, 105, 110, 100, 46, 0
.Ls47: db 111, 110, 108, 121, 32, 97, 32, 116, 101, 120, 116, 32, 116, 97, 107, 101, 115, 32, 119, 114, 105, 116, 105, 110, 103, 59, 32, 39, 103, 111, 39, 32, 116, 111, 32, 111, 110, 101, 32, 102, 105, 114, 115, 116, 46, 0
.Ls48: db 116, 104, 105, 115, 32, 111, 110, 101, 32, 105, 115, 32, 114, 101, 97, 100, 45, 111, 110, 108, 121, 32, 105, 110, 32, 121, 111, 117, 114, 32, 104, 97, 110, 100, 46, 0
.Ls49: db 119, 114, 105, 116, 101, 32, 119, 104, 97, 116, 63, 0
.Ls50: db 105, 116, 32, 104, 97, 115, 32, 110, 111, 32, 114, 111, 111, 109, 32, 108, 101, 102, 116, 46, 0
.Ls51: db 119, 114, 105, 116, 116, 101, 110, 46, 0
.Ls52: db 116, 101, 120, 116, 0
.Ls53: db 108, 105, 115, 116, 0
.Ls54: db 109, 97, 107, 101, 32, 116, 101, 120, 116, 32, 60, 110, 97, 109, 101, 62, 44, 32, 111, 114, 32, 109, 97, 107, 101, 32, 108, 105, 115, 116, 32, 60, 110, 97, 109, 101, 62, 46, 0
.Ls55: db 110, 97, 109, 101, 32, 105, 116, 46, 0
.Ls56: db 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 108, 97, 121, 32, 116, 104, 105, 110, 103, 115, 32, 105, 110, 32, 104, 101, 114, 101, 46, 0
.Ls57: db 110, 111, 116, 104, 105, 110, 103, 32, 99, 97, 109, 101, 32, 111, 102, 32, 105, 116, 59, 32, 109, 101, 109, 111, 114, 121, 32, 105, 115, 32, 115, 104, 111, 114, 116, 46, 0
.Ls58: db 110, 111, 32, 114, 111, 111, 109, 32, 102, 111, 114, 32, 97, 110, 111, 116, 104, 101, 114, 32, 114, 101, 102, 101, 114, 101, 110, 99, 101, 32, 104, 101, 114, 101, 46, 0
.Ls59: db 32, 32, 108, 105, 101, 115, 32, 104, 101, 114, 101, 32, 110, 111, 119, 44, 32, 115, 108, 111, 116, 32, 0
.Ls60: db 99, 111, 112, 121, 32, 119, 104, 105, 99, 104, 63, 0
.Ls61: db 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 114, 101, 97, 100, 32, 116, 104, 97, 116, 46, 0
.Ls62: db 116, 104, 101, 32, 99, 111, 112, 121, 32, 119, 111, 117, 108, 100, 32, 108, 105, 101, 32, 104, 101, 114, 101, 44, 32, 97, 110, 100, 32, 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 108, 97, 121, 32, 116, 104, 105, 110, 103, 115, 32, 105, 110, 32, 104, 101, 114, 101, 46, 0
.Ls63: db 116, 104, 105, 115, 32, 107, 105, 110, 100, 32, 99, 97, 110, 110, 111, 116, 32, 98, 101, 32, 99, 111, 112, 105, 101, 100, 46, 0
.Ls64: db 110, 111, 116, 104, 105, 110, 103, 32, 99, 97, 109, 101, 32, 111, 102, 32, 105, 116, 59, 32, 109, 101, 109, 111, 114, 121, 32, 105, 115, 32, 115, 104, 111, 114, 116, 46, 0
.Ls65: db 32, 99, 111, 112, 121, 0
.Ls66: db 110, 111, 32, 114, 111, 111, 109, 32, 116, 111, 32, 108, 97, 121, 32, 116, 104, 101, 32, 99, 111, 112, 121, 32, 104, 101, 114, 101, 46, 0
.Ls67: db 32, 32, 108, 105, 101, 115, 32, 98, 101, 115, 105, 100, 101, 32, 105, 116, 46, 0
.Ls68: db 32, 116, 111, 32, 0
.Ls69: db 114, 101, 110, 97, 109, 101, 32, 60, 110, 97, 109, 101, 62, 32, 116, 111, 32, 60, 110, 101, 119, 32, 110, 97, 109, 101, 62, 46, 0
.Ls70: db 116, 104, 101, 32, 110, 97, 109, 101, 32, 108, 105, 118, 101, 115, 32, 111, 110, 32, 116, 104, 105, 115, 32, 104, 111, 108, 100, 101, 114, 44, 32, 97, 110, 100, 32, 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 99, 104, 97, 110, 103, 101, 32, 105, 116, 46, 0
.Ls71: db 32, 32, 105, 115, 32, 110, 111, 119, 32, 99, 97, 108, 108, 101, 100, 32, 32, 0
.Ls72: db 108, 101, 116, 32, 103, 111, 32, 111, 102, 32, 119, 104, 105, 99, 104, 63, 0
.Ls73: db 115, 116, 97, 110, 100, 32, 98, 101, 115, 105, 100, 101, 32, 105, 116, 44, 32, 110, 111, 116, 32, 111, 110, 32, 105, 116, 58, 32, 39, 98, 97, 99, 107, 39, 32, 102, 105, 114, 115, 116, 46, 0
.Ls74: db 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 116, 97, 107, 101, 32, 116, 104, 105, 110, 103, 115, 32, 111, 117, 116, 32, 111, 102, 32, 104, 101, 114, 101, 46, 0
.Ls75: db 98, 105, 110, 0
.Ls76: db 98, 105, 110, 0
.Ls77: db 98, 105, 110, 0
.Ls78: db 108, 101, 116, 32, 103, 111, 44, 32, 102, 111, 114, 32, 103, 111, 111, 100, 46, 0
.Ls79: db 105, 116, 32, 108, 105, 101, 115, 32, 105, 110, 32, 116, 104, 101, 32, 98, 105, 110, 32, 110, 111, 119, 46, 0
.Ls80: db 110, 111, 116, 104, 105, 110, 103, 32, 99, 97, 109, 101, 32, 111, 102, 32, 105, 116, 59, 32, 109, 101, 109, 111, 114, 121, 32, 105, 115, 32, 115, 104, 111, 114, 116, 46, 0
.Ls81: db 32, 99, 111, 100, 101, 0
.Ls82: db 32, 111, 98, 106, 101, 99, 116, 0
.Ls83: db 110, 111, 32, 114, 111, 111, 109, 32, 116, 111, 32, 108, 97, 121, 32, 105, 116, 32, 104, 101, 114, 101, 46, 0
.Ls84: db 32, 32, 108, 105, 101, 115, 32, 98, 101, 115, 105, 100, 101, 32, 105, 116, 58, 32, 0
.Ls85: db 32, 98, 121, 116, 101, 115, 32, 111, 102, 32, 105, 109, 97, 103, 101, 46, 32, 32, 39, 114, 117, 110, 39, 32, 105, 116, 46, 0
.Ls86: db 32, 98, 121, 116, 101, 115, 32, 111, 102, 32, 111, 98, 106, 101, 99, 116, 46, 32, 32, 105, 116, 32, 119, 97, 105, 116, 115, 32, 102, 111, 114, 32, 111, 116, 104, 101, 114, 32, 116, 101, 120, 116, 115, 39, 32, 110, 97, 109, 101, 115, 58, 0
.Ls87: db 32, 32, 0
.Ls88: db 32, 32, 39, 108, 105, 110, 107, 39, 32, 106, 111, 105, 110, 115, 32, 111, 98, 106, 101, 99, 116, 115, 59, 32, 39, 98, 117, 105, 108, 100, 39, 32, 109, 97, 107, 101, 115, 32, 116, 104, 101, 109, 32, 102, 114, 111, 109, 32, 97, 32, 108, 105, 115, 116, 32, 111, 102, 32, 116, 101, 120, 116, 115, 46, 0
.Ls89: db 97, 115, 115, 101, 109, 98, 108, 101, 32, 119, 104, 105, 99, 104, 32, 116, 101, 120, 116, 63, 0
.Ls90: db 111, 110, 108, 121, 32, 97, 32, 116, 101, 120, 116, 32, 99, 97, 110, 32, 98, 101, 32, 97, 115, 115, 101, 109, 98, 108, 101, 100, 46, 0
.Ls91: db 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 114, 101, 97, 100, 32, 116, 104, 97, 116, 46, 0
.Ls92: db 116, 104, 101, 32, 105, 109, 97, 103, 101, 32, 119, 111, 117, 108, 100, 32, 108, 105, 101, 32, 104, 101, 114, 101, 44, 32, 97, 110, 100, 32, 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 108, 97, 121, 32, 116, 104, 105, 110, 103, 115, 32, 105, 110, 32, 104, 101, 114, 101, 46, 0
.Ls93: db 116, 104, 101, 114, 101, 32, 105, 115, 32, 110, 111, 32, 114, 111, 111, 109, 32, 102, 111, 114, 32, 116, 104, 101, 32, 116, 111, 111, 108, 115, 39, 32, 116, 97, 98, 108, 101, 115, 46, 0
.Ls94: db 99, 111, 109, 112, 105, 108, 101, 32, 119, 104, 105, 99, 104, 32, 116, 101, 120, 116, 63, 0
.Ls95: db 111, 110, 108, 121, 32, 97, 32, 116, 101, 120, 116, 32, 99, 97, 110, 32, 98, 101, 32, 99, 111, 109, 112, 105, 108, 101, 100, 46, 0
.Ls96: db 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 114, 101, 97, 100, 32, 116, 104, 97, 116, 46, 0
.Ls97: db 119, 104, 97, 116, 32, 105, 116, 32, 109, 97, 107, 101, 115, 32, 119, 111, 117, 108, 100, 32, 108, 105, 101, 32, 104, 101, 114, 101, 44, 32, 97, 110, 100, 32, 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 108, 97, 121, 32, 116, 104, 105, 110, 103, 115, 32, 105, 110, 32, 104, 101, 114, 101, 46, 0
.Ls98: db 116, 104, 101, 114, 101, 32, 105, 115, 32, 110, 111, 32, 114, 111, 111, 109, 32, 102, 111, 114, 32, 116, 104, 101, 32, 116, 111, 111, 108, 115, 39, 32, 116, 97, 98, 108, 101, 115, 46, 0
.Ls99: db 110, 111, 116, 104, 105, 110, 103, 32, 99, 97, 109, 101, 32, 111, 102, 32, 105, 116, 59, 32, 109, 101, 109, 111, 114, 121, 32, 105, 115, 32, 115, 104, 111, 114, 116, 46, 0
.Ls100: db 32, 97, 115, 109, 0
.Ls101: db 110, 111, 32, 114, 111, 111, 109, 32, 116, 111, 32, 108, 97, 121, 32, 116, 104, 101, 32, 97, 115, 115, 101, 109, 98, 108, 121, 32, 104, 101, 114, 101, 46, 0
.Ls102: db 32, 32, 108, 105, 101, 115, 32, 98, 101, 115, 105, 100, 101, 32, 105, 116, 58, 32, 0
.Ls103: db 32, 108, 101, 116, 116, 101, 114, 115, 32, 111, 102, 32, 97, 115, 115, 101, 109, 98, 108, 121, 46, 0
.Ls104: db 116, 104, 101, 32, 97, 115, 115, 101, 109, 98, 108, 101, 114, 32, 114, 101, 102, 117, 115, 101, 100, 32, 119, 104, 97, 116, 32, 116, 104, 101, 32, 99, 111, 109, 112, 105, 108, 101, 114, 32, 109, 97, 100, 101, 58, 0
.Ls105: db 116, 104, 101, 114, 101, 32, 105, 115, 32, 110, 111, 116, 104, 105, 110, 103, 32, 116, 111, 32, 108, 105, 110, 107, 32, 105, 110, 32, 105, 116, 46, 0
.Ls106: db 107, 109, 97, 105, 110, 0
.Ls107: db 116, 104, 101, 114, 101, 32, 105, 115, 32, 110, 111, 32, 114, 111, 111, 109, 32, 102, 111, 114, 32, 116, 104, 101, 32, 116, 111, 111, 108, 115, 39, 32, 116, 97, 98, 108, 101, 115, 46, 0
.Ls108: db 107, 101, 114, 110, 101, 108, 46, 101, 108, 102, 0
.Ls109: db 32, 99, 111, 100, 101, 0
.Ls110: db 110, 111, 116, 104, 105, 110, 103, 32, 99, 97, 109, 101, 32, 111, 102, 32, 105, 116, 59, 32, 109, 101, 109, 111, 114, 121, 32, 105, 115, 32, 115, 104, 111, 114, 116, 46, 0
.Ls111: db 110, 111, 32, 114, 111, 111, 109, 32, 116, 111, 32, 108, 97, 121, 32, 105, 116, 32, 105, 110, 32, 116, 104, 101, 32, 108, 105, 115, 116, 46, 0
.Ls112: db 98, 117, 105, 108, 100, 58, 32, 37, 115, 44, 32, 37, 108, 108, 117, 32, 98, 121, 116, 101, 115, 32, 102, 114, 111, 109, 32, 37, 117, 32, 111, 98, 106, 101, 99, 116, 115, 10, 0
.Ls113: db 32, 32, 108, 105, 101, 115, 32, 105, 110, 32, 116, 104, 101, 32, 108, 105, 115, 116, 58, 32, 0
.Ls114: db 32, 98, 121, 116, 101, 115, 44, 32, 0
.Ls115: db 32, 111, 98, 106, 101, 99, 116, 115, 44, 32, 116, 104, 101, 32, 107, 101, 114, 110, 101, 108, 39, 115, 32, 115, 104, 97, 112, 101, 46, 32, 32, 39, 119, 114, 105, 116, 101, 32, 111, 117, 116, 39, 32, 99, 97, 114, 114, 105, 101, 115, 32, 105, 116, 32, 116, 111, 32, 116, 104, 101, 32, 100, 105, 115, 107, 46, 0
.Ls116: db 32, 111, 98, 106, 101, 99, 116, 115, 46, 32, 32, 39, 114, 117, 110, 39, 32, 105, 116, 46, 0
.Ls117: db 108, 105, 110, 107, 32, 119, 104, 105, 99, 104, 32, 108, 105, 115, 116, 63, 0
.Ls118: db 108, 105, 110, 107, 32, 116, 97, 107, 101, 115, 32, 97, 32, 108, 105, 115, 116, 32, 111, 102, 32, 111, 98, 106, 101, 99, 116, 115, 46, 0
.Ls119: db 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 114, 101, 97, 100, 32, 116, 104, 97, 116, 46, 0
.Ls120: db 119, 104, 97, 116, 32, 105, 116, 32, 109, 97, 107, 101, 115, 32, 119, 111, 117, 108, 100, 32, 108, 105, 101, 32, 105, 110, 32, 116, 104, 101, 32, 108, 105, 115, 116, 44, 32, 97, 110, 100, 32, 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 108, 97, 121, 32, 116, 104, 105, 110, 103, 115, 32, 105, 110, 32, 116, 104, 101, 114, 101, 46, 0
.Ls121: db 98, 117, 105, 108, 100, 32, 119, 104, 105, 99, 104, 32, 108, 105, 115, 116, 63, 0
.Ls122: db 98, 117, 105, 108, 100, 32, 116, 97, 107, 101, 115, 32, 97, 32, 108, 105, 115, 116, 32, 111, 102, 32, 116, 101, 120, 116, 115, 46, 0
.Ls123: db 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 114, 101, 97, 100, 32, 116, 104, 97, 116, 46, 0
.Ls124: db 119, 104, 97, 116, 32, 105, 116, 32, 109, 97, 107, 101, 115, 32, 119, 111, 117, 108, 100, 32, 108, 105, 101, 32, 105, 110, 32, 116, 104, 101, 32, 108, 105, 115, 116, 44, 32, 97, 110, 100, 32, 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 108, 97, 121, 32, 116, 104, 105, 110, 103, 115, 32, 105, 110, 32, 116, 104, 101, 114, 101, 46, 0
.Ls125: db 116, 104, 101, 114, 101, 32, 105, 115, 32, 110, 111, 32, 114, 111, 111, 109, 32, 102, 111, 114, 32, 116, 104, 101, 32, 116, 111, 111, 108, 115, 39, 32, 116, 97, 98, 108, 101, 115, 46, 0
.Ls126: db 58, 32, 0
.Ls127: db 58, 32, 0
.Ls128: db 32, 32, 0
.Ls129: db 32, 32, 0
.Ls130: db 32, 98, 121, 116, 101, 115, 32, 111, 102, 32, 111, 98, 106, 101, 99, 116, 0
.Ls131: db 116, 104, 101, 114, 101, 32, 105, 115, 32, 110, 111, 32, 116, 101, 120, 116, 32, 111, 102, 32, 99, 32, 111, 114, 32, 97, 115, 115, 101, 109, 98, 108, 121, 32, 105, 110, 32, 105, 116, 46, 0
.Ls132: db 116, 97, 107, 101, 32, 105, 110, 32, 116, 111, 32, 119, 104, 105, 99, 104, 32, 108, 105, 115, 116, 63, 0
.Ls133: db 116, 97, 107, 101, 32, 105, 110, 32, 102, 105, 108, 108, 115, 32, 97, 32, 108, 105, 115, 116, 32, 121, 111, 117, 32, 109, 97, 121, 32, 119, 114, 105, 116, 101, 46, 0
.Ls134: db 116, 104, 101, 114, 101, 32, 105, 115, 32, 110, 111, 32, 101, 120, 99, 104, 97, 110, 103, 101, 32, 100, 105, 115, 107, 46, 0
.Ls135: db 32, 102, 105, 108, 101, 115, 32, 99, 97, 109, 101, 32, 105, 110, 46, 0
.Ls136: db 119, 114, 105, 116, 101, 32, 111, 117, 116, 32, 119, 104, 105, 99, 104, 32, 108, 105, 115, 116, 63, 0
.Ls137: db 119, 114, 105, 116, 101, 32, 111, 117, 116, 32, 116, 97, 107, 101, 115, 32, 97, 32, 108, 105, 115, 116, 32, 121, 111, 117, 32, 109, 97, 121, 32, 114, 101, 97, 100, 46, 0
.Ls138: db 116, 104, 101, 114, 101, 32, 105, 115, 32, 110, 111, 32, 101, 120, 99, 104, 97, 110, 103, 101, 32, 100, 105, 115, 107, 46, 0
.Ls139: db 32, 102, 105, 108, 101, 115, 32, 119, 101, 110, 116, 32, 111, 117, 116, 46, 0
.Ls140: db 114, 117, 110, 32, 119, 104, 105, 99, 104, 32, 116, 101, 120, 116, 44, 32, 111, 114, 32, 119, 104, 105, 99, 104, 32, 105, 109, 97, 103, 101, 63, 0
.Ls141: db 116, 104, 101, 32, 114, 117, 110, 110, 105, 110, 103, 32, 111, 110, 101, 32, 119, 111, 117, 108, 100, 32, 108, 105, 101, 32, 104, 101, 114, 101, 44, 32, 97, 110, 100, 32, 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 108, 97, 121, 32, 116, 104, 105, 110, 103, 115, 32, 105, 110, 32, 104, 101, 114, 101, 46, 0
.Ls142: db 116, 104, 111, 115, 101, 32, 98, 121, 116, 101, 115, 32, 97, 114, 101, 32, 110, 111, 32, 112, 114, 111, 103, 114, 97, 109, 32, 105, 109, 97, 103, 101, 59, 32, 39, 97, 115, 115, 101, 109, 98, 108, 101, 39, 32, 109, 97, 107, 101, 115, 32, 111, 110, 101, 46, 0
.Ls143: db 105, 116, 32, 119, 111, 117, 108, 100, 32, 110, 111, 116, 32, 115, 116, 97, 114, 116, 46, 0
.Ls144: db 105, 116, 32, 114, 117, 110, 115, 44, 32, 98, 117, 116, 32, 116, 104, 101, 114, 101, 32, 119, 97, 115, 32, 110, 111, 32, 114, 111, 111, 109, 32, 116, 111, 32, 108, 97, 121, 32, 105, 116, 32, 104, 101, 114, 101, 46, 0
.Ls145: db 105, 116, 32, 114, 117, 110, 115, 59, 32, 116, 104, 101, 32, 106, 111, 117, 114, 110, 97, 108, 32, 99, 97, 114, 114, 105, 101, 115, 32, 119, 104, 97, 116, 32, 105, 116, 32, 115, 97, 121, 115, 46, 0
.Ls146: db 111, 110, 108, 121, 32, 97, 32, 116, 101, 120, 116, 32, 111, 114, 32, 97, 110, 32, 105, 109, 97, 103, 101, 32, 99, 97, 110, 32, 114, 117, 110, 46, 0
.Ls147: db 105, 116, 32, 119, 111, 117, 108, 100, 32, 110, 111, 116, 32, 115, 116, 97, 114, 116, 46, 0
.Ls148: db 105, 116, 32, 114, 117, 110, 115, 44, 32, 98, 117, 116, 32, 116, 104, 101, 114, 101, 32, 119, 97, 115, 32, 110, 111, 32, 114, 111, 111, 109, 32, 116, 111, 32, 108, 97, 121, 32, 105, 116, 32, 104, 101, 114, 101, 46, 0
.Ls149: db 105, 116, 32, 114, 117, 110, 115, 59, 32, 116, 104, 101, 32, 106, 111, 117, 114, 110, 97, 108, 32, 99, 97, 114, 114, 105, 101, 115, 32, 119, 104, 97, 116, 32, 105, 116, 32, 115, 97, 121, 115, 46, 0
.Ls150: db 32, 116, 111, 32, 0
.Ls151: db 103, 105, 118, 101, 32, 60, 110, 97, 109, 101, 62, 32, 116, 111, 32, 60, 112, 114, 111, 103, 114, 97, 109, 62, 46, 0
.Ls152: db 111, 110, 108, 121, 32, 97, 32, 114, 117, 110, 110, 105, 110, 103, 32, 112, 114, 111, 103, 114, 97, 109, 32, 99, 97, 110, 32, 98, 101, 32, 103, 105, 118, 101, 110, 32, 116, 111, 46, 0
.Ls153: db 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 103, 105, 118, 101, 32, 116, 111, 32, 116, 104, 97, 116, 32, 112, 114, 111, 103, 114, 97, 109, 46, 0
.Ls154: db 105, 116, 32, 99, 111, 117, 108, 100, 32, 110, 111, 116, 32, 98, 101, 32, 104, 97, 110, 100, 101, 100, 32, 111, 118, 101, 114, 46, 0
.Ls155: db 32, 32, 104, 111, 108, 100, 115, 32, 105, 116, 32, 110, 111, 119, 44, 32, 119, 105, 116, 104, 32, 119, 104, 97, 116, 32, 121, 111, 117, 32, 104, 101, 108, 100, 46, 0
.Ls156: db 101, 110, 100, 32, 119, 104, 105, 99, 104, 32, 112, 114, 111, 103, 114, 97, 109, 63, 0
.Ls157: db 111, 110, 108, 121, 32, 97, 32, 112, 114, 111, 103, 114, 97, 109, 32, 99, 97, 110, 32, 98, 101, 32, 101, 110, 100, 101, 100, 46, 0
.Ls158: db 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 101, 110, 100, 32, 116, 104, 97, 116, 32, 111, 110, 101, 46, 0
.Ls159: db 105, 116, 32, 119, 97, 115, 32, 110, 111, 116, 32, 114, 117, 110, 110, 105, 110, 103, 46, 0
.Ls160: db 115, 121, 115, 116, 101, 109, 0
.Ls161: db 97, 32, 112, 114, 111, 103, 114, 97, 109, 32, 119, 97, 115, 32, 101, 110, 100, 101, 100, 32, 98, 121, 32, 104, 97, 110, 100, 0
.Ls162: db 101, 110, 100, 101, 100, 46, 32, 32, 105, 116, 32, 102, 105, 110, 105, 115, 104, 101, 115, 32, 97, 116, 32, 105, 116, 115, 32, 110, 101, 120, 116, 32, 115, 116, 101, 112, 32, 105, 110, 116, 111, 32, 116, 104, 101, 32, 107, 101, 114, 110, 101, 108, 46, 0
.Ls163: db 115, 101, 110, 100, 32, 119, 104, 105, 99, 104, 63, 0
.Ls164: db 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 114, 101, 97, 100, 32, 116, 104, 97, 116, 44, 32, 115, 111, 32, 121, 111, 117, 32, 109, 97, 121, 32, 110, 111, 116, 32, 115, 101, 110, 100, 32, 105, 116, 46, 0
.Ls165: db 111, 110, 32, 105, 116, 115, 32, 119, 97, 121, 32, 116, 111, 32, 116, 104, 101, 32, 112, 101, 101, 114, 46, 0
.Ls166: db 116, 104, 101, 32, 112, 105, 112, 101, 32, 119, 111, 117, 108, 100, 32, 110, 111, 116, 32, 116, 97, 107, 101, 32, 105, 116, 46, 32, 32, 105, 115, 32, 97, 32, 112, 101, 101, 114, 32, 110, 97, 109, 101, 100, 63, 32, 32, 39, 115, 99, 97, 110, 39, 32, 97, 110, 100, 32, 39, 112, 111, 105, 110, 116, 32, 97, 116, 39, 32, 115, 101, 116, 32, 111, 110, 101, 46, 0
.Ls167: db 97, 115, 107, 32, 119, 105, 116, 104, 32, 119, 104, 105, 99, 104, 32, 116, 97, 115, 107, 63, 0
.Ls168: db 97, 32, 116, 97, 115, 107, 32, 105, 115, 32, 97, 32, 116, 101, 120, 116, 46, 0
.Ls169: db 116, 104, 101, 32, 100, 101, 115, 107, 32, 104, 97, 115, 32, 105, 116, 46, 32, 32, 116, 104, 101, 32, 97, 110, 115, 119, 101, 114, 32, 108, 97, 110, 100, 115, 32, 105, 110, 32, 116, 104, 101, 32, 116, 97, 115, 107, 32, 105, 116, 115, 101, 108, 102, 44, 32, 111, 114, 32, 105, 110, 32, 97, 114, 114, 105, 118, 97, 108, 115, 46, 0
.Ls170: db 116, 104, 101, 32, 100, 101, 115, 107, 32, 119, 111, 117, 108, 100, 32, 110, 111, 116, 32, 116, 97, 107, 101, 32, 105, 116, 46, 0
.Ls171: db 115, 97, 121, 32, 119, 104, 97, 116, 63, 0
.Ls172: db 115, 97, 105, 100, 59, 32, 105, 116, 32, 115, 116, 97, 110, 100, 115, 32, 111, 110, 32, 116, 104, 101, 32, 108, 105, 110, 101, 46, 0
.Ls173: db 110, 111, 98, 111, 100, 121, 32, 105, 115, 32, 111, 110, 32, 116, 104, 101, 32, 108, 105, 110, 101, 44, 32, 97, 110, 100, 32, 110, 111, 32, 112, 101, 101, 114, 32, 105, 115, 32, 110, 97, 109, 101, 100, 32, 105, 110, 32, 116, 104, 101, 32, 115, 101, 116, 116, 105, 110, 103, 115, 46, 0
.Ls174: db 116, 104, 101, 32, 99, 97, 108, 108, 32, 105, 115, 32, 111, 117, 116, 46, 32, 32, 39, 102, 111, 117, 110, 100, 39, 32, 115, 104, 111, 119, 115, 32, 119, 104, 111, 32, 97, 110, 115, 119, 101, 114, 101, 100, 46, 0
.Ls175: db 110, 111, 32, 97, 110, 115, 119, 101, 114, 115, 32, 121, 101, 116, 59, 32, 116, 104, 101, 32, 99, 97, 108, 108, 32, 105, 115, 32, 115, 116, 105, 108, 108, 32, 111, 117, 116, 46, 0
.Ls176: db 110, 111, 98, 111, 100, 121, 32, 104, 97, 115, 32, 97, 110, 115, 119, 101, 114, 101, 100, 46, 32, 32, 39, 115, 99, 97, 110, 39, 32, 99, 97, 108, 108, 115, 32, 97, 103, 97, 105, 110, 46, 0
.Ls177: db 32, 32, 0
.Ls178: db 32, 32, 0
.Ls179: db 40, 110, 111, 32, 110, 97, 109, 101, 41, 0
.Ls180: db 32, 32, 116, 97, 107, 101, 115, 32, 119, 111, 114, 107, 44, 32, 0
.Ls181: db 77, 32, 102, 114, 101, 101, 0
.Ls182: db 112, 111, 105, 110, 116, 32, 97, 116, 32, 119, 104, 111, 109, 63, 32, 32, 97, 32, 102, 111, 117, 110, 100, 32, 110, 97, 109, 101, 44, 32, 111, 114, 32, 97, 110, 32, 97, 100, 100, 114, 101, 115, 115, 46, 0
.Ls183: db 116, 104, 97, 116, 32, 110, 97, 109, 101, 115, 32, 110, 111, 32, 109, 97, 99, 104, 105, 110, 101, 32, 105, 32, 99, 97, 110, 32, 115, 101, 101, 46, 0
.Ls184: db 110, 111, 32, 115, 101, 116, 116, 105, 110, 103, 115, 32, 115, 116, 97, 110, 100, 46, 0
.Ls185: db 112, 101, 101, 114, 32, 32, 32, 32, 32, 124, 32, 0
.Ls186: db 116, 104, 101, 32, 115, 101, 116, 116, 105, 110, 103, 115, 32, 112, 97, 103, 101, 32, 104, 97, 115, 32, 110, 111, 32, 114, 111, 111, 109, 32, 108, 101, 102, 116, 46, 0
.Ls187: db 116, 104, 101, 32, 112, 105, 112, 101, 32, 112, 111, 105, 110, 116, 115, 32, 97, 116, 32, 0
.Ls188: db 32, 110, 111, 119, 59, 32, 115, 101, 110, 100, 44, 32, 97, 115, 107, 32, 97, 110, 100, 32, 115, 97, 121, 32, 114, 101, 97, 99, 104, 32, 105, 116, 46, 0
.Ls189: db 102, 105, 110, 100, 32, 119, 104, 97, 116, 63, 0
.Ls190: db 32, 32, 104, 111, 109, 101, 0
.Ls191: db 32, 62, 32, 0
.Ls192: db 40, 117, 110, 110, 97, 109, 101, 100, 41, 0
.Ls193: db 110, 111, 116, 104, 105, 110, 103, 32, 104, 111, 108, 100, 115, 32, 116, 104, 111, 115, 101, 32, 119, 111, 114, 100, 115, 46, 0
.Ls194: db 46, 46, 46, 97, 110, 100, 32, 109, 97, 121, 98, 101, 32, 109, 111, 114, 101, 59, 32, 116, 104, 101, 32, 102, 105, 114, 115, 116, 32, 115, 105, 120, 116, 101, 101, 110, 32, 97, 114, 101, 32, 115, 104, 111, 119, 110, 46, 0
.Ls195: db 40, 116, 104, 101, 32, 119, 97, 108, 107, 32, 119, 97, 115, 32, 99, 117, 116, 32, 115, 104, 111, 114, 116, 59, 32, 116, 104, 101, 32, 103, 114, 97, 112, 104, 32, 105, 115, 32, 108, 97, 114, 103, 101, 114, 32, 116, 104, 97, 110, 32, 116, 104, 101, 32, 115, 101, 97, 114, 99, 104, 46, 41, 0
.Ls196: db 110, 111, 32, 106, 111, 117, 114, 110, 97, 108, 32, 115, 116, 97, 110, 100, 115, 46, 0
.Ls197: db 110, 111, 116, 104, 105, 110, 103, 32, 104, 97, 115, 32, 104, 97, 112, 112, 101, 110, 101, 100, 32, 121, 101, 116, 46, 0
.Ls198: db 32, 32, 45, 45, 32, 32, 117, 112, 32, 0
.Ls199: db 32, 115, 101, 99, 111, 110, 100, 115, 0
.Ls200: db 111, 110, 101, 32, 115, 104, 97, 112, 101, 44, 32, 97, 108, 119, 97, 121, 115, 58, 32, 97, 32, 118, 101, 114, 98, 44, 32, 97, 32, 110, 97, 109, 101, 44, 32, 97, 110, 100, 32, 39, 116, 111, 39, 32, 111, 114, 32, 39, 97, 116, 39, 32, 119, 104, 101, 110, 0
.Ls201: db 116, 119, 111, 32, 116, 104, 105, 110, 103, 115, 32, 109, 101, 101, 116, 46, 32, 32, 110, 97, 109, 101, 115, 32, 109, 97, 121, 32, 104, 97, 118, 101, 32, 115, 112, 97, 99, 101, 115, 59, 32, 110, 117, 109, 98, 101, 114, 115, 32, 99, 111, 117, 110, 116, 32, 115, 108, 111, 116, 115, 46, 0
.Ls202: db 108, 111, 111, 107, 105, 110, 103, 32, 97, 114, 111, 117, 110, 100, 0
.Ls203: db 32, 32, 108, 111, 111, 107, 32, 91, 110, 97, 109, 101, 93, 32, 32, 32, 32, 32, 32, 119, 104, 97, 116, 32, 115, 116, 97, 110, 100, 115, 32, 104, 101, 114, 101, 44, 32, 111, 114, 32, 119, 104, 97, 116, 32, 116, 104, 97, 116, 32, 112, 111, 105, 110, 116, 115, 32, 97, 116, 0
.Ls204: db 32, 32, 103, 111, 32, 60, 110, 97, 109, 101, 62, 32, 32, 32, 32, 32, 32, 32, 32, 102, 111, 108, 108, 111, 119, 32, 97, 32, 114, 101, 102, 101, 114, 101, 110, 99, 101, 0
.Ls205: db 32, 32, 98, 97, 99, 107, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 111, 110, 101, 32, 115, 116, 101, 112, 32, 98, 97, 99, 107, 59, 32, 32, 104, 111, 109, 101, 32, 32, 114, 101, 116, 117, 114, 110, 115, 32, 116, 111, 32, 116, 104, 101, 32, 115, 116, 97, 114, 116, 0
.Ls206: db 32, 32, 119, 104, 101, 114, 101, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 116, 104, 101, 32, 119, 97, 108, 107, 32, 115, 111, 32, 102, 97, 114, 0
.Ls207: db 32, 32, 102, 105, 110, 100, 32, 60, 119, 111, 114, 100, 115, 62, 32, 32, 32, 32, 32, 115, 101, 97, 114, 99, 104, 32, 110, 97, 109, 101, 115, 32, 97, 110, 100, 32, 116, 101, 120, 116, 115, 44, 32, 101, 118, 101, 114, 121, 119, 104, 101, 114, 101, 32, 121, 111, 117, 32, 114, 101, 97, 99, 104, 0
.Ls208: db 116, 104, 105, 110, 103, 115, 0
.Ls209: db 32, 32, 114, 101, 97, 100, 32, 91, 110, 97, 109, 101, 93, 32, 32, 32, 32, 32, 32, 116, 104, 101, 32, 116, 104, 105, 110, 103, 32, 105, 116, 115, 101, 108, 102, 58, 32, 108, 101, 116, 116, 101, 114, 115, 44, 32, 98, 121, 116, 101, 115, 44, 32, 115, 105, 122, 101, 0
.Ls210: db 32, 32, 119, 114, 105, 116, 101, 32, 60, 119, 111, 114, 100, 115, 62, 32, 32, 32, 32, 97, 100, 100, 32, 97, 32, 108, 105, 110, 101, 32, 116, 111, 32, 116, 104, 101, 32, 116, 101, 120, 116, 32, 121, 111, 117, 32, 115, 116, 97, 110, 100, 32, 111, 110, 0
.Ls211: db 32, 32, 109, 97, 107, 101, 32, 116, 101, 120, 116, 32, 60, 110, 97, 109, 101, 62, 32, 32, 32, 32, 32, 97, 32, 102, 114, 101, 115, 104, 32, 116, 101, 120, 116, 44, 32, 108, 97, 105, 100, 32, 105, 110, 32, 104, 101, 114, 101, 0
.Ls212: db 32, 32, 109, 97, 107, 101, 32, 108, 105, 115, 116, 32, 60, 110, 97, 109, 101, 62, 32, 32, 32, 32, 32, 97, 32, 102, 114, 101, 115, 104, 32, 108, 105, 115, 116, 44, 32, 108, 97, 105, 100, 32, 105, 110, 32, 104, 101, 114, 101, 0
.Ls213: db 32, 32, 99, 111, 112, 121, 32, 60, 110, 97, 109, 101, 62, 32, 32, 32, 32, 32, 32, 97, 32, 99, 111, 112, 121, 44, 32, 108, 97, 105, 100, 32, 98, 101, 115, 105, 100, 101, 32, 105, 116, 0
.Ls214: db 32, 32, 114, 101, 110, 97, 109, 101, 32, 60, 110, 97, 109, 101, 62, 32, 116, 111, 32, 60, 110, 101, 119, 32, 110, 97, 109, 101, 62, 0
.Ls215: db 32, 32, 108, 101, 116, 32, 103, 111, 32, 60, 110, 97, 109, 101, 62, 32, 32, 32, 32, 105, 110, 116, 111, 32, 116, 104, 101, 32, 98, 105, 110, 59, 32, 105, 110, 32, 116, 104, 101, 32, 98, 105, 110, 44, 32, 102, 111, 114, 32, 103, 111, 111, 100, 0
.Ls216: db 112, 114, 111, 103, 114, 97, 109, 115, 0
.Ls217: db 32, 32, 114, 117, 110, 32, 60, 110, 97, 109, 101, 62, 32, 32, 32, 32, 32, 32, 32, 114, 117, 110, 32, 116, 104, 97, 116, 32, 116, 101, 120, 116, 44, 32, 111, 114, 32, 116, 104, 97, 116, 32, 105, 109, 97, 103, 101, 44, 32, 97, 115, 32, 97, 32, 112, 114, 111, 103, 114, 97, 109, 44, 32, 104, 101, 114, 101, 0
.Ls218: db 32, 32, 97, 115, 115, 101, 109, 98, 108, 101, 32, 60, 110, 97, 109, 101, 62, 32, 32, 116, 117, 114, 110, 32, 116, 104, 97, 116, 32, 116, 101, 120, 116, 32, 111, 102, 32, 105, 110, 115, 116, 114, 117, 99, 116, 105, 111, 110, 115, 32, 105, 110, 116, 111, 32, 97, 110, 32, 105, 109, 97, 103, 101, 0
.Ls219: db 32, 32, 99, 111, 109, 112, 105, 108, 101, 32, 60, 110, 97, 109, 101, 62, 32, 32, 32, 116, 117, 114, 110, 32, 116, 104, 97, 116, 32, 116, 101, 120, 116, 32, 111, 102, 32, 99, 32, 105, 110, 116, 111, 32, 97, 115, 115, 101, 109, 98, 108, 121, 44, 32, 97, 110, 100, 32, 116, 104, 97, 116, 32, 105, 110, 116, 111, 32, 97, 110, 32, 105, 109, 97, 103, 101, 0
.Ls220: db 32, 32, 108, 105, 110, 107, 32, 60, 108, 105, 115, 116, 62, 32, 32, 32, 32, 32, 32, 106, 111, 105, 110, 32, 116, 104, 101, 32, 111, 98, 106, 101, 99, 116, 115, 32, 105, 110, 32, 97, 32, 108, 105, 115, 116, 32, 105, 110, 116, 111, 32, 111, 110, 101, 32, 105, 109, 97, 103, 101, 44, 32, 111, 114, 32, 97, 32, 107, 101, 114, 110, 101, 108, 0
.Ls221: db 32, 32, 98, 117, 105, 108, 100, 32, 60, 108, 105, 115, 116, 62, 32, 32, 32, 32, 32, 99, 111, 109, 112, 105, 108, 101, 32, 97, 110, 100, 32, 97, 115, 115, 101, 109, 98, 108, 101, 32, 101, 118, 101, 114, 121, 32, 116, 101, 120, 116, 32, 105, 110, 32, 97, 32, 108, 105, 115, 116, 44, 32, 116, 104, 101, 110, 32, 108, 105, 110, 107, 0
.Ls222: db 32, 32, 116, 97, 107, 101, 32, 105, 110, 32, 60, 108, 105, 115, 116, 62, 32, 32, 32, 116, 104, 101, 32, 101, 120, 99, 104, 97, 110, 103, 101, 32, 100, 105, 115, 107, 39, 115, 32, 102, 105, 108, 101, 115, 44, 32, 105, 110, 116, 111, 32, 116, 104, 101, 32, 108, 105, 115, 116, 0
.Ls223: db 32, 32, 119, 114, 105, 116, 101, 32, 111, 117, 116, 32, 60, 108, 105, 115, 116, 62, 32, 116, 104, 101, 32, 108, 105, 115, 116, 39, 115, 32, 116, 101, 120, 116, 115, 32, 97, 110, 100, 32, 98, 121, 116, 101, 115, 44, 32, 111, 110, 116, 111, 32, 116, 104, 101, 32, 101, 120, 99, 104, 97, 110, 103, 101, 32, 100, 105, 115, 107, 0
.Ls224: db 32, 32, 103, 105, 118, 101, 32, 60, 110, 97, 109, 101, 62, 32, 116, 111, 32, 60, 112, 114, 111, 103, 114, 97, 109, 62, 32, 32, 32, 104, 97, 110, 100, 32, 105, 116, 32, 97, 32, 114, 101, 102, 101, 114, 101, 110, 99, 101, 0
.Ls225: db 32, 32, 101, 110, 100, 32, 60, 110, 97, 109, 101, 62, 32, 32, 32, 32, 32, 32, 32, 101, 110, 100, 32, 97, 32, 114, 117, 110, 110, 105, 110, 103, 32, 112, 114, 111, 103, 114, 97, 109, 0
.Ls226: db 116, 104, 101, 32, 111, 116, 104, 101, 114, 32, 109, 97, 99, 104, 105, 110, 101, 115, 0
.Ls227: db 32, 32, 115, 99, 97, 110, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 99, 97, 108, 108, 32, 111, 117, 116, 58, 32, 119, 104, 111, 32, 101, 108, 115, 101, 32, 105, 115, 32, 111, 110, 32, 116, 104, 101, 32, 119, 105, 114, 101, 63, 0
.Ls228: db 32, 32, 102, 111, 117, 110, 100, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 119, 104, 111, 32, 97, 110, 115, 119, 101, 114, 101, 100, 0
.Ls229: db 32, 32, 112, 111, 105, 110, 116, 32, 97, 116, 32, 60, 110, 97, 109, 101, 32, 111, 114, 32, 97, 100, 100, 114, 101, 115, 115, 62, 32, 32, 32, 99, 104, 111, 111, 115, 101, 32, 116, 104, 101, 32, 112, 101, 101, 114, 0
.Ls230: db 32, 32, 115, 101, 110, 100, 32, 60, 110, 97, 109, 101, 62, 32, 32, 32, 32, 32, 32, 99, 97, 114, 114, 121, 32, 97, 32, 116, 104, 105, 110, 103, 32, 116, 111, 32, 116, 104, 101, 32, 112, 101, 101, 114, 0
.Ls231: db 32, 32, 97, 115, 107, 32, 60, 110, 97, 109, 101, 62, 32, 32, 32, 32, 32, 32, 32, 104, 97, 118, 101, 32, 116, 104, 101, 32, 109, 97, 99, 104, 105, 110, 101, 115, 32, 119, 111, 114, 107, 32, 97, 32, 116, 97, 115, 107, 32, 116, 101, 120, 116, 0
.Ls232: db 32, 32, 115, 97, 121, 32, 60, 119, 111, 114, 100, 115, 62, 32, 32, 32, 32, 32, 32, 115, 112, 101, 97, 107, 32, 111, 110, 32, 116, 104, 101, 32, 108, 105, 110, 101, 0
.Ls233: db 116, 104, 101, 32, 109, 97, 99, 104, 105, 110, 101, 0
.Ls234: db 32, 32, 106, 111, 117, 114, 110, 97, 108, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 116, 104, 101, 32, 108, 97, 115, 116, 32, 116, 104, 105, 110, 103, 115, 32, 116, 104, 97, 116, 32, 104, 97, 112, 112, 101, 110, 101, 100, 0
.Ls235: db 32, 32, 116, 105, 109, 101, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 116, 104, 101, 32, 119, 97, 108, 108, 32, 99, 108, 111, 99, 107, 44, 32, 97, 110, 100, 32, 104, 111, 119, 32, 108, 111, 110, 103, 32, 105, 116, 32, 104, 97, 115, 32, 114, 117, 110, 0
.Ls236: db 62, 32, 0
.Ls237: db 0
.Ls238: db 104, 101, 108, 112, 0
.Ls239: db 108, 111, 111, 107, 0
.Ls240: db 119, 104, 101, 114, 101, 0
.Ls241: db 103, 111, 0
.Ls242: db 98, 97, 99, 107, 0
.Ls243: db 104, 111, 109, 101, 0
.Ls244: db 102, 105, 110, 100, 0
.Ls245: db 114, 101, 97, 100, 0
.Ls246: db 119, 114, 105, 116, 101, 32, 111, 117, 116, 0
.Ls247: db 116, 97, 107, 101, 32, 105, 110, 0
.Ls248: db 119, 114, 105, 116, 101, 0
.Ls249: db 109, 97, 107, 101, 0
.Ls250: db 99, 111, 112, 121, 0
.Ls251: db 114, 101, 110, 97, 109, 101, 0
.Ls252: db 108, 101, 116, 32, 103, 111, 0
.Ls253: db 114, 117, 110, 0
.Ls254: db 97, 115, 115, 101, 109, 98, 108, 101, 0
.Ls255: db 99, 111, 109, 112, 105, 108, 101, 0
.Ls256: db 108, 105, 110, 107, 0
.Ls257: db 98, 117, 105, 108, 100, 0
.Ls258: db 103, 105, 118, 101, 0
.Ls259: db 101, 110, 100, 0
.Ls260: db 115, 101, 110, 100, 0
.Ls261: db 97, 115, 107, 0
.Ls262: db 115, 97, 121, 0
.Ls263: db 115, 99, 97, 110, 0
.Ls264: db 102, 111, 117, 110, 100, 0
.Ls265: db 112, 111, 105, 110, 116, 32, 97, 116, 0
.Ls266: db 106, 111, 117, 114, 110, 97, 108, 0
.Ls267: db 116, 105, 109, 101, 0
.Ls268: db 105, 32, 100, 111, 32, 110, 111, 116, 32, 107, 110, 111, 119, 32, 39, 0
.Ls269: db 39, 46, 32, 32, 39, 104, 101, 108, 112, 39, 32, 110, 97, 109, 101, 115, 32, 116, 104, 101, 32, 119, 111, 114, 100, 115, 46, 0

section bss
private sessions
    align 8
sessions:
    res 52992
private troot
    align 8
troot:
    res 8
private troot_rights
    align 4
troot_rights:
    res 4
private arena
    align 8
arena:
    res 8
private units
    align 8
units:
    res 3072
private unit_names
    align 1
unit_names:
    res 5120
private seen.2
    align 8
seen.2:
    res 2048
private parent.3
    align 4
parent.3:
    res 1024
private label.4
    align 1
label.4:
    res 6144
