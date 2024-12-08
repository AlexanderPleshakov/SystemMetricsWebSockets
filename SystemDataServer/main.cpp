#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <ctime>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <mutex>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <fcntl.h>
#include <sys/stat.h>

const char* LOG_PIPE = "/tmp/log_system_data";
const int PORT = 8081;
std::chrono::time_point<std::chrono::system_clock> start_time = std::chrono::system_clock::now();

std::mutex clients_mutex;
std::vector<int> clients;

std::atomic<bool> running(true);

std::string last_sent_memory = "";
std::string last_sent_user_time = "";

int log_fd;

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "Signal received, shutting down...\n";
        running = false;
    }
}

// Подключение к FIFO для логирования
void connectToLogPipe() {
    log_fd = open(LOG_PIPE, O_WRONLY);
    if (log_fd == -1) {
        perror("Failed to open FIFO for writing");
        exit(EXIT_FAILURE);
    }
}

// Логирование сообщений
void logToPipe(const std::string& message) {
    if (log_fd > 0) {
        std::string formatted_message = message + "\n";
        write(log_fd, formatted_message.c_str(), formatted_message.length());
    }
}

// Получение текущего часового пояса
std::string getFreeMemoryPercentage() {
    vm_statistics64_data_t vm_stats;
    mach_msg_type_number_t info_count = HOST_VM_INFO64_COUNT;
    kern_return_t kr = host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stats, &info_count);

    if (kr == KERN_SUCCESS) {
        uint64_t free_memory = (vm_stats.free_count + vm_stats.inactive_count) * vm_page_size;
        uint64_t total_memory;
        size_t size = sizeof(total_memory);

        if (sysctlbyname("hw.memsize", &total_memory, &size, nullptr, 0) == 0) {
            double percentage = (static_cast<double>(free_memory) / total_memory) * 100;
            return std::to_string(static_cast<int>(percentage)) + "%";
        }
    }

    return "N/A";
}


// Вычисление продолжительности текущей сессии
std::string getUserTime() {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        long seconds = usage.ru_utime.tv_sec;
        int microseconds = usage.ru_utime.tv_usec;
        return std::to_string(seconds) + "." + std::to_string(microseconds) + "s";
    }
    return "N/A";
}


// Проверка изменения состояния
bool hasStateChanged(std::string& current_memory, std::string& current_user_time) {
    std::string new_memory = getFreeMemoryPercentage();
    std::string new_time = getUserTime();

    if (new_memory != last_sent_memory || new_time != last_sent_user_time) {
        last_sent_memory = new_memory;
        last_sent_user_time = new_time;
        current_memory = new_memory;
        current_user_time = new_time;
        return true;
    }
    return false;
}

void broadcastUpdates() {
    while (running) {
        std::string memory;
        std::string user_time;

        if (hasStateChanged(memory, user_time)) {
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::string current_time = std::ctime(&now);
            current_time.pop_back();

            std::string response = "Current Memory: " + memory + "\n"
                                   "User Time: " + user_time + "\n"
                                   "Current Time: " + current_time + "\n";
            logToPipe("Response: " + response);

            std::lock_guard<std::mutex> lock(clients_mutex);

            for (auto it = clients.begin(); it != clients.end();) {
                if (send(*it, response.c_str(), response.length(), 0) == -1) {
                    if (errno == EPIPE) {
                        std::cerr << "Client disconnected (broken pipe).\n";
                    } else {
                        perror("Error sending data to client");
                    }
                    logToPipe("Error: " + std::string(strerror(errno)));

                    // Удаление разорванного соединения
                    it = clients.erase(it);
                } else {
                    ++it;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// Основной сервер
int main() {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    connectToLogPipe();
    
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    std::vector<std::thread> threads;

    // Создание сокета
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        logToPipe("Error: Socket failed");
        exit(EXIT_FAILURE);
    }

    // Настройка сокета
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        logToPipe("Error: setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Привязка сокета
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        logToPipe("Error: Bind failed");
        exit(EXIT_FAILURE);
    }

    // Прослушивание подключений
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        logToPipe("Error: Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server is running on port " << PORT << "...\n";
    logToPipe("Server started on port " + std::to_string(PORT));
    
    std::thread broadcaster(broadcastUpdates);

    while (running) {
        client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        std::cout << "Client socket " << client_socket << "\n";
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back(client_socket);
        }
        
        // Обработка ошибок accept
        if (client_socket < 0) {
            if (errno == EINTR) {
                // Сервер был прерван (например, остановлен)
                logToPipe("Error: " + std::string(strerror(errno)));
                std::cout << "Server interrupted. Shutting down.\n";
                break;
            }
            perror("Accept failed");
            logToPipe("Error: " + std::string(strerror(errno)));
            continue;
        }

        std::cout << "New client connected.\n";
        logToPipe("Client connected: " + std::to_string(client_socket));
    }

    // Завершение работы потоков
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    broadcaster.join();
    close(server_fd);
    logToPipe("Server stopped.");

    return 0;
}
