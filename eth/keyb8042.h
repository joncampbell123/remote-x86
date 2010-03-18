
void keyb8042_wait_for_output_ready();
void keyb8042_wait_for_input_ready();
void keyb8042_write_buffer(unsigned char c);
int keyb8042_read_buffer_imm();
int keyb8042_read_buffer();
void keyb8042_write_command(unsigned char c);
void keyb8042_write_command_byte(unsigned char c);
void keyb8042_write_leds(unsigned char c);
void keyb8042_init();
int keyb8042_readkey();
int keyb8042_waitkey();

