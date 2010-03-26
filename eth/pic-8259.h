
void pic_8259_init();
void pic_8259_seoi(unsigned int i);
unsigned int pending_interrupts();
unsigned int in_service_interrupts();

