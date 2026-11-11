# block cache

## Architecture

- The cache works on a fixed 128 entry of 512-byte sectors.
- Cache is decoupled from block devices, using a BlockDevice struct.
- A cache entry is identified by BlockDevice, sector lba, and generation.
- The generation is used to detect media changes.
- Entry eviction uses the CLOCK algorithm
- A cache entry has multiple flags:
    - assigned: the entry has been assigned
    - valid: the entry's buffer data is valid
    - referenced: referenced bit for CLOCK eviction
    - pinned: I/O is in progress for the entry. It must not be evicted, and operations must wait for I/O to complete
- Metadata is protected by a mutex.
- Per-device generation is stored on a growing dynamically allocated buffer. For now, this buffer never shrinks
- Previously, each entry had a mutex. But concurrency issues made that mutex obsolete, so it was removed

## NULL buffer tests

```
for i in 1 2 3 4; do
    make UNIT_TESTS=1 \
        --eval='build/test_block_cache.o: CFLAGS += -DBLOCK_CACHE_NULL_CASE=4' \
        -W test/test_block_cache.c \
        test
done
```

