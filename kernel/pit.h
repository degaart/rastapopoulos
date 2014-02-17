#ifndef _PIT_H_
#define _PIT_H_

	void pit_set_interval(int hz);
	uint32_t pit_read_count();
	void pit_set_reload(uint16_t val);

#endif //_PIT_H_

