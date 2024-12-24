#include <iostream>
#include <fstream>
#include <unordered_map>
#include <ctime>
#include <chrono>
#include <unistd.h>
#include <list>
#include <fcntl.h>

#include "cache.hpp"
#include <sys/stat.h>
#include <string.h>

struct AccessHint
{
    std::chrono::time_point<std::chrono::steady_clock> next_access_time;
    access_hint_t hint_type;
};

struct CacheBlock
{
    off_t offset;
    size_t size;
    char *data;
    AccessHint access_hint;
};

struct Cache
{
    size_t size;
    std::unordered_map<off_t, CacheBlock *> table;
    std::list<off_t> usage_list; // хранение порядка использования
};

static Cache cache;

// инициализация кэша
int cache_init(size_t size)
{
    std::cout << "Cache initializing with size " << size << std::endl;
    cache.size = size;
    return 0;
}

size_t CACHE_PAGE_SIZE = 2048;
int DEFAULT_TIME_USAGE = 2;

// освобождение памяти кэша
void cache_free()
{
    std::cout << "Проверка перед вызовом cache_free" << std::endl;
    for (const auto &entry : cache.table)
    {
        std::cout << "Сдвиг = " << entry.second->offset
                  << " данные = " << static_cast<void *>(entry.second->data) << std::endl;
    }

    for (auto &entry : cache.table)
    {
        if (entry.second && entry.second->data)
        {
            std::cout << "Освобождение блока co свдвигом = " << entry.second->offset
                      << " и адресом данных " << static_cast<void *>(entry.second->data) << std::endl;
            free(entry.second->data);
            entry.second->data = nullptr;
            delete entry.second;
        }
    }
    cache.table.clear();
    cache.usage_list.clear();
    std::cout << "Кэш успешно очищен." << std::endl;

    std::cout << "cache.table.size() = " << cache.table.size() << std::endl;
    std::cout << "cache.usage_list.size() = " << cache.usage_list.size() << std::endl;

    std::cout << "Проверка после вызова cache_free" << std::endl;
    for (const auto &entry : cache.table)
    {
        std::cout << "Сдвиг = " << entry.second->offset
                  << " данные = " << static_cast<void *>(entry.second->data) << std::endl;
    }
}

int lab2_open(const char *path)
{
    int flags = O_RDWR;
    int fd = open(path, flags);
    if (fd == -1)
    {
        perror("Failed to open file");
    }
    return fd;
}

int lab2_close(int fd)
{
    int result = close(fd);
    if (result < 0)
    {
        perror("Failed to close file");
    }
    return result;
}

CacheBlock *get_block(off_t offset)
{
    std::cout << "Taking block with offset " << offset << std::endl;
    auto iterator = cache.table.find(offset);
    if (iterator != cache.table.end())
    {
        if (iterator->second->access_hint.hint_type == FUTURE_ACCESS) 
            iterator->second->access_hint.next_access_time = get_next_time_usage(DEFAULT_TIME_USAGE);
        std::cout << "Found block" << offset << std::endl;
        return iterator->second;
    }

    return nullptr;
}

std::chrono::time_point<std::chrono::steady_clock> get_next_time_usage(int seconds_until_next_use)
{
    return std::chrono::steady_clock::now() + std::chrono::seconds(seconds_until_next_use);
}

CacheBlock *init_cache_block(off_t offset, size_t bytes_read, void *aligned_buffer, access_hint_t hint)
{
    std::cout << "Creating cache block" << std::endl;
    CacheBlock *new_block = new CacheBlock();
    new_block->offset = offset;
    new_block->size = bytes_read;
    new_block->data = static_cast<char *>(aligned_buffer);
    new_block->access_hint.next_access_time = get_next_time_usage(DEFAULT_TIME_USAGE);
    new_block->access_hint.hint_type = hint;
    std::cout << "block intialized" << std::endl;

    cache.table[offset] = new_block;
    std::cout << "Added block to table" << std::endl;
    cache.usage_list.push_back(offset);

    std::cout << "Table structures initialized" << std::endl;

    return new_block;
}

void *align_buffer(size_t size)
{
    void *buffer;
    if (posix_memalign(&buffer, CACHE_PAGE_SIZE, size) != 0)
    {
        perror("Failed while aligning buffer");
        return nullptr;
    }
    return buffer;
}

// Чтение из файла
ssize_t lab2_read(int fd, void *buffer, size_t count, access_hint_t hint)
{
    std::cout << "Reading file" << std::endl;
    off_t offset = lab2_lseek(fd, 0, SEEK_CUR);

    CacheBlock *block = get_block(offset);
    if (block && block->size >= count)
    {
        memcpy(buffer, block->data, count);
        return count;
    }

    void *aligned_buffer = align_buffer(count);

    ssize_t bytes_read = read(fd, aligned_buffer, count);
    if (bytes_read < 0)
    {
        perror("read");
        free(aligned_buffer);
        return -1;
    }

    if (cache.table.size() >= cache.size)
        eject_block();

    init_cache_block(offset, bytes_read, aligned_buffer, hint);

    return bytes_read;
}

// Запись в файл
ssize_t lab2_write(int fd, const void *buffer, size_t count, access_hint_t hint)
{
    std::cout << "Writing file" << std::endl;
    off_t offset = lab2_lseek(fd, 0, SEEK_CUR);

    void *aligned_buffer = align_buffer(count);

    memcpy(aligned_buffer, buffer, count);
    ssize_t bytes_written = write(fd, aligned_buffer, count);

    if (bytes_written < 0)
    {
        perror("write");
        free(aligned_buffer);
        return -1;
    }

    if (cache.table.size() >= cache.size)
        eject_block();

    if (cache.table.find(offset) == cache.table.end()) {
        std::cout << "block not found" << std::endl;
        init_cache_block(offset, bytes_written, aligned_buffer, hint);
    } else {
        std::cout << "block found, clearing aligned buffer" << std::endl;
        free(aligned_buffer);
    }

    return bytes_written;
}

// Перемещение указателя на данные
off_t lab2_lseek(int fd, off_t offset, int whence)
{
    std::cout << "lseek to offset " << offset << std::endl;
    off_t new_offset = lseek(fd, offset, whence);
    if (new_offset == -1)
    {
        perror("lseek");
        return -1;
    }
    return new_offset;
}

int lab2_fsync(int fd)
{
    std::cout << "synchronizing file" << std::endl;
    if (fsync(fd) < 0)
    {
        perror("fsync");
        return -1;
    }
    return 0;
}

void eject_block()
{
    std::cout << "Start ejecting proccess" << std::endl;
    if (cache.table.empty())
    {
        std::cerr << "Cache is empty. No blocks to eject." << std::endl;
        return;
    }

    off_t block_to_eject = -1;
    auto farthest_time = std::chrono::time_point<std::chrono::steady_clock>::min();

    // Ищем блок с самым дальним временем следующего использования
    for (const auto &offset : cache.usage_list)
    {
        CacheBlock *block = cache.table[offset];
        if (block->access_hint.next_access_time > farthest_time)
        {
            farthest_time = block->access_hint.next_access_time;
            block_to_eject = offset;
        }
    }

    if (block_to_eject != -1)
    {
        std::cout << "block to eject with offset " << block_to_eject << std::endl;
        delete[] cache.table[block_to_eject]->data; // Освобождаем память для данных
        delete cache.table[block_to_eject];         // Освобождаем память для блока
        cache.table.erase(block_to_eject);          // Удаляем блок из таблицы кэша
        cache.usage_list.remove(block_to_eject);    // Удаляем смещение из списка использования

        std::cout << "Ejected block at offset: " << block_to_eject << std::endl;
    }
}

int lab2_advice(off_t offset, access_hint_t hint)
{
    // Проверяем, существует ли дескриптор файла
    if (cache.table.find(offset) == cache.table.end())
    {
        perror("Not found block access hint");
        return -1;
    }

    // Устанавливаем подсказку о следующем доступе
    CacheBlock *block = cache.table[offset];
    block->access_hint.hint_type = hint;

    if (hint == FUTURE_ACCESS)
    {
        block->access_hint.next_access_time = get_next_time_usage(DEFAULT_TIME_USAGE);
        std::cout << "Set future access hint for offset " << offset << " for time: " << std::chrono::duration_cast<std::chrono::seconds>(block->access_hint.next_access_time.time_since_epoch()).count() << std::endl;
    }
    else
        std::cout << "No specific access hint for offset " << offset << std::endl;

    return 0; // Успешно обработано
}
