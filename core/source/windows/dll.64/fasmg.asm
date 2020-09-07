; how to increase low memory? (note: no relocation)

format PE64 DLL GUI 5.0 at 0xFFF0_0000
entry DllEntryPoint

include 'win64wxp.inc'

; this is needed to translate the assembler's variable usage to EBP frame

calminstruction mov? dest*,src*
	local	tmp
	match	tmp], src
	jyes	regular
	check	src relativeto ebp & src - ebp
	jno	regular
	arrange tmp, =lea dest,[src]
	assemble tmp
	exit
    regular:
	arrange tmp, =mov dest,src
	assemble tmp
end calminstruction

include '../../version.inc'

struct MEMORY_REGION
	address dq ?
	size dq ?
ends

section '.rdata' data readable

data import

	library kernel32,'KERNEL32.DLL'

	import kernel32,\
	       __imp_CloseHandle,'CloseHandle',\
	       __imp_CreateFile,'CreateFileA',\
	       __imp_ExitProcess,'ExitProcess',\
	       __imp_GetCommandLine,'GetCommandLineA',\
	       __imp_GetEnvironmentVariable,'GetEnvironmentVariableA',\
	       __imp_GetStdHandle,'GetStdHandle',\
	       __imp_GetSystemTimeAsFileTime,'GetSystemTimeAsFileTime',\
	       __imp_GetTickCount,'GetTickCount',\
	       __imp_HeapAlloc,'HeapAlloc',\
	       __imp_HeapCreate,'HeapCreate',\
	       __imp_HeapDestroy,'HeapDestroy',\
	       __imp_HeapFree,'HeapFree',\
	       __imp_HeapReAlloc,'HeapReAlloc',\
	       __imp_HeapSize,'HeapSize',\
	       __imp_VirtualAlloc,'VirtualAlloc',\
	       __imp_VirtualFree,'VirtualFree',\
	       __imp_ReadFile,'ReadFile',\
	       __imp_SetFilePointerEx,'SetFilePointerEx',\
	       __imp_WriteFile,'WriteFile',\
	       __imp_GetLastError,'GetLastError'

end data

align 4

data export

	include 'ToDLL_Name.inc'
	export __DLL_NAME__,\
	       fasmg_GetVersion,'fasmg_GetVersion',\
	       fasmg_Assemble,'fasmg_Assemble'

end data

  include '../../tables.inc'
  include '../../messages.inc'

  version_string db VERSION,0



section '.text' code executable
	use32
  include '../../assembler.inc'
  include '../../symbols.inc'
  include '../../expressions.inc'
  include '../../conditions.inc'
  include '../../floats.inc'
  include '../../directives.inc'
  include '../../calm.inc'
  include '../../errors.inc'
  include '../../map.inc'
  include '../../reader.inc'
  include '../../output.inc'
  include '../../console.inc'

; far interface to above 32-bit assembler code (no change needed)
assembly_init_th32:
	call assembly_init
	retf

assembly_pass_th32:
	call assembly_pass
	retf

assembly_shutdown_th32:
	call assembly_shutdown
	retf

get_output_length_th32:
	call get_output_length
	retf

read_from_output_th32:
	call read_from_output
	retf

write_output_file_th32:
	call write_output_file
	retf

show_display_data_th32:
	call show_display_data
	retf

show_errors_th32:
	call show_errors
	retf
	use64

	align 16
assembly_init_32	dp (23h shl 32) or assembly_init_th32
assembly_pass_32	dp (23h shl 32) or assembly_pass_th32
assembly_shutdown_32	dp (23h shl 32) or assembly_shutdown_th32
get_output_length_32	dp (23h shl 32) or get_output_length_th32
read_from_output_32	dp (23h shl 32) or read_from_output_th32
write_output_file_32	dp (23h shl 32) or write_output_file_th32
show_display_data_32	dp (23h shl 32) or show_display_data_th32
show_errors_32		dp (23h shl 32) or show_errors_th32


  DllEntryPoint:
	push 1
	pop rax
	retn

  fasmg_GetVersion:
	lea rax,[version_string]
	retn

  fasmg_Assemble:

	virtual at ebp - LOCAL_VARIABLES_SIZE

		LocalVariables:

			include '../../variables.inc'

			maximum_number_of_passes dd ?

			rb  (LocalVariables - $) and 111b

			pProxy		dq ?
			timestamp	dq ?
			filetime	FILETIME
			memory		dq ?
			systmp		dq ?

			rb  (LocalVariables - $) and 1111b

		LOCAL_VARIABLES_SIZE = $ - LocalVariables
		assert $ - ebp = 0

		previous_frame	dq ? ; rbp
		stored_rbx	dq ?
		stored_rdi	dq ?
		stored_rsi	dq ?
		stored_r15	dq ?
		return_address	dq ?

		FunctionParameters:

			source_string	dq ?
			source_path	dq ?
			output_region	dq ?
			output_path	dq ?
			stdout		dq ?
			stderr		dq ?

		FUNCTION_PARAMETERS_SIZE = $ - FunctionParameters
	end virtual
	L64? equ +rbp-ebp+

	push	r15 rsi rdi rbx
	enter	LOCAL_VARIABLES_SIZE,0
	mov	[L64 source_string],rcx
	mov	[L64 source_path],rdx
	mov	[L64 output_region],r8
	mov	[L64 output_path],r9

	call	system_init

	mov	[L64 maximum_number_of_passes],100
	mov	[L64 maximum_number_of_errors],1000
	mov	[L64 maximum_depth_of_stack],10000

	xor	al,al
	call	assembly_init_32
	jmp	assemble

  assemble_pass_limit:
	mov	eax,[L64 current_pass]
	cmp	eax,[L64 maximum_number_of_passes]
	jnc	assembly_non_halting
  assemble:
	mov	rsi,[L64 source_string]
	mov	rdx,[L64 source_path]
	call	[assembly_pass_32]
	jnc	assemble_pass_limit
  assembly_done:
	call	[show_display_data_32]

	cmp	[L64 first_error],0
	jnz	assembly_failed

	mov	rsi,[L64 output_region]
	test	rsi,rsi
	jz	output_copied
	call	[get_output_length_32]
	test	edx,edx
	jnz	out_of_memory
	mov	[L64 value_length],eax
	mov	rdx,[rsi+MEMORY_REGION.size]

	mov	[rsi+MEMORY_REGION.size],rax
	mov	rax,[rsi+MEMORY_REGION.address]
	test	rax,rax
	jz	validate_output_region

	cmp	[rsi+MEMORY_REGION.size],rdx
	jnc	copy_output

	sub	rsp,32

   validate_output_region:
	mov	rcx,[rsi+MEMORY_REGION.address]
	mov	rdx,[rsi+MEMORY_REGION.size]
	mov	r8d,MEM_COMMIT
	mov	r9d,PAGE_READWRITE
	call	VirtualAlloc
	test	rax,rax
	jnz	copy_output
	xchg	rax,[rsi+MEMORY_REGION.address]
	test	rax,rax
	jz	out_of_memory
	xchg	rcx,rax
	xor	edx,edx
	mov	r8d,MEM_RELEASE
	call	VirtualFree
	jmp	validate_output_region

   copy_output:
	mov	[rsi+MEMORY_REGION.address],rax
	xchg	rdi,rax
	and	[L64 file_offset],0
	call	[read_from_output_32]
   output_copied:

	mov	rbx,[L64 source_path]
	mov	rdi,[L64 output_path]
	mov	rax,rbx
	or	rax,rdi
	jz	output_written
	call	[write_output_file_32]
	jc	write_failed
   output_written:

	call	[assembly_shutdown_32]
	call	system_shutdown
	xor	eax,eax
   assembly_return:
	leave
	pop	rbx rdi rsi r15
	retn


  assembly_failed:
	mov	ecx,[L64 first_error]
	push	0
      count_errors:
	mov	ecx,[rcx+Error.next]
	inc	dword[rsp]
	jrcxz	counted_errors
	jmp	count_errors
      counted_errors:
	call	[show_errors_32]
  error_return:
	call	[assembly_shutdown_32]
	call	system_shutdown
	pop	rax
	jmp	assembly_return

  write_failed:
	push	-3
	jmp	error_return

  assembly_non_halting:
	call	[show_display_data_32]
	push	-2
	jmp	error_return

  out_of_memory:
	push	-1
	jmp	error_return


  include 'system.inc'
