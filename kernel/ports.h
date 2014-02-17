#ifndef _PORTS_H_
#define _PORTS_H_

	#define PORT_PIC1_COMMAND			0x20
	#define PORT_PIC1_DATA				(PORT_PIC1_COMMAND+1)
	#define PORT_PIC2_COMMAND			0xA0
	#define PORT_PIC2_DATA				(PORT_PIC2_COMMAND+1)
	
	#define PORT_KBD_DATA				0x60
	#define PORT_KBD_COMMAND			0x64
	
	#define PORT_PIT_CHAN0_DATA			0x40
	#define PORT_PIT_CHAN1_DATA			0x41
	#define PORT_PIT_CHAN2_DATA			0x42
	#define PORT_PIT_COMMAND			0x43
	
	#define PORT_UNUSED					0x80
	
	#define IOWAIT()					_inb(PORT_UNUSED)
	

#endif // _PORTS_H_
