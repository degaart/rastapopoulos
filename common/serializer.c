#include "serializer.h"
#include "debug.h"

void serializer_init(struct serializer* serializer,
                     void* buffer,
                     size_t buffer_size)
{
    serializer->buffer = buffer;
    serializer->size = buffer_size;
    serializer->pos = 0;
    serializer->locked = 0;
}

int* serialize_int(struct serializer* serializer, int val)
{
    if(serializer->locked)
        return NULL;

    size_t max_size;
    int* buf = serialize_buffer(serializer, &max_size);
    if(max_size < sizeof(int)) {
        serialize_buffer_finish(serializer, 0);
        return NULL;
    }
    serialize_buffer_finish(serializer, sizeof(int));
    *buf = val;
    return buf;
}

size_t* serialize_size_t(struct serializer* serializer, size_t val)
{
    if(serializer->locked)
        return NULL;

    size_t max_size;
    size_t* buf = serialize_buffer(serializer, &max_size);
    if(max_size < sizeof(size_t)) {
        serialize_buffer_finish(serializer, 0);
        return NULL;
    }
    serialize_buffer_finish(serializer, sizeof(size_t));
    *buf = val;
    return buf;
}

void* serialize_buffer(struct serializer* serializer,
                       size_t* max_size)
{
    if(serializer->locked)
        return NULL;
    
    size_t rem = serializer->size - serializer->pos;
    if(!rem)
        return NULL;

    serializer->locked = 1;
    *max_size = rem;
    return serializer->buffer + serializer->pos;
}

void serialize_buffer_finish(struct serializer* serializer,
                             size_t size)
{
    assert(serializer->locked);

    assert(size <= serializer->size - serializer->pos);
    serializer->pos += size;
    serializer->locked = 0;
}

size_t serializer_finish(struct serializer* serializer)
{
    return serializer->pos;
}

void deserializer_init(struct deserializer* deserializer,
                       const void* buffer,
                       size_t buffer_size)
{
    deserializer->buffer = buffer;
    deserializer->size = buffer_size;
    deserializer->pos = 0;
}

int deserialize_int(struct deserializer* deserializer)
{
    const void* buf = deserialize_buffer(deserializer, sizeof(int));
    assert(buf != NULL);
    return *((const int*)buf);
}

size_t deserialize_size_t(struct deserializer* deserializer)
{
    const void* buf = deserialize_buffer(deserializer, sizeof(size_t));
    assert(buf != NULL);
    return *((const size_t*)buf);
}

const void* deserialize_buffer(struct deserializer* deserializer, size_t size)
{
    if(size > deserializer->size - deserializer->pos)
        return NULL;
    const void* result = deserializer->buffer + deserializer->pos;
    deserializer->pos += size;
    return result;
}




