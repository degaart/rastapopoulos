#include "initrd.h"
#include "debug.h"
#include "util.h"
#include "string.h"
#include "vmm.h"
#include "pmm.h"
#include "initrd_header.h"

Initrd* Initrd::_instance = nullptr;

Initrd::Initrd(void* buffer, unsigned size) {
    _header = (InitrdHeader_t*)buffer;
    _size = size;
}

Initrd::~Initrd() {

}

Initrd::File* Initrd::open(const char* name) {
    if(!_header)
        return nullptr;

    InitrdHeader_t* hdr = _header;
    while(1) {
        assert(hdr < _header + _size);

        if(!strcmp(hdr->name, name)) {
            return new File((uint8_t*)hdr+sizeof(*hdr), hdr->size);
        }
        if(hdr->last)
            break;

        hdr = (InitrdHeader_t*)((uint8_t*)hdr + sizeof(InitrdHeader_t) + hdr->size);
    }

    return nullptr;
}

Initrd::File::File(uint8_t* data, unsigned size)
: _data(data), _size(size), _offset(0) {

}

Initrd::File::~File() {

}

void Initrd::init(void* buffer, unsigned size) {
    assert(!_instance);
    _instance = new Initrd(buffer, size);
}

uint32_t Initrd::File::size() {
    return _size;
}

void* Initrd::File::data() {
    return _data;
}

signed Initrd::File::read(void* buffer, unsigned size) {
    assert(_offset <= _size);
    if(_offset == _size)
        return 0;

    if(size > _size - _offset)
        size = _size -_offset;
    memcpy(buffer, _data + _offset, size);
    _offset += size;
    return size;
}

void Initrd::File::seek(unsigned offset) {
    _offset = offset;
}

Initrd& Initrd::get() {
    assert(_instance != nullptr);
    return *_instance;
}

void Initrd::free() {
    if(_instance)
        delete _instance;
    _instance = nullptr;
}


