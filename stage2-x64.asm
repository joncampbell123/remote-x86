use64

PML2_ENTRIES	equ		16384
PML4_LOCATION	equ		0x10000							; where we keep the PML4 page
PML3_LOCATION	equ		PML4_LOCATION + 0x1000					; PML3 where we keep 4 entries for memory ranges 0-4GB
PML2_LOCATION	equ		PML3_LOCATION + 0x1000					; PML3 is one page long so we store the actual 2MB entries from here on out
											; PML3 is arranged so that this part is just one big long array with 1:1
											; correllation between memory and entry. 4 * 512 * 8 = 16384 bytes to cover
											; 16384/8 * 2MB = 2048 * 2MB = 4GB

; just enough to cover 4GB. We use 2MB large pages for extra laziness.
; PML4              PML3               PML2
;   [0] ---------->  [0] 1GB ---------> [0] 2MB -------------> actual page
;  N/A               [1] 2GB            [1] 2MB
;  N/A               [2] 3GB            [2] 2MB
;  N/A               [3] 4GB            [3] 2MB

CODE_SELECTOR	equ		0x08
DATA_SELECTOR	equ		0x10

SECTION		.text

; we are jumped to from the 8086 loader, so this part is real-mode code
global _jmp_x64
use16			; <- Woo! Nasm lets me use 16-bit code in an ELF64 object file!

_jmp_x64:	cli
		mov		ax,cs
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		mov		sp,0x7BF0

		mov		word [x_gdtr_32],0xFFFF	; limit
		mov		dword [x_gdtr_32+2],_gdt ; offset
		call		gen_gdt_32

		sgdt		[x_gdtr_old]
		lgdt		[x_gdtr_32]

		mov		eax,0x00000001
		mov		cr0,eax			; enable protected mode but NOT paging

		jmp		dword CODE_SELECTOR:.protmode32
use32
.protmode32:
		mov		ax,DATA_SELECTOR
		mov		ds,ax
		mov		es,ax
		mov		ss,ax
		mov		fs,ax
		mov		gs,ax

; update the GDT to enable long mode when we jump.
; in the mean time the cached segment registers we're using still reflect 32-bit code+data,
; just DON'T RELOAD THEM!
		mov		edi,_gdt
		mov		byte [edi + CODE_SELECTOR + 6],0xAF		; D=0 L=1 64-bit code
		mov		byte [edi + DATA_SELECTOR + 6],0xAF		; D=0 L=1 64-bit data

; switch on A20. anything that supports 64-bit is new enough to support the port 0x92
; trick (fast A20) and is too new to suffer the quirks mentioned on various sites about
; embedded video cards screwing up when we use it.
		in		al,0x92
		or		al,2		; enable A20
		and		al,0xFE		; DONT reset
		out		0x92,al

; build the damn page tables. 4 levels deep even!
; keep it simple by only mapping the first 4GB of memory.
; the host can upload code later that fills out the rest if it cares to
; peek above 4GB.
		cld
		mov		edi,PML4_LOCATION
		mov		ecx,511		; 512 * 64-bit entries = 512 * 8 = 4096
		; make ONE entry for the 0x0000`0000`0000 - 0x007F`FFFF`FFFF range
		mov		eax,PML3_LOCATION | 7 ; Present, Writeable, Supervisor, base 12:31 = page table addr
		stosd
		xor		eax,eax		; EBX=0 and upper 32 bits zero
		stosd
		; and the rest are N/A
.pml4zero:	xor		eax,eax
		stosd
		stosd
		loop		.pml4zero
; okay, and then just enough Page Directory Pointer entries to cover the first 4GB. each entry is indexed from linear addr bits 30:38
; this generates entries to make the lower level one big convenient array 
		xor		edx,edx		; this is used to count bits 30:31 for the loop below
		mov		edi,PML3_LOCATION
.pml3loop:	mov		eax,edx
		shl		eax,12		; EAX = (EDX << 12) | 7
		add		eax,PML2_LOCATION
		or		eax,0x00000007	; Present, Writeable, Supervisor, base 12:31
		stosd
		xor		eax,eax		; EBX=0 and upper 32 bits zero
		stosd
		inc		edx
		cmp		edx,4
		jb		.pml3loop
		; and fill out the rest as N/A
.pml3zero:	xor		eax,eax
		stosd
		stosd
		inc		edx
		cmp		edx,512
		jb		.pml3zero

; and finally fill out the mappings. use 2MB large pages. the above loop delibrately placed the upper
; level entries to make this lower level one big array, so our job is easy.
		xor		edx,edx
		mov		edi,PML2_LOCATION
.pml2loop:	mov		eax,edx
		shl		eax,21
		or		eax,0x00000087	; Present, Writeable, Supervisor, base, 2MB large
		stosd
		xor		eax,eax
		stosd
		inc		edx
		cmp		eax,512
		jb		.pml2loop

; switch on long mode
		mov		eax,0x000000B0	; enable PGE, PAE, PSE
		mov		cr4,eax

		mov		ecx,0xC0000080	; EFER MSR
		rdmsr
		or		eax,0x00000100	; turn on long mode (LME) and IA-32e mode (LMA)
		wrmsr

		mov		eax,PML4_LOCATION
		mov		cr3,eax		; where page table level 4 is

		mov		eax,cr0
		or		eax,0x80000001	; protected mode + paging
		mov		cr0,eax

		jmp		CODE_SELECTOR:.longmode
use64
.longmode:	mov		rax,rbx
		jmp		short $

; generate 386 GDT
use16
gen_gdt_32:	mov		di,_gdt
		cld
		xor		ax,ax
; #0 NULL entry
		stosw
		stosw
		stosw
		stosw
; #1 code
	; 0:15
		mov		cl,4
		mov		ax,0xFFFF		; limit
		stosw
	; 16:31
		xor		ax,ax			; base
		stosw
	; 32:47
		xor		al,al
		mov		ah,0x9A			; access = present, ring 0, executable, readable
		stosw
	; 48:63
		mov		ax,0x00CF		; base 24:31 granular 4KB pages, 32-bit selector
		stosw
; #2 data
	; 0:15
		mov		cl,4
		mov		ax,0xFFFF		; limit
		stosw
	; 16:31
		xor		ax,ax			; base
		stosw
	; 32:47
		xor		al,al
		mov		ah,0x92			; access = present, ring 0, readable/writeable, data
		stosw
	; 48:63
		mov		ax,0x00CF		; base 24:31 granular 4KB pages, 32-bit selector
		stosw
; done
		ret

x_gdtr_old:	dd		0,0
x_gdtr_32:	dd		0,0
x_gdtr_64:	dd		0,0,0,0,0,0,0,0		; <- how long is it?

align		16
_gdt		times (32 * 8) db 0

extern _jmp_8086

