#ifndef CACHE_H
#define CACHE_H

#include <chrono>
#include <string>

// size_t MAX_CACHE_SIZE = 4096;
// #define CACHE_PAGE_SIZE 512;

enum access_hint_t
{
    FUTURE_ACCESS,
    NO_HINT
};

int cache_init(size_t size);
void cache_free();

// CacheBlock *get_block(off_t offset);
std::chrono::time_point<std::chrono::steady_clock> get_next_time_usage(int seconds_until_next_use);
// CacheBlock *init_cache_block(off_t offset, size_t bytes_read, void *aligned_buffer, access_hint_t hint);
void *align_buffer(size_t size);


int lab2_open(const char *path);
int lab2_close(int fd);
ssize_t lab2_read(int fd, void *buffer, size_t count, access_hint_t hint);
ssize_t lab2_write(int fd, const void *buffer, size_t count, access_hint_t hint);
off_t lab2_lseek(int fd, off_t offset, int whence);
int lab2_fsync(int fd);

int lab2_advice(off_t offset, access_hint_t hint);
void eject_block();

#endif // CACHE_H
