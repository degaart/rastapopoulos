#ifndef _STREAM_H_
#define _STREAM_H_

class OutputStream {
public:
	virtual void write(int ch) = 0;
};

class InputStream {
public:
	virtual int read() = 0;
};


#endif //_STREAM_H_
