#pragma once

#include <stddef.h>

struct serializer {
    unsigned char* buffer;
    size_t size;
    size_t pos;
    int locked;
};

struct deserializer {
    const unsigned char* buffer;
    size_t size;
    size_t pos;
};

void serializer_init(struct serializer* serializer,
                     void* buffer,
                     size_t buffer_size);
int* serialize_int(struct serializer* serializer, int val);
size_t* serialize_size_t(struct serializer* serializer, size_t val);
long long* serialize_int64(struct serializer* serializer, long long val);
void* serialize_buffer(struct serializer* serializer,
                       size_t* max_size);
void serialize_buffer_finish(struct serializer* serializer,
                             size_t size);
size_t serializer_finish(struct serializer* serializer);



void deserializer_init(struct deserializer* deserializer,
                       const void* buffer,
                       size_t buffer_size);
int deserialize_int(struct deserializer* deserializer);
size_t deserialize_size_t(struct deserializer* deserializer);
long long deserialize_int64(struct deserializer* deserializer);
const void* deserialize_buffer(struct deserializer* deserializer, size_t size);


