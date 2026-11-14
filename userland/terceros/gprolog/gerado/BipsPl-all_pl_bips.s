# BipsPl/all_pl_bips.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/all_pl_bips.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F616C6C5F706C5F62697073__a0,@function
.globl X1_247573655F616C6C5F706C5F62697073__a0

X1_247573655F616C6C5F706C5F62697073__a0:
	movl	$0, %eax
	call	Pl_Allocate
	movl	$.Lcont1, pl_reg_bank+1040
	jmp	X1_247573655F636F6E74726F6C__a0
.Lcont1:
	movl	$.Lcont2, pl_reg_bank+1040
	jmp	X1_247573655F63616C6C__a0
.Lcont2:
	movl	$.Lcont3, pl_reg_bank+1040
	jmp	X1_247573655F63616C6C5F61726773__a0
.Lcont3:
	movl	$.Lcont4, pl_reg_bank+1040
	jmp	X1_247573655F6361746368__a0
.Lcont4:
	movl	$.Lcont5, pl_reg_bank+1040
	jmp	X1_247573655F7468726F77__a0
.Lcont5:
	movl	$.Lcont6, pl_reg_bank+1040
	jmp	X1_247573655F756E696679__a0
.Lcont6:
	movl	$.Lcont7, pl_reg_bank+1040
	jmp	X1_247573655F61726974685F696E6C__a0
.Lcont7:
	movl	$.Lcont8, pl_reg_bank+1040
	jmp	X1_247573655F617373657274__a0
.Lcont8:
	movl	$.Lcont9, pl_reg_bank+1040
	jmp	X1_247573655F616C6C5F736F6C7574__a0
.Lcont9:
	movl	$.Lcont10, pl_reg_bank+1040
	jmp	X1_247573655F736F7274__a0
.Lcont10:
	movl	$.Lcont11, pl_reg_bank+1040
	jmp	X1_247573655F6C697374__a0
.Lcont11:
	movl	$.Lcont12, pl_reg_bank+1040
	jmp	X1_247573655F73747265616D__a0
.Lcont12:
	movl	$.Lcont13, pl_reg_bank+1040
	jmp	X1_247573655F6C655F696E74657266__a0
.Lcont13:
	movl	$.Lcont14, pl_reg_bank+1040
	jmp	X1_247573655F66696C65__a0
.Lcont14:
	movl	$.Lcont15, pl_reg_bank+1040
	jmp	X1_247573655F636861725F696F__a0
.Lcont15:
	movl	$.Lcont16, pl_reg_bank+1040
	jmp	X1_247573655F72656164__a0
.Lcont16:
	movl	$.Lcont17, pl_reg_bank+1040
	jmp	X1_247573655F7772697465__a0
.Lcont17:
	movl	$.Lcont18, pl_reg_bank+1040
	jmp	X1_247573655F7072696E74__a0
.Lcont18:
	movl	$.Lcont19, pl_reg_bank+1040
	jmp	X1_247573655F636F6E73745F696F__a0
.Lcont19:
	movl	$.Lcont20, pl_reg_bank+1040
	jmp	X1_247573655F6F706572__a0
.Lcont20:
	movl	$.Lcont21, pl_reg_bank+1040
	jmp	X1_247573655F70726564__a0
.Lcont21:
	movl	$.Lcont22, pl_reg_bank+1040
	jmp	X1_247573655F61746F6D__a0
.Lcont22:
	movl	$.Lcont23, pl_reg_bank+1040
	jmp	X1_247573655F666C6167__a0
.Lcont23:
	movl	$.Lcont24, pl_reg_bank+1040
	jmp	X1_247573655F7465726D5F696E6C__a0
.Lcont24:
	movl	$.Lcont25, pl_reg_bank+1040
	jmp	X1_247573655F747970655F696E6C__a0
.Lcont25:
	movl	$.Lcont26, pl_reg_bank+1040
	jmp	X1_247573655F675F7661725F696E6C__a0
.Lcont26:
	movl	$.Lcont27, pl_reg_bank+1040
	jmp	X1_247573655F73746174__a0
.Lcont27:
	movl	$.Lcont28, pl_reg_bank+1040
	jmp	X1_247573655F6465633130696F__a0
.Lcont28:
	movl	$.Lcont29, pl_reg_bank+1040
	jmp	X1_247573655F666F726D6174__a0
.Lcont29:
	movl	$.Lcont30, pl_reg_bank+1040
	jmp	X1_247573655F6F735F696E74657266__a0
.Lcont30:
	movl	$.Lcont31, pl_reg_bank+1040
	jmp	X1_247573655F657870616E64__a0
.Lcont31:
	movl	$.Lcont32, pl_reg_bank+1040
	jmp	X1_247573655F707265747479__a0
.Lcont32:
	movl	$.Lcont33, pl_reg_bank+1040
	jmp	X1_247573655F72616E646F6D__a0
.Lcont33:
	movl	$.Lcont34, pl_reg_bank+1040
	jmp	X1_247573655F736F636B657473__a0
.Lcont34:
	call	Pl_Deallocate
	jmp	X1_247573655F7372635F726472__a0
	.p2align	4,,15
	.type	Object_Initializer,@function

Object_Initializer:
	pushl	%esi
	subl	$132, %esp
	movl	$Prolog_Object_Initializer+0, 0(%esp)
	movl	$System_Directives+0, 4(%esp)
	movl	$User_Directives+0, 8(%esp)
	call	Pl_New_Object
	addl	$132, %esp
	popl	%esi
	ret	
	.p2align	4,,15
	.type	Prolog_Object_Initializer,@function

Prolog_Object_Initializer:
	pushl	%esi
	subl	$132, %esp
	movl	$.LC0, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$39, 0(%esp)
	movl	$129, 4(%esp)
	movl	$X1_247573655F616C6C5F706C5F62697073__a0+0, 8(%esp)
	call	Pl_Create_Pred
	addl	$132, %esp
	popl	%esi
	ret	
	.p2align	4,,15
	.type	System_Directives,@function

System_Directives:
	pushl	%esi
	subl	$132, %esp
	addl	$132, %esp
	popl	%esi
	ret	
	.p2align	4,,15
	.type	User_Directives,@function

User_Directives:
	pushl	%esi
	subl	$132, %esp
	addl	$132, %esp
	popl	%esi
	ret	
	.section	.ctors,"aw",@progbits
	.align	4
	.long	Object_Initializer
	.section	.rodata.str1.1,"aMS",@progbits,1
.LC0:
	.string	"$use_all_pl_bips"
.LC1:
	.string	"/home/diaz/GP/src/BipsPl/all_pl_bips.pl"
.LC2:
	.string	"user"
	.data	
	.align	4
	.local	at
	.comm	at,12,4
	.section	.note.GNU-stack,"",@progbits
