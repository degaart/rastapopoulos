
template<typename T>
DynamicArray<T>::DynamicArray()
: _storage(new T[0]), _size(0) {
    
}

template<typename T>
DynamicArray<T>::DynamicArray(const DynamicArray<T>& a) {
    _storage = new T[a._size];
    _size = a._size;
    
    for(unsigned i = 0; i < _size; i++)
        _storage[i] = a._storage[i];
}

template<typename T>
DynamicArray<T>::~DynamicArray() {
    if(_storage)
        delete[] _storage;
}

template<typename T>
void DynamicArray<T>::set(unsigned index, const T& val) {
    if(index >= _size)
        resize(index+1);
    
    _storage[index] = val;
}

template<typename T>
T& DynamicArray<T>::get(unsigned index) {
    assert((_size != 0) && (index < _size));
    return _storage[index];
}

template<typename T>
const T& DynamicArray<T>::get(unsigned index) const {
    assert((_size != 0) && (index < _size));
    return _storage[index];
}

template<typename T>
void DynamicArray<T>::resize(unsigned new_size) {
    T* new_storage = new T[new_size];
    for(unsigned i = 0; i < new_size; i++) {
        if(i < _size)
            new_storage[i] = _storage[i];
    }
    delete[] _storage;
    _storage = new_storage;
    _size = new_size;
}

template<typename T>
unsigned DynamicArray<T>::size() const {
    return _size;
}

template<typename T>
T& DynamicArray<T>::operator[](unsigned index) {
    return get(index);
}

template<typename T>
const T& DynamicArray<T>::operator[](unsigned index) const {
    return get(index);
}

