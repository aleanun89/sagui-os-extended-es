# BipsPl/flag.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog 1.5.0)
# a partir de fonte/BipsPl/flag.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING).
	.text	

fail:
	movl	pl_reg_bank+1028, %eax
	jmp	*-4(%eax)
	.p2align	4,,15
	.type	X1_247573655F666C6167__a0,@function
.globl X1_247573655F666C6167__a0

X1_247573655F666C6167__a0:
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_set_prolog_flag__a2,@function
.globl X0_set_prolog_flag__a2

X0_set_prolog_flag__a2:
	movl	at+12, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Set_Prolog_Flag_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_current_prolog_flag__a2,@function
.globl X0_current_prolog_flag__a2

X0_current_prolog_flag__a2:
	movl	at+16, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Current_Prolog_Flag_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_2463757272656E745F70726F6C6F675F666C61675F616C74__a0,@function
.globl X1_2463757272656E745F70726F6C6F675F666C61675F616C74__a0

X1_2463757272656E745F70726F6C6F675F666C61675F616C74__a0:
	call	Pl_Current_Prolog_Flag_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247379735F7661725F7772697465__a2,@function
.globl X1_247379735F7661725F7772697465__a2

X1_247379735F7661725F7772697465__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Sys_Var_Write_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247379735F7661725F72656164__a2,@function
.globl X1_247379735F7661725F72656164__a2

X1_247379735F7661725F72656164__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Sys_Var_Read_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247379735F7661725F696E63__a1,@function
.globl X1_247379735F7661725F696E63__a1

X1_247379735F7661725F696E63__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Sys_Var_Inc_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247379735F7661725F646563__a1,@function
.globl X1_247379735F7661725F646563__a1

X1_247379735F7661725F646563__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Sys_Var_Dec_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247379735F7661725F7365745F626974__a2,@function
.globl X1_247379735F7661725F7365745F626974__a2

X1_247379735F7661725F7365745F626974__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Sys_Var_Set_Bit_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247379735F7661725F72657365745F626974__a2,@function
.globl X1_247379735F7661725F72657365745F626974__a2

X1_247379735F7661725F72657365745F626974__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Sys_Var_Reset_Bit_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247379735F7661725F6765745F626974__a3,@function
.globl X1_247379735F7661725F6765745F626974__a3

X1_247379735F7661725F6765745F626974__a3:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	movl	pl_reg_bank+8, %eax
	movl	%eax, 8(%esp)
	call	Pl_Sys_Var_Get_Bit_3
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247379735F7661725F707574__a2,@function
.globl X1_247379735F7661725F707574__a2

X1_247379735F7661725F707574__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Sys_Var_Put_2
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247379735F7661725F676574__a2,@function
.globl X1_247379735F7661725F676574__a2

X1_247379735F7661725F676574__a2:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Sys_Var_Get_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_246765745F63757272656E745F42__a1,@function
.globl X1_246765745F63757272656E745F42__a1

X1_246765745F63757272656E745F42__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Get_Current_B_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_247365745F63757272656E745F42__a1,@function
.globl X1_247365745F63757272656E745F42__a1

X1_247365745F63757272656E745F42__a1:
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Set_Current_B_1
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_write_pl_state_file__a1,@function
.globl X0_write_pl_state_file__a1

X0_write_pl_state_file__a1:
	movl	at+68, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Write_Pl_State_File
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_read_pl_state_file__a1,@function
.globl X0_read_pl_state_file__a1

X0_read_pl_state_file__a1:
	movl	at+72, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Read_Pl_State_File
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_argument_counter__a1,@function
.globl X0_argument_counter__a1

X0_argument_counter__a1:
	movl	at+76, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Argument_Counter_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_argument_value__a2,@function
.globl X0_argument_value__a2

X0_argument_value__a2:
	movl	at+80, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Argument_Value_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_argument_list__a1,@function
.globl X0_argument_list__a1

X0_argument_list__a1:
	movl	at+84, %eax
	movl	%eax, 0(%esp)
	movl	$1, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	call	Pl_Argument_List_1
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X0_environ__a2,@function
.globl X0_environ__a2

X0_environ__a2:
	movl	at+88, %eax
	movl	%eax, 0(%esp)
	movl	$2, 4(%esp)
	call	Pl_Set_Bip_Name_Untagged_2
	movl	pl_reg_bank+0, %eax
	movl	%eax, 0(%esp)
	movl	pl_reg_bank+4, %eax
	movl	%eax, 4(%esp)
	call	Pl_Environ_2
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
	.p2align	4,,15
	.type	X1_24656E7669726F6E5F616C74__a0,@function
.globl X1_24656E7669726F6E5F616C74__a0

X1_24656E7669726F6E5F616C74__a0:
	call	Pl_Environ_Alt_0
	testl	%eax, %eax
	je	fail
	jmp	*pl_reg_bank+1040
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
	movl	%eax, at+20
	movl	$.LC1, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+92
	movl	$.LC2, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+60
	movl	$.LC3, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+64
	movl	$.LC4, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+36
	movl	$.LC5, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+56
	movl	$.LC6, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+48
	movl	$.LC7, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+32
	movl	$.LC8, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+52
	movl	$.LC9, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+28
	movl	$.LC10, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+44
	movl	$.LC11, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+40
	movl	$.LC12, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+24
	movl	$.LC13, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+4
	movl	$.LC14, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+0
	movl	$.LC15, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+76
	movl	$.LC16, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+84
	movl	$.LC17, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+80
	movl	$.LC18, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+16
	movl	$.LC19, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+88
	movl	$.LC20, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+72
	movl	$.LC21, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+12
	movl	$.LC22, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+8
	movl	$.LC23, 0(%esp)
	call	Pl_Create_Atom
	movl	%eax, at+68
	movl	at+4, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$41, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247573655F666C6167__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+12, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$44, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_set_prolog_flag__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+16, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$51, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_current_prolog_flag__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+20, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$58, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_2463757272656E745F70726F6C6F675F666C61675F616C74__a0+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+24, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$64, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247379735F7661725F7772697465__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+28, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$67, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247379735F7661725F72656164__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+32, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$74, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247379735F7661725F696E63__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+36, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$77, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247379735F7661725F646563__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+40, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$83, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247379735F7661725F7365745F626974__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+44, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$86, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247379735F7661725F72657365745F626974__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+48, %eax
	movl	$3, %edx
	movl	at+0, %ecx
	movl	$89, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247379735F7661725F6765745F626974__a3+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+52, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$95, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247379735F7661725F707574__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+56, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$98, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247379735F7661725F676574__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+60, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$104, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_246765745F63757272656E745F42__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+64, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$107, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_247365745F63757272656E745F42__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+68, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$113, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_write_pl_state_file__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+72, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$120, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_read_pl_state_file__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+76, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$127, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_argument_counter__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+80, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$134, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_argument_value__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+84, %eax
	movl	$1, %edx
	movl	at+0, %ecx
	movl	$141, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_argument_list__a1+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+88, %eax
	movl	$2, %edx
	movl	at+0, %ecx
	movl	$146, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X0_environ__a2+0, 8(%esp)
	call	Pl_Create_Pred
	movl	at+92, %eax
	movl	$0, %edx
	movl	at+0, %ecx
	movl	$151, 0(%esp)
	movl	$137, 4(%esp)
	movl	$X1_24656E7669726F6E5F616C74__a0+0, 8(%esp)
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
	.string	"$current_prolog_flag_alt"
.LC1:
	.string	"$environ_alt"
.LC2:
	.string	"$get_current_B"
.LC3:
	.string	"$set_current_B"
.LC4:
	.string	"$sys_var_dec"
.LC5:
	.string	"$sys_var_get"
.LC6:
	.string	"$sys_var_get_bit"
.LC7:
	.string	"$sys_var_inc"
.LC8:
	.string	"$sys_var_put"
.LC9:
	.string	"$sys_var_read"
.LC10:
	.string	"$sys_var_reset_bit"
.LC11:
	.string	"$sys_var_set_bit"
.LC12:
	.string	"$sys_var_write"
.LC13:
	.string	"$use_flag"
.LC14:
	.string	"/home/diaz/GP/src/BipsPl/flag.pl"
.LC15:
	.string	"argument_counter"
.LC16:
	.string	"argument_list"
.LC17:
	.string	"argument_value"
.LC18:
	.string	"current_prolog_flag"
.LC19:
	.string	"environ"
.LC20:
	.string	"read_pl_state_file"
.LC21:
	.string	"set_prolog_flag"
.LC22:
	.string	"system"
.LC23:
	.string	"write_pl_state_file"
	.data	
	.align	4
	.local	at
	.comm	at,96,4
	.section	.note.GNU-stack,"",@progbits
