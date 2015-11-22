#ifndef _DYNAMIC_ARRAY_H_
#define _DYNAMIC_ARRAY_H_

#include <stdint.h>
#include "debug.h"

template<typename T>
class DynamicArray {
private:
    T* _storage;
    unsigned _size;
public:
    DynamicArray();
    DynamicArray(const DynamicArray&);
    ~DynamicArray();
    void set(unsigned index, const T& val);
    T& get(unsigned index);
    const T& get(unsigned index) const;
    void resize(unsigned new_size);
    unsigned size() const;
    T& operator[](unsigned index);
    const T& operator[](unsigned index) const;
};

#include "dynamic_array.cxx"

#endif /* defined(_DYNAMIC_ARRAY_H_) */
