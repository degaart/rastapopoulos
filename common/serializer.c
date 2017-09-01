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

#define SERIALIZE(name, datatype)                                   \
datatype * name (struct serializer* serializer, datatype val) {     \
    assert(!serializer->locked);                                    \
    size_t max_size;                                                \
    datatype * buf = serialize_buffer(serializer, &max_size);       \
    assert(buf && max_size >= sizeof(datatype));                    \
    serialize_buffer_finish(serializer, sizeof(datatype));          \
    *buf = val;                                                     \
    return buf;                                                     \
}

SERIALIZE(serialize_int, int);
SERIALIZE(serialize_size_t, size_t);
SERIALIZE(serialize_int64, long long);

void* serialize_buffer(struct serializer* serializer,
                       size_t* max_size)
{
    assert(!serializer->locked);
    
    size_t rem = serializer->size - serializer->pos;
    assert(rem > 0);

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

long long deserialize_int64(struct deserializer* deserializer)
{
    const void* buf = deserialize_buffer(deserializer, sizeof(long long));
    assert(buf != NULL);
    return *((const long long*)buf);
}

const void* deserialize_buffer(struct deserializer* deserializer, size_t size)
{
    assert(size <= deserializer->size - deserializer->pos);
    const void* result = deserializer->buffer + deserializer->pos;
    deserializer->pos += size;
    return result;
}




