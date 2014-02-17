#ifndef _PIC_H_
#define _PIC_H_

	void pic_remap(int offset_master, int offset_slave);
	void pic_send_eoi(uint8_t irq);
	void pic_disable();
	void pic_write_mask(uint8_t pic1_mask, uint8_t pic2_mask);
	void pic_read_mask(uint8_t* pic1_mask, uint8_t* pic2_mask);
	void pic_disable_line(uint8_t line);
	void pic_enable_line(uint8_t line);
	uint16_t pic_get_irr(void);
	uint16_t pic_get_isr(void);
	
#endif

