#include <iostream>
#include <utility>
#include <vector>
#include <string>
#include <chrono>
#include <map>
#include <thread>
#include <filesystem>
#include <fstream>
#include <array>

#define fs std::filesystem

// Classes
struct Timer {
    std::chrono::duration<double> length {};
    std::chrono::time_point<std::chrono::system_clock> start;
    Timer *coolDown = nullptr;
    bool done {};
};

// Global variables
std::map<std::string, Timer> timers;
bool running = true;

// Utility functions
std::vector<std::string> split(const std::string &str, const std::string &delim) {
    size_t pos_start = 0, pos_end, delim_len = delim.length();
    std::string token;
    std::vector<std::string> toReturn;

    while ((pos_end = str.find(delim, pos_start)) != std::string::npos) {
        token = str.substr (pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        toReturn.push_back(token);
    }

    toReturn.push_back(str.substr (pos_start));
    return toReturn;
}

std::ostream &operator<<(std::ostream &stream, std::chrono::duration<double> duration) {
    int hours = (int)duration.count() / 3600;
    int minutes = (int)(duration.count() - hours * 3600) / 60;
    int seconds = (int)(duration.count() - hours * 3600 - minutes * 60);
    stream << hours << (hours == 1 ? " hour, " : " hours, ") <<
           minutes << (minutes == 1 ? " minute, " : " minutes, ") <<
           seconds << (seconds == 1 ? " second" : " seconds");
    return stream;
}

// Main code
void watchTimers() {
    while (running) {
        for (auto& timer : timers) {
            if (timer.second.done) {
                if (timer.second.coolDown->start + timer.second.coolDown->length < std::chrono::system_clock::now()) {
                    delete timer.second.coolDown;
                    timer.second.coolDown = nullptr;
                    timer.second.start = std::chrono::system_clock::now();
                    timer.second.done = false;
                    std::cout << "\rTimer '" << timer.first << "' was reset automatically." << std::endl << "> " << std::flush;
                }
            } else if (timer.second.start + timer.second.length < std::chrono::system_clock::now()) {
                timer.second.done = true;
                timer.second.coolDown = new Timer { std::chrono::minutes(15), std::chrono::system_clock::now() };
                std::cout << "\rTimer '" << timer.first << "' has finished! Beginning 15 minute cool down." << std::endl << "> " << std::flush;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

std::vector<std::string> readCommand() {
    std::cout << "> " << std::flush;
    std::string command;
    std::getline(std::cin, command);
    return split(command, " ");
}

void saveAndExit() {
#ifdef _WIN32
    fs::path path = getenv("APPDATA");
    path.append("fort.txt");
#else
    fs::path path = getenv("HOME");
    path.append("fort.txt");
#endif
    std::ofstream file(path);
    for (auto& timer : timers) {
        file << timer.first << " " << timer.second.length.count() << " " <<
            std::chrono::system_clock::to_time_t(timer.second.start) << '\n';
    }
    file << std::flush;
    std::exit(0);
}

void load() {
#ifdef _WIN32
    fs::path path = getenv("APPDATA");
    path.append("fort.txt");
#else
    fs::path path = getenv("HOME");
    path.append("fort.txt");
#endif
    std::ifstream file(path);
    if (!file.good()) return;
    std::string line;
    while (std::getline(file, line)) {
        if (file.eof()) break;
        std::vector<std::string> parts = split(line, " ");
        Timer timer;
        timer.length = std::chrono::seconds(std::stoi(parts[1]));
        timer.start = std::chrono::system_clock::from_time_t(std::stoi(parts[2]));
        timers[parts[0]] = timer;
    }
}

std::chrono::duration<double> parseTime(const std::string &time) {
    std::vector<std::string> parts = split(time, ":");
    int toReturn[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; i++) {
        if (i < parts.size()) toReturn[i + (3 - parts.size())] = std::stoi(parts[i]);
    }
    return std::chrono::seconds(toReturn[2]) +
           std::chrono::minutes(toReturn[1]) + std::chrono::hours(toReturn[0]);
}

int main() {
    signal(SIGTERM, [](int) { running = false; saveAndExit(); });

    std::thread watchThread(watchTimers);

    load();

    std::cout << "Welcome to fort" << std::endl;
    std::cout << "Enter a command. Type 'help' for a list of commands." << std::endl;
    std::vector<std::string> command;
    while ((command = readCommand())[0] != "exit") {
        if (command[0] == "help") {
            std::cout << "Available commands:" << std::endl;
            std::cout << "    help: print this menu" << std::endl;
            std::cout << "    exit: exit fort (stops all running timers)" << std::endl;
            std::cout << "    create <name> <length>: start a new timer" << std::endl;
            std::cout << "    delete <name>: delete a timer" << std::endl;
            std::cout << "    reset <name>: start a timer again from 0" << std::endl;
            std::cout << "    query [name]: get remaining time" << std::endl;
        } else if (command[0] == "create") {
            std::string name;
            std::chrono::duration<double> length {};
            if (command.size() == 2) {
                name = command[1];
                std::string lengthStr;
                std::cout << "Enter a length for the timer (HH:MM:SS): " << std::flush;
                std::getline(std::cin, lengthStr);
                length = parseTime(lengthStr);
            } else if (command.size() == 3) {
                name = command[1];
                length = parseTime(command[2]);
            } else {
                std::cout << "Enter a name for the timer: " << std::flush;
                std::string nameTmp;
                std::getline(std::cin, nameTmp);
                name = nameTmp;
                std::string lengthStr;
                std::cout << "Enter a length for the timer (HH:MM:SS): " << std::flush;
                std::getline(std::cin, lengthStr);
                length = parseTime(lengthStr);
            }
            if (length > std::chrono::hours(17)) {
                std::cout << "Timer length cannot be greater than 17 hours" << std::endl;
                continue;
            }
            if (timers.find(name) != timers.end()) {
                std::cout << "A timer with that name already exists" << std::endl;
                continue;
            }
            Timer timer { length, std::chrono::system_clock::now() };
            timers[name] = timer;
            std::cout << "Timer '" << name << "' created (" << length << ")" << std::endl;
        } else if (command[0] == "delete") {
            if (command.size() == 2) {
                if (timers.find(command[1]) != timers.end()) {
                    timers.erase(command[1]);
                    std::cout << "Timer '" << command[1] << "' deleted" << std::endl;
                } else {
                    std::cout << "Timer '" << command[1] << "' not found" << std::endl;
                }
            } else {
                std::cout << "Enter the name of the timer: " << std::flush;
                std::string name;
                std::getline(std::cin, name);
                if (timers.find(name) != timers.end()) {
                    timers.erase(name);
                    std::cout << "Timer '" << name << "' deleted" << std::endl;
                } else {
                    std::cout << "Timer '" << name << "' not found" << std::endl;
                }
            }
        } else if (command[0] == "query") {
            if (command.size() == 2) {
                if (timers.find(command[1]) != timers.end()) {
                    std::cout << timers[command[1]].length -
                        (std::chrono::system_clock::now() - timers[command[1]].start) << " remaining" << std::endl;
                } else {
                    std::cout << "Timer '" << command[1] << "' not found" << std::endl;
                }
            } else {
                if (timers.empty()) {
                    std::cout << "No timers to display" << std::endl;
                    continue;
                }
                for (auto& timer : timers) {
                    std::cout<< timer.first << ": " << timer.second.length -
                        (std::chrono::system_clock::now() - timer.second.start) << " remaining" << std::endl;
                }
            }
        } else if (command[0] == "reset") {
            if (command.size() == 2) {
                if (timers.find(command[1]) != timers.end()) {
                    delete timers[command[1]].coolDown;
                    timers[command[1]].coolDown = nullptr;
                    timers[command[1]].start = std::chrono::system_clock::now();
                    std::cout << "Timer '" << command[1] << "' reset" << std::endl;
                } else {
                    std::cout << "Timer '" << command[1] << "' not found" << std::endl;
                }
            } else {
                std::cout << "Enter the name of the timer: " << std::flush;
                std::string name;
                std::getline(std::cin, name);
                if (timers.find(name) != timers.end()) {
                    delete timers[command[1]].coolDown;
                    timers[command[1]].coolDown = nullptr;
                    timers[command[1]].start = std::chrono::system_clock::now();
                    timers[command[1]].done = false;
                    std::cout << "Timer '" << name << "' reset" << std::endl;
                } else {
                    std::cout << "Timer '" << name << "' not found" << std::endl;
                }
            }
        } else {
            std::cout << "Unknown command: " << command[0] << std::endl;
        }
    }

    running = false;
    saveAndExit();
}
