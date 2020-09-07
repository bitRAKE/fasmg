
LINE_FEED equ 13,10

system_init:
	enter	8*4,0
	and	spl,-16
	xor	r8,r8
	mov	edx,20000h
	xor	ecx,ecx
	call	HeapCreate
	mov	[memory],rax
	test	rax,rax
	jz	out_of_memory
	lea	rcx,[L64 filetime]
	call	GetSystemTimeAsFileTime
	mov	rax,-116444736000000000
	add	rax,qword[L64 filetime]
	xor	edx,edx
	mov	ecx,10000000
	div	rcx
	mov	[L64 timestamp],rax

	include "hProxy.inc"
	pProxy hProxy 127 ; bit of a hack, but it works

	leave
	retn

system_shutdown:
	mov	rcx,[L64 memory]
	jrcxz	memory_released
	enter	8*4,0
	and	spl,-16
	call	HeapDestroy
	leave
    memory_released:
	retn


; The system functions below are called from within the assembler. This poses
; a number of problems if no changes are made to the core of the assembler:
;	1) need transition to 64-bit code segment to use 64-bit Windows API
;	2) need to use proxies for 64-bit file handles
;	3) stack alignment is unknown - could be +0,+4,+8,+12 MOD 16 (!)
;	4) erroring out safely to top level requires unwinding the frame

	use32
malloc_growable:
malloc_fixed:
malloc:
	call	33h:?.64
	retn
	use64
?.64:
; in: ecx = requested size
; out: eax - allocated block, ecx = allocated size, on error jumps to out_of_memory (does not return)
; preserves: ebx, esi, edi
; note:
;  use of malloc_fixed hints that block will be kept as is until the end of assembly
;  use of malloc_growable hints that block is likely to be resized
	enter	8*5,0
	and	spl,-16
	mov	r8,rcx
	xor	edx,edx
	mov	rcx,[L64 memory]
	call	HeapAlloc
	test	rax,rax
	jz	out_of_memory
    memory_allocated:
	mov	[rsp+8*4],rax
	xchg	r8,rax
	xor	edx,edx
	mov	rcx,[L64 memory]
	call	HeapSize
	lea	rcx,[rax+1]
	mov	rax,[rsp+8*4]
	add	[rsp+8*4],rcx ; end of address range + 1
	test	rcx,rcx
	jz	out_of_memory
	cmp	dword[rsp+8*4+4],0
	jnz	out_of_memory ; don't exceed 32-bit addressible
	leave
	retf


	use32
realloc:
	call	33h:?.64
	retn
	use64
?.64:
; in: eax - memory block, ecx = requested size
; out: eax - resized block, ecx = allocated size, on error jumps to out_of_memory (does not return)
; preserves: ebx, esi, edi
	mov	r9,rcx
	xchg	r8,rax
	xor	edx,edx
	mov	rcx,[L64 memory]
	enter	8*5,0
	and	spl,-16
	call	HeapReAlloc
	test	rax,rax
	jnz	memory_allocated
	jmp	out_of_memory


	use32
mfree:
	call 33h:?.64
	retn
	use64
?.64:
; in: eax - memory block
; out: cf set on error
; preserves: ebx, esi, edi
; note: eax may have value 0 or -1, it should be treated as invalid input then
	xchg	ecx,eax
	jrcxz	interface_error
	inc	ecx
	jrcxz	interface_error

	lea	r8,[rcx-1]
	xor	edx,edx
	mov	rcx,[L64 memory]
	enter	8*4,0
	and	spl,-16
	call	HeapFree ; BOOL
	leave
	xchg	ecx,eax
	jrcxz	interface_error
	clc
	retf
    interface_error:
	stc
	retf


	use32
open:
	call 33h:?.64
	retn
	use64
?.64:
; in: edx - path to file
; out: ebx = file handle, cf set on error
; preserves: esi, edi
	enter	8*7,0
	and	spl,-16
	mov	rcx,rdx ; TODO: convert UTF8 -> WideChar
	mov	edx,GENERIC_READ
	mov	dword[rsp+8*4],OPEN_EXISTING
	mov	r8d,FILE_SHARE_READ

    create_open:
	and	qword[rsp+8*6],0
	and	qword[rsp+8*5],0
	xor	r9,r9
	call	CreateFile
	leave
	cmp	rax,-1
	jz	interface_error
	Proxy_From_hFile rbx,rax ; EBX becomes handle proxy
	clc
	retf


	use32
create:
	call 33h:?.64
	retn
	use64
?.64:
; in: edx - path to file
; out: ebx = file handle, cf set on error
; preserves: esi, edi
	enter	8*7,0
	and	spl,-16
	mov	rcx,rdx ; TODO: convert UTF8 -> WideChar
	mov	edx,GENERIC_WRITE
	mov	dword[rsp+8*4],CREATE_ALWAYS
	xor	r8,r8
	jmp	create_open


	use32
write:	lea eax,[WriteFile] ; #32#
	call 33h:Read_Write_File
	retn

read:	lea eax,[ReadFile] ; #32#
	call 33h:Read_Write_File
	retn
	use64
Read_Write_File:
; in: ebx = file handle, edx - buffer, ecx = number of bytes
; out: cf set on error
; preserves: ebx, esi, edi
	lea	r9,[L64 systmp]
	enter	8*6,0
	and	spl,-16
	mov	[rsp+8*5],ecx
	and	qword[rsp+8*4],0
	mov	r8d,ecx
	hFile_From_Proxy rcx,rbx
	call	rax
	mov	ecx,[rsp+8*5]
	leave
	test	eax,eax ; BOOL
	jz	interface_error
	cmp	ecx,dword [L64 systmp]
	jnz	interface_error
	clc
	retf


	use32
close:
	call 33h:?.64
	retn
	use64
?.64:
; in: ebx = file handle
; preserves: ebx, esi, edi
	hFile_From_Proxy rcx,rbx,1 ; <- clear proxy flaged
	enter	8*4,0
	and	spl,-16
	call	CloseHandle
	leave
	retf


	use32
lseek:
	call 33h:?.64
	retn
	use64
?.64:
; in: ebx = file handle, cl = method, edx:eax = offset
; out: edx:eax = new offset from the beginning of file, cf set on error
; preserves: ebx, esi, edi
	lea	r8,[L64 systmp]
	movzx	r9,cl
	shl	rdx,32
	or	rdx,rax
	hFile_From_Proxy rcx,rbx
	enter	8*4,0
	and	spl,-16
	call	SetFilePointerEx
	leave
	test	eax,eax ; BOOL
	jz	interface_error
	mov	eax,dword [L64 systmp]
	mov	edx,dword [L64 systmp+4]
	clc
	retf


	use32
get_timestamp:
; out: edx:eax = timestamp
; preserves: ebx, ecx, esi, edi
	mov	eax,dword [timestamp]
	mov	edx,dword [timestamp+4]
	retn
	use64


	use32
display_string:
	call 33h:?.64
	retn
	use64
?.64:
; in:
;  esi - string
;  ecx = string length, zero for ASCIIZ string
; preserves: ebx, esi
	cmp	[L64 stdout],0
	jz	hidden_display
	test	ecx,ecx
	jnz	write_string_to_stdout
	xor	al,al
	mov	edi,esi
	or	ecx,-1
	repnz	scasb
	neg	ecx
	sub	ecx,2
   write_string_to_stdout:
	enter	8*6,0
	and	spl,-16
	mov	[rsp+8*5],ecx
	mov	edi,esi
	jmp	write_string_portion
   write_portion_to_stdout:
	mov	rdx,rdi
	add	rdi,rax
	xchg	r8,rax
	and	qword[rsp+8*4],0
	lea	r9,[L64 systmp]
	mov	rcx,[L64 stdout]
	call	WriteFile
   write_string_portion:
	mov	eax,51200
	sub	[rsp+8*5],eax
	jge	write_portion_to_stdout
	add	eax,[rsp+8*5]
	jg	write_portion_to_stdout
	leave
   hidden_display:
	retf


	use32
display_error_string:
	call 33h:?.64
	retn
	use64
?.64:
; in:
;  esi - string
;  ecx = string length, zero for ASCIIZ string
; preserves: ebx, esi
	cmp	[L64 stderr],0
	jz	hidden_error
	test	ecx,ecx
	jnz	write_string_to_stderr
	xor	al,al
	mov	edi,esi
	or	ecx,-1
	repne	scasb
	neg	ecx
	sub	ecx,2
   write_string_to_stderr:
	enter	8*5,0
	and	spl,-16
	and	qword[rsp+8*4],0
	lea	r9,[L64 systmp]
	mov	r8,rcx
	mov	rdx,rsi
	mov	rcx,[L64 stderr]
	call	WriteFile
	leave
   hidden_error:
	retf


	use32
get_environment_variable:
	call 33h:?.64
	retn
	use64
?.64:
; in:
;  esi - name
;  edi - buffer for value
;  ecx = size of buffer
; out:
;  eax = length of value
; preserves: ebx, esi, edi
	push	rcx
	mov	r8,rcx
	mov	rdx,rdi
	mov	rcx,rsi
	enter	32,0
	and	spl,-16
	call	GetEnvironmentVariable
	leave
	pop	rcx
	cmp	eax,ecx
	jae	environment_variable_ready
	mov	byte [rdi+rax],0
   environment_variable_ready:
	inc	eax
	retf


CloseHandle:
	jmp	[__imp_CloseHandle]
CreateFile:
	jmp	[__imp_CreateFile]
ExitProcess:
	jmp	[__imp_ExitProcess]
GetCommandLine:
	jmp	[__imp_GetCommandLine]
GetEnvironmentVariable:
	jmp	[__imp_GetEnvironmentVariable]
GetStdHandle:
	jmp	[__imp_GetStdHandle]
GetSystemTimeAsFileTime:
	jmp	[__imp_GetSystemTimeAsFileTime]
GetTickCount:
	jmp	[__imp_GetTickCount]
HeapAlloc:
	jmp	[__imp_HeapAlloc]
HeapCreate:
	jmp	[__imp_HeapCreate]
HeapDestroy:
	jmp	[__imp_HeapDestroy]
HeapFree:
	jmp	[__imp_HeapFree]
HeapReAlloc:
	jmp	[__imp_HeapReAlloc]
HeapSize:
	jmp	[__imp_HeapSize]
VirtualAlloc:
	jmp	[__imp_VirtualAlloc]
VirtualFree:
	jmp	[__imp_VirtualFree]
ReadFile:
	jmp	[__imp_ReadFile]
SetFilePointerEx:
	jmp	[__imp_SetFilePointerEx]
WriteFile:
	jmp	[__imp_WriteFile]
GetLastError:
	jmp	[__imp_GetLastError]
