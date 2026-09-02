; made by the compiler; the source lies beside this
section code
    mov rbp, rsp
    call f_main
    mov rdi, rax
    mov rax, 0
    syscall

f_fail:
    push rbp
    mov rbp, rsp
    sub rsp, 80
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163884
    movzx rax, byte [rax]
    test rax, rax
    je .L1
    jmp .Lret1
    jmp .L2
.L1:
.L2:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163884
    push rax
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 28]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 40]
    push rax
    lea rax, [.Ls0]
    pop rdi
    mov [rdi], rax
.L3:
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L5
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163896
    mov eax, dword [rax]
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
    je .L5
    mov rax, 1
    jmp .L6
.L5:
    mov rax, 0
.L6:
    test rax, rax
    je .L4
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163888
    mov rax, [rax]
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
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
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
    jmp .L3
.L4:
    lea rax, [rbp - 56]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 60]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163880
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 60]
    mov eax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L7
    lea rax, [rbp - 52]
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
    mov rax, 48
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    jmp .L8
.L7:
.L8:
.L9:
    lea rax, [rbp - 60]
    mov eax, dword [rax]
    test rax, rax
    je .L10
    lea rax, [rbp - 52]
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
    mov rax, 48
    push rax
    lea rax, [rbp - 60]
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
    lea rax, [rbp - 60]
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
    jmp .L9
.L10:
.L11:
    lea rax, [rbp - 56]
    mov eax, dword [rax]
    test rax, rax
    je .L13
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163896
    mov eax, dword [rax]
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
    je .L13
    mov rax, 1
    jmp .L14
.L13:
    mov rax, 0
.L14:
    test rax, rax
    je .L12
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163888
    mov rax, [rax]
    push rax
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
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 52]
    push rax
    lea rax, [rbp - 56]
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
    jmp .L11
.L12:
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163896
    mov eax, dword [rax]
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
    je .L15
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163888
    mov rax, [rax]
    push rax
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
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 58
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    jmp .L16
.L15:
.L16:
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163896
    mov eax, dword [rax]
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
    je .L17
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163888
    mov rax, [rax]
    push rax
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
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 32
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    jmp .L18
.L17:
.L18:
    lea rax, [rbp - 64]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L19:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 64]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L22
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163896
    mov eax, dword [rax]
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
    je .L22
    mov rax, 1
    jmp .L23
.L22:
    mov rax, 0
.L23:
    test rax, rax
    je .L21
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163888
    mov rax, [rax]
    push rax
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
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 64]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L20:
    lea rax, [rbp - 64]
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
.L21:
    lea rax, [rbp - 24]
    mov rax, [rax]
    test rax, rax
    je .L26
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L26
    mov rax, 1
    jmp .L27
.L26:
    mov rax, 0
.L27:
    test rax, rax
    je .L24
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163896
    mov eax, dword [rax]
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
    je .L28
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163888
    mov rax, [rax]
    push rax
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
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 32
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    jmp .L29
.L28:
.L29:
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163896
    mov eax, dword [rax]
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
    je .L30
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163888
    mov rax, [rax]
    push rax
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
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 39
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    jmp .L31
.L30:
.L31:
    lea rax, [rbp - 68]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L32:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 68]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L35
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163896
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L35
    mov rax, 1
    jmp .L36
.L35:
    mov rax, 0
.L36:
    test rax, rax
    je .L34
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163888
    mov rax, [rax]
    push rax
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
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 24]
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
.L33:
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
    jmp .L32
.L34:
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163896
    mov eax, dword [rax]
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
    je .L37
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163888
    mov rax, [rax]
    push rax
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
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 39
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    jmp .L38
.L37:
.L38:
    jmp .L25
.L24:
.L25:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163888
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.Lret1:
    mov rsp, rbp
    pop rbp
    ret

f_here:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 28
    movzx rax, byte [rax]
    test rax, rax
    je .L39
    mov rax, 17825792
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 24
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    jmp .L40
.L39:
    mov rax, 16777216
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
.L40:
    jmp .Lret2
.Lret2:
    mov rsp, rbp
    pop rbp
    ret

f_emit8:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 28
    movzx rax, byte [rax]
    test rax, rax
    je .L41
    mov rax, 256
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 24
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L43
    mov rax, 0
    push rax
    lea rax, [.Ls1]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret3
    jmp .L44
.L43:
.L44:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 24
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
    movzx rax, byte [rax]
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L42
.L41:
    mov rax, 256
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L45
    mov rax, 0
    push rax
    lea rax, [.Ls2]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret3
    jmp .L46
.L45:
.L46:
    lea rax, [rbp - 8]
    mov rax, [rax]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 8
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
    movzx rax, byte [rax]
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L42:
.Lret3:
    mov rsp, rbp
    pop rbp
    ret

f_emit32:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov dword [rbp - 12], esi
    lea rax, [rbp - 16]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L47:
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L49
    lea rax, [rbp - 12]
    mov eax, dword [rax]
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
    mov rcx, rdi
    shr rax, cl
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
.L48:
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
    jmp .L47
.L49:
.Lret4:
    mov rsp, rbp
    pop rbp
    ret

f_emit64:
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
.L50:
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L52
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shr rax, cl
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
.L51:
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
    jmp .L50
.L52:
.Lret5:
    mov rsp, rbp
    pop rbp
    ret

f_emit16:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov word [rbp - 10], si
    lea rax, [rbp - 10]
    movzx rax, word [rax]
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 10]
    movzx rax, word [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shr rax, cl
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
.Lret6:
    mov rsp, rbp
    pop rbp
    ret

f_emit_imm:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov [rbp - 24], rdx
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L53
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, ax
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit16
    jmp .L54
.L53:
    lea rax, [rbp - 24]
    mov rax, [rax]
    mov eax, eax
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit32
.L54:
.Lret7:
    mov rsp, rbp
    pop rbp
    ret

f_patch32:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov dword [rbp - 20], edx
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 28
    movzx rax, byte [rax]
    test rax, rax
    je .L55
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 16
    mov rax, [rax]
    jmp .L56
.L55:
    lea rax, [rbp - 8]
    mov rax, [rax]
    mov rax, [rax]
.L56:
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 28
    movzx rax, byte [rax]
    test rax, rax
    je .L57
    mov rax, 17825792
    jmp .L58
.L57:
    mov rax, 16777216
.L58:
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 52]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L59:
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L61
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shr rax, cl
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L60:
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
    jmp .L59
.L61:
.Lret8:
    mov rsp, rbp
    pop rbp
    ret

f_label_find:
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
.L62:
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163872
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L64
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 32
    push rax
    lea rax, [rbp - 28]
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
    pop rdi
    pop rsi
    call f_strcmp
    movsxd rax, eax
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L65
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 32
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 32
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    mov rax, 1
    movzx rax, al
    jmp .Lret9
    jmp .L66
.L65:
.L66:
.L63:
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
    jmp .L62
.L64:
    mov rax, 0
    movzx rax, al
    jmp .Lret9
.Lret9:
    mov rsp, rbp
    pop rbp
    ret

f_label_set:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163876
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L67
    jmp .Lret10
    jmp .L68
.L67:
.L68:
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
    pop rdx
    call f_label_find
    movzx rax, al
    test rax, rax
    je .L69
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [.Ls3]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret10
    jmp .L70
.L69:
.L70:
    mov rax, 4096
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163872
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L71
    mov rax, 0
    push rax
    lea rax, [.Ls4]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret10
    jmp .L72
.L71:
.L72:
    lea rax, [rbp - 36]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L73:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L75
    lea rax, [rbp - 36]
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
    je .L75
    mov rax, 1
    jmp .L76
.L75:
    mov rax, 0
.L76:
    test rax, rax
    je .L74
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 32
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163872
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
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
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
    jmp .L73
.L74:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 32
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163872
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
    lea rax, [rbp - 36]
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
    add rax, 32
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163872
    mov eax, dword [rax]
    push rax
    mov rax, 40
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 32
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163872
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
.Lret10:
    mov rsp, rbp
    pop rbp
    ret

f_name_addr:
    push rbp
    mov rbp, rsp
    sub rsp, 32
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
    pop rdx
    call f_label_find
    movzx rax, al
    test rax, rax
    je .L77
    lea rax, [rbp - 24]
    mov rax, [rax]
    jmp .Lret11
    jmp .L78
.L77:
.L78:
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163876
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L79
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [.Ls5]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .L80
.L79:
.L80:
    mov rax, 0
    jmp .Lret11
.Lret11:
    mov rsp, rbp
    pop rbp
    ret

f_is_name_char:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov byte [rbp - 1], dil
    mov rax, 97
    push rax
    lea rax, [rbp - 1]
    movsx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L89
    lea rax, [rbp - 1]
    movsx rax, byte [rax]
    push rax
    mov rax, 122
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L89
    mov rax, 1
    jmp .L90
.L89:
    mov rax, 0
.L90:
    test rax, rax
    jne .L87
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
    je .L91
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
    je .L91
    mov rax, 1
    jmp .L92
.L91:
    mov rax, 0
.L92:
    test rax, rax
    jne .L87
    mov rax, 0
    jmp .L88
.L87:
    mov rax, 1
.L88:
    test rax, rax
    jne .L85
    mov rax, 48
    push rax
    lea rax, [rbp - 1]
    movsx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L93
    lea rax, [rbp - 1]
    movsx rax, byte [rax]
    push rax
    mov rax, 57
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L93
    mov rax, 1
    jmp .L94
.L93:
    mov rax, 0
.L94:
    test rax, rax
    jne .L85
    mov rax, 0
    jmp .L86
.L85:
    mov rax, 1
.L86:
    test rax, rax
    jne .L83
    lea rax, [rbp - 1]
    movsx rax, byte [rax]
    push rax
    mov rax, 95
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L83
    mov rax, 0
    jmp .L84
.L83:
    mov rax, 1
.L84:
    test rax, rax
    jne .L81
    lea rax, [rbp - 1]
    movsx rax, byte [rax]
    push rax
    mov rax, 46
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L81
    mov rax, 0
    jmp .L82
.L81:
    mov rax, 1
.L82:
    movzx rax, al
    jmp .Lret12
.Lret12:
    mov rsp, rbp
    pop rbp
    ret

f_number:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    mov dword [rbp - 12], esi
    mov [rbp - 24], rdx
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
    je .L95
    mov rax, 0
    movzx rax, al
    jmp .Lret13
    jmp .L96
.L95:
.L96:
    lea rax, [rbp - 25]
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 45
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L97
    lea rax, [rbp - 25]
    push rax
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    push rax
    mov rax, 1
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L98
.L97:
.L98:
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L99
    mov rax, 0
    movzx rax, al
    jmp .Lret13
    jmp .L100
.L99:
.L100:
    lea rax, [rbp - 40]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L105
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 39
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L105
    mov rax, 1
    jmp .L106
.L105:
    mov rax, 0
.L106:
    test rax, rax
    je .L103
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 39
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L103
    mov rax, 1
    jmp .L104
.L103:
    mov rax, 0
.L104:
    test rax, rax
    je .L101
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    movzx rax, al
    pop rdi
    mov [rdi], rax
    jmp .L102
.L101:
    mov rax, 2
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L111
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
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
    sete al
    movzx rax, al
    test rax, rax
    je .L111
    mov rax, 1
    jmp .L112
.L111:
    mov rax, 0
.L112:
    test rax, rax
    je .L109
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 120
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L113
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 88
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L113
    mov rax, 0
    jmp .L114
.L113:
    mov rax, 1
.L114:
    test rax, rax
    je .L109
    mov rax, 1
    jmp .L110
.L109:
    mov rax, 0
.L110:
    test rax, rax
    je .L107
    lea rax, [rbp - 44]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L115:
    lea rax, [rbp - 44]
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
    je .L117
    lea rax, [rbp - 45]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 44]
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
    lea rax, [rbp - 45]
    movsx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L120
    lea rax, [rbp - 45]
    movsx rax, byte [rax]
    push rax
    mov rax, 57
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L120
    mov rax, 1
    jmp .L121
.L120:
    mov rax, 0
.L121:
    test rax, rax
    je .L118
    lea rax, [rbp - 52]
    push rax
    lea rax, [rbp - 45]
    movsx rax, byte [rax]
    push rax
    mov rax, 48
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L119
.L118:
    mov rax, 97
    push rax
    lea rax, [rbp - 45]
    movsx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L124
    lea rax, [rbp - 45]
    movsx rax, byte [rax]
    push rax
    mov rax, 102
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L124
    mov rax, 1
    jmp .L125
.L124:
    mov rax, 0
.L125:
    test rax, rax
    je .L122
    lea rax, [rbp - 52]
    push rax
    lea rax, [rbp - 45]
    movsx rax, byte [rax]
    push rax
    mov rax, 97
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    add rax, rdi
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L123
.L122:
    mov rax, 65
    push rax
    lea rax, [rbp - 45]
    movsx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L128
    lea rax, [rbp - 45]
    movsx rax, byte [rax]
    push rax
    mov rax, 70
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L128
    mov rax, 1
    jmp .L129
.L128:
    mov rax, 0
.L129:
    test rax, rax
    je .L126
    lea rax, [rbp - 52]
    push rax
    lea rax, [rbp - 45]
    movsx rax, byte [rax]
    push rax
    mov rax, 65
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    add rax, rdi
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L127
.L126:
    mov rax, 0
    movzx rax, al
    jmp .Lret13
.L127:
.L123:
.L119:
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    or rax, rdi
    pop rdi
    mov [rdi], rax
.L116:
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
    jmp .L115
.L117:
    jmp .L108
.L107:
    lea rax, [rbp - 56]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L130:
    lea rax, [rbp - 56]
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
    je .L132
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
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
    jne .L135
    mov rax, 57
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
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
    jne .L135
    mov rax, 0
    jmp .L136
.L135:
    mov rax, 1
.L136:
    test rax, rax
    je .L133
    mov rax, 0
    movzx rax, al
    jmp .Lret13
    jmp .L134
.L133:
.L134:
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
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
.L131:
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
    jmp .L130
.L132:
.L108:
.L102:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    test rax, rax
    je .L137
    lea rax, [rbp - 40]
    mov rax, [rax]
    neg rax
    jmp .L138
.L137:
    lea rax, [rbp - 40]
    mov rax, [rax]
.L138:
    pop rdi
    mov [rdi], rax
    mov rax, 1
    movzx rax, al
    jmp .Lret13
.Lret13:
    mov rsp, rbp
    pop rbp
    ret

f_reg_named:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov dword [rbp - 12], esi
    mov [rbp - 24], rdx
    mov [rbp - 32], rcx
    lea rax, [rbp - 36]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L139:
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L141
    lea rax, [v_regs64]
    push rax
    lea rax, [rbp - 36]
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
    call f_strlen
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L144
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [v_regs64]
    push rax
    lea rax, [rbp - 36]
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
    pop rdx
    call f_memcmp
    movsxd rax, eax
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L144
    mov rax, 1
    jmp .L145
.L144:
    mov rax, 0
.L145:
    test rax, rax
    je .L142
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 64
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    mov rax, 1
    movzx rax, al
    jmp .Lret14
    jmp .L143
.L142:
.L143:
    lea rax, [v_regs32]
    push rax
    lea rax, [rbp - 36]
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
    call f_strlen
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L148
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [v_regs32]
    push rax
    lea rax, [rbp - 36]
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
    pop rdx
    call f_memcmp
    movsxd rax, eax
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L148
    mov rax, 1
    jmp .L149
.L148:
    mov rax, 0
.L149:
    test rax, rax
    je .L146
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 32
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    mov rax, 1
    movzx rax, al
    jmp .Lret14
    jmp .L147
.L146:
.L147:
    lea rax, [v_regs16]
    push rax
    lea rax, [rbp - 36]
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
    call f_strlen
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L152
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [v_regs16]
    push rax
    lea rax, [rbp - 36]
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
    pop rdx
    call f_memcmp
    movsxd rax, eax
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L152
    mov rax, 1
    jmp .L153
.L152:
    mov rax, 0
.L153:
    test rax, rax
    je .L150
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 16
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    mov rax, 1
    movzx rax, al
    jmp .Lret14
    jmp .L151
.L150:
.L151:
    lea rax, [v_regs8]
    push rax
    lea rax, [rbp - 36]
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
    call f_strlen
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L156
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [v_regs8]
    push rax
    lea rax, [rbp - 36]
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
    pop rdx
    call f_memcmp
    movsxd rax, eax
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
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
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 8
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    mov rax, 1
    movzx rax, al
    jmp .Lret14
    jmp .L155
.L154:
.L155:
    lea rax, [v_regsx]
    push rax
    lea rax, [rbp - 36]
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
    call f_strlen
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L160
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [v_regsx]
    push rax
    lea rax, [rbp - 36]
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
    pop rdx
    call f_memcmp
    movsxd rax, eax
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L160
    mov rax, 1
    jmp .L161
.L160:
    mov rax, 0
.L161:
    test rax, rax
    je .L158
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 128
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    mov rax, 1
    movzx rax, al
    jmp .Lret14
    jmp .L159
.L158:
.L159:
.L140:
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
    jmp .L139
.L141:
    mov rax, 0
    movzx rax, al
    jmp .Lret14
.Lret14:
    mov rsp, rbp
    pop rbp
    ret

f_skip_sp:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov dword [rbp - 12], esi
    mov dword [rbp - 16], edx
.L162:
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
    je .L164
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
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
    jne .L166
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 9
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L166
    mov rax, 0
    jmp .L167
.L166:
    mov rax, 1
.L167:
    test rax, rax
    je .L164
    mov rax, 1
    jmp .L165
.L164:
    mov rax, 0
.L165:
    test rax, rax
    je .L163
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
    jmp .L162
.L163:
    lea rax, [rbp - 16]
    mov eax, dword [rax]
    mov eax, eax
    jmp .Lret15
.Lret15:
    mov rsp, rbp
    pop rbp
    ret

f_parse_mem:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov dword [rbp - 20], edx
    mov [rbp - 32], rcx
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 3
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 4
    push rax
    mov rax, 1
    neg rax
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 8
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 16
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
    lea rax, [rbp - 36]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_skip_sp
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L168:
    lea rax, [rbp - 40]
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
    je .L170
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
    pop rdi
    call f_is_name_char
    movzx rax, al
    test rax, rax
    je .L170
    mov rax, 1
    jmp .L171
.L170:
    mov rax, 0
.L171:
    test rax, rax
    je .L169
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
    jmp .L168
.L169:
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L172
    mov rax, 0
    push rax
    lea rax, [.Ls86]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 0
    movzx rax, al
    jmp .Lret16
    jmp .L173
.L172:
.L173:
    lea rax, [rbp - 42]
    push rax
    lea rax, [rbp - 41]
    push rax
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_reg_named
    movzx rax, al
    test rax, rax
    je .L174
    lea rax, [rbp - 42]
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L176
    mov rax, 0
    push rax
    lea rax, [.Ls87]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 0
    movzx rax, al
    jmp .Lret16
    jmp .L177
.L176:
.L177:
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 4
    push rax
    lea rax, [rbp - 41]
    movzx rax, byte [rax]
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
    jmp .L175
.L174:
    lea rax, [rbp - 48]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L178:
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 48]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L180
    lea rax, [rbp - 48]
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
    je .L180
    mov rax, 1
    jmp .L181
.L180:
    mov rax, 0
.L181:
    test rax, rax
    je .L179
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 16
    push rax
    lea rax, [rbp - 48]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 48]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
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
    jmp .L178
.L179:
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 16
    push rax
    lea rax, [rbp - 48]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 16
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_name_addr
    pop rdi
    mov [rdi], rax
.L175:
    lea rax, [rbp - 36]
    push rax
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_skip_sp
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L182
    mov rax, 1
    movzx rax, al
    jmp .Lret16
    jmp .L183
.L182:
.L183:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 43
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L186
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 45
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L186
    mov rax, 1
    jmp .L187
.L186:
    mov rax, 0
.L187:
    test rax, rax
    je .L184
    mov rax, 0
    push rax
    lea rax, [.Ls88]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 0
    movzx rax, al
    jmp .Lret16
    jmp .L185
.L184:
.L185:
    lea rax, [rbp - 49]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 45
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 36]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_skip_sp
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 64]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_number
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L188
    mov rax, 0
    push rax
    lea rax, [.Ls89]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 0
    movzx rax, al
    jmp .Lret16
    jmp .L189
.L188:
.L189:
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 49]
    movzx rax, byte [rax]
    test rax, rax
    je .L190
    lea rax, [rbp - 64]
    mov rax, [rax]
    neg rax
    jmp .L191
.L190:
    lea rax, [rbp - 64]
    mov rax, [rax]
.L191:
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
    mov rax, 1
    movzx rax, al
    jmp .Lret16
.Lret16:
    mov rsp, rbp
    pop rbp
    ret

f_parse_operand:
    push rbp
    mov rbp, rsp
    sub rsp, 96
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov dword [rbp - 20], edx
    mov [rbp - 32], rcx
    mov rax, 48
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_memset
    lea rax, [rbp - 36]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_skip_sp
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L192:
    lea rax, [rbp - 36]
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
    je .L194
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
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
    jne .L196
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
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
    mov rax, 9
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L196
    mov rax, 0
    jmp .L197
.L196:
    mov rax, 1
.L197:
    test rax, rax
    je .L194
    mov rax, 1
    jmp .L195
.L194:
    mov rax, 0
.L195:
    test rax, rax
    je .L193
    lea rax, [rbp - 20]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, -1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L192
.L193:
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L198
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    mov rax, 1
    movzx rax, al
    jmp .Lret17
    jmp .L199
.L198:
.L199:
    lea rax, [rbp - 37]
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    mov rax, 5
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L202
    mov rax, 5
    push rax
    lea rax, [.Ls90]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_memcmp
    movsxd rax, eax
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L202
    mov rax, 1
    jmp .L203
.L202:
    mov rax, 0
.L203:
    test rax, rax
    je .L200
    lea rax, [rbp - 37]
    push rax
    mov rax, 8
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 36]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    mov rax, 5
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_skip_sp
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L201
.L200:
    mov rax, 5
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L206
    mov rax, 5
    push rax
    lea rax, [.Ls91]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_memcmp
    movsxd rax, eax
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L206
    mov rax, 1
    jmp .L207
.L206:
    mov rax, 0
.L207:
    test rax, rax
    je .L204
    lea rax, [rbp - 37]
    push rax
    mov rax, 16
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 36]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    mov rax, 5
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_skip_sp
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L205
.L204:
    mov rax, 6
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L210
    mov rax, 6
    push rax
    lea rax, [.Ls92]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_memcmp
    movsxd rax, eax
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L210
    mov rax, 1
    jmp .L211
.L210:
    mov rax, 0
.L211:
    test rax, rax
    je .L208
    lea rax, [rbp - 37]
    push rax
    mov rax, 32
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 36]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    mov rax, 6
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_skip_sp
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L209
.L208:
    mov rax, 6
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L214
    mov rax, 6
    push rax
    lea rax, [.Ls93]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_memcmp
    movsxd rax, eax
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L214
    mov rax, 1
    jmp .L215
.L214:
    mov rax, 0
.L215:
    test rax, rax
    je .L212
    lea rax, [rbp - 37]
    push rax
    mov rax, 64
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 36]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    mov rax, 6
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_skip_sp
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L213
.L212:
.L213:
.L209:
.L205:
.L201:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 91
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L216
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
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
    mov rax, 93
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L218
    mov rax, 0
    push rax
    lea rax, [.Ls94]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 0
    movzx rax, al
    jmp .Lret17
    jmp .L219
.L218:
.L219:
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
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
    pop rdx
    pop rcx
    call f_parse_mem
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L220
    mov rax, 0
    movzx rax, al
    jmp .Lret17
    jmp .L221
.L220:
.L221:
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 1
    push rax
    lea rax, [rbp - 37]
    movzx rax, byte [rax]
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    mov rax, 1
    movzx rax, al
    jmp .Lret17
    jmp .L217
.L216:
.L217:
    lea rax, [rbp - 37]
    movzx rax, byte [rax]
    test rax, rax
    je .L222
    mov rax, 0
    push rax
    lea rax, [.Ls95]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 0
    movzx rax, al
    jmp .Lret17
    jmp .L223
.L222:
.L223:
    lea rax, [rbp - 39]
    push rax
    lea rax, [rbp - 38]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_reg_named
    movzx rax, al
    test rax, rax
    je .L224
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 2
    push rax
    lea rax, [rbp - 38]
    movzx rax, byte [rax]
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 1
    push rax
    lea rax, [rbp - 39]
    movzx rax, byte [rax]
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    mov rax, 1
    movzx rax, al
    jmp .Lret17
    jmp .L225
.L224:
.L225:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_number
    movzx rax, al
    test rax, rax
    je .L226
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 2
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    mov rax, 1
    movzx rax, al
    jmp .Lret17
    jmp .L227
.L226:
.L227:
    lea rax, [rbp - 52]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L228:
    lea rax, [rbp - 52]
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
    je .L230
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
    pop rdi
    call f_is_name_char
    movzx rax, al
    test rax, rax
    je .L230
    mov rax, 1
    jmp .L231
.L230:
    mov rax, 0
.L231:
    test rax, rax
    je .L229
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
    jmp .L228
.L229:
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L234
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L234
    mov rax, 1
    jmp .L235
.L234:
    mov rax, 0
.L235:
    test rax, rax
    je .L232
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 4
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 56]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L236:
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 56]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L238
    lea rax, [rbp - 56]
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
    je .L238
    mov rax, 1
    jmp .L239
.L238:
    mov rax, 0
.L239:
    test rax, rax
    je .L237
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 16
    push rax
    lea rax, [rbp - 56]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 56]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
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
    jmp .L236
.L237:
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 16
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
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 8
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 16
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_name_addr
    pop rdi
    mov [rdi], rax
    mov rax, 1
    movzx rax, al
    jmp .Lret17
    jmp .L233
.L232:
.L233:
    lea rax, [rbp - 92]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L240:
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L242
    lea rax, [rbp - 92]
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
    je .L242
    mov rax, 1
    jmp .L243
.L242:
    mov rax, 0
.L243:
    test rax, rax
    je .L241
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
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
    jmp .L240
.L241:
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
    lea rax, [rbp - 88]
    push rax
    lea rax, [.Ls96]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 0
    movzx rax, al
    jmp .Lret17
.Lret17:
    mov rsp, rbp
    pop rbp
    ret

f_fits8:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov rax, 128
    neg rax
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L244
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 127
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L244
    mov rax, 1
    jmp .L245
.L244:
    mov rax, 0
.L245:
    movzx rax, al
    jmp .Lret18
.Lret18:
    mov rsp, rbp
    pop rbp
    ret

f_fits32:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov rax, 2147483648
    neg rax
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L246
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 2147483647
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L246
    mov rax, 1
    jmp .L247
.L246:
    mov rax, 0
.L247:
    movzx rax, al
    jmp .Lret19
.Lret19:
    mov rsp, rbp
    pop rbp
    ret

f_rex:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov byte [rbp - 10], dl
    mov byte [rbp - 11], cl
    mov byte [rbp - 12], r8b
    mov byte [rbp - 13], r9b
    lea rax, [rbp - 14]
    push rax
    mov rax, 64
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    test rax, rax
    je .L248
    lea rax, [rbp - 14]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    mov r8, rax
    movzx rax, byte [rax]
    or rax, rdi
    mov rdi, r8
    movzx rax, al
    mov byte [rdi], al
    jmp .L249
.L248:
.L249:
    lea rax, [rbp - 10]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    je .L250
    lea rax, [rbp - 14]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    mov r8, rax
    movzx rax, byte [rax]
    or rax, rdi
    mov rdi, r8
    movzx rax, al
    mov byte [rdi], al
    jmp .L251
.L250:
.L251:
    lea rax, [rbp - 11]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    je .L252
    lea rax, [rbp - 14]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    mov r8, rax
    movzx rax, byte [rax]
    or rax, rdi
    mov rdi, r8
    movzx rax, al
    mov byte [rdi], al
    jmp .L253
.L252:
.L253:
    lea rax, [rbp - 12]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    and rax, rdi
    test rax, rax
    je .L254
    lea rax, [rbp - 14]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov r8, rax
    movzx rax, byte [rax]
    or rax, rdi
    mov rdi, r8
    movzx rax, al
    mov byte [rdi], al
    jmp .L255
.L254:
.L255:
    lea rax, [rbp - 14]
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L258
    lea rax, [rbp - 13]
    movzx rax, byte [rax]
    test rax, rax
    jne .L258
    mov rax, 0
    jmp .L259
.L258:
    mov rax, 1
.L259:
    test rax, rax
    je .L256
    lea rax, [rbp - 14]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L257
.L256:
.L257:
.Lret20:
    mov rsp, rbp
    pop rbp
    ret

f_byte_needs_rex:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L266
    lea rax, [rbp - 8]
    mov rax, [rax]
    test rax, rax
    je .L266
    mov rax, 1
    jmp .L267
.L266:
    mov rax, 0
.L267:
    test rax, rax
    je .L264
    lea rax, [rbp - 8]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L264
    mov rax, 1
    jmp .L265
.L264:
    mov rax, 0
.L265:
    test rax, rax
    je .L262
    mov rax, 4
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
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
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L260
    mov rax, 1
    jmp .L261
.L260:
    mov rax, 0
.L261:
    movzx rax, al
    jmp .Lret21
.Lret21:
    mov rsp, rbp
    pop rbp
    ret

f_mem_tail:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov [rbp - 24], rdx
    mov dword [rbp - 28], ecx
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 4
    movsxd rax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L268
    mov rax, 5
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    and rax, rdi
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call f_here
    pop rdi
    mov [rdi], rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit32
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call f_here
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov eax, eax
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
    call f_patch32
    jmp .Lret22
    jmp .L269
.L268:
.L269:
    lea rax, [rbp - 49]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 4
    movsxd rax, dword [rax]
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L272
    lea rax, [rbp - 49]
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    and rax, rdi
    push rax
    mov rax, 5
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L272
    mov rax, 1
    jmp .L273
.L272:
    mov rax, 0
.L273:
    test rax, rax
    je .L270
    lea rax, [rbp - 50]
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L271
.L270:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    push rax
    pop rdi
    call f_fits8
    movzx rax, al
    test rax, rax
    je .L274
    lea rax, [rbp - 50]
    push rax
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L275
.L274:
    lea rax, [rbp - 50]
    push rax
    mov rax, 2
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L275:
.L271:
    lea rax, [rbp - 50]
    movzx rax, byte [rax]
    push rax
    mov rax, 6
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    and rax, rdi
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    lea rax, [rbp - 49]
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    and rax, rdi
    mov rdi, rax
    pop rax
    or rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 49]
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    and rax, rdi
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L276
    mov rax, 36
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L277
.L276:
.L277:
    lea rax, [rbp - 50]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L278
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    movsx rax, al
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L279
.L278:
.L279:
    lea rax, [rbp - 50]
    movzx rax, byte [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L280
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    push rax
    pop rdi
    call f_fits32
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L282
    mov rax, 0
    push rax
    lea rax, [.Ls97]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret22
    jmp .L283
.L282:
.L283:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    mov eax, eax
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit32
    jmp .L281
.L280:
.L281:
.Lret22:
    mov rsp, rbp
    pop rbp
    ret

f_modrm_rr:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov byte [rbp - 10], dl
    mov rax, 192
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    and rax, rdi
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    mov rcx, rdi
    shl rax, cl
    mov rdi, rax
    pop rax
    or rax, rdi
    push rax
    lea rax, [rbp - 10]
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    and rax, rdi
    mov rdi, rax
    pop rax
    or rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
.Lret23:
    mov rsp, rbp
    pop rbp
    ret

f_rm_reg:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov byte [rbp - 10], dl
    mov [rbp - 24], rcx
    mov [rbp - 32], r8
    mov byte [rbp - 33], r9b
    lea rax, [rbp - 34]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L284
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 4
    movsxd rax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L286
    mov rax, 0
    jmp .L287
.L286:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 4
    movsxd rax, dword [rax]
    movzx rax, al
.L287:
    jmp .L285
.L284:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
.L285:
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L288
    mov rax, 102
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L289
.L288:
.L289:
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_byte_needs_rex
    movzx rax, al
    test rax, rax
    jne .L290
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_byte_needs_rex
    movzx rax, al
    test rax, rax
    jne .L290
    mov rax, 0
    jmp .L291
.L290:
    mov rax, 1
.L291:
    push rax
    lea rax, [rbp - 34]
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L292
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    jmp .L293
.L292:
    lea rax, [rbp - 10]
    movzx rax, byte [rax]
.L293:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L294
    mov rax, 0
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_mem_tail
    jmp .L295
.L294:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_modrm_rr
.L295:
.Lret24:
    mov rsp, rbp
    pop rbp
    ret

f_rm_digit:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov byte [rbp - 10], dl
    mov byte [rbp - 11], cl
    mov [rbp - 24], r8
    mov byte [rbp - 25], r9b
    lea rax, [rbp - 26]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L296
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 4
    movsxd rax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L298
    mov rax, 0
    jmp .L299
.L298:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 4
    movsxd rax, dword [rax]
    movzx rax, al
.L299:
    jmp .L297
.L296:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
.L297:
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L300
    mov rax, 102
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L301
.L300:
.L301:
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_byte_needs_rex
    movzx rax, al
    push rax
    lea rax, [rbp - 26]
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L302
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    jmp .L303
.L302:
    lea rax, [rbp - 10]
    movzx rax, byte [rax]
.L303:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L304
    lea rax, [rbp + 16]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 11]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_mem_tail
    jmp .L305
.L304:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 11]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_modrm_rr
.L305:
.Lret25:
    mov rsp, rbp
    pop rbp
    ret

f_sse_op:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov byte [rbp - 10], dl
    mov byte [rbp - 11], cl
    mov [rbp - 24], r8
    mov [rbp - 32], r9
    lea rax, [rbp - 33]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L306
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 4
    movsxd rax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L308
    mov rax, 0
    jmp .L309
.L308:
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 4
    movsxd rax, dword [rax]
    movzx rax, al
.L309:
    jmp .L307
.L306:
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
.L307:
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    test rax, rax
    je .L310
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L311
.L310:
.L311:
    mov rax, 0
    push rax
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 11]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 15
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 10]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 32]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L312
    mov rax, 0
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_mem_tail
    jmp .L313
.L312:
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_modrm_rr
.L313:
.Lret26:
    mov rsp, rbp
    pop rbp
    ret

f_is_xmm:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L314
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 128
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L314
    mov rax, 1
    jmp .L315
.L314:
    mov rax, 0
.L315:
    movzx rax, al
    jmp .Lret27
.Lret27:
    mov rsp, rbp
    pop rbp
    ret

f_is_r64:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L316
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L316
    mov rax, 1
    jmp .L317
.L316:
    mov rax, 0
.L317:
    movzx rax, al
    jmp .Lret28
.Lret28:
    mov rsp, rbp
    pop rbp
    ret

f_do_sse:
    push rbp
    mov rbp, rsp
    sub rsp, 288
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov dword [rbp - 20], edx
    mov [rbp - 32], rcx
    mov [rbp - 40], r8
    lea rax, [rbp - 280]
    mov rdi, rax
    lea rsi, [.Li0]
    mov rcx, 240
.L318:
    mov al, byte [rsi]
    mov byte [rdi], al
    inc rsi
    inc rdi
    dec rcx
    jne .L318
    lea rax, [rbp - 284]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L319:
    lea rax, [rbp - 284]
    mov eax, dword [rax]
    push rax
    mov rax, 240
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cqo
    idiv rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L321
    lea rax, [rbp - 280]
    push rax
    lea rax, [rbp - 284]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L322
    jmp .L320
    jmp .L323
.L322:
.L323:
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    pop rdi
    call f_is_xmm
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L326
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call f_is_xmm
    movzx rax, al
    test rax, rax
    jne .L328
    lea rax, [rbp - 40]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L328
    mov rax, 0
    jmp .L329
.L328:
    mov rax, 1
.L329:
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L326
    mov rax, 0
    jmp .L327
.L326:
    mov rax, 1
.L327:
    test rax, rax
    je .L324
    mov rax, 0
    push rax
    lea rax, [.Ls113]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 1
    movzx rax, al
    jmp .Lret29
    jmp .L325
.L324:
.L325:
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 280]
    push rax
    lea rax, [rbp - 284]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 9
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 280]
    push rax
    lea rax, [rbp - 284]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 8
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_sse_op
    mov rax, 1
    movzx rax, al
    jmp .Lret29
.L320:
    lea rax, [rbp - 284]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L319
.L321:
    lea rax, [.Ls114]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    jne .L332
    lea rax, [.Ls115]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    jne .L332
    mov rax, 0
    jmp .L333
.L332:
    mov rax, 1
.L333:
    test rax, rax
    je .L330
    lea rax, [rbp - 285]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 3
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
    je .L336
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 100
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L336
    mov rax, 1
    jmp .L337
.L336:
    mov rax, 0
.L337:
    test rax, rax
    je .L334
    mov rax, 242
    jmp .L335
.L334:
    mov rax, 243
.L335:
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    pop rdi
    call f_is_xmm
    movzx rax, al
    test rax, rax
    je .L340
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call f_is_xmm
    movzx rax, al
    test rax, rax
    jne .L342
    lea rax, [rbp - 40]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L342
    mov rax, 0
    jmp .L343
.L342:
    mov rax, 1
.L343:
    test rax, rax
    je .L340
    mov rax, 1
    jmp .L341
.L340:
    mov rax, 0
.L341:
    test rax, rax
    je .L338
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 16
    push rax
    lea rax, [rbp - 285]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_sse_op
    mov rax, 1
    movzx rax, al
    jmp .Lret29
    jmp .L339
.L338:
.L339:
    lea rax, [rbp - 32]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L346
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call f_is_xmm
    movzx rax, al
    test rax, rax
    je .L346
    mov rax, 1
    jmp .L347
.L346:
    mov rax, 0
.L347:
    test rax, rax
    je .L344
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 17
    push rax
    lea rax, [rbp - 285]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_sse_op
    mov rax, 1
    movzx rax, al
    jmp .Lret29
    jmp .L345
.L344:
.L345:
    mov rax, 0
    push rax
    lea rax, [.Ls116]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 1
    movzx rax, al
    jmp .Lret29
    jmp .L331
.L330:
.L331:
    lea rax, [.Ls117]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    jne .L350
    lea rax, [.Ls118]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    jne .L350
    mov rax, 0
    jmp .L351
.L350:
    mov rax, 1
.L351:
    test rax, rax
    je .L348
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    pop rdi
    call f_is_xmm
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L354
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call f_is_r64
    movzx rax, al
    test rax, rax
    jne .L356
    lea rax, [rbp - 40]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L358
    lea rax, [rbp - 40]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 64
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
    jne .L356
    mov rax, 0
    jmp .L357
.L356:
    mov rax, 1
.L357:
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L354
    mov rax, 0
    jmp .L355
.L354:
    mov rax, 1
.L355:
    test rax, rax
    je .L352
    mov rax, 0
    push rax
    lea rax, [.Ls119]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 1
    movzx rax, al
    jmp .Lret29
    jmp .L353
.L352:
.L353:
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 42
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 100
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L360
    mov rax, 242
    jmp .L361
.L360:
    mov rax, 243
.L361:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_sse_op
    mov rax, 1
    movzx rax, al
    jmp .Lret29
    jmp .L349
.L348:
.L349:
    lea rax, [.Ls120]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    jne .L364
    lea rax, [.Ls121]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    jne .L364
    mov rax, 0
    jmp .L365
.L364:
    mov rax, 1
.L365:
    test rax, rax
    je .L362
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    pop rdi
    call f_is_r64
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L368
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call f_is_xmm
    movzx rax, al
    test rax, rax
    jne .L370
    lea rax, [rbp - 40]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L370
    mov rax, 0
    jmp .L371
.L370:
    mov rax, 1
.L371:
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L368
    mov rax, 0
    jmp .L369
.L368:
    mov rax, 1
.L369:
    test rax, rax
    je .L366
    mov rax, 0
    push rax
    lea rax, [.Ls122]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 1
    movzx rax, al
    jmp .Lret29
    jmp .L367
.L366:
.L367:
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 44
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 5
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 100
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L372
    mov rax, 242
    jmp .L373
.L372:
    mov rax, 243
.L373:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_sse_op
    mov rax, 1
    movzx rax, al
    jmp .Lret29
    jmp .L363
.L362:
.L363:
    lea rax, [.Ls123]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L374
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    pop rdi
    call f_is_xmm
    movzx rax, al
    test rax, rax
    je .L378
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call f_is_r64
    movzx rax, al
    test rax, rax
    je .L378
    mov rax, 1
    jmp .L379
.L378:
    mov rax, 0
.L379:
    test rax, rax
    je .L376
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 110
    push rax
    mov rax, 102
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_sse_op
    mov rax, 1
    movzx rax, al
    jmp .Lret29
    jmp .L377
.L376:
.L377:
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    pop rdi
    call f_is_r64
    movzx rax, al
    test rax, rax
    je .L382
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    pop rdi
    call f_is_xmm
    movzx rax, al
    test rax, rax
    je .L382
    mov rax, 1
    jmp .L383
.L382:
    mov rax, 0
.L383:
    test rax, rax
    je .L380
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    mov rax, 1
    push rax
    mov rax, 126
    push rax
    mov rax, 102
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_sse_op
    mov rax, 1
    movzx rax, al
    jmp .Lret29
    jmp .L381
.L380:
.L381:
    mov rax, 0
    push rax
    lea rax, [.Ls124]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 1
    movzx rax, al
    jmp .Lret29
    jmp .L375
.L374:
.L375:
    mov rax, 0
    movzx rax, al
    jmp .Lret29
.Lret29:
    mov rsp, rbp
    pop rbp
    ret

f_width_of:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    lea rax, [rbp - 16]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L384
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    movzx rax, al
    jmp .Lret30
    jmp .L385
.L384:
.L385:
    lea rax, [rbp - 24]
    mov rax, [rax]
    test rax, rax
    je .L388
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L388
    mov rax, 1
    jmp .L389
.L388:
    mov rax, 0
.L389:
    test rax, rax
    je .L386
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    movzx rax, al
    jmp .Lret30
    jmp .L387
.L386:
.L387:
    lea rax, [rbp - 16]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L392
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    test rax, rax
    je .L392
    mov rax, 1
    jmp .L393
.L392:
    mov rax, 0
.L393:
    test rax, rax
    je .L390
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    movzx rax, al
    jmp .Lret30
    jmp .L391
.L390:
.L391:
    lea rax, [rbp - 24]
    mov rax, [rax]
    test rax, rax
    je .L398
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
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
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    test rax, rax
    je .L396
    mov rax, 1
    jmp .L397
.L396:
    mov rax, 0
.L397:
    test rax, rax
    je .L394
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    movzx rax, al
    jmp .Lret30
    jmp .L395
.L394:
.L395:
    mov rax, 0
    push rax
    lea rax, [.Ls125]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 0
    movzx rax, al
    jmp .Lret30
.Lret30:
    mov rsp, rbp
    pop rbp
    ret

f_arith:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov [rbp - 24], rdx
    mov [rbp - 32], rcx
    lea rax, [rbp - 33]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_width_of
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L400
    jmp .Lret31
    jmp .L401
.L400:
.L401:
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L406
    lea rax, [rbp - 32]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L406
    mov rax, 1
    jmp .L407
.L406:
    mov rax, 0
.L407:
    test rax, rax
    je .L404
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L404
    mov rax, 1
    jmp .L405
.L404:
    mov rax, 0
.L405:
    test rax, rax
    je .L402
    mov rax, 0
    push rax
    lea rax, [.Ls126]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret31
    jmp .L403
.L402:
.L403:
    lea rax, [rbp - 32]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L410
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L412
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L412
    mov rax, 0
    jmp .L413
.L412:
    mov rax, 1
.L413:
    test rax, rax
    je .L410
    mov rax, 1
    jmp .L411
.L410:
    mov rax, 0
.L411:
    test rax, rax
    je .L408
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_reg
    jmp .Lret31
    jmp .L409
.L408:
.L409:
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L416
    lea rax, [rbp - 32]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L416
    mov rax, 1
    jmp .L417
.L416:
    mov rax, 0
.L417:
    test rax, rax
    je .L414
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_reg
    jmp .Lret31
    jmp .L415
.L414:
.L415:
    lea rax, [rbp - 32]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L420
    lea rax, [rbp - 32]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L420
    mov rax, 0
    jmp .L421
.L420:
    mov rax, 1
.L421:
    test rax, rax
    je .L418
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L422
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call f_fits8
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L426
    mov rax, 0
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L428
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    mov rax, 255
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    je .L428
    mov rax, 1
    jmp .L429
.L428:
    mov rax, 0
.L429:
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L426
    mov rax, 1
    jmp .L427
.L426:
    mov rax, 0
.L427:
    test rax, rax
    je .L424
    mov rax, 0
    push rax
    lea rax, [.Ls127]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret31
    jmp .L425
.L424:
.L425:
    mov rax, 1
    push rax
    mov rax, 8
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 128
    push rax
    mov rax, 128
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_digit
    add rsp, 8
    lea rax, [rbp - 48]
    mov rax, [rax]
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L423
.L422:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call f_fits8
    movzx rax, al
    test rax, rax
    je .L430
    mov rax, 1
    push rax
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 131
    push rax
    mov rax, 131
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_digit
    add rsp, 8
    lea rax, [rbp - 48]
    mov rax, [rax]
    movsx rax, al
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L431
.L430:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call f_fits32
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L432
    mov rax, 0
    push rax
    lea rax, [.Ls128]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret31
    jmp .L433
.L432:
.L433:
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L434
    mov rax, 2
    jmp .L435
.L434:
    mov rax, 4
.L435:
    push rax
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 129
    push rax
    mov rax, 129
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_digit
    add rsp, 8
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_emit_imm
.L431:
.L423:
    jmp .Lret31
    jmp .L419
.L418:
.L419:
    mov rax, 0
    push rax
    lea rax, [.Ls129]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
.Lret31:
    mov rsp, rbp
    pop rbp
    ret

f_do_mov:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    lea rax, [rbp - 16]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L438
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L440
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L440
    mov rax, 0
    jmp .L441
.L440:
    mov rax, 1
.L441:
    test rax, rax
    je .L438
    mov rax, 1
    jmp .L439
.L438:
    mov rax, 0
.L439:
    test rax, rax
    je .L436
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L442
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    pop rdi
    call f_fits32
    movzx rax, al
    test rax, rax
    je .L446
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L446
    mov rax, 1
    jmp .L447
.L446:
    mov rax, 0
.L447:
    test rax, rax
    je .L444
    mov rax, 0
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    mov rax, 1
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 199
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_modrm_rr
    lea rax, [rbp - 32]
    mov rax, [rax]
    mov eax, eax
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit32
    jmp .L445
.L444:
    mov rax, 0
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    mov rax, 1
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 184
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    and rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit64
.L445:
    jmp .L443
.L442:
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L448
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 2147483648
    neg rax
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L452
    mov rax, 4294967295
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L452
    mov rax, 0
    jmp .L453
.L452:
    mov rax, 1
.L453:
    test rax, rax
    je .L450
    mov rax, 0
    push rax
    lea rax, [.Ls130]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret32
    jmp .L451
.L450:
.L451:
    mov rax, 0
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 184
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    and rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 32]
    mov rax, [rax]
    mov eax, eax
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit32
    jmp .L449
.L448:
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L454
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 32768
    neg rax
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L458
    mov rax, 65535
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L458
    mov rax, 0
    jmp .L459
.L458:
    mov rax, 1
.L459:
    test rax, rax
    je .L456
    mov rax, 0
    push rax
    lea rax, [.Ls131]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret32
    jmp .L457
.L456:
.L457:
    mov rax, 102
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    mov rax, 0
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 184
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    and rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 32]
    mov rax, [rax]
    movzx rax, ax
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit16
    jmp .L455
.L454:
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 128
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L460
    mov rax, 0
    push rax
    lea rax, [.Ls132]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret32
    jmp .L461
.L460:
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 128
    neg rax
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L464
    mov rax, 255
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L464
    mov rax, 0
    jmp .L465
.L464:
    mov rax, 1
.L465:
    test rax, rax
    je .L462
    mov rax, 0
    push rax
    lea rax, [.Ls133]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret32
    jmp .L463
.L462:
.L463:
    mov rax, 8
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_byte_needs_rex
    movzx rax, al
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 176
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    and rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 32]
    mov rax, [rax]
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
.L461:
.L455:
.L449:
.L443:
    jmp .Lret32
    jmp .L437
.L436:
.L437:
    lea rax, [rbp - 16]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L468
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L470
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L470
    mov rax, 0
    jmp .L471
.L470:
    mov rax, 1
.L471:
    test rax, rax
    je .L468
    mov rax, 1
    jmp .L469
.L468:
    mov rax, 0
.L469:
    test rax, rax
    je .L466
    lea rax, [rbp - 33]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L472
    mov rax, 0
    push rax
    lea rax, [.Ls134]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret32
    jmp .L473
.L472:
.L473:
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L474
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    mov rax, 128
    neg rax
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L478
    mov rax, 255
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L478
    mov rax, 0
    jmp .L479
.L478:
    mov rax, 1
.L479:
    test rax, rax
    je .L476
    mov rax, 0
    push rax
    lea rax, [.Ls135]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret32
    jmp .L477
.L476:
.L477:
    mov rax, 1
    push rax
    mov rax, 8
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 198
    push rax
    mov rax, 198
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_digit
    add rsp, 8
    lea rax, [rbp - 48]
    mov rax, [rax]
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L475
.L474:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    pop rdi
    call f_fits32
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L480
    mov rax, 0
    push rax
    lea rax, [.Ls136]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret32
    jmp .L481
.L480:
.L481:
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L482
    mov rax, 2
    jmp .L483
.L482:
    mov rax, 4
.L483:
    push rax
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 199
    push rax
    mov rax, 199
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_digit
    add rsp, 8
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_emit_imm
.L475:
    jmp .Lret32
    jmp .L467
.L466:
.L467:
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L486
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 128
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L486
    mov rax, 1
    jmp .L487
.L486:
    mov rax, 0
.L487:
    test rax, rax
    je .L484
    mov rax, 0
    push rax
    lea rax, [.Ls137]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret32
    jmp .L485
.L484:
.L485:
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L490
    lea rax, [rbp - 16]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L492
    lea rax, [rbp - 16]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L492
    mov rax, 0
    jmp .L493
.L492:
    mov rax, 1
.L493:
    test rax, rax
    je .L490
    mov rax, 1
    jmp .L491
.L490:
    mov rax, 0
.L491:
    test rax, rax
    je .L488
    lea rax, [rbp - 49]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 16]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L496
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 49]
    movzx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L496
    mov rax, 1
    jmp .L497
.L496:
    mov rax, 0
.L497:
    test rax, rax
    je .L494
    mov rax, 0
    push rax
    lea rax, [.Ls138]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret32
    jmp .L495
.L494:
.L495:
    lea rax, [rbp - 49]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 137
    push rax
    mov rax, 136
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_reg
    jmp .Lret32
    jmp .L489
.L488:
.L489:
    lea rax, [rbp - 16]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L500
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
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
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 139
    push rax
    mov rax, 138
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_reg
    jmp .Lret32
    jmp .L499
.L498:
.L499:
    mov rax, 0
    push rax
    lea rax, [.Ls139]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
.Lret32:
    mov rsp, rbp
    pop rbp
    ret

f_do_movzx:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    mov byte [rbp - 25], cl
    lea rax, [rbp - 16]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L506
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L506
    mov rax, 0
    jmp .L507
.L506:
    mov rax, 1
.L507:
    test rax, rax
    jne .L504
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 128
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L504
    mov rax, 0
    jmp .L505
.L504:
    mov rax, 1
.L505:
    test rax, rax
    je .L502
    mov rax, 0
    push rax
    lea rax, [.Ls140]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret33
    jmp .L503
.L502:
.L503:
    lea rax, [rbp - 26]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L508
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    jmp .L509
.L508:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
.L509:
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 26]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L514
    lea rax, [rbp - 26]
    movzx rax, byte [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L514
    mov rax, 0
    jmp .L515
.L514:
    mov rax, 1
.L515:
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L512
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L516
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L516
    mov rax, 0
    jmp .L517
.L516:
    mov rax, 1
.L517:
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L512
    mov rax, 0
    jmp .L513
.L512:
    mov rax, 1
.L513:
    test rax, rax
    je .L510
    mov rax, 0
    push rax
    lea rax, [.Ls141]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret33
    jmp .L511
.L510:
.L511:
    lea rax, [rbp - 27]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L518
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 4
    movsxd rax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L520
    mov rax, 0
    jmp .L521
.L520:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 4
    movsxd rax, dword [rax]
    movzx rax, al
.L521:
    jmp .L519
.L518:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
.L519:
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 26]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_byte_needs_rex
    movzx rax, al
    push rax
    lea rax, [rbp - 27]
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 15
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    test rax, rax
    je .L522
    mov rax, 190
    jmp .L523
.L522:
    mov rax, 182
.L523:
    push rax
    lea rax, [rbp - 26]
    movzx rax, byte [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L524
    mov rax, 1
    jmp .L525
.L524:
    mov rax, 0
.L525:
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L526
    mov rax, 0
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_mem_tail
    jmp .L527
.L526:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_modrm_rr
.L527:
.Lret33:
    mov rsp, rbp
    pop rbp
    ret

f_do_movsxd:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    lea rax, [rbp - 16]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L530
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L530
    mov rax, 0
    jmp .L531
.L530:
    mov rax, 1
.L531:
    test rax, rax
    je .L528
    mov rax, 0
    push rax
    lea rax, [.Ls142]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret34
    jmp .L529
.L528:
.L529:
    lea rax, [rbp - 25]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L534
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L534
    mov rax, 1
    jmp .L535
.L534:
    mov rax, 0
.L535:
    test rax, rax
    jne .L532
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L536
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 32
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L536
    mov rax, 1
    jmp .L537
.L536:
    mov rax, 0
.L537:
    test rax, rax
    jne .L532
    mov rax, 0
    jmp .L533
.L532:
    mov rax, 1
.L533:
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L538
    mov rax, 0
    push rax
    lea rax, [.Ls143]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret34
    jmp .L539
.L538:
.L539:
    lea rax, [rbp - 26]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L540
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 4
    movsxd rax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L542
    mov rax, 0
    jmp .L543
.L542:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 4
    movsxd rax, dword [rax]
    movzx rax, al
.L543:
    jmp .L541
.L540:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
.L541:
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    mov rax, 0
    push rax
    lea rax, [rbp - 26]
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 99
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L544
    mov rax, 0
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_mem_tail
    jmp .L545
.L544:
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_modrm_rr
.L545:
.Lret34:
    mov rsp, rbp
    pop rbp
    ret

f_do_setcc:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov [rbp - 24], rdx
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L548
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L548
    mov rax, 0
    jmp .L549
.L548:
    mov rax, 1
.L549:
    test rax, rax
    je .L546
    mov rax, 0
    push rax
    lea rax, [.Ls144]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret35
    jmp .L547
.L546:
.L547:
    mov rax, 8
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_byte_needs_rex
    movzx rax, al
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 15
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    mov rax, 144
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 128
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_modrm_rr
.Lret35:
    mov rsp, rbp
    pop rbp
    ret

f_do_lea:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    lea rax, [rbp - 16]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L554
    lea rax, [rbp - 16]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L554
    mov rax, 0
    jmp .L555
.L554:
    mov rax, 1
.L555:
    test rax, rax
    jne .L552
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L552
    mov rax, 0
    jmp .L553
.L552:
    mov rax, 1
.L553:
    test rax, rax
    je .L550
    mov rax, 0
    push rax
    lea rax, [.Ls145]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret36
    jmp .L551
.L550:
.L551:
    mov rax, 64
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 141
    push rax
    mov rax, 141
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_reg
.Lret36:
    mov rsp, rbp
    pop rbp
    ret

f_do_test:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    lea rax, [rbp - 25]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
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
    call f_width_of
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L556
    jmp .Lret37
    jmp .L557
.L556:
.L557:
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L558
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 133
    push rax
    mov rax, 132
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_reg
    jmp .Lret37
    jmp .L559
.L558:
.L559:
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L560
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L562
    mov rax, 1
    push rax
    mov rax, 8
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 246
    push rax
    mov rax, 246
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_digit
    add rsp, 8
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L563
.L562:
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L564
    mov rax, 2
    jmp .L565
.L564:
    mov rax, 4
.L565:
    push rax
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 247
    push rax
    mov rax, 247
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_digit
    add rsp, 8
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    push rax
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_emit_imm
.L563:
    jmp .Lret37
    jmp .L561
.L560:
.L561:
    mov rax, 0
    push rax
    lea rax, [.Ls146]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
.Lret37:
    mov rsp, rbp
    pop rbp
    ret

f_unary:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov byte [rbp - 10], dl
    mov byte [rbp - 11], cl
    mov [rbp - 24], r8
    lea rax, [rbp - 25]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_width_of
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L566
    jmp .Lret38
    jmp .L567
.L566:
.L567:
    mov rax, 0
    push rax
    lea rax, [rbp - 25]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 11]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 10]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_digit
    add rsp, 8
.Lret38:
    mov rsp, rbp
    pop rbp
    ret

f_shift:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov [rbp - 24], rdx
    mov [rbp - 32], rcx
    lea rax, [rbp - 33]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_width_of
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L568
    jmp .Lret39
    jmp .L569
.L568:
.L569:
    lea rax, [rbp - 32]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L570
    mov rax, 1
    push rax
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 193
    push rax
    mov rax, 192
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_digit
    add rsp, 8
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L571
.L570:
    lea rax, [rbp - 32]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L576
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L576
    mov rax, 1
    jmp .L577
.L576:
    mov rax, 0
.L577:
    test rax, rax
    je .L574
    lea rax, [rbp - 32]
    mov rax, [rax]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L574
    mov rax, 1
    jmp .L575
.L574:
    mov rax, 0
.L575:
    test rax, rax
    je .L572
    mov rax, 0
    push rax
    lea rax, [rbp - 33]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 211
    push rax
    mov rax, 210
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_digit
    add rsp, 8
    jmp .L573
.L572:
    mov rax, 0
    push rax
    lea rax, [.Ls147]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
.L573:
.L571:
.Lret39:
    mov rsp, rbp
    pop rbp
    ret

f_jump_rel:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov byte [rbp - 10], dl
    mov [rbp - 24], rcx
    lea rax, [rbp - 24]
    mov rax, [rax]
    movzx rax, byte [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L578
    mov rax, 0
    push rax
    lea rax, [.Ls148]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret40
    jmp .L579
.L578:
.L579:
    lea rax, [rbp - 10]
    movzx rax, byte [rax]
    test rax, rax
    je .L580
    mov rax, 15
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L581
.L580:
.L581:
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call f_here
    pop rdi
    mov [rdi], rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit32
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call f_here
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    add rax, 8
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov eax, eax
    push rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_patch32
.Lret40:
    mov rsp, rbp
    pop rbp
    ret

f_lay_data:
    push rbp
    mov rbp, rsp
    sub rsp, 96
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    mov [rbp - 24], rdx
    mov dword [rbp - 28], ecx
    lea rax, [rbp - 32]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L582:
    lea rax, [rbp - 32]
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
    je .L583
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_skip_sp
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L584
    jmp .L583
    jmp .L585
.L584:
.L585:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 34
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L586
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L588
    mov rax, 0
    push rax
    lea rax, [.Ls165]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret41
    jmp .L589
.L588:
.L589:
    lea rax, [rbp - 36]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L590:
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
    je .L592
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 34
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L592
    mov rax, 1
    jmp .L593
.L592:
    mov rax, 0
.L593:
    test rax, rax
    je .L591
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
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
    jmp .L590
.L591:
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L594
    mov rax, 0
    push rax
    lea rax, [.Ls166]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret41
    jmp .L595
.L594:
.L595:
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L587
.L586:
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L596:
    lea rax, [rbp - 40]
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
    je .L598
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 44
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L598
    mov rax, 1
    jmp .L599
.L598:
    mov rax, 0
.L599:
    test rax, rax
    je .L597
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
    jmp .L596
.L597:
    lea rax, [rbp - 44]
    push rax
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L600:
    lea rax, [rbp - 32]
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
    je .L602
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 44]
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
    jne .L604
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 44]
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
    mov rax, 9
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L604
    mov rax, 0
    jmp .L605
.L604:
    mov rax, 1
.L605:
    test rax, rax
    je .L602
    mov rax, 1
    jmp .L603
.L602:
    mov rax, 0
.L603:
    test rax, rax
    je .L601
    lea rax, [rbp - 44]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, -1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L600
.L601:
    lea rax, [rbp - 56]
    push rax
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_number
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L606
    lea rax, [rbp - 92]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L608:
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L610
    lea rax, [rbp - 92]
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
    je .L610
    mov rax, 1
    jmp .L611
.L610:
    mov rax, 0
.L611:
    test rax, rax
    je .L609
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
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
    jmp .L608
.L609:
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
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L612
    mov rax, 0
    push rax
    lea rax, [.Ls167]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret41
    jmp .L613
.L612:
.L613:
    lea rax, [rbp - 56]
    push rax
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_name_addr
    pop rdi
    mov [rdi], rax
    jmp .L607
.L606:
.L607:
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L614
    lea rax, [rbp - 56]
    mov rax, [rax]
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L615
.L614:
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L616
    lea rax, [rbp - 56]
    mov rax, [rax]
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    mov rcx, rdi
    sar rax, cl
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L617
.L616:
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L618
    lea rax, [rbp - 56]
    mov rax, [rax]
    mov eax, eax
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit32
    jmp .L619
.L618:
    lea rax, [rbp - 56]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit64
.L619:
.L617:
.L615:
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L587:
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_skip_sp
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 32]
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
    je .L620
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 44
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L622
    mov rax, 0
    push rax
    lea rax, [.Ls168]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret41
    jmp .L623
.L622:
.L623:
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
    jmp .L621
.L620:
.L621:
    jmp .L582
.L583:
.Lret41:
    mov rsp, rbp
    pop rbp
    ret

f_word_is:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov [rbp - 8], rdi
    mov dword [rbp - 12], esi
    mov [rbp - 24], rdx
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    pop rdi
    call f_strlen
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L624
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_memcmp
    movsxd rax, eax
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L624
    mov rax, 1
    jmp .L625
.L624:
    mov rax, 0
.L625:
    movzx rax, al
    jmp .Lret42
.Lret42:
    mov rsp, rbp
    pop rbp
    ret

f_split_ops:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov dword [rbp - 12], esi
    mov [rbp - 24], rdx
    lea rax, [rbp - 28]
    push rax
    mov rax, 0
    pop rdi
    movsxd rax, eax
    mov dword [rdi], eax
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
.L626:
    lea rax, [rbp - 36]
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
    je .L628
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 34
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L629
    lea rax, [rbp - 29]
    push rax
    lea rax, [rbp - 29]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L630
.L629:
.L630:
    lea rax, [rbp - 29]
    movzx rax, byte [rax]
    test rax, rax
    je .L631
    jmp .L627
    jmp .L632
.L631:
.L632:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 91
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L633
    lea rax, [rbp - 28]
    mov rdi, rax
    movsxd rax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    movsxd rax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L634
.L633:
.L634:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 93
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L635
    lea rax, [rbp - 28]
    mov rdi, rax
    movsxd rax, dword [rax]
    mov r8, rax
    add rax, -1
    push rdi
    movsxd rax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L636
.L635:
.L636:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 44
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L639
    lea rax, [rbp - 28]
    movsxd rax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L639
    mov rax, 1
    jmp .L640
.L639:
    mov rax, 0
.L640:
    test rax, rax
    je .L637
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .Lret43
    jmp .L638
.L637:
.L638:
.L627:
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
    jmp .L626
.L628:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 12]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.Lret43:
    mov rsp, rbp
    pop rbp
    ret

f_assemble_line:
    push rbp
    mov rbp, rsp
    sub rsp, 288
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov dword [rbp - 20], edx
    lea rax, [rbp - 24]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L641:
    lea rax, [rbp - 24]
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
    je .L643
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 34
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L644
    lea rax, [rbp - 28]
    push rax
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L646:
    lea rax, [rbp - 28]
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
    je .L648
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 34
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
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
    jmp .L646
.L647:
    lea rax, [rbp - 24]
    push rax
    lea rax, [rbp - 28]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L642
    jmp .L645
.L644:
.L645:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 59
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L650
    lea rax, [rbp - 20]
    push rax
    lea rax, [rbp - 24]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L643
    jmp .L651
.L650:
.L651:
.L642:
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
    jmp .L641
.L643:
    lea rax, [rbp - 32]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_skip_sp
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L652:
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
    je .L654
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
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
    jne .L658
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
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
    mov rax, 9
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L658
    mov rax, 0
    jmp .L659
.L658:
    mov rax, 1
.L659:
    test rax, rax
    jne .L656
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 20]
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
    mov rax, 13
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L656
    mov rax, 0
    jmp .L657
.L656:
    mov rax, 1
.L657:
    test rax, rax
    je .L654
    mov rax, 1
    jmp .L655
.L654:
    mov rax, 0
.L655:
    test rax, rax
    je .L653
    lea rax, [rbp - 20]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, -1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L652
.L653:
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L660
    jmp .Lret44
    jmp .L661
.L660:
.L661:
    lea rax, [rbp - 36]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L662:
    lea rax, [rbp - 36]
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
    je .L664
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    pop rdi
    call f_is_name_char
    movzx rax, al
    test rax, rax
    je .L664
    mov rax, 1
    jmp .L665
.L664:
    mov rax, 0
.L665:
    test rax, rax
    je .L663
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
    jmp .L662
.L663:
    lea rax, [rbp - 36]
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
    je .L668
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 58
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L668
    mov rax, 1
    jmp .L669
.L668:
    mov rax, 0
.L669:
    test rax, rax
    je .L666
    lea rax, [rbp - 72]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L670:
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 72]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L672
    lea rax, [rbp - 72]
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
    je .L672
    mov rax, 1
    jmp .L673
.L672:
    mov rax, 0
.L673:
    test rax, rax
    je .L671
    lea rax, [rbp - 68]
    push rax
    lea rax, [rbp - 72]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 72]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
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
    jmp .L670
.L671:
    lea rax, [rbp - 68]
    push rax
    lea rax, [rbp - 72]
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
    push rax
    pop rdi
    call f_here
    push rax
    lea rax, [rbp - 68]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_label_set
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_skip_sp
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L674
    jmp .Lret44
    jmp .L675
.L674:
.L675:
    lea rax, [rbp - 36]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L676:
    lea rax, [rbp - 36]
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
    je .L678
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    pop rdi
    call f_is_name_char
    movzx rax, al
    test rax, rax
    je .L678
    mov rax, 1
    jmp .L679
.L678:
    mov rax, 0
.L679:
    test rax, rax
    je .L677
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
    jmp .L676
.L677:
    jmp .L667
.L666:
.L667:
    lea rax, [rbp - 80]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 84]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 32]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 88]
    push rax
    lea rax, [rbp - 36]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_skip_sp
    mov eax, eax
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 96]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 100]
    push rax
    lea rax, [rbp - 20]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [.Ls169]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L680
    lea rax, [.Ls170]
    push rax
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L682
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 28
    push rax
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L683
.L682:
    lea rax, [.Ls171]
    push rax
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L684
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 28
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    jmp .L685
.L684:
    mov rax, 0
    push rax
    lea rax, [.Ls172]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
.L685:
.L683:
    jmp .Lret44
    jmp .L681
.L680:
.L681:
    lea rax, [.Ls173]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L686
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    mov rax, 1
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_lay_data
    jmp .Lret44
    jmp .L687
.L686:
.L687:
    lea rax, [.Ls174]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L688
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    mov rax, 2
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_lay_data
    jmp .Lret44
    jmp .L689
.L688:
.L689:
    lea rax, [.Ls175]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L690
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    mov rax, 4
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_lay_data
    jmp .Lret44
    jmp .L691
.L690:
.L691:
    lea rax, [.Ls176]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L692
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    mov rax, 8
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_lay_data
    jmp .Lret44
    jmp .L693
.L692:
.L693:
    lea rax, [.Ls177]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L694
    lea rax, [rbp - 112]
    push rax
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_number
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L700
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
    jne .L700
    mov rax, 0
    jmp .L701
.L700:
    mov rax, 1
.L701:
    test rax, rax
    jne .L698
    mov rax, 256
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L698
    mov rax, 0
    jmp .L699
.L698:
    mov rax, 1
.L699:
    test rax, rax
    je .L696
    mov rax, 0
    push rax
    lea rax, [.Ls178]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret44
    jmp .L697
.L696:
.L697:
    lea rax, [rbp - 120]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L702:
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L705
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163884
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L705
    mov rax, 1
    jmp .L706
.L705:
    mov rax, 0
.L706:
    test rax, rax
    je .L704
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
.L703:
    lea rax, [rbp - 120]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L702
.L704:
    jmp .Lret44
    jmp .L695
.L694:
.L695:
    lea rax, [.Ls179]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L707
    lea rax, [rbp - 128]
    push rax
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_number
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L713
    lea rax, [rbp - 128]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setle al
    movzx rax, al
    test rax, rax
    jne .L713
    mov rax, 0
    jmp .L714
.L713:
    mov rax, 1
.L714:
    test rax, rax
    jne .L711
    mov rax, 4096
    push rax
    lea rax, [rbp - 128]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    jne .L711
    mov rax, 0
    jmp .L712
.L711:
    mov rax, 1
.L712:
    test rax, rax
    je .L709
    mov rax, 0
    push rax
    lea rax, [.Ls180]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret44
    jmp .L710
.L709:
.L710:
.L715:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    call f_here
    push rax
    lea rax, [rbp - 128]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    xor edx, edx
    div rdi
    mov rax, rdx
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    je .L717
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 163884
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L717
    mov rax, 1
    jmp .L718
.L717:
    mov rax, 0
.L718:
    test rax, rax
    je .L716
    lea rax, [rbp - 8]
    mov rax, [rax]
    add rax, 28
    movzx rax, byte [rax]
    test rax, rax
    je .L719
    mov rax, 0
    jmp .L720
.L719:
    mov rax, 144
.L720:
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .L715
.L716:
    jmp .Lret44
    jmp .L708
.L707:
.L708:
    lea rax, [rbp - 228]
    push rax
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_split_ops
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 228]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_parse_operand
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L721
    jmp .Lret44
    jmp .L722
.L721:
.L722:
    lea rax, [rbp - 228]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L723
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 100]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 228]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    lea rax, [rbp - 96]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 228]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
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
    pop rdx
    pop rcx
    call f_parse_operand
    movzx rax, al
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L725
    jmp .Lret44
    jmp .L726
.L725:
.L726:
    jmp .L724
.L723:
    lea rax, [rbp - 224]
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L724:
    lea rax, [rbp - 232]
    push rax
    lea rax, [rbp - 176]
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    push rax
    lea rax, [rbp - 224]
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [.Ls181]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L727
    lea rax, [rbp - 232]
    mov eax, dword [rax]
    test rax, rax
    je .L729
    jmp .Lg44_count
    jmp .L730
.L729:
.L730:
    mov rax, 15
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    mov rax, 5
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .Lret44
    jmp .L728
.L727:
.L728:
    lea rax, [.Ls182]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L731
    lea rax, [rbp - 232]
    mov eax, dword [rax]
    test rax, rax
    je .L733
    jmp .Lg44_count
    jmp .L734
.L733:
.L734:
    mov rax, 195
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .Lret44
    jmp .L732
.L731:
.L732:
    lea rax, [.Ls183]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L735
    lea rax, [rbp - 232]
    mov eax, dword [rax]
    test rax, rax
    je .L737
    jmp .Lg44_count
    jmp .L738
.L737:
.L738:
    mov rax, 144
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .Lret44
    jmp .L736
.L735:
.L736:
    lea rax, [.Ls184]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L739
    lea rax, [rbp - 232]
    mov eax, dword [rax]
    test rax, rax
    je .L741
    jmp .Lg44_count
    jmp .L742
.L741:
.L742:
    mov rax, 72
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    mov rax, 153
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .Lret44
    jmp .L740
.L739:
.L740:
    lea rax, [.Ls185]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L743
    lea rax, [rbp - 232]
    mov eax, dword [rax]
    test rax, rax
    je .L745
    jmp .Lg44_count
    jmp .L746
.L745:
.L746:
    mov rax, 153
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .Lret44
    jmp .L744
.L743:
.L744:
    lea rax, [.Ls186]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L747
    lea rax, [rbp - 232]
    mov eax, dword [rax]
    test rax, rax
    je .L749
    jmp .Lg44_count
    jmp .L750
.L749:
.L750:
    mov rax, 72
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    mov rax, 152
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .Lret44
    jmp .L748
.L747:
.L748:
    lea rax, [rbp - 232]
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L751
    lea rax, [.Ls187]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L753
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_do_mov
    jmp .Lret44
    jmp .L754
.L753:
.L754:
    lea rax, [.Ls188]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L755
    mov rax, 0
    push rax
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_do_movzx
    jmp .Lret44
    jmp .L756
.L755:
.L756:
    lea rax, [.Ls189]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L757
    mov rax, 1
    push rax
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_do_movzx
    jmp .Lret44
    jmp .L758
.L757:
.L758:
    lea rax, [.Ls190]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L759
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_do_movsxd
    jmp .Lret44
    jmp .L760
.L759:
.L760:
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
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
    call f_do_sse
    movzx rax, al
    test rax, rax
    je .L761
    jmp .Lret44
    jmp .L762
.L761:
.L762:
    lea rax, [.Ls191]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L763
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_do_lea
    jmp .Lret44
    jmp .L764
.L763:
.L764:
    lea rax, [.Ls192]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L765
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_arith
    jmp .Lret44
    jmp .L766
.L765:
.L766:
    lea rax, [.Ls193]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L767
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    mov rax, 1
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_arith
    jmp .Lret44
    jmp .L768
.L767:
.L768:
    lea rax, [.Ls194]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L769
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    mov rax, 4
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_arith
    jmp .Lret44
    jmp .L770
.L769:
.L770:
    lea rax, [.Ls195]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L771
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    mov rax, 5
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_arith
    jmp .Lret44
    jmp .L772
.L771:
.L772:
    lea rax, [.Ls196]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L773
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    mov rax, 6
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_arith
    jmp .Lret44
    jmp .L774
.L773:
.L774:
    lea rax, [.Ls197]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L775
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    mov rax, 7
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_arith
    jmp .Lret44
    jmp .L776
.L775:
.L776:
    lea rax, [.Ls198]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L777
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_do_test
    jmp .Lret44
    jmp .L778
.L777:
.L778:
    lea rax, [.Ls199]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L779
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    mov rax, 4
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_shift
    jmp .Lret44
    jmp .L780
.L779:
.L780:
    lea rax, [.Ls200]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L781
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    mov rax, 5
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_shift
    jmp .Lret44
    jmp .L782
.L781:
.L782:
    lea rax, [.Ls201]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L783
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    mov rax, 7
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_shift
    jmp .Lret44
    jmp .L784
.L783:
.L784:
    lea rax, [.Ls202]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L785
    lea rax, [rbp - 176]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L789
    lea rax, [rbp - 176]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L789
    mov rax, 0
    jmp .L790
.L789:
    mov rax, 1
.L790:
    test rax, rax
    je .L787
    mov rax, 0
    push rax
    lea rax, [.Ls203]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret44
    jmp .L788
.L787:
.L788:
    lea rax, [rbp - 233]
    push rax
    lea rax, [rbp - 224]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L791
    lea rax, [rbp - 224]
    add rax, 4
    movsxd rax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L793
    mov rax, 0
    jmp .L794
.L793:
    lea rax, [rbp - 224]
    add rax, 4
    movsxd rax, dword [rax]
    movzx rax, al
.L794:
    jmp .L792
.L791:
    lea rax, [rbp - 224]
    add rax, 2
    movzx rax, byte [rax]
.L792:
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    mov rax, 0
    push rax
    lea rax, [rbp - 233]
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 176]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 176]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 15
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    mov rax, 175
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 224]
    movzx rax, byte [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L795
    mov rax, 0
    push rax
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_mem_tail
    jmp .L796
.L795:
    lea rax, [rbp - 224]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 176]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_modrm_rr
.L796:
    jmp .Lret44
    jmp .L786
.L785:
.L786:
    lea rax, [.Ls204]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L797
    lea rax, [rbp - 176]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L803
    lea rax, [rbp - 224]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L803
    mov rax, 0
    jmp .L804
.L803:
    mov rax, 1
.L804:
    test rax, rax
    jne .L801
    lea rax, [rbp - 176]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 224]
    add rax, 1
    movzx rax, byte [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L801
    mov rax, 0
    jmp .L802
.L801:
    mov rax, 1
.L802:
    test rax, rax
    je .L799
    mov rax, 0
    push rax
    lea rax, [.Ls205]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret44
    jmp .L800
.L799:
.L800:
    lea rax, [rbp - 176]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 224]
    push rax
    lea rax, [rbp - 176]
    push rax
    mov rax, 135
    push rax
    mov rax, 134
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rm_reg
    jmp .Lret44
    jmp .L798
.L797:
.L798:
    jmp .L752
.L751:
.L752:
    lea rax, [rbp - 232]
    mov eax, dword [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L805
    lea rax, [.Ls206]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L807
    lea rax, [rbp - 176]
    push rax
    mov rax, 0
    push rax
    mov rax, 255
    push rax
    mov rax, 254
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call f_unary
    jmp .Lret44
    jmp .L808
.L807:
.L808:
    lea rax, [.Ls207]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L809
    lea rax, [rbp - 176]
    push rax
    mov rax, 1
    push rax
    mov rax, 255
    push rax
    mov rax, 254
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call f_unary
    jmp .Lret44
    jmp .L810
.L809:
.L810:
    lea rax, [.Ls208]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L811
    lea rax, [rbp - 176]
    push rax
    mov rax, 2
    push rax
    mov rax, 247
    push rax
    mov rax, 246
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call f_unary
    jmp .Lret44
    jmp .L812
.L811:
.L812:
    lea rax, [.Ls209]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L813
    lea rax, [rbp - 176]
    push rax
    mov rax, 3
    push rax
    mov rax, 247
    push rax
    mov rax, 246
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call f_unary
    jmp .Lret44
    jmp .L814
.L813:
.L814:
    lea rax, [.Ls210]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L815
    lea rax, [rbp - 176]
    push rax
    mov rax, 4
    push rax
    mov rax, 247
    push rax
    mov rax, 246
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call f_unary
    jmp .Lret44
    jmp .L816
.L815:
.L816:
    lea rax, [.Ls211]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L817
    lea rax, [rbp - 176]
    push rax
    mov rax, 5
    push rax
    mov rax, 247
    push rax
    mov rax, 246
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call f_unary
    jmp .Lret44
    jmp .L818
.L817:
.L818:
    lea rax, [.Ls212]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L819
    lea rax, [rbp - 176]
    push rax
    mov rax, 6
    push rax
    mov rax, 247
    push rax
    mov rax, 246
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call f_unary
    jmp .Lret44
    jmp .L820
.L819:
.L820:
    lea rax, [.Ls213]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L821
    lea rax, [rbp - 176]
    push rax
    mov rax, 7
    push rax
    mov rax, 247
    push rax
    mov rax, 246
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call f_unary
    jmp .Lret44
    jmp .L822
.L821:
.L822:
    lea rax, [.Ls214]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L823
    lea rax, [rbp - 176]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L827
    lea rax, [rbp - 176]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L827
    mov rax, 1
    jmp .L828
.L827:
    mov rax, 0
.L828:
    test rax, rax
    je .L825
    mov rax, 0
    push rax
    lea rax, [rbp - 176]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 80
    push rax
    lea rax, [rbp - 176]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    and rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .Lret44
    jmp .L826
.L825:
.L826:
    lea rax, [rbp - 176]
    movzx rax, byte [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L831
    lea rax, [rbp - 176]
    add rax, 8
    mov rax, [rax]
    push rax
    pop rdi
    call f_fits32
    movzx rax, al
    test rax, rax
    je .L831
    mov rax, 1
    jmp .L832
.L831:
    mov rax, 0
.L832:
    test rax, rax
    je .L829
    mov rax, 104
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 176]
    add rax, 8
    mov rax, [rax]
    mov eax, eax
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit32
    jmp .Lret44
    jmp .L830
.L829:
.L830:
    mov rax, 0
    push rax
    lea rax, [.Ls215]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret44
    jmp .L824
.L823:
.L824:
    lea rax, [.Ls216]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L833
    lea rax, [rbp - 176]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L837
    lea rax, [rbp - 176]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L837
    mov rax, 1
    jmp .L838
.L837:
    mov rax, 0
.L838:
    test rax, rax
    je .L835
    mov rax, 0
    push rax
    lea rax, [rbp - 176]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 88
    push rax
    lea rax, [rbp - 176]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    and rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, al
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    jmp .Lret44
    jmp .L836
.L835:
.L836:
    mov rax, 0
    push rax
    lea rax, [.Ls217]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret44
    jmp .L834
.L833:
.L834:
    lea rax, [.Ls218]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L839
    lea rax, [rbp - 176]
    movzx rax, byte [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L841
    lea rax, [rbp - 176]
    push rax
    mov rax, 0
    push rax
    mov rax, 232
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_jump_rel
    jmp .Lret44
    jmp .L842
.L841:
.L842:
    lea rax, [rbp - 176]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L845
    lea rax, [rbp - 176]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L845
    mov rax, 1
    jmp .L846
.L845:
    mov rax, 0
.L846:
    test rax, rax
    je .L843
    mov rax, 0
    push rax
    lea rax, [rbp - 176]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 255
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 176]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 2
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_modrm_rr
    jmp .Lret44
    jmp .L844
.L843:
.L844:
    mov rax, 0
    push rax
    lea rax, [.Ls219]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret44
    jmp .L840
.L839:
.L840:
    lea rax, [.Ls220]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L847
    lea rax, [rbp - 176]
    movzx rax, byte [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L849
    lea rax, [rbp - 176]
    push rax
    mov rax, 0
    push rax
    mov rax, 233
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_jump_rel
    jmp .Lret44
    jmp .L850
.L849:
.L850:
    lea rax, [rbp - 176]
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L853
    lea rax, [rbp - 176]
    add rax, 1
    movzx rax, byte [rax]
    push rax
    mov rax, 64
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L853
    mov rax, 1
    jmp .L854
.L853:
    mov rax, 0
.L854:
    test rax, rax
    je .L851
    mov rax, 0
    push rax
    lea rax, [rbp - 176]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    mov rax, 0
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    pop r9
    call f_rex
    mov rax, 255
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    call f_emit8
    lea rax, [rbp - 176]
    add rax, 2
    movzx rax, byte [rax]
    push rax
    mov rax, 4
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_modrm_rr
    jmp .Lret44
    jmp .L852
.L851:
.L852:
    mov rax, 0
    push rax
    lea rax, [.Ls221]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    jmp .Lret44
    jmp .L848
.L847:
.L848:
    lea rax, [rbp - 240]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L855:
    lea rax, [rbp - 240]
    mov eax, dword [rax]
    push rax
    mov rax, 256
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cqo
    idiv rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L857
    lea rax, [v_jccs]
    push rax
    lea rax, [rbp - 240]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L858
    lea rax, [rbp - 176]
    push rax
    mov rax, 1
    push rax
    lea rax, [v_jccs]
    push rax
    lea rax, [rbp - 240]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 8
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    call f_jump_rel
    jmp .Lret44
    jmp .L859
.L858:
.L859:
.L856:
    lea rax, [rbp - 240]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L855
.L857:
    mov rax, 3
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L866
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    mov rax, 0
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
    je .L866
    mov rax, 1
    jmp .L867
.L866:
    mov rax, 0
.L867:
    test rax, rax
    je .L864
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 101
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L864
    mov rax, 1
    jmp .L865
.L864:
    mov rax, 0
.L865:
    test rax, rax
    je .L862
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 116
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L862
    mov rax, 1
    jmp .L863
.L862:
    mov rax, 0
.L863:
    test rax, rax
    je .L860
    lea rax, [rbp - 244]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L868:
    lea rax, [rbp - 244]
    mov eax, dword [rax]
    push rax
    mov rax, 256
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cqo
    idiv rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L870
    lea rax, [v_jccs]
    push rax
    lea rax, [rbp - 244]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    sub rax, rdi
    push rax
    lea rax, [rbp - 80]
    mov rax, [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_word_is
    movzx rax, al
    test rax, rax
    je .L871
    lea rax, [rbp - 176]
    push rax
    lea rax, [v_jccs]
    push rax
    lea rax, [rbp - 244]
    mov eax, dword [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    imul rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    add rax, 8
    movzx rax, byte [rax]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_do_setcc
    jmp .Lret44
    jmp .L872
.L871:
.L872:
.L869:
    lea rax, [rbp - 244]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L868
.L870:
    jmp .L861
.L860:
.L861:
    jmp .L806
.L805:
.L806:
.Lg44_count:
    lea rax, [rbp - 280]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L873:
    lea rax, [rbp - 280]
    mov eax, dword [rax]
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L875
    lea rax, [rbp - 280]
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
    je .L875
    mov rax, 1
    jmp .L876
.L875:
    mov rax, 0
.L876:
    test rax, rax
    je .L874
    lea rax, [rbp - 276]
    push rax
    lea rax, [rbp - 280]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 80]
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
    jmp .L873
.L874:
    lea rax, [rbp - 276]
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
    lea rax, [rbp - 276]
    push rax
    lea rax, [.Ls222]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
.Lret44:
    mov rsp, rbp
    pop rbp
    ret

f_asm_assemble:
    push rbp
    mov rbp, rsp
    sub rsp, 96
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    mov [rbp - 32], rcx
    mov [rbp - 40], r8
    mov dword [rbp - 44], r9d
    mov rax, 163904
    push rax
    mov rax, 0
    push rax
    lea rax, [v_a.1]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_memset
    lea rax, [v_a.1]
    push rax
    lea rax, [v_code_buf]
    pop rdi
    mov [rdi], rax
    lea rax, [v_a.1]
    add rax, 16
    push rax
    lea rax, [v_data_buf]
    pop rdi
    mov [rdi], rax
    lea rax, [v_a.1]
    add rax, 163888
    push rax
    lea rax, [rbp - 40]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [v_a.1]
    add rax, 163896
    push rax
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    test rax, rax
    je .L877
    lea rax, [rbp - 40]
    mov rax, [rax]
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
    jmp .L878
.L877:
.L878:
    lea rax, [rbp - 48]
    push rax
    mov rax, 1
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L879:
    lea rax, [rbp - 48]
    mov eax, dword [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L882
    lea rax, [v_a.1]
    add rax, 163884
    movzx rax, byte [rax]
    test rax, rax
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
    je .L881
    lea rax, [v_a.1]
    add rax, 163876
    push rax
    lea rax, [rbp - 48]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [v_a.1]
    add rax, 8
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [v_a.1]
    add rax, 24
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [v_a.1]
    add rax, 28
    push rax
    mov rax, 0
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [v_a.1]
    add rax, 163880
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 56]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 64]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L884:
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setbe al
    movzx rax, al
    test rax, rax
    je .L887
    lea rax, [v_a.1]
    add rax, 163884
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L887
    mov rax, 1
    jmp .L888
.L887:
    mov rax, 0
.L888:
    test rax, rax
    je .L886
    lea rax, [rbp - 65]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L891
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L891
    mov rax, 0
    jmp .L892
.L891:
    mov rax, 1
.L892:
    test rax, rax
    jne .L889
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 64]
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
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 65]
    movzx rax, byte [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    je .L893
    jmp .L885
    jmp .L894
.L893:
.L894:
    lea rax, [v_a.1]
    add rax, 163880
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    sub rax, rdi
    mov eax, eax
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 56]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [v_a.1]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_assemble_line
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L897
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L897
    mov rax, 0
    jmp .L898
.L897:
    mov rax, 1
.L898:
    test rax, rax
    je .L895
    jmp .L886
    jmp .L896
.L895:
.L896:
    lea rax, [rbp - 56]
    push rax
    lea rax, [rbp - 64]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
.L885:
    lea rax, [rbp - 64]
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
.L880:
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
    jmp .L879
.L881:
    lea rax, [v_a.1]
    add rax, 163884
    movzx rax, byte [rax]
    test rax, rax
    je .L899
    mov rax, 1
    neg rax
    jmp .Lret45
    jmp .L900
.L899:
.L900:
    lea rax, [v_a.1]
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
    je .L901
    mov rax, 0
    push rax
    lea rax, [.Ls223]
    push rax
    lea rax, [v_a.1]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 1
    neg rax
    jmp .Lret45
    jmp .L902
.L901:
.L902:
    lea rax, [rbp - 80]
    push rax
    mov rax, 16
    push rax
    lea rax, [v_a.1]
    add rax, 8
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [v_a.1]
    add rax, 24
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 32]
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
    je .L903
    mov rax, 0
    push rax
    lea rax, [.Ls224]
    push rax
    lea rax, [v_a.1]
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_fail
    mov rax, 1
    neg rax
    jmp .Lret45
    jmp .L904
.L903:
.L904:
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 69
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 66
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 88
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 49
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 84]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L905:
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L907
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 4
    push rax
    lea rax, [rbp - 84]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [v_a.1]
    add rax, 8
    mov eax, dword [rax]
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
    mov rcx, rdi
    shr rax, cl
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L906:
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
    jmp .L905
.L907:
    lea rax, [rbp - 88]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L908:
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L910
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 8
    push rax
    lea rax, [rbp - 88]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [v_a.1]
    add rax, 24
    mov eax, dword [rax]
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
    mov rcx, rdi
    shr rax, cl
    movzx rax, al
    pop rdi
    movzx rax, al
    mov byte [rdi], al
.L909:
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
    jmp .L908
.L910:
    lea rax, [rbp - 92]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L911:
    lea rax, [rbp - 92]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L913
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 12
    push rax
    lea rax, [rbp - 92]
    mov eax, dword [rax]
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
.L912:
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
    jmp .L911
.L913:
    lea rax, [v_a.1]
    add rax, 8
    mov eax, dword [rax]
    push rax
    lea rax, [v_a.1]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_memcpy
    lea rax, [v_a.1]
    add rax, 24
    mov eax, dword [rax]
    push rax
    lea rax, [v_a.1]
    add rax, 16
    mov rax, [rax]
    push rax
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [v_a.1]
    add rax, 8
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    pop rdi
    pop rsi
    pop rdx
    call f_memcpy
    lea rax, [rbp - 80]
    mov rax, [rax]
    jmp .Lret45
.Lret45:
    mov rsp, rbp
    pop rbp
    ret

f_code_image_ok:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov [rbp - 24], rdx
    mov [rbp - 32], rcx
    mov [rbp - 40], r8
    lea rax, [rbp - 8]
    mov rax, [rax]
    test rax, rax
    sete al
    movzx rax, al
    test rax, rax
    jne .L916
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    jne .L916
    mov rax, 0
    jmp .L917
.L916:
    mov rax, 1
.L917:
    test rax, rax
    je .L914
    mov rax, 0
    movzx rax, al
    jmp .Lret46
    jmp .L915
.L914:
.L915:
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 69
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L924
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 66
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L924
    mov rax, 0
    jmp .L925
.L924:
    mov rax, 1
.L925:
    test rax, rax
    jne .L922
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 88
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L922
    mov rax, 0
    jmp .L923
.L922:
    mov rax, 1
.L923:
    test rax, rax
    jne .L920
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 49
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setne al
    movzx rax, al
    test rax, rax
    jne .L920
    mov rax, 0
    jmp .L921
.L920:
    mov rax, 1
.L921:
    test rax, rax
    je .L918
    mov rax, 0
    movzx rax, al
    jmp .Lret46
    jmp .L919
.L918:
.L919:
    lea rax, [rbp - 44]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 48]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 52]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 56]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L926:
    lea rax, [rbp - 56]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L928
    lea rax, [rbp - 44]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 4
    push rax
    lea rax, [rbp - 56]
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
    lea rax, [rbp - 56]
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
    mov rdi, rax
    pop rax
    mov r8, rax
    mov eax, dword [rax]
    or rax, rdi
    mov rdi, r8
    mov eax, eax
    mov dword [rdi], eax
.L927:
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
    jmp .L926
.L928:
    lea rax, [rbp - 60]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L929:
    lea rax, [rbp - 60]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L931
    lea rax, [rbp - 48]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 8
    push rax
    lea rax, [rbp - 60]
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
    lea rax, [rbp - 60]
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
    mov rdi, rax
    pop rax
    mov r8, rax
    mov eax, dword [rax]
    or rax, rdi
    mov rdi, r8
    mov eax, eax
    mov dword [rdi], eax
.L930:
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
    jmp .L929
.L931:
    lea rax, [rbp - 64]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
.L932:
    lea rax, [rbp - 64]
    mov eax, dword [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L934
    lea rax, [rbp - 52]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 12
    push rax
    lea rax, [rbp - 64]
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
    lea rax, [rbp - 64]
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
    mov rdi, rax
    pop rax
    mov r8, rax
    mov eax, dword [rax]
    or rax, rdi
    mov rdi, r8
    mov eax, eax
    mov dword [rdi], eax
.L933:
    lea rax, [rbp - 64]
    mov rdi, rax
    mov eax, dword [rax]
    mov r8, rax
    add rax, 1
    push rdi
    mov eax, eax
    pop rdi
    mov dword [rdi], eax
    mov rax, r8
    jmp .L932
.L934:
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    jne .L941
    mov rax, 256
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    jne .L941
    mov rax, 0
    jmp .L942
.L941:
    mov rax, 1
.L942:
    test rax, rax
    jne .L939
    mov rax, 256
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 48]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    jne .L939
    mov rax, 0
    jmp .L940
.L939:
    mov rax, 1
.L940:
    test rax, rax
    jne .L937
    mov rax, 256
    push rax
    mov rax, 1024
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    jne .L937
    mov rax, 0
    jmp .L938
.L937:
    mov rax, 1
.L938:
    test rax, rax
    je .L935
    mov rax, 0
    movzx rax, al
    jmp .Lret46
    jmp .L936
.L935:
.L936:
    lea rax, [rbp - 16]
    mov rax, [rax]
    push rax
    mov rax, 16
    push rax
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 48]
    mov eax, dword [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setb al
    movzx rax, al
    test rax, rax
    je .L943
    mov rax, 0
    movzx rax, al
    jmp .Lret46
    jmp .L944
.L943:
.L944:
    lea rax, [rbp - 24]
    mov rax, [rax]
    test rax, rax
    je .L945
    lea rax, [rbp - 24]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 44]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L946
.L945:
.L946:
    lea rax, [rbp - 32]
    mov rax, [rax]
    test rax, rax
    je .L947
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L948
.L947:
.L948:
    lea rax, [rbp - 40]
    mov rax, [rax]
    test rax, rax
    je .L949
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 52]
    mov eax, dword [rax]
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    jmp .L950
.L949:
.L950:
    mov rax, 1
    movzx rax, al
    jmp .Lret46
.Lret46:
    mov rsp, rbp
    pop rbp
    ret

f_main:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    mov rax, 0
    jmp .Lret47
.Lret47:
    mov rsp, rbp
    pop rbp
    ret

section data
    align 8
v_regs64:
    dq .Ls6
    dq .Ls7
    dq .Ls8
    dq .Ls9
    dq .Ls10
    dq .Ls11
    dq .Ls12
    dq .Ls13
    dq .Ls14
    dq .Ls15
    dq .Ls16
    dq .Ls17
    dq .Ls18
    dq .Ls19
    dq .Ls20
    dq .Ls21
    align 8
v_regs32:
    dq .Ls22
    dq .Ls23
    dq .Ls24
    dq .Ls25
    dq .Ls26
    dq .Ls27
    dq .Ls28
    dq .Ls29
    dq .Ls30
    dq .Ls31
    dq .Ls32
    dq .Ls33
    dq .Ls34
    dq .Ls35
    dq .Ls36
    dq .Ls37
    align 8
v_regs16:
    dq .Ls38
    dq .Ls39
    dq .Ls40
    dq .Ls41
    dq .Ls42
    dq .Ls43
    dq .Ls44
    dq .Ls45
    dq .Ls46
    dq .Ls47
    dq .Ls48
    dq .Ls49
    dq .Ls50
    dq .Ls51
    dq .Ls52
    dq .Ls53
    align 8
v_regs8:
    dq .Ls54
    dq .Ls55
    dq .Ls56
    dq .Ls57
    dq .Ls58
    dq .Ls59
    dq .Ls60
    dq .Ls61
    dq .Ls62
    dq .Ls63
    dq .Ls64
    dq .Ls65
    dq .Ls66
    dq .Ls67
    dq .Ls68
    dq .Ls69
    align 8
v_regsx:
    dq .Ls70
    dq .Ls71
    dq .Ls72
    dq .Ls73
    dq .Ls74
    dq .Ls75
    dq .Ls76
    dq .Ls77
    dq .Ls78
    dq .Ls79
    dq .Ls80
    dq .Ls81
    dq .Ls82
    dq .Ls83
    dq .Ls84
    dq .Ls85
    align 8
v_jccs:
    dq .Ls149
    db 132, 88, 0, 0, 0, 0, 0, 0
    dq .Ls150
    db 132, 92, 0, 0, 0, 0, 0, 0
    dq .Ls151
    db 133, 89, 0, 0, 0, 0, 0, 0
    dq .Ls152
    db 133, 94, 0, 0, 0, 0, 0, 0
    dq .Ls153
    db 140, 81, 0, 0, 0, 0, 0, 0
    dq .Ls154
    db 141, 46, 0, 0, 0, 0, 0, 0
    dq .Ls155
    db 142, 88, 0, 0, 0, 0, 0, 0
    dq .Ls156
    db 143, 92, 0, 0, 0, 0, 0, 0
    dq .Ls157
    db 130, 89, 0, 0, 0, 0, 0, 0
    dq .Ls158
    db 131, 94, 0, 0, 0, 0, 0, 0
    dq .Ls159
    db 134, 46, 0, 0, 0, 0, 0, 0
    dq .Ls160
    db 135, 90, 0, 0, 0, 0, 0, 0
    dq .Ls161
    db 136, 90, 0, 0, 0, 0, 0, 0
    dq .Ls162
    db 137, 87, 0, 0, 0, 0, 0, 0
    dq .Ls163
    db 130, 87, 0, 0, 0, 0, 0, 0
    dq .Ls164
    db 131, 0, 0, 0, 0, 0, 0, 0
    align 1
v_code_buf:
    res 262144
    align 1
v_data_buf:
    res 262144
    align 8
v_a.1:
    res 163904
    align 8
.Li0:
    dq .Ls98
    db 242, 88, 0, 0, 0, 0, 0, 0
    dq .Ls99
    db 242, 92, 0, 0, 0, 0, 0, 0
    dq .Ls100
    db 242, 89, 0, 0, 0, 0, 0, 0
    dq .Ls101
    db 242, 94, 0, 0, 0, 0, 0, 0
    dq .Ls102
    db 242, 81, 0, 0, 0, 0, 0, 0
    dq .Ls103
    db 102, 46, 0, 0, 0, 0, 0, 0
    dq .Ls104
    db 243, 88, 0, 0, 0, 0, 0, 0
    dq .Ls105
    db 243, 92, 0, 0, 0, 0, 0, 0
    dq .Ls106
    db 243, 89, 0, 0, 0, 0, 0, 0
    dq .Ls107
    db 243, 94, 0, 0, 0, 0, 0, 0
    dq .Ls108
    db 0, 46, 0, 0, 0, 0, 0, 0
    dq .Ls109
    db 243, 90, 0, 0, 0, 0, 0, 0
    dq .Ls110
    db 242, 90, 0, 0, 0, 0, 0, 0
    dq .Ls111
    db 102, 87, 0, 0, 0, 0, 0, 0
    dq .Ls112
    db 0, 87, 0, 0, 0, 0, 0, 0
.Ls0: db 108, 105, 110, 101, 32, 0
.Ls1: db 116, 104, 101, 32, 100, 97, 116, 97, 32, 105, 115, 32, 116, 111, 111, 32, 108, 97, 114, 103, 101, 0
.Ls2: db 116, 104, 101, 32, 99, 111, 100, 101, 32, 105, 115, 32, 116, 111, 111, 32, 108, 97, 114, 103, 101, 0
.Ls3: db 116, 104, 97, 116, 32, 110, 97, 109, 101, 32, 105, 115, 32, 117, 115, 101, 100, 32, 116, 119, 105, 99, 101, 0
.Ls4: db 116, 111, 111, 32, 109, 97, 110, 121, 32, 110, 97, 109, 101, 115, 0
.Ls5: db 116, 104, 97, 116, 32, 105, 115, 32, 110, 111, 116, 32, 97, 32, 110, 97, 109, 101, 32, 108, 97, 105, 100, 32, 100, 111, 119, 110, 32, 97, 110, 121, 119, 104, 101, 114, 101, 58, 0
.Ls6: db 114, 97, 120, 0
.Ls7: db 114, 99, 120, 0
.Ls8: db 114, 100, 120, 0
.Ls9: db 114, 98, 120, 0
.Ls10: db 114, 115, 112, 0
.Ls11: db 114, 98, 112, 0
.Ls12: db 114, 115, 105, 0
.Ls13: db 114, 100, 105, 0
.Ls14: db 114, 56, 0
.Ls15: db 114, 57, 0
.Ls16: db 114, 49, 48, 0
.Ls17: db 114, 49, 49, 0
.Ls18: db 114, 49, 50, 0
.Ls19: db 114, 49, 51, 0
.Ls20: db 114, 49, 52, 0
.Ls21: db 114, 49, 53, 0
.Ls22: db 101, 97, 120, 0
.Ls23: db 101, 99, 120, 0
.Ls24: db 101, 100, 120, 0
.Ls25: db 101, 98, 120, 0
.Ls26: db 101, 115, 112, 0
.Ls27: db 101, 98, 112, 0
.Ls28: db 101, 115, 105, 0
.Ls29: db 101, 100, 105, 0
.Ls30: db 114, 56, 100, 0
.Ls31: db 114, 57, 100, 0
.Ls32: db 114, 49, 48, 100, 0
.Ls33: db 114, 49, 49, 100, 0
.Ls34: db 114, 49, 50, 100, 0
.Ls35: db 114, 49, 51, 100, 0
.Ls36: db 114, 49, 52, 100, 0
.Ls37: db 114, 49, 53, 100, 0
.Ls38: db 97, 120, 0
.Ls39: db 99, 120, 0
.Ls40: db 100, 120, 0
.Ls41: db 98, 120, 0
.Ls42: db 115, 112, 0
.Ls43: db 98, 112, 0
.Ls44: db 115, 105, 0
.Ls45: db 100, 105, 0
.Ls46: db 114, 56, 119, 0
.Ls47: db 114, 57, 119, 0
.Ls48: db 114, 49, 48, 119, 0
.Ls49: db 114, 49, 49, 119, 0
.Ls50: db 114, 49, 50, 119, 0
.Ls51: db 114, 49, 51, 119, 0
.Ls52: db 114, 49, 52, 119, 0
.Ls53: db 114, 49, 53, 119, 0
.Ls54: db 97, 108, 0
.Ls55: db 99, 108, 0
.Ls56: db 100, 108, 0
.Ls57: db 98, 108, 0
.Ls58: db 115, 112, 108, 0
.Ls59: db 98, 112, 108, 0
.Ls60: db 115, 105, 108, 0
.Ls61: db 100, 105, 108, 0
.Ls62: db 114, 56, 98, 0
.Ls63: db 114, 57, 98, 0
.Ls64: db 114, 49, 48, 98, 0
.Ls65: db 114, 49, 49, 98, 0
.Ls66: db 114, 49, 50, 98, 0
.Ls67: db 114, 49, 51, 98, 0
.Ls68: db 114, 49, 52, 98, 0
.Ls69: db 114, 49, 53, 98, 0
.Ls70: db 120, 109, 109, 48, 0
.Ls71: db 120, 109, 109, 49, 0
.Ls72: db 120, 109, 109, 50, 0
.Ls73: db 120, 109, 109, 51, 0
.Ls74: db 120, 109, 109, 52, 0
.Ls75: db 120, 109, 109, 53, 0
.Ls76: db 120, 109, 109, 54, 0
.Ls77: db 120, 109, 109, 55, 0
.Ls78: db 120, 109, 109, 56, 0
.Ls79: db 120, 109, 109, 57, 0
.Ls80: db 120, 109, 109, 49, 48, 0
.Ls81: db 120, 109, 109, 49, 49, 0
.Ls82: db 120, 109, 109, 49, 50, 0
.Ls83: db 120, 109, 109, 49, 51, 0
.Ls84: db 120, 109, 109, 49, 52, 0
.Ls85: db 120, 109, 109, 49, 53, 0
.Ls86: db 119, 104, 97, 116, 32, 105, 115, 32, 105, 110, 32, 116, 104, 101, 32, 98, 114, 97, 99, 107, 101, 116, 115, 63, 0
.Ls87: db 111, 110, 108, 121, 32, 97, 32, 54, 52, 45, 98, 105, 116, 32, 114, 101, 103, 105, 115, 116, 101, 114, 32, 99, 97, 110, 32, 104, 111, 108, 100, 32, 97, 110, 32, 97, 100, 100, 114, 101, 115, 115, 0
.Ls88: db 111, 110, 108, 121, 32, 43, 32, 111, 114, 32, 45, 32, 109, 97, 121, 32, 102, 111, 108, 108, 111, 119, 32, 116, 104, 101, 32, 114, 101, 103, 105, 115, 116, 101, 114, 0
.Ls89: db 97, 32, 110, 117, 109, 98, 101, 114, 32, 115, 104, 111, 117, 108, 100, 32, 102, 111, 108, 108, 111, 119, 32, 116, 104, 101, 32, 115, 105, 103, 110, 0
.Ls90: db 98, 121, 116, 101, 32, 0
.Ls91: db 119, 111, 114, 100, 32, 0
.Ls92: db 100, 119, 111, 114, 100, 32, 0
.Ls93: db 113, 119, 111, 114, 100, 32, 0
.Ls94: db 116, 104, 101, 32, 98, 114, 97, 99, 107, 101, 116, 32, 110, 101, 118, 101, 114, 32, 99, 108, 111, 115, 101, 115, 0
.Ls95: db 97, 32, 119, 105, 100, 116, 104, 32, 103, 111, 101, 115, 32, 98, 101, 102, 111, 114, 101, 32, 98, 114, 97, 99, 107, 101, 116, 115, 0
.Ls96: db 105, 32, 99, 97, 110, 110, 111, 116, 32, 114, 101, 97, 100, 0
.Ls97: db 116, 104, 101, 32, 100, 105, 115, 112, 108, 97, 99, 101, 109, 101, 110, 116, 32, 100, 111, 101, 115, 32, 110, 111, 116, 32, 102, 105, 116, 0
.Ls98: db 97, 100, 100, 115, 100, 0
.Ls99: db 115, 117, 98, 115, 100, 0
.Ls100: db 109, 117, 108, 115, 100, 0
.Ls101: db 100, 105, 118, 115, 100, 0
.Ls102: db 115, 113, 114, 116, 115, 100, 0
.Ls103: db 117, 99, 111, 109, 105, 115, 100, 0
.Ls104: db 97, 100, 100, 115, 115, 0
.Ls105: db 115, 117, 98, 115, 115, 0
.Ls106: db 109, 117, 108, 115, 115, 0
.Ls107: db 100, 105, 118, 115, 115, 0
.Ls108: db 117, 99, 111, 109, 105, 115, 115, 0
.Ls109: db 99, 118, 116, 115, 115, 50, 115, 100, 0
.Ls110: db 99, 118, 116, 115, 100, 50, 115, 115, 0
.Ls111: db 120, 111, 114, 112, 100, 0
.Ls112: db 120, 111, 114, 112, 115, 0
.Ls113: db 116, 104, 97, 116, 32, 119, 97, 110, 116, 115, 32, 97, 110, 32, 120, 109, 109, 32, 114, 101, 103, 105, 115, 116, 101, 114, 44, 32, 116, 104, 101, 110, 32, 97, 110, 32, 120, 109, 109, 32, 114, 101, 103, 105, 115, 116, 101, 114, 32, 111, 114, 32, 109, 101, 109, 111, 114, 121, 0
.Ls114: db 109, 111, 118, 115, 100, 0
.Ls115: db 109, 111, 118, 115, 115, 0
.Ls116: db 109, 111, 118, 115, 100, 32, 109, 111, 118, 101, 115, 32, 98, 101, 116, 119, 101, 101, 110, 32, 97, 110, 32, 120, 109, 109, 32, 114, 101, 103, 105, 115, 116, 101, 114, 32, 97, 110, 100, 32, 109, 101, 109, 111, 114, 121, 32, 111, 114, 32, 97, 110, 111, 116, 104, 101, 114, 32, 120, 109, 109, 0
.Ls117: db 99, 118, 116, 115, 105, 50, 115, 100, 0
.Ls118: db 99, 118, 116, 115, 105, 50, 115, 115, 0
.Ls119: db 99, 118, 116, 115, 105, 50, 115, 100, 32, 119, 97, 110, 116, 115, 32, 97, 110, 32, 120, 109, 109, 32, 114, 101, 103, 105, 115, 116, 101, 114, 44, 32, 116, 104, 101, 110, 32, 97, 32, 54, 52, 45, 98, 105, 116, 32, 114, 101, 103, 105, 115, 116, 101, 114, 0
.Ls120: db 99, 118, 116, 116, 115, 100, 50, 115, 105, 0
.Ls121: db 99, 118, 116, 116, 115, 115, 50, 115, 105, 0
.Ls122: db 99, 118, 116, 116, 115, 100, 50, 115, 105, 32, 119, 97, 110, 116, 115, 32, 97, 32, 54, 52, 45, 98, 105, 116, 32, 114, 101, 103, 105, 115, 116, 101, 114, 44, 32, 116, 104, 101, 110, 32, 97, 110, 32, 120, 109, 109, 32, 114, 101, 103, 105, 115, 116, 101, 114, 0
.Ls123: db 109, 111, 118, 113, 0
.Ls124: db 109, 111, 118, 113, 32, 109, 111, 118, 101, 115, 32, 98, 101, 116, 119, 101, 101, 110, 32, 97, 110, 32, 120, 109, 109, 32, 114, 101, 103, 105, 115, 116, 101, 114, 32, 97, 110, 100, 32, 97, 32, 54, 52, 45, 98, 105, 116, 32, 114, 101, 103, 105, 115, 116, 101, 114, 0
.Ls125: db 115, 97, 121, 32, 98, 121, 116, 101, 44, 32, 100, 119, 111, 114, 100, 32, 111, 114, 32, 113, 119, 111, 114, 100, 0
.Ls126: db 116, 104, 101, 32, 114, 101, 103, 105, 115, 116, 101, 114, 115, 32, 100, 105, 102, 102, 101, 114, 32, 105, 110, 32, 119, 105, 100, 116, 104, 0
.Ls127: db 116, 104, 101, 32, 110, 117, 109, 98, 101, 114, 32, 100, 111, 101, 115, 32, 110, 111, 116, 32, 102, 105, 116, 32, 97, 32, 98, 121, 116, 101, 0
.Ls128: db 116, 104, 101, 32, 110, 117, 109, 98, 101, 114, 32, 100, 111, 101, 115, 32, 110, 111, 116, 32, 102, 105, 116, 0
.Ls129: db 116, 104, 111, 115, 101, 32, 111, 112, 101, 114, 97, 110, 100, 115, 32, 100, 111, 32, 110, 111, 116, 32, 103, 111, 32, 116, 111, 103, 101, 116, 104, 101, 114, 0
.Ls130: db 116, 104, 101, 32, 110, 117, 109, 98, 101, 114, 32, 100, 111, 101, 115, 32, 110, 111, 116, 32, 102, 105, 116, 0
.Ls131: db 116, 104, 101, 32, 110, 117, 109, 98, 101, 114, 32, 100, 111, 101, 115, 32, 110, 111, 116, 32, 102, 105, 116, 32, 97, 32, 119, 111, 114, 100, 0
.Ls132: db 97, 110, 32, 120, 109, 109, 32, 114, 101, 103, 105, 115, 116, 101, 114, 32, 116, 97, 107, 101, 115, 32, 110, 111, 32, 110, 117, 109, 98, 101, 114, 59, 32, 109, 111, 118, 113, 32, 105, 116, 32, 102, 114, 111, 109, 32, 97, 32, 114, 101, 103, 105, 115, 116, 101, 114, 0
.Ls133: db 116, 104, 101, 32, 110, 117, 109, 98, 101, 114, 32, 100, 111, 101, 115, 32, 110, 111, 116, 32, 102, 105, 116, 32, 97, 32, 98, 121, 116, 101, 0
.Ls134: db 115, 97, 121, 32, 98, 121, 116, 101, 44, 32, 100, 119, 111, 114, 100, 32, 111, 114, 32, 113, 119, 111, 114, 100, 0
.Ls135: db 116, 104, 101, 32, 110, 117, 109, 98, 101, 114, 32, 100, 111, 101, 115, 32, 110, 111, 116, 32, 102, 105, 116, 32, 97, 32, 98, 121, 116, 101, 0
.Ls136: db 116, 104, 101, 32, 110, 117, 109, 98, 101, 114, 32, 100, 111, 101, 115, 32, 110, 111, 116, 32, 102, 105, 116, 59, 32, 108, 111, 97, 100, 32, 105, 116, 32, 105, 110, 116, 111, 32, 97, 32, 114, 101, 103, 105, 115, 116, 101, 114, 32, 102, 105, 114, 115, 116, 0
.Ls137: db 117, 115, 101, 32, 109, 111, 118, 115, 100, 32, 111, 114, 32, 109, 111, 118, 113, 32, 102, 111, 114, 32, 97, 110, 32, 120, 109, 109, 32, 114, 101, 103, 105, 115, 116, 101, 114, 0
.Ls138: db 116, 104, 101, 32, 114, 101, 103, 105, 115, 116, 101, 114, 115, 32, 100, 105, 102, 102, 101, 114, 32, 105, 110, 32, 119, 105, 100, 116, 104, 0
.Ls139: db 116, 104, 111, 115, 101, 32, 111, 112, 101, 114, 97, 110, 100, 115, 32, 100, 111, 32, 110, 111, 116, 32, 103, 111, 32, 116, 111, 103, 101, 116, 104, 101, 114, 0
.Ls140: db 116, 104, 97, 116, 32, 119, 97, 110, 116, 115, 32, 97, 32, 119, 105, 100, 101, 114, 32, 114, 101, 103, 105, 115, 116, 101, 114, 32, 102, 105, 114, 115, 116, 0
.Ls141: db 116, 104, 97, 116, 32, 116, 97, 107, 101, 115, 32, 97, 32, 98, 121, 116, 101, 32, 111, 114, 32, 97, 32, 119, 111, 114, 100, 58, 32, 97, 32, 98, 121, 116, 101, 32, 111, 114, 32, 119, 111, 114, 100, 32, 114, 101, 103, 105, 115, 116, 101, 114, 44, 32, 111, 114, 32, 98, 121, 116, 101, 47, 119, 111, 114, 100, 32, 91, 46, 46, 93, 0
.Ls142: db 109, 111, 118, 115, 120, 100, 32, 119, 97, 110, 116, 115, 32, 97, 32, 54, 52, 45, 98, 105, 116, 32, 114, 101, 103, 105, 115, 116, 101, 114, 32, 102, 105, 114, 115, 116, 0
.Ls143: db 109, 111, 118, 115, 120, 100, 32, 116, 97, 107, 101, 115, 32, 97, 32, 100, 111, 117, 98, 108, 101, 119, 111, 114, 100, 58, 32, 97, 32, 51, 50, 45, 98, 105, 116, 32, 114, 101, 103, 105, 115, 116, 101, 114, 44, 32, 111, 114, 32, 100, 119, 111, 114, 100, 32, 91, 46, 46, 93, 0
.Ls144: db 115, 101, 116, 46, 46, 32, 119, 97, 110, 116, 115, 32, 97, 32, 98, 121, 116, 101, 32, 114, 101, 103, 105, 115, 116, 101, 114, 0
.Ls145: db 108, 101, 97, 32, 119, 97, 110, 116, 115, 32, 97, 32, 54, 52, 45, 98, 105, 116, 32, 114, 101, 103, 105, 115, 116, 101, 114, 32, 97, 110, 100, 32, 98, 114, 97, 99, 107, 101, 116, 115, 0
.Ls146: db 116, 101, 115, 116, 32, 116, 97, 107, 101, 115, 32, 97, 32, 114, 101, 103, 105, 115, 116, 101, 114, 32, 111, 114, 32, 97, 32, 110, 117, 109, 98, 101, 114, 32, 115, 101, 99, 111, 110, 100, 0
.Ls147: db 97, 32, 115, 104, 105, 102, 116, 32, 99, 111, 117, 110, 116, 32, 105, 115, 32, 97, 32, 110, 117, 109, 98, 101, 114, 32, 111, 114, 32, 99, 108, 0
.Ls148: db 106, 117, 109, 112, 32, 116, 111, 32, 97, 32, 110, 97, 109, 101, 0
.Ls149: db 106, 101, 0
.Ls150: db 106, 122, 0
.Ls151: db 106, 110, 101, 0
.Ls152: db 106, 110, 122, 0
.Ls153: db 106, 108, 0
.Ls154: db 106, 103, 101, 0
.Ls155: db 106, 108, 101, 0
.Ls156: db 106, 103, 0
.Ls157: db 106, 98, 0
.Ls158: db 106, 97, 101, 0
.Ls159: db 106, 98, 101, 0
.Ls160: db 106, 97, 0
.Ls161: db 106, 115, 0
.Ls162: db 106, 110, 115, 0
.Ls163: db 106, 99, 0
.Ls164: db 106, 110, 99, 0
.Ls165: db 115, 116, 114, 105, 110, 103, 115, 32, 103, 111, 32, 119, 105, 116, 104, 32, 100, 98, 0
.Ls166: db 116, 104, 101, 32, 113, 117, 111, 116, 101, 32, 110, 101, 118, 101, 114, 32, 99, 108, 111, 115, 101, 115, 0
.Ls167: db 97, 32, 110, 97, 109, 101, 32, 103, 111, 101, 115, 32, 119, 105, 116, 104, 32, 100, 113, 0
.Ls168: db 97, 32, 99, 111, 109, 109, 97, 32, 115, 104, 111, 117, 108, 100, 32, 115, 101, 112, 97, 114, 97, 116, 101, 32, 116, 104, 101, 32, 118, 97, 108, 117, 101, 115, 0
.Ls169: db 115, 101, 99, 116, 105, 111, 110, 0
.Ls170: db 100, 97, 116, 97, 0
.Ls171: db 99, 111, 100, 101, 0
.Ls172: db 115, 101, 99, 116, 105, 111, 110, 32, 99, 111, 100, 101, 44, 32, 111, 114, 32, 115, 101, 99, 116, 105, 111, 110, 32, 100, 97, 116, 97, 0
.Ls173: db 100, 98, 0
.Ls174: db 100, 119, 0
.Ls175: db 100, 100, 0
.Ls176: db 100, 113, 0
.Ls177: db 114, 101, 115, 0
.Ls178: db 114, 101, 115, 32, 119, 97, 110, 116, 115, 32, 97, 32, 99, 111, 117, 110, 116, 0
.Ls179: db 97, 108, 105, 103, 110, 0
.Ls180: db 97, 108, 105, 103, 110, 32, 119, 97, 110, 116, 115, 32, 97, 32, 99, 111, 117, 110, 116, 0
.Ls181: db 115, 121, 115, 99, 97, 108, 108, 0
.Ls182: db 114, 101, 116, 0
.Ls183: db 110, 111, 112, 0
.Ls184: db 99, 113, 111, 0
.Ls185: db 99, 100, 113, 0
.Ls186: db 99, 100, 113, 101, 0
.Ls187: db 109, 111, 118, 0
.Ls188: db 109, 111, 118, 122, 120, 0
.Ls189: db 109, 111, 118, 115, 120, 0
.Ls190: db 109, 111, 118, 115, 120, 100, 0
.Ls191: db 108, 101, 97, 0
.Ls192: db 97, 100, 100, 0
.Ls193: db 111, 114, 0
.Ls194: db 97, 110, 100, 0
.Ls195: db 115, 117, 98, 0
.Ls196: db 120, 111, 114, 0
.Ls197: db 99, 109, 112, 0
.Ls198: db 116, 101, 115, 116, 0
.Ls199: db 115, 104, 108, 0
.Ls200: db 115, 104, 114, 0
.Ls201: db 115, 97, 114, 0
.Ls202: db 105, 109, 117, 108, 0
.Ls203: db 105, 109, 117, 108, 32, 119, 97, 110, 116, 115, 32, 97, 32, 119, 105, 100, 101, 32, 114, 101, 103, 105, 115, 116, 101, 114, 32, 102, 105, 114, 115, 116, 0
.Ls204: db 120, 99, 104, 103, 0
.Ls205: db 120, 99, 104, 103, 32, 119, 97, 110, 116, 115, 32, 116, 119, 111, 32, 114, 101, 103, 105, 115, 116, 101, 114, 115, 32, 97, 108, 105, 107, 101, 0
.Ls206: db 105, 110, 99, 0
.Ls207: db 100, 101, 99, 0
.Ls208: db 110, 111, 116, 0
.Ls209: db 110, 101, 103, 0
.Ls210: db 109, 117, 108, 0
.Ls211: db 105, 109, 117, 108, 0
.Ls212: db 100, 105, 118, 0
.Ls213: db 105, 100, 105, 118, 0
.Ls214: db 112, 117, 115, 104, 0
.Ls215: db 112, 117, 115, 104, 32, 116, 97, 107, 101, 115, 32, 97, 32, 54, 52, 45, 98, 105, 116, 32, 114, 101, 103, 105, 115, 116, 101, 114, 32, 111, 114, 32, 97, 32, 110, 117, 109, 98, 101, 114, 0
.Ls216: db 112, 111, 112, 0
.Ls217: db 112, 111, 112, 32, 116, 97, 107, 101, 115, 32, 97, 32, 54, 52, 45, 98, 105, 116, 32, 114, 101, 103, 105, 115, 116, 101, 114, 0
.Ls218: db 99, 97, 108, 108, 0
.Ls219: db 99, 97, 108, 108, 32, 97, 32, 110, 97, 109, 101, 32, 111, 114, 32, 97, 32, 54, 52, 45, 98, 105, 116, 32, 114, 101, 103, 105, 115, 116, 101, 114, 0
.Ls220: db 106, 109, 112, 0
.Ls221: db 106, 109, 112, 32, 116, 111, 32, 97, 32, 110, 97, 109, 101, 32, 111, 114, 32, 97, 32, 54, 52, 45, 98, 105, 116, 32, 114, 101, 103, 105, 115, 116, 101, 114, 0
.Ls222: db 105, 32, 100, 111, 32, 110, 111, 116, 32, 107, 110, 111, 119, 0
.Ls223: db 116, 104, 101, 114, 101, 32, 105, 115, 32, 110, 111, 32, 99, 111, 100, 101, 32, 105, 110, 32, 105, 116, 0
.Ls224: db 116, 104, 101, 32, 105, 109, 97, 103, 101, 32, 105, 115, 32, 108, 97, 114, 103, 101, 114, 32, 116, 104, 97, 110, 32, 116, 104, 101, 32, 114, 111, 111, 109, 32, 102, 111, 114, 32, 105, 116, 0
