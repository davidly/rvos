; Build on Windows in a Visual Studio vcvars64.bat cmd window using a .bat script like this:
; ml64 /nologo %1.asm /Zd /Zf /Zi /link /OPT:REF /nologo ^
;      /subsystem:console ^
;      /defaultlib:kernel32.lib ^
;      /defaultlib:user32.lib ^
;      /defaultlib:libucrt.lib ^
;      /defaultlib:libcmt.lib ^
;      /entry:mainCRTStartup
;
; BA flags: use registers: yes, expression optimization: yes
extern printf: PROC
extern exit: PROC
extern atoi: PROC
extern QueryPerformanceCounter: PROC
extern QueryPerformanceFrequency: PROC
extern GetLocalTime: PROC
data_segment SEGMENT ALIGN( 4096 ) 'DATA'
  align 16
       var_b DD 9 DUP (0)
  align 16
      var_sp DD 10 DUP (0)
  align 16
      var_sv DD 10 DUP (0)
  align 16
      var_sa DD 10 DUP (0)
  align 16
      var_sb DD 10 DUP (0)
    str_6_2   db  'start time: ', 0
    str_26_4   db  ' for ', 0
    str_26_8   db  ' iterations', 0
    str_27_2   db  'end time: ', 0
    str_28_2   db  'final move count ', 0
   ; variable wi% (referenced 31 times) will use register r15d
   ; variable st% (referenced 17 times) will use register r14d
   ; variable v% (referenced 14 times) will use register r13d
   ; variable re% (referenced 11 times) will use register r12d
   ; variable al% (referenced 9 times) will use register r11d
   ; variable be% (referenced 9 times) will use register r10d
   ; variable p% (referenced 9 times) will use register r9d
   ; variable lo% (referenced 4 times) will use register esi
  align 16
      var_av DD   0
       var_l DD   0
      var_mc DD   0
  align 16
    gosubCount     dq    0
    startTicks     dq    0
    perfFrequency  dq    0
    currentTicks   dq    0
    currentTime    dq 2  DUP(0)
    errorString    db    'internal error', 10, 0
    startString    db    'running basic', 10, 0
    stopString     db    'done running basic', 10, 0
    newlineString  db    10, 0
    elapString     db    '%lld microseconds (-6)', 0
    timeString     db    '%02d:%02d:%02d:%03d', 0
    intString      db    '%d', 0
    strString      db    '%s', 0
data_segment ENDS
code_segment SEGMENT ALIGN( 4096 ) 'CODE'
main PROC
    push     rbp
    mov      rbp, rsp
    sub      rsp, 32 + 8 * 4
    cmp      rcx, 2
    jl       no_arguments
    mov      rcx, [ rdx + 8 ]
    call     atoi
    mov      DWORD PTR [var_av], eax
  no_arguments:
    lea      rcx, [startString]
    call     printf
    lea      rcx, [startTicks]
    call     QueryPerformanceCounter
    lea      rcx, [perfFrequency]
    call     QueryPerformanceFrequency
    xor      esi, esi
    xor      r9d, r9d
    xor      r10d, r10d
    xor      r11d, r11d
    xor      r12d, r12d
    xor      r13d, r13d
    xor      r14d, r14d
    xor      r15d, r15d
  line_number_0:   ; ===>>> 25 dim b%(9)
  line_number_1:   ; ===>>> 26 dim sp%(10)
  line_number_2:   ; ===>>> 27 dim sv%(10)
  line_number_3:   ; ===>>> 28 dim sa%(10)
  line_number_4:   ; ===>>> 29 dim sb%(10)
  line_number_5:   ; ===>>> 30 mc% = 0
    mov      DWORD PTR [var_mc], 0
  line_number_6:   ; ===>>> 31 print "start time: "; time$
    lea      rcx, [strString]
    lea      rdx, [str_6_2]
    call     call_printf
    call     printTime
    lea      rcx, [newlineString]
    call     call_printf
  line_number_7:   ; ===>>> 32 if 0 <> av% then lo% = av% else lo% = 1
    mov      eax, [var_av]
    cmp      rax, 0
    je       label_else_7
    mov      eax, [var_av]
    mov      esi, eax
    jmp      line_number_8
    align    16
  label_else_7:
    mov      esi, 1
  line_number_8:   ; ===>>> 40 for l% = 1 to lo%
    mov      [var_l], 1
  for_loop_8:
    mov      eax, esi
    cmp      [var_l], eax
    jg       after_for_loop_8
  line_number_9:   ; ===>>> 41 mc% = 0
    mov      DWORD PTR [var_mc], 0
  line_number_10:   ; ===>>> 42 al% = 2
    mov      r11d, 2
  line_number_11:   ; ===>>> 43 be% = 9
    mov      r10d, 9
  line_number_12:   ; ===>>> 44 b%(0) = 1
    mov      eax, 0
    shl      rax, 2
    lea      rbx, [var_b]
    mov      DWORD PTR [rbx + rax], 1
  line_number_13:   ; ===>>> 45 gosub 4000
    lea      rax, line_number_52
    call     label_gosub
  line_number_14:   ; ===>>> 58 al% = 2
    mov      r11d, 2
  line_number_15:   ; ===>>> 59 be% = 9
    mov      r10d, 9
  line_number_16:   ; ===>>> 60 b%(0) = 0
    mov      eax, 0
    shl      rax, 2
    lea      rbx, [var_b]
    mov      DWORD PTR [rbx + rax], 0
  line_number_17:   ; ===>>> 61 b%(1) = 1
    mov      eax, 1
    shl      rax, 2
    lea      rbx, [var_b]
    mov      DWORD PTR [rbx + rax], 1
  line_number_18:   ; ===>>> 62 gosub 4000
    lea      rax, line_number_52
    call     label_gosub
  line_number_19:   ; ===>>> 68 al% = 2
    mov      r11d, 2
  line_number_20:   ; ===>>> 69 be% = 9
    mov      r10d, 9
  line_number_21:   ; ===>>> 70 b%(1) = 0
    mov      eax, 1
    shl      rax, 2
    lea      rbx, [var_b]
    mov      DWORD PTR [rbx + rax], 0
  line_number_22:   ; ===>>> 71 b%(4) = 1
    mov      eax, 4
    shl      rax, 2
    lea      rbx, [var_b]
    mov      DWORD PTR [rbx + rax], 1
  line_number_23:   ; ===>>> 72 gosub 4000
    lea      rax, line_number_52
    call     label_gosub
  line_number_24:   ; ===>>> 73 b%(4) = 0
    mov      eax, 4
    shl      rax, 2
    lea      rbx, [var_b]
    mov      DWORD PTR [rbx + rax], 0
  line_number_25:   ; ===>>> 80 next l%
    inc      DWORD PTR [var_l]
    jmp      for_loop_8
    align    16
  after_for_loop_8:
  line_number_26:   ; ===>>> 85 print elap$ ; " for "; lo%; " iterations"
    call     printElap
    lea      rcx, [strString]
    lea      rdx, [str_26_4]
    call     call_printf
    mov      eax, esi
    lea      rcx, [intString]
    mov      rdx, rax
    call     call_printf
    lea      rcx, [strString]
    lea      rdx, [str_26_8]
    call     call_printf
    lea      rcx, [newlineString]
    call     call_printf
  line_number_27:   ; ===>>> 86 print "end time: "; time$
    lea      rcx, [strString]
    lea      rdx, [str_27_2]
    call     call_printf
    call     printTime
    lea      rcx, [newlineString]
    call     call_printf
  line_number_28:   ; ===>>> 87 print "final move count "; mc%
    lea      rcx, [strString]
    lea      rdx, [str_28_2]
    call     call_printf
    mov      eax, [var_mc]
    lea      rcx, [intString]
    mov      rdx, rax
    call     call_printf
    lea      rcx, [newlineString]
    call     call_printf
  line_number_29:   ; ===>>> 100 end
    jmp      end_execution
  line_number_30:   ; ===>>> 2000 wi% = b%( 0 )
    mov      r15d, [ var_b + 0 ]
  line_number_31:   ; ===>>> 2010 if 0 = wi% goto 2100
    test     r15d, r15d
    je       line_number_34
  line_number_32:   ; ===>>> 2020 if wi% = b%( 1 ) and wi% = b%( 2 ) then return
    cmp      r15d, DWORD PTR [ var_b + 4 ]
    jne      SHORT line_number_33
    cmp      r15d, DWORD PTR [ var_b + 8 ]
    je       label_gosub_return
  line_number_33:   ; ===>>> 2030 if wi% = b%( 3 ) and wi% = b%( 6 ) then return
    cmp      r15d, DWORD PTR [ var_b + 12 ]
    jne      SHORT line_number_34
    cmp      r15d, DWORD PTR [ var_b + 24 ]
    je       label_gosub_return
  line_number_34:   ; ===>>> 2100 wi% = b%( 3 )
    mov      r15d, [ var_b + 12 ]
  line_number_35:   ; ===>>> 2110 if 0 = wi% goto 2200
    test     r15d, r15d
    je       line_number_37
  line_number_36:   ; ===>>> 2120 if wi% = b%( 4 ) and wi% = b%( 5 ) then return
    cmp      r15d, DWORD PTR [ var_b + 16 ]
    jne      SHORT line_number_37
    cmp      r15d, DWORD PTR [ var_b + 20 ]
    je       label_gosub_return
  line_number_37:   ; ===>>> 2200 wi% = b%( 6 )
    mov      r15d, [ var_b + 24 ]
  line_number_38:   ; ===>>> 2210 if 0 = wi% goto 2300
    test     r15d, r15d
    je       line_number_40
  line_number_39:   ; ===>>> 2220 if wi% = b%( 7 ) and wi% = b%( 8 ) then return
    cmp      r15d, DWORD PTR [ var_b + 28 ]
    jne      SHORT line_number_40
    cmp      r15d, DWORD PTR [ var_b + 32 ]
    je       label_gosub_return
  line_number_40:   ; ===>>> 2300 wi% = b%( 1 )
    mov      r15d, [ var_b + 4 ]
  line_number_41:   ; ===>>> 2310 if 0 = wi% goto 2400
    test     r15d, r15d
    je       line_number_43
  line_number_42:   ; ===>>> 2320 if wi% = b%( 4 ) and wi% = b%( 7 ) then return
    cmp      r15d, DWORD PTR [ var_b + 16 ]
    jne      SHORT line_number_43
    cmp      r15d, DWORD PTR [ var_b + 28 ]
    je       label_gosub_return
  line_number_43:   ; ===>>> 2400 wi% = b%( 2 )
    mov      r15d, [ var_b + 8 ]
  line_number_44:   ; ===>>> 2410 if 0 = wi% goto 2500
    test     r15d, r15d
    je       line_number_46
  line_number_45:   ; ===>>> 2420 if wi% = b%( 5 ) and wi% = b%( 8 ) then return
    cmp      r15d, DWORD PTR [ var_b + 20 ]
    jne      SHORT line_number_46
    cmp      r15d, DWORD PTR [ var_b + 32 ]
    je       label_gosub_return
  line_number_46:   ; ===>>> 2500 wi% = b%( 4 )
    mov      r15d, [ var_b + 16 ]
  line_number_47:   ; ===>>> 2510 if 0 = wi% then return
    test     r15d, r15d
    je       label_gosub_return
  line_number_48:   ; ===>>> 2520 if wi% = b%( 0 ) and wi% = b%( 8 ) then return
    cmp      r15d, DWORD PTR [ var_b + 0 ]
    jne      SHORT line_number_49
    cmp      r15d, DWORD PTR [ var_b + 32 ]
    je       label_gosub_return
  line_number_49:   ; ===>>> 2530 if wi% = b%( 2 ) and wi% = b%( 6 ) then return
    cmp      r15d, DWORD PTR [ var_b + 8 ]
    jne      SHORT line_number_50
    cmp      r15d, DWORD PTR [ var_b + 24 ]
    je       label_gosub_return
  line_number_50:   ; ===>>> 2540 wi% = 0
    mov      r15d, 0
  line_number_51:   ; ===>>> 2550 return
    jmp      label_gosub_return
  line_number_52:   ; ===>>> 4030 st% = 0
    mov      r14d, 0
  line_number_53:   ; ===>>> 4040 v% = 0
    mov      r13d, 0
  line_number_54:   ; ===>>> 4060 re% = 0
    mov      r12d, 0
  line_number_55:   ; ===>>> 4100 mc% = mc% + 1
    inc      DWORD PTR [var_mc]
  line_number_56:   ; ===>>> 4104 if st% < 4 then goto 4150
    cmp      r14d, 4
    jl       line_number_63
  line_number_57:   ; ===>>> 4105 gosub 2000
    lea      rax, line_number_30
    call     label_gosub
  line_number_58:   ; ===>>> 4107 if 0 = wi% then goto 4140
    test     r15d, r15d
    je       line_number_62
  line_number_59:   ; ===>>> 4110 if wi% = 1 then re% = 6: goto 4280
    mov      eax, 6
    cmp      r15d, 1
    cmove    r12d, eax
    je       line_number_73
  line_number_60:   ; ===>>> 4115 re% = 4
    mov      r12d, 4
  line_number_61:   ; ===>>> 4116 goto 4280
    jmp      line_number_73
  line_number_62:   ; ===>>> 4140 if st% = 8 then re% = 5: goto 4280
    mov      eax, 5
    cmp      r14d, 8
    cmove    r12d, eax
    je       line_number_73
  line_number_63:   ; ===>>> 4150 if st% and 1 then v% = 2 else v% = 9
    mov      r13d, 9
    mov      eax, 2
    test     r14d, 1
    cmovnz   r13d, eax
  line_number_64:   ; ===>>> 4160 p% = 0
    mov      r9d, 0
  line_number_65:   ; ===>>> 4180 if 0 <> b%(p%) then goto 4500
    mov      ebx, r9d
    shl      rbx, 2
    lea      rcx, var_b
    mov      eax, DWORD PTR [rbx + rcx]
    test     eax, eax
    jnz      line_number_89
  line_number_66:   ; ===>>> 4200 if st% and 1 then b%(p%) = 1 else b%(p%) = 2
    mov      ecx, 2
    mov      r8d, 1
    test     r14d, 1
    cmovnz   ecx, r8d
    lea      rax, var_b
    mov      ebx, r9d
    shl      ebx, 2
    mov      DWORD PTR [ rbx + rax ], ecx
  line_number_67:   ; ===>>> 4210 sp%(st%) = p%
    mov      eax, r14d
    shl      rax, 2
    lea      rbx, [var_sp]
    mov      DWORD PTR [rbx + rax], r9d
  line_number_68:   ; ===>>> 4230 sv%(st%) = v%
    mov      eax, r14d
    shl      rax, 2
    lea      rbx, [var_sv]
    mov      DWORD PTR [rbx + rax], r13d
  line_number_69:   ; ===>>> 4245 sa%(st%) = al%
    mov      eax, r14d
    shl      rax, 2
    lea      rbx, [var_sa]
    mov      DWORD PTR [rbx + rax], r11d
  line_number_70:   ; ===>>> 4246 sb%(st%) = be%
    mov      eax, r14d
    shl      rax, 2
    lea      rbx, [var_sb]
    mov      DWORD PTR [rbx + rax], r10d
  line_number_71:   ; ===>>> 4260 st% = st% + 1
    inc      r14d
  line_number_72:   ; ===>>> 4270 goto 4100
    jmp      line_number_55
  line_number_73:   ; ===>>> 4280 st% = st% - 1
    dec      r14d
  line_number_74:   ; ===>>> 4290 p% = sp%(st%)
    mov      eax, r14d
    shl      rax, 2
    lea      rbx, [var_sp]
    mov      r9d, [ rax + rbx ]
  line_number_75:   ; ===>>> 4310 v% = sv%(st%)
    mov      eax, r14d
    shl      rax, 2
    lea      rbx, [var_sv]
    mov      r13d, [ rax + rbx ]
  line_number_76:   ; ===>>> 4325 al% = sa%(st%)
    mov      eax, r14d
    shl      rax, 2
    lea      rbx, [var_sa]
    mov      r11d, [ rax + rbx ]
  line_number_77:   ; ===>>> 4326 be% = sb%(st%)
    mov      eax, r14d
    shl      rax, 2
    lea      rbx, [var_sb]
    mov      r10d, [ rax + rbx ]
  line_number_78:   ; ===>>> 4328 b%(p%) = 0
    mov      eax, r9d
    shl      rax, 2
    lea      rbx, [var_b]
    mov      DWORD PTR [rbx + rax], 0
  line_number_79:   ; ===>>> 4330 if st% and 1 then goto 4340
    test     r14d, 1
    jnz      line_number_85
  line_number_80:   ; ===>>> 4331 if re% = 4 then goto 4530
    cmp      r12d, 4
    je       line_number_92
  line_number_81:   ; ===>>> 4332 if re% < v% then v% = re%
    cmp      r12d, r13d
    cmovl    r13d, r12d
  line_number_82:   ; ===>>> 4334 if v% < be% then be% = v%
    cmp      r13d, r10d
    cmovl    r10d, r13d
  line_number_83:   ; ===>>> 4336 if be% <= al% then goto 4520
    cmp      r10d, r11d
    jle      line_number_91
  line_number_84:   ; ===>>> 4338 goto 4500
    jmp      line_number_89
  line_number_85:   ; ===>>> 4340 if re% = 6 then goto 4530
    cmp      r12d, 6
    je       line_number_92
  line_number_86:   ; ===>>> 4341 if re% > v% then v% = re%
    cmp      r12d, r13d
    cmovg    r13d, r12d
  line_number_87:   ; ===>>> 4342 if v% > al% then al% = v%
    cmp      r13d, r11d
    cmovg    r11d, r13d
  line_number_88:   ; ===>>> 4344 if al% >= be% then goto 4520
    cmp      r11d, r10d
    jge      line_number_91
  line_number_89:   ; ===>>> 4500 p% = p% + 1
    inc      r9d
  line_number_90:   ; ===>>> 4505 if p% < 9 then goto 4180
    cmp      r9d, 9
    jl       line_number_65
  line_number_91:   ; ===>>> 4520 re% = v%
    mov      r12d, r13d
  line_number_92:   ; ===>>> 4530 if st% = 0 then return
    test     r14d, r14d
    je       label_gosub_return
  line_number_93:   ; ===>>> 4540 goto 4280
    jmp      line_number_73
  line_number_94:   ; ===>>> 2000000000 end
    jmp      end_execution
  label_gosub_return:
    pop      rax
    ret
  label_gosub:
    push     rax
    jmp      rax
  error_exit:
    lea      rcx, [errorString]
    call     call_printf
    jmp      leave_execution
  end_execution:
    lea      rcx, [stopString]
    call     call_printf
  leave_execution:
    xor      rcx, rcx
    call     call_exit
    ret    ; should never get here
main ENDP
align 16
printElap PROC
    push     r8
    push     r9
    push     r10
    push     r11
    push     rbp
    mov      rbp, rsp
    sub      rsp, 32
    lea      rcx, [currentTicks]
    call     call_QueryPerformanceCounter
    mov      rax, [currentTicks]
    sub      rax, [startTicks]
    mov      rcx, [perfFrequency]
    xor      rdx, rdx
    mov      rbx, 1000000
    mul      rbx
    div      rcx
    lea      rcx, [elapString]
    mov      rdx, rax
    call     printf
    leave
    pop      r11
    pop      r10
    pop      r9
    pop      r8
    ret
printElap ENDP
align 16
printTime PROC
    push     r8
    push     r9
    push     r10
    push     r11
    push     rbp
    mov      rbp, rsp
    sub      rsp, 64
    lea      rcx, [currentTime]
    call     GetLocalTime
    lea      rax, [currentTime]
    lea      rcx, [timeString]
    movzx    rdx, WORD PTR [currentTime + 8]
    movzx    r8, WORD PTR [currentTime + 10]
    movzx    r9, WORD PTR [currentTime + 12]
    movzx    r10, WORD PTR [currentTime + 14]
    mov      QWORD PTR [rsp + 32], r10
    call     printf
    leave
    pop      r11
    pop      r10
    pop      r9
    pop      r8
    ret
printTime ENDP
align 16
call_printf PROC
    push     r8
    push     r9
    push     r10
    push     r11
    push     rbp
    mov      rbp, rsp
    sub      rsp, 32
    call     printf
    leave
    pop      r11
    pop      r10
    pop      r9
    pop      r8
    ret
call_printf ENDP
align 16
call_exit PROC
    push     rbp
    mov      rbp, rsp
    sub      rsp, 32
    call     exit
    leave   ; should never get here
    ret
call_exit ENDP
align 16
call_QueryPerformanceCounter PROC
    push     r8
    push     r9
    push     r10
    push     r11
    push     rbp
    mov      rbp, rsp
    sub      rsp, 32
    call     QueryPerformanceCounter
    leave
    pop      r11
    pop      r10
    pop      r9
    pop      r8
    ret
call_QueryPerformanceCounter ENDP
code_segment ENDS
END
