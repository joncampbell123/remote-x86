
#define DECLARE_INTERRUPT_FUNCTION(x)	extern void x(); \
					extern void x##_iret(); \
					__asm__ (	#x "_iret:	pushfl\n" \
							"		pushal\n" \
							"		call		" #x "\n" \
							"		popal\n" \
							"		popfl\n" \
							"		iret");

#define INTERRUPT_FUNCTION(x)		x##_iret

#define MAX_IRQ			16

extern unsigned char __attribute__((aligned(16))) IDT[256*8];
extern unsigned char __attribute__((aligned(8))) IDTR[8];
extern void (*irq_function[MAX_IRQ])(int n);

void idt_init();
void do_irq(unsigned int n);
void set_idt(unsigned char interrupt,void *intfunc);

