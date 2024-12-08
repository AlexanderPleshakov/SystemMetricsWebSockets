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

const int PORT = 8080;
std::chrono::time_point<std::chrono::system_clock> start_time = std::chrono::system_clock::now();

std::mutex clients_mutex;
std::vector<int> clients;

std::atomic<bool> running(true);

std::string last_sent_timezone = "";
std::string last_sent_duration = "";

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

            std::lock_guard<std::mutex> lock(clients_mutex);

            for (auto it = clients.begin(); it != clients.end();) {
                if (send(*it, response.c_str(), response.length(), 0) == -1) {
                    if (errno == EPIPE) {
                        std::cerr << "Client disconnected (broken pipe).\n";
                    } else {
                        perror("Error sending data to client");
                    }

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
    
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    std::vector<std::thread> threads;

    // Создание сокета
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Настройка сокета
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Привязка сокета
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Прослушивание подключений
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server is running on port " << PORT << "...\n";
    
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
                std::cout << "Server interrupted. Shutting down.\n";
                break;
            }
            perror("Accept failed");
            continue;
        }

        std::cout << "New client connected.\n";

        // threads.emplace_back(std::thread(handleClient, client_socket));
    }

    // Завершение работы потоков
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    broadcaster.join();
    close(server_fd);
    return 0;
}
