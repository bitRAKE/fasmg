
; TODO: use memory above 32-bit range

struc hProxy entries,granularity=128,reg=r11
	if entries relativeto 0
		mov ecx,8*(entries+1)
	else
		lea ecx,[8*(entries+1)]
	end if
	push 33h
	call malloc.64
	shr ecx,3
	mov dword[rax],ecx
	mov [.],rax

macro hProxy__put_handle dest=rcx,source=rax
	mov reg,[.]
	xor r32.dest,r32.dest
@@:	add r32.dest,1
	cmp qword[reg+dest*8],0 ; linear scan
	jz @F
	cmp r32.dest,[reg]
	jc @B
	push source
	lea ecx,[8*(dest+granularity)]
	mov rax,reg
	push 33h
	call realloc.64
	mov [.],rax
	xchg reg,rax
	pop source
	mov dest,[reg]
	add dword[reg],granularity
@@:	mov [reg+dest*8],source
end macro ; psuedo-handle is the index (dest)

macro hProxy__get_handle dest=rax,source=rbx, andclear=0
	mov reg,[.]
	mov dest,[reg+source*8]
	if andclear
		push 0
		pop [reg+source*8]
	end if
end macro

macro hProxy__remove_handle source=rax
	mov reg,[.]
	push 0
	pop [reg+source*8]
end macro

restruc hProxy ; just one
end struc
