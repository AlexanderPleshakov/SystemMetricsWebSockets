#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <atomic>
#include <csignal>
#include <ctime>

const char* LOG_SYSTEM_DATA = "/tmp/log_system_data";
const char* LOG_TIME = "/tmp/log_time";

std::atomic<bool> running(true);

// Форматирование текущей даты и времени для имени файла
std::string getCurrentTimestamp() {
    char buffer[64];
    std::time_t now = std::time(nullptr);
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", std::localtime(&now));
    return std::string(buffer);
}

// Обработка сигналов завершения
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "Signal received, shutting down...\n";
        running = false;
    }
}

void startLoggingServer() {
    // Установка обработчика сигналов
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Создание именованных каналов
    if (mkfifo(LOG_SYSTEM_DATA, 0666) == -1 && errno != EEXIST) {
        perror("Failed to create log_system FIFO");
        return;
    }

    if (mkfifo(LOG_TIME, 0666) == -1 && errno != EEXIST) {
        perror("Failed to create log_time FIFO");
        return;
    }

    // Создание лог-файлов с текущей датой и временем
    std::string system_log_file = "/var/log/system_data_logs_" + getCurrentTimestamp() + ".txt";
    std::string time_log_file = "/var/log/time_logs_" + getCurrentTimestamp() + ".txt";

    std::ofstream log_file_system(system_log_file);
    if (!log_file_system.is_open()) {
        std::cerr << "Failed to open " << system_log_file << "\n";
        return;
    }

    std::ofstream log_file_time(time_log_file);
    if (!log_file_time.is_open()) {
        std::cerr << "Failed to open " << time_log_file << "\n";
        return;
    }

    std::cout << "Logging server is running...\n";

    // Открытие FIFO для чтения
    int fd_system = open(LOG_SYSTEM_DATA, O_RDONLY | O_NONBLOCK);
    if (fd_system == -1) {
        perror("Failed to open LOG_SYSTEM_DATA FIFO");
        return;
    }

    int fd_time = open(LOG_TIME, O_RDONLY | O_NONBLOCK);
    if (fd_time == -1) {
        perror("Failed to open LOG_TIME FIFO");
        return;
    }

    char buffer_system[1024];
    char buffer_time[1024];

    while (running) {
        // Чтение данных из LOG_SYSTEM_DATA
        ssize_t bytes_read_system = read(fd_system, buffer_system, sizeof(buffer_system) - 1);
        if (bytes_read_system > 0) {
            buffer_system[bytes_read_system] = '\0';
            log_file_system << buffer_system << std::endl;
            std::cout << "System Log: " << buffer_system << std::endl;
        }

        // Чтение данных из LOG_TIME
        ssize_t bytes_read_time = read(fd_time, buffer_time, sizeof(buffer_time) - 1);
        if (bytes_read_time > 0) {
            buffer_time[bytes_read_time] = '\0';
            log_file_time << buffer_time << std::endl;
            std::cout << "Time Log: " << buffer_time << std::endl;
        }

        // Маленькая задержка для уменьшения нагрузки на CPU
        usleep(100000); // 100ms
    }

    // Закрытие дескрипторов и файлов
    close(fd_system);
    close(fd_time);
    log_file_system.close();
    log_file_time.close();

    std::cout << "Logging server stopped.\n";
}

int main() {
    startLoggingServer();
    return 0;
}

