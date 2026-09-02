; made by the compiler; the source lies beside this
section code
    mov rbp, rsp
    call f_main
    mov rdi, rax
    mov rax, 0
    syscall

f_from_words:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    imul rax, rdi
    jmp .Lret1
.Lret1:
    mov rsp, rbp
    pop rbp
    ret

f_add:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    jmp .Lret2
.Lret2:
    mov rsp, rbp
    pop rbp
    ret

f_mul:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 16]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    imul rax, rdi
    jmp .Lret3
.Lret3:
    mov rsp, rbp
    pop rbp
    ret

f_say:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    lea rax, [rbp - 40]
    push rax
    lea rax, [rbp - 32]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 48]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L1:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L3
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 32
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L2:
    lea rax, [rbp - 48]
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
    lea rax, [rbp - 48]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L4:
    lea rax, [rbp - 48]
    mov rax, [rax]
    push rax
    mov rax, 24
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L7
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    test rax, rax
    je .L7
    mov rax, 1
    jmp .L8
.L7:
    mov rax, 0
.L8:
    test rax, rax
    je .L6
    lea rax, [rbp - 40]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    lea rax, [rbp - 48]
    mov rax, [rax]
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    pop rdi
    movsx rax, al
    mov byte [rdi], al
.L5:
    lea rax, [rbp - 48]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L4
.L6:
    lea rax, [rbp - 32]
    push rax
    mov rax, 2
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
    lea rax, [rbp - 32]
    push rax
    mov rax, 1
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
    lea rax, [rbp - 32]
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
    push rax
    mov rax, 1415071060
    push rax
    lea rax, [v_console_handle]
    mov rax, [rax]
    push rax
    mov rax, 2
    push rax
    pop rax
    pop rdi
    pop rsi
    pop rdx
    pop r10
    pop r8
    syscall
.Lret4:
    mov rsp, rbp
    pop rbp
    ret

f_check:
    push rbp
    mov rbp, rsp
    sub rsp, 48
    mov [rbp - 8], rdi
    mov byte [rbp - 9], sil
    lea rax, [rbp - 33]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 99
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 104
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 101
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 99
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 107
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    push rax
    mov rax, 5
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 32
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    push rax
    mov rax, 6
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 48
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    cqo
    idiv rdi
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, al
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 48
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
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
    lea rax, [rbp - 33]
    push rax
    mov rax, 8
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 32
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 9]
    movzx rax, byte [rax]
    test rax, rax
    je .L9
    lea rax, [rbp - 33]
    push rax
    mov rax, 9
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 111
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 107
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    push rax
    mov rax, 11
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    jmp .L10
.L9:
    lea rax, [rbp - 33]
    push rax
    mov rax, 9
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 98
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 97
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    push rax
    mov rax, 11
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 100
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 33]
    push rax
    mov rax, 12
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 0
    pop rdi
    movsx rax, al
    mov byte [rdi], al
    lea rax, [v_bad_count]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
.L10:
    lea rax, [rbp - 33]
    push rax
    pop rdi
    call f_say
.Lret5:
    mov rsp, rbp
    pop rbp
    ret

f_sum_var:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    mov [rbp - 48], rdi
    mov [rbp - 40], rsi
    mov [rbp - 32], rdx
    mov [rbp - 24], rcx
    mov [rbp - 16], r8
    mov [rbp - 8], r9
    lea rax, [rbp - 64]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 56]
    lea rcx, [rbp - 40]
    mov [rax], rcx
.L11:
    mov rax, 0
    push rax
    lea rax, [rbp - 48]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, -1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L12
    lea rax, [rbp - 64]
    push rax
    lea rax, [rbp - 56]
    mov rdi, rax
    mov rax, [rdi]
    add qword [rdi], 8
    mov rax, [rax]
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
    jmp .L11
.L12:
    mov rax, 0
    lea rax, [rbp - 64]
    mov rax, [rax]
    jmp .Lret6
.Lret6:
    mov rsp, rbp
    pop rbp
    ret

f_average:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    movsd [rbp - 8], xmm0
    movsd [rbp - 16], xmm1
    movsd [rbp - 24], xmm2
    lea rax, [rbp - 8]
    movsd xmm0, [rax]
    sub rsp, 8
    movsd [rsp], xmm0
    lea rax, [rbp - 16]
    movsd xmm0, [rax]
    movsd xmm1, xmm0
    movsd xmm0, [rsp]
    add rsp, 8
    addsd xmm0, xmm1
    sub rsp, 8
    movsd [rsp], xmm0
    lea rax, [rbp - 24]
    movsd xmm0, [rax]
    movsd xmm1, xmm0
    movsd xmm0, [rsp]
    add rsp, 8
    addsd xmm0, xmm1
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 4613937818241073152
    movq xmm0, rax
    movsd xmm1, xmm0
    movsd xmm0, [rsp]
    add rsp, 8
    divsd xmm0, xmm1
    jmp .Lret7
.Lret7:
    mov rsp, rbp
    pop rbp
    ret

f_counted:
    push rbp
    mov rbp, rsp
    lea rax, [v_calls.1]
    mov rdi, rax
    mov rax, [rax]
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    jmp .Lret8
.Lret8:
    mov rsp, rbp
    pop rbp
    ret

f_main:
    push rbp
    mov rbp, rsp
    sub rsp, 144
    mov [rbp - 8], rdi
    mov [rbp - 16], rsi
    lea rax, [v_console_handle]
    push rax
    lea rax, [rbp - 8]
    mov rax, [rax]
    pop rdi
    mov [rdi], rax
    lea rax, [v_bad_count]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    mov rax, 4
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 16
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L19
    mov rax, 9
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L21
    mov rax, 3
    jmp .L22
.L21:
    mov rax, 9
.L22:
    push rax
    mov rax, 9
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L19
    mov rax, 1
    jmp .L20
.L19:
    mov rax, 0
.L20:
    test rax, rax
    je .L17
    mov rax, 1
    test rax, rax
    je .L17
    mov rax, 1
    jmp .L18
.L17:
    mov rax, 0
.L18:
    push rax
    mov rax, 1
    push rax
    pop rdi
    pop rsi
    call f_check
    lea rax, [.Ls5]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 104
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L23
    mov rax, 12
    push rax
    mov rax, 12
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L23
    mov rax, 1
    jmp .L24
.L23:
    mov rax, 0
.L24:
    push rax
    mov rax, 2
    push rax
    pop rdi
    pop rsi
    call f_check
    lea rax, [rbp - 23]
    push rax
    mov rax, 1
    pop rdi
    movzx rax, al
    mov byte [rdi], al
    lea rax, [rbp - 23]
    add rax, 1
    push rax
    mov rax, 287454020
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 23]
    add rax, 5
    push rax
    mov rax, 21862
    pop rdi
    movzx rax, ax
    mov word [rdi], ax
    lea rax, [rbp - 32]
    push rax
    lea rax, [rbp - 23]
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 68
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L29
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 17
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L29
    mov rax, 1
    jmp .L30
.L29:
    mov rax, 0
.L30:
    test rax, rax
    je .L27
    lea rax, [rbp - 32]
    mov rax, [rax]
    push rax
    mov rax, 5
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 102
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L27
    mov rax, 1
    jmp .L28
.L27:
    mov rax, 0
.L28:
    test rax, rax
    je .L25
    mov rax, 7
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L25
    mov rax, 1
    jmp .L26
.L25:
    mov rax, 0
.L26:
    push rax
    mov rax, 3
    push rax
    pop rdi
    pop rsi
    call f_check
    lea rax, [v_seed]
    movzx rax, byte [rax]
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L35
    lea rax, [v_seed]
    add rax, 4
    mov eax, dword [rax]
    push rax
    mov rax, 70000
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L35
    mov rax, 1
    jmp .L36
.L35:
    mov rax, 0
.L36:
    test rax, rax
    je .L33
    lea rax, [v_seed]
    add rax, 8
    movzx rax, word [rax]
    push rax
    mov rax, 300
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L33
    mov rax, 1
    jmp .L34
.L33:
    mov rax, 0
.L34:
    test rax, rax
    je .L31
    mov rax, 12
    push rax
    mov rax, 12
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L31
    mov rax, 1
    jmp .L32
.L31:
    mov rax, 0
.L32:
    push rax
    mov rax, 4
    push rax
    pop rdi
    pop rsi
    call f_check
    lea rax, [rbp - 36]
    push rax
    mov rax, 67305985
    pop rdi
    mov eax, eax
    mov dword [rdi], eax
    lea rax, [rbp - 36]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L39
    lea rax, [rbp - 36]
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    add rax, rdi
    movzx rax, byte [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L39
    mov rax, 1
    jmp .L40
.L39:
    mov rax, 0
.L40:
    test rax, rax
    je .L37
    mov rax, 4
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L37
    mov rax, 1
    jmp .L38
.L37:
    mov rax, 0
.L38:
    push rax
    mov rax, 5
    push rax
    pop rdi
    pop rsi
    call f_check
    lea rax, [rbp - 40]
    push rax
    mov rax, 5
    pop rdi
    mov eax, eax
    mov rcx, 7
    and rax, rcx
    mov r9, rax
    mov eax, dword [rdi]
    mov rcx, 7
    not rcx
    and rax, rcx
    or rax, r9
    mov dword [rdi], eax
    lea rax, [rbp - 40]
    push rax
    mov rax, 1
    pop rdi
    mov eax, eax
    mov rcx, 1
    and rax, rcx
    shl rax, 3
    mov r9, rax
    mov eax, dword [rdi]
    mov rcx, 8
    not rcx
    and rax, rcx
    or rax, r9
    mov dword [rdi], eax
    lea rax, [rbp - 40]
    push rax
    mov rax, 9
    pop rdi
    mov eax, eax
    mov rcx, 15
    and rax, rcx
    shl rax, 4
    mov r9, rax
    mov eax, dword [rdi]
    mov rcx, 240
    not rcx
    and rax, rcx
    or rax, r9
    mov dword [rdi], eax
    lea rax, [rbp - 40]
    push rax
    mov rax, 3
    neg rax
    pop rdi
    movsxd rax, eax
    mov rcx, 31
    and rax, rcx
    shl rax, 8
    mov r9, rax
    mov eax, dword [rdi]
    mov rcx, 7936
    not rcx
    and rax, rcx
    or rax, r9
    mov dword [rdi], eax
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    shl rax, 61
    shr rax, 61
    push rax
    mov rax, 5
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L45
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    shl rax, 60
    shr rax, 63
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L45
    mov rax, 1
    jmp .L46
.L45:
    mov rax, 0
.L46:
    test rax, rax
    je .L43
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    shl rax, 56
    shr rax, 60
    push rax
    mov rax, 9
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L43
    mov rax, 1
    jmp .L44
.L43:
    mov rax, 0
.L44:
    test rax, rax
    je .L41
    mov rax, 4
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L41
    mov rax, 1
    jmp .L42
.L41:
    mov rax, 0
.L42:
    push rax
    mov rax, 6
    push rax
    pop rdi
    pop rsi
    call f_check
    lea rax, [rbp - 40]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    mov r8, rax
    mov eax, dword [rax]
    shl rax, 56
    shr rax, 60
    add rax, rdi
    mov rdi, r8
    mov eax, eax
    mov rcx, 15
    and rax, rcx
    shl rax, 4
    mov r9, rax
    mov eax, dword [rdi]
    mov rcx, 240
    not rcx
    and rax, rcx
    or rax, r9
    mov dword [rdi], eax
    lea rax, [rbp - 40]
    push rax
    mov rax, 7
    pop rdi
    mov eax, eax
    mov rcx, 7
    and rax, rcx
    mov r9, rax
    mov eax, dword [rdi]
    mov rcx, 7
    not rcx
    and rax, rcx
    or rax, r9
    mov dword [rdi], eax
    lea rax, [rbp - 40]
    push rax
    mov rax, 0
    pop rdi
    mov eax, eax
    mov rcx, 1
    and rax, rcx
    shl rax, 3
    mov r9, rax
    mov eax, dword [rdi]
    mov rcx, 8
    not rcx
    and rax, rcx
    or rax, r9
    mov dword [rdi], eax
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    shl rax, 56
    shr rax, 60
    push rax
    mov rax, 11
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L51
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    shl rax, 61
    shr rax, 61
    push rax
    mov rax, 7
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L51
    mov rax, 1
    jmp .L52
.L51:
    mov rax, 0
.L52:
    test rax, rax
    je .L49
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    shl rax, 60
    shr rax, 63
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L49
    mov rax, 1
    jmp .L50
.L49:
    mov rax, 0
.L50:
    test rax, rax
    je .L47
    lea rax, [rbp - 40]
    mov eax, dword [rax]
    shl rax, 51
    sar rax, 59
    push rax
    mov rax, 3
    neg rax
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L47
    mov rax, 1
    jmp .L48
.L47:
    mov rax, 0
.L48:
    push rax
    mov rax, 7
    push rax
    pop rdi
    pop rsi
    call f_check
    lea rax, [v_table]
    push rax
    mov rax, 1
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
    mov rax, 20
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L57
    lea rax, [v_table]
    push rax
    mov rax, 3
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
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L57
    mov rax, 1
    jmp .L58
.L57:
    mov rax, 0
.L58:
    test rax, rax
    je .L55
    lea rax, [v_names]
    push rax
    mov rax, 2
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
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    movsx rax, byte [rax]
    push rax
    mov rax, 119
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L55
    mov rax, 1
    jmp .L56
.L55:
    mov rax, 0
.L56:
    test rax, rax
    je .L53
    lea rax, [v_ops]
    push rax
    mov rax, 1
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
    mov rax, 7
    push rax
    mov rax, 6
    push rax
    pop rdi
    pop rsi
    mov rax, [rsp + 0]
    call rax
    add rsp, 8
    push rax
    mov rax, 42
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L53
    mov rax, 1
    jmp .L54
.L53:
    mov rax, 0
.L54:
    push rax
    mov rax, 8
    push rax
    pop rdi
    pop rsi
    call f_check
    lea rax, [rbp - 64]
    mov rdi, rax
    lea rsi, [.Li0]
    mov rcx, 24
.L59:
    mov al, byte [rsi]
    mov byte [rdi], al
    inc rsi
    inc rdi
    dec rcx
    jne .L59
    lea rax, [rbp - 76]
    mov rdi, rax
    lea rsi, [.Li1]
    mov rcx, 12
.L60:
    mov al, byte [rsi]
    mov byte [rdi], al
    inc rsi
    inc rdi
    dec rcx
    jne .L60
    lea rax, [rbp - 64]
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
    push rax
    lea rax, [rbp - 64]
    push rax
    mov rax, 1
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
    add rax, rdi
    push rax
    lea rax, [rbp - 64]
    push rax
    mov rax, 2
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
    add rax, rdi
    push rax
    mov rax, 6
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L63
    lea rax, [rbp - 76]
    movzx rax, byte [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L63
    mov rax, 1
    jmp .L64
.L63:
    mov rax, 0
.L64:
    test rax, rax
    je .L61
    lea rax, [rbp - 76]
    add rax, 8
    movzx rax, word [rax]
    push rax
    mov rax, 5
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L61
    mov rax, 1
    jmp .L62
.L61:
    mov rax, 0
.L62:
    push rax
    mov rax, 9
    push rax
    pop rdi
    pop rsi
    call f_check
    mov rax, 40
    push rax
    mov rax, 30
    push rax
    mov rax, 20
    push rax
    mov rax, 10
    push rax
    mov rax, 4
    push rax
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop r8
    call f_sum_var
    push rax
    mov rax, 100
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L65
    mov rax, 0
    push rax
    pop rdi
    call f_sum_var
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L65
    mov rax, 1
    jmp .L66
.L65:
    mov rax, 0
.L66:
    push rax
    mov rax, 10
    push rax
    pop rdi
    pop rsi
    call f_check
    lea rax, [rbp - 88]
    push rax
    mov rax, 4618441417868443648
    movq xmm0, rax
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 4611686018427387904
    movq xmm0, rax
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 4607182418800017408
    movq xmm0, rax
    sub rsp, 8
    movsd [rsp], xmm0
    movsd xmm0, [rsp]
    add rsp, 8
    movsd xmm1, [rsp]
    add rsp, 8
    movsd xmm2, [rsp]
    add rsp, 8
    call f_average
    pop rdi
    movsd [rdi], xmm0
    mov rax, 4613915300242936300
    movq xmm0, rax
    sub rsp, 8
    movsd [rsp], xmm0
    lea rax, [rbp - 88]
    movsd xmm0, [rax]
    movsd xmm1, xmm0
    movsd xmm0, [rsp]
    add rsp, 8
    ucomisd xmm1, xmm0
    seta al
    movzx rax, al
    test rax, rax
    je .L67
    lea rax, [rbp - 88]
    movsd xmm0, [rax]
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 4613960336239210004
    movq xmm0, rax
    movsd xmm1, xmm0
    movsd xmm0, [rsp]
    add rsp, 8
    ucomisd xmm1, xmm0
    seta al
    movzx rax, al
    test rax, rax
    je .L67
    mov rax, 1
    jmp .L68
.L67:
    mov rax, 0
.L68:
    push rax
    mov rax, 11
    push rax
    pop rdi
    pop rsi
    call f_check
    lea rax, [rbp - 92]
    push rax
    mov rax, 4612811918334230528
    movq xmm0, rax
    pop rdi
    cvtsd2ss xmm1, xmm0
    movss dword [rdi], xmm1
    lea rax, [rbp - 104]
    push rax
    lea rax, [rbp - 92]
    movss xmm0, dword [rax]
    cvtss2sd xmm0, xmm0
    sub rsp, 8
    movsd [rsp], xmm0
    lea rax, [rbp - 92]
    movss xmm0, dword [rax]
    cvtss2sd xmm0, xmm0
    movsd xmm1, xmm0
    movsd xmm0, [rsp]
    add rsp, 8
    mulsd xmm0, xmm1
    pop rdi
    movsd [rdi], xmm0
    lea rax, [rbp - 112]
    push rax
    lea rax, [rbp - 104]
    movsd xmm0, [rax]
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 4616189618054758400
    movq xmm0, rax
    movsd xmm1, xmm0
    movsd xmm0, [rsp]
    add rsp, 8
    mulsd xmm0, xmm1
    cvttsd2si rax, xmm0
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 112]
    mov rax, [rax]
    push rax
    mov rax, 25
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L71
    mov rax, 4609434218613702656
    movq xmm0, rax
    mov rax, -9223372036854775808
    movq xmm1, rax
    xorpd xmm0, xmm1
    cvttsd2si rax, xmm0
    push rax
    mov rax, 1
    neg rax
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
    je .L69
    mov rax, 10
    cvtsi2sd xmm0, rax
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 4616189618054758400
    movq xmm0, rax
    movsd xmm1, xmm0
    movsd xmm0, [rsp]
    add rsp, 8
    divsd xmm0, xmm1
    sub rsp, 8
    movsd [rsp], xmm0
    mov rax, 4612811918334230528
    movq xmm0, rax
    movsd xmm1, xmm0
    movsd xmm0, [rsp]
    add rsp, 8
    ucomisd xmm0, xmm1
    sete al
    movzx rax, al
    test rax, rax
    je .L69
    mov rax, 1
    jmp .L70
.L69:
    mov rax, 0
.L70:
    push rax
    mov rax, 12
    push rax
    pop rdi
    pop rsi
    call f_check
    lea rax, [rbp - 120]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.Lg9_again:
    lea rax, [rbp - 120]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    mov rax, 5
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L73
    jmp .Lg9_again
    jmp .L74
.L73:
.L74:
    lea rax, [rbp - 120]
    mov rax, [rax]
    push rax
    mov rax, 5
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    push rax
    mov rax, 13
    push rax
    pop rdi
    pop rsi
    call f_check
    call f_counted
    call f_counted
    lea rax, [rbp - 122]
    push rax
    mov rax, 7
    neg rax
    pop rdi
    movsx rax, ax
    mov word [rdi], ax
    lea rax, [rbp - 124]
    push rax
    mov rax, 65535
    pop rdi
    movzx rax, ax
    mov word [rdi], ax
    call f_counted
    push rax
    mov rax, 3
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L79
    lea rax, [rbp - 122]
    movsx rax, word [rax]
    push rax
    mov rax, 2
    mov rdi, rax
    pop rax
    imul rax, rdi
    push rax
    mov rax, 14
    neg rax
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L79
    mov rax, 1
    jmp .L80
.L79:
    mov rax, 0
.L80:
    test rax, rax
    je .L77
    lea rax, [rbp - 124]
    movzx rax, word [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    add rax, rdi
    push rax
    mov rax, 65536
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
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
    mov rax, 300
    movzx rax, al
    push rax
    mov rax, 44
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L75
    mov rax, 1
    jmp .L76
.L75:
    mov rax, 0
.L76:
    push rax
    mov rax, 14
    push rax
    pop rdi
    pop rsi
    call f_check
    lea rax, [rbp - 136]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
    lea rax, [rbp - 144]
    push rax
    mov rax, 0
    pop rdi
    mov [rdi], rax
.L81:
    lea rax, [rbp - 144]
    mov rax, [rax]
    push rax
    mov rax, 6
    mov rdi, rax
    pop rax
    cmp rax, rdi
    setl al
    movzx rax, al
    test rax, rax
    je .L83
    lea rax, [rbp - 144]
    mov rax, [rax]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L84
    jmp .L82
    jmp .L85
.L84:
.L85:
    lea rax, [rbp - 144]
    mov rax, [rax]
    mov rcx, 3
    cmp rax, rcx
    je .L15
    mov rcx, 2
    cmp rax, rcx
    je .L14
    mov rcx, 0
    cmp rax, rcx
    je .L13
    jmp .L16
.L13:
    lea rax, [rbp - 136]
    push rax
    mov rax, 1
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
.L14:
    lea rax, [rbp - 136]
    push rax
    mov rax, 10
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
    jmp .L86
.L15:
    lea rax, [rbp - 136]
    push rax
    mov rax, 100
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
    jmp .L86
.L16:
    lea rax, [rbp - 136]
    push rax
    mov rax, 1000
    mov rdi, rax
    pop rax
    mov r8, rax
    mov rax, [rax]
    add rax, rdi
    mov rdi, r8
    mov [rdi], rax
.L86:
    lea rax, [rbp - 144]
    mov rax, [rax]
    push rax
    mov rax, 4
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L87
    jmp .L83
    jmp .L88
.L87:
.L88:
.L82:
    lea rax, [rbp - 144]
    mov rdi, rax
    mov rax, [rax]
    mov r8, rax
    add rax, 1
    push rdi
    pop rdi
    mov [rdi], rax
    mov rax, r8
    jmp .L81
.L83:
    lea rax, [rbp - 136]
    mov rax, [rax]
    push rax
    mov rax, 1121
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    push rax
    mov rax, 15
    push rax
    pop rdi
    pop rsi
    call f_check
    mov rax, 3
    push rax
    pop rdi
    call f_from_words
    push rax
    mov rax, 30
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    push rax
    mov rax, 16
    push rax
    pop rdi
    pop rsi
    call f_check
    lea rax, [v_bad_count]
    mov rax, [rax]
    push rax
    mov rax, 0
    mov rdi, rax
    pop rax
    cmp rax, rdi
    sete al
    movzx rax, al
    test rax, rax
    je .L89
    lea rax, [.Ls6]
    push rax
    pop rdi
    call f_say
    jmp .L90
.L89:
    lea rax, [.Ls7]
    push rax
    pop rdi
    call f_say
.L90:
    lea rax, [v_bad_count]
    mov rax, [rax]
    jmp .Lret9
.Lret9:
    mov rsp, rbp
    pop rbp
    ret

section data
    align 8
v_names:
    dq .Ls2
    dq .Ls3
    dq .Ls4
    align 8
v_table:
    db 10, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0
    db 30, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    align 4
v_seed:
    db 7, 0, 0, 0, 112, 17, 1, 0, 44, 1, 0, 0
    align 8
v_ops:
    dq f_add
    dq f_mul
    align 8
v_console_handle:
    res 8
    align 8
v_bad_count:
    res 8
    align 8
v_calls.1:
    db 0, 0, 0, 0, 0, 0, 0, 0
    align 8
.Li0:
    db 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0
    db 3, 0, 0, 0, 0, 0, 0, 0
    align 8
.Li1:
    db 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0
.Ls0: db 112, 97, 99, 107, 101, 100, 32, 115, 116, 114, 117, 99, 116, 0
.Ls1: db 112, 97, 100, 100, 101, 100, 32, 115, 116, 114, 117, 99, 116, 0
.Ls2: db 122, 101, 114, 111, 0
.Ls3: db 111, 110, 101, 0
.Ls4: db 116, 119, 111, 0
.Ls5: db 104, 101, 108, 108, 111, 0
.Ls6: db 97, 108, 108, 32, 99, 104, 101, 99, 107, 115, 32, 111, 107, 0
.Ls7: db 115, 111, 109, 101, 32, 99, 104, 101, 99, 107, 115, 32, 98, 97, 100, 0
