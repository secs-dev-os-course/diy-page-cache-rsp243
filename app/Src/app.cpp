#include <iostream>
#include "cache.hpp"
#include <string.h>
#include <unistd.h>

#define TEST_FILE "test.txt"
#define BUFFER_SIZE 8192

int main() {
    char write_data[BUFFER_SIZE];
    char read_data[BUFFER_SIZE];
    memset(write_data, 'M', BUFFER_SIZE);
    memset(read_data, 0, BUFFER_SIZE);

    cache_init(8192);

    int fd = lab2_open(TEST_FILE);
    if (fd < 0)
    {
        std::cerr << "Ошибка: файл " << TEST_FILE << " не найден или не может быть открыт" << std::endl;
        cache_free();
        return 1;
    }

    if (lab2_write(fd, write_data, BUFFER_SIZE, FUTURE_ACCESS) != BUFFER_SIZE)
    {
        std::cerr << "Ошибка записи" << std::endl;
        lab2_close(fd);
        cache_free();
        return 1;
    }

    lab2_lseek(fd, 0, SEEK_SET);

    if (lab2_read(fd, read_data, BUFFER_SIZE, FUTURE_ACCESS) != BUFFER_SIZE)
    {
        std::cerr << "Ошибка чтения" << std::endl;
        lab2_close(fd);
        cache_free();
        return 1;
    }

    if (memcmp(write_data, read_data, BUFFER_SIZE) != 0)
    {
        std::cerr << "Данные не совпадают" << std::endl;
        lab2_close(fd);
        cache_free();
        return 1;
    }

    lab2_fsync(fd);
    lab2_close(fd);
    cache_free();

    std::cout << "Тест пройден успешно" << std::endl;
    return 0;
}
