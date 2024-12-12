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
#include <fcntl.h>
#include <sys/stat.h>

const char* LOG_PIPE = "/tmp/log_time";
const int PORT = 8080;

std::chrono::time_point<std::chrono::system_clock> start_time = std::chrono::system_clock::now();

std::mutex clients_mutex;
std::vector<int> clients;

std::atomic<bool> running(true);

std::string last_sent_timezone = "";
std::string last_sent_duration = "";

int log_fd;

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "Signal received, shutting down...\n";
        running = false;

        // Закрытие всех клиентских сокетов
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (int client_socket : clients) {
            close(client_socket);
        }
        clients.clear();
    }
}

// Подключение к FIFO для логирования
void connectToLogPipe() {
    log_fd = open(LOG_PIPE, O_WRONLY | O_NONBLOCK);
    if (log_fd == -1) {
        perror("Failed to open FIFO for writing");
        return;
    }
    return;
}

// Логирование сообщений
void logToPipe(const std::string& message) {
    if (log_fd > 0) {
        std::string formatted_message = message + "\n";
        write(log_fd, formatted_message.c_str(), formatted_message.length());
    } else {
        connectToLogPipe();
    }
}

// Получение текущего часового пояса
std::string getTimeZone() {
    time_t rawtime;
    struct tm* timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    char buffer[10];
    strftime(buffer, sizeof(buffer), "%Z", timeinfo);
    return std::string(buffer);
}

// Вычисление продолжительности текущей сессии
std::string getSessionDuration() {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
    long hours = duration / 3600;
    int minutes = (duration % 3600) / 60;
    int seconds = duration % 60;

    return std::to_string(hours) + "h " + std::to_string(minutes) + "m " + std::to_string(seconds) + "s";
}

// Проверка изменения состояния
bool hasStateChanged(std::string& current_timezone, std::string& current_duration) {
    std::string new_timezone = getTimeZone();
    std::string new_duration = getSessionDuration();

    if (new_timezone != last_sent_timezone || new_duration != last_sent_duration) {
        last_sent_timezone = new_timezone;
        last_sent_duration = new_duration;
        current_timezone = new_timezone;
        current_duration = new_duration;
        return true;
    }
    return false;
}

void broadcastUpdates() {
    while (running) {
        std::string timezone;
        std::string session_duration;

        if (hasStateChanged(timezone, session_duration)) {
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::string current_time = std::ctime(&now);
            current_time.pop_back();

            std::string response = "Current Timezone: " + timezone + "\n"
                                   "Session Duration: " + session_duration + "\n"
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
        logToPipe("Error: Socket failed\n");
        exit(EXIT_FAILURE);
    }

    // Настройка сокета
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        logToPipe("Error: setsockopt failed\n");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Привязка сокета
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        logToPipe("Error: Bind failed\n");
        exit(EXIT_FAILURE);
    }

    // Прослушивание подключений
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        logToPipe("Error: Listen failed\n");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server is running on port " << PORT << "...\n";
    logToPipe("Server started on port " + std::to_string(PORT));
    
    std::thread broadcaster(broadcastUpdates);

    while (running) {
        client_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        std::cout << "Client socket " << client_socket << "\n";
        
        if (!running) {
            break; // Прерывание при завершении работы
        }
        
        // Обработка ошибок accept
        if (client_socket < 0) {
            if (errno == EINTR) {
                // Сервер был прерван
                std::cout << "Server interrupted. Shutting down.\n";
                logToPipe("Interrupted: Server interrupted. Shutting down.\n");
                break;
            }
            logToPipe("Error: " + std::string(strerror(errno)));
            perror("Accept failed");
            continue;
        }
        
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.push_back(client_socket);
        }

        std::cout << "New client connected.\n";
        logToPipe("Client connected: " + std::to_string(client_socket));
    }
    
    broadcaster.join();
    
    shutdown(server_fd, SHUT_RDWR);
    close(server_fd); // Закрываем основной серверный сокет.
    if (log_fd > 0) {
        close(log_fd); // Закрываем дескриптор FIFO.
    }
    
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    logToPipe("Server stopped.");

    return 0;
}
