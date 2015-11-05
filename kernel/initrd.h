#ifndef _INITRD_H_
#define _INITRD_H_

#include <stdint.h>

struct InitrdHeader_t;

class Initrd {
private:
    InitrdHeader_t*     _header;
    unsigned            _size;
    static Initrd*      _instance;

    Initrd(void* buffer, unsigned size);
    ~Initrd();
public:
    class File {
    private:
        friend Initrd;

        uint8_t* _data;
        unsigned _size;
        unsigned _offset;

        File(uint8_t* data, unsigned size);
    public:
        ~File();
        uint32_t size();
        void* data();
        signed read(void* buffer, unsigned size);
    };

    static void init(void* buffer, unsigned size);
    static Initrd& get();
    static void free();

    File* open(const char* name);
};

#endif //_INITRD_H_
