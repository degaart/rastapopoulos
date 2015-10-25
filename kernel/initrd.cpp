#include "initrd.h"
#include "debug.h"
#include "util.h"
#include "string.h"
#include "../bootldr/kernel_params.h"
#include "vmm.h"
#include "pmm.h"
#include "initrd_header.h"

static const kernel_params* _kparams = (const kernel_params*)KERNEL_PARAMS;
Initrd* Initrd::_instance = nullptr;

Initrd::Initrd() {
    /* identity map initrd pages RO */
    uint32_t aligned_start = truncate(_kparams->initrd_address, VMM::PAGE_SIZE);
    for(
        uint32_t page = aligned_start;
        page < _kparams->initrd_address + _kparams->initrd_size;
        page += VMM::PAGE_SIZE)
    {
        PMM::reserve(page);
        VMM::map(page, page, VMM::PAGE_PRESENT);
    }
    TRACE("Initrd: 0x%X - 0x%X", _kparams->initrd_address, _kparams->initrd_address + _kparams->initrd_size);

    _header = (InitrdHeader_t*)_kparams->initrd_address;
    _size = _kparams->initrd_size;
}

Initrd::~Initrd() {
    uint32_t aligned_start = truncate(_kparams->initrd_address, VMM::PAGE_SIZE);
    for(
        uint32_t page = aligned_start;
        page < _kparams->initrd_address + _kparams->initrd_size;
        page += VMM::PAGE_SIZE)
    {
        PMM::free(page);
        VMM::unmap(page);
    }    
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

Initrd& Initrd::get() {
    if(_instance == nullptr)
        _instance = new Initrd();
    return *_instance;
}

void Initrd::free() {
    if(_instance)
        delete _instance;
    _instance = nullptr;
}


