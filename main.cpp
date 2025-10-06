#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <locale>
#include <codecvt>

#ifdef _WIN32
#include <windows.h>
#endif

class FileSizeMonitor {
private:
    struct FileConfig {
        std::string path;
        double max_size_bytes;
        std::string original_size_str;
        std::string action;
        bool has_warned;
        std::string type; // Add type field to indicate the item type (file or path)
    };

    std::vector<FileConfig> file_configs;
    bool running = false;

public:
    // Convert UTF-8 string to wide string (for Windows)
    static std::wstring utf8_to_wide(const std::string& utf8_str) {
        #ifdef _WIN32
        if (utf8_str.empty()) return {};
        int wide_size = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, nullptr, 0);
        if (wide_size == 0) return {};
        std::wstring wide_str(wide_size, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, &wide_str[0], wide_size);
        return wide_str;
        #else
        // For non-Windows systems, use code convert (though it's deprecated in C++17)
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.from_bytes(utf8_str);
        #endif
    }

    // Convert wide string to UTF-8 string
    static std::string wide_to_utf8(const std::wstring& wide_str) {
        #ifdef _WIN32
        if (wide_str.empty()) return {};
        int utf8_size = WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (utf8_size == 0) return {};
        std::string utf8_str(utf8_size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wide_str.c_str(), -1, &utf8_str[0], utf8_size, nullptr, nullptr);
        return utf8_str;
        #else
        std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
        return converter.to_bytes(wide_str);
        #endif
    }

    // Parse size string
    static double parseSizeString(const std::string& size_str) {
        if (size_str.empty()) return 0.0;

        std::string num_str = size_str;
        std::string unit;

        // Separate number and unit
        size_t i = 0;
        while (i < num_str.length() && (std::isdigit(num_str[i]) || num_str[i] == '.')) {
            i++;
        }

        if (i < num_str.length()) {
            unit = num_str.substr(i);
            num_str = num_str.substr(0, i);
        }

        double size_value;
        try {
            size_value = std::stod(num_str);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Invalid size format '" << num_str << "', using 0 as default" << std::endl;
            return 0.0;
        }

        // Convert to lowercase for comparison
        std::string unit_lower = unit;
        std::transform(unit_lower.begin(), unit_lower.end(), unit_lower.begin(), ::tolower);

        // Convert based on unit
        if (unit_lower.empty() || unit_lower == "b") {
            return size_value;
        } else if (unit_lower == "k" || unit_lower == "kb") {
            return size_value * 1024.0;
        } else if (unit_lower == "m" || unit_lower == "mb") {
            return size_value * 1024.0 * 1024.0;
        } else if (unit_lower == "g" || unit_lower == "gb") {
            return size_value * 1024.0 * 1024.0 * 1024.0;
        } else if (unit_lower == "t" || unit_lower == "tb") {
            return size_value * 1024.0 * 1024.0 * 1024.0 * 1024.0;
        } else {
            std::cerr << "Warning: Unknown unit '" << unit << "', using bytes as default" << std::endl;
            return size_value;
        }
    }

    // Parse action
    static std::string parseAction(const std::string& action_str) {
        std::string action_lower = action_str;
        std::transform(action_lower.begin(), action_lower.end(), action_lower.begin(), ::tolower);

        if (action_lower == "warn" || action_lower == "trash") {
            return action_lower;
        } else {
            std::cerr << "Warning: Unknown action '" << action_str << "', using 'warn' as default" << std::endl;
            return "warn";
        }
    }

    // Read TSV file with encoding handling
    bool loadConfig(const std::string& tsv_path) {
        // Open file in binary mode to handle encoding properly
        std::ifstream file(tsv_path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Cannot open file: " << tsv_path << std::endl;
            return false;
        }

        // Read the entire file content
        std::string content;
        file.seekg(0, std::ios::end);
        content.resize(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read(&content[0], content.size());
        file.close();

        // Remove UTF-8 BOM if present
        if (content.size() >= 3 &&
            static_cast<unsigned char>(content[0]) == 0xEF &&
            static_cast<unsigned char>(content[1]) == 0xBB &&
            static_cast<unsigned char>(content[2]) == 0xBF) {
            content = content.substr(3);
        }

        // Process each line
        std::istringstream iss(content);
        std::string line;
        int line_num = 0;
        bool first_line = true;

        while (std::getline(iss, line)) {
            line_num++;

            // Remove any carriage return characters (for Windows line endings)
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            // Skip empty lines
            if (line.empty()) {
                continue;
            }

            // Skip first line (header)
            if (first_line) {
                first_line = false;
                continue;
            }

            // Parse TSV line format: fields separated by tabs
            std::vector<std::string> fields;
            std::istringstream lineStream(line);
            std::string field;
            
            while (std::getline(lineStream, field, '\t')) {
                // Trim whitespace from field
                field.erase(0, field.find_first_not_of(" \t"));
                field.erase(field.find_last_not_of(" \t") + 1);
                fields.push_back(field);
            }

            if (fields.size() < 3) {
                std::cerr << "Warning: Line " << line_num << " has incorrect format, skipping. Fields found: " << fields.size() << std::endl;
                std::cerr << "Line content: " << line << std::endl;
                continue;
            }

            std::string file_path = fields[0];
            std::string size_str = fields[1];
            std::string action_str = fields[2];
            std::string type = "file"; // Default type is file
        
            // If there's a type column, read its value
            if (fields.size() >= 4) {
                type = fields[3];
                // Convert to lowercase for comparison
                std::transform(type.begin(), type.end(), type.begin(), ::tolower);
                // Ensure type is either file or path
                if (type != "file" && type != "path") {
                    std::cerr << "Warning: Invalid type '" << type << "' in line " << line_num 
                              << ", using 'file' as default" << std::endl;
                    type = "file";
                }
            }

            // Normalize file path - replace forward slashes with backslashes on Windows
            #ifdef _WIN32
            std::replace(file_path.begin(), file_path.end(), '/', '\\');
            #endif

            double max_bytes = parseSizeString(size_str);
            std::string action = parseAction(action_str);

            file_configs.push_back({file_path, max_bytes, size_str, action, false, type});

            std::cout << "Loaded config: " << file_path << " -> " << size_str
                      << " [" << action << "] (type: " << type << ", " << max_bytes << " bytes)" << std::endl;
        }

        std::cout << "Successfully loaded " << file_configs.size() << " file configurations" << std::endl;
        return !file_configs.empty();
    }

    // Get current file size with proper encoding handling
    static double getCurrentFileSize(const std::string& file_path) {
        try {
            #ifdef _WIN32
            // On Windows, convert UTF-8 to wide string for filesystem operations
            std::wstring wide_path = utf8_to_wide(file_path);
            std::filesystem::path fs_path(wide_path);
            #else
            // On other systems, use the path directly
            std::filesystem::path fs_path(file_path);
            #endif

            if (!std::filesystem::exists(fs_path)) {
                std::cerr << "File does not exist: " << file_path << std::endl;
                return -1.0;
            }

            if (!std::filesystem::is_regular_file(fs_path)) {
                std::cerr << "Path is not a regular file: " << file_path << std::endl;
                return -1.0;
            }

            return static_cast<double>(std::filesystem::file_size(fs_path));
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Filesystem error for '" << file_path << "': " << e.what() << std::endl;
            return -1.0;
        } catch (const std::exception& e) {
            std::cerr << "Error getting file size for '" << file_path << "': " << e.what() << std::endl;
            return -1.0;
        }
    }

    // Structure for directory size calculation results
    struct DirectorySizeResult {
        uintmax_t total_size;  // Total size in bytes
        size_t file_count;     // Number of files
        size_t folder_count;   // Number of subdirectories
    };

    // Get directory size with proper encoding handling
    static double getDirectorySize(const std::string& dir_path) {
        DirectorySizeResult result = calculateDirectorySize(dir_path);
        return static_cast<double>(result.total_size);
    }

    // Internal implementation to calculate directory size and return complete statistics
    static DirectorySizeResult calculateDirectorySize(const std::string& dir_path) {
        DirectorySizeResult result{0, 0, 0};
        
        if (!std::filesystem::exists(dir_path)) {
            std::cerr << "Path does not exist: " << dir_path << std::endl;
            return result;
        }
        
        try {
            auto options = std::filesystem::directory_options::none;
                
            for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path, options)) {
                try {
                    if (std::filesystem::is_regular_file(entry)) {
                        result.total_size += std::filesystem::file_size(entry);
                        result.file_count++;
                    } else if (std::filesystem::is_directory(entry)) {
                        result.folder_count++;
                    }
                } catch (const std::filesystem::filesystem_error& ex) {
                    // Ignore permission issues or other errors, continue with other files
                    std::cerr << "Error: " << ex.what() << std::endl;
                }
            }
        } catch (const std::filesystem::filesystem_error& ex) {
            std::cerr << "Error: " << ex.what() << std::endl;
        }
        
        return result;
    }

    // Delete file with proper encoding handling using system command
    static bool deleteFileWithSystem(const std::string& file_path) {
        try {
            #ifdef _WIN32
            // On Windows, use direct del command without cmd.exe
            std::string command = "del /f /q \"" + file_path + "\"";
            int result = system(command.c_str());
            if (result == 0) {
                std::cout << "Successfully deleted file using system command: " << file_path << std::endl;
                return true;
            } else {
                std::cerr << "Failed to delete file using system command: " << file_path << " (Error code: " << result << ")" << std::endl;
                return false;
            }
            #else
            // On other systems, use rm command
            std::string command = "rm -f '" + file_path + "'";
            int result = system(command.c_str());
            if (result == 0) {
                std::cout << "Successfully deleted file using system command: " << file_path << std::endl;
                return true;
            } else {
                std::cerr << "Failed to delete file using system command: " << file_path << " (Error code: " << result << ")" << std::endl;
                return false;
            }
            #endif
        } catch (const std::exception& e) {
            std::cerr << "Exception while deleting file with system command '" << file_path << "': " << e.what() << std::endl;
            return false;
        }
    }

    // Delete directory and create empty directory at the same location using system commands
    static bool deleteDirectoryWithSystem(const std::string& dir_path) {
        try {
            #ifdef _WIN32
            // On Windows, first delete all contents recursively without cmd.exe
            std::string delete_command = "rd /s /q \"" + dir_path + "\"";
            int delete_result = system(delete_command.c_str());
            if (delete_result != 0) {
                std::cerr << "Failed to delete directory using system command: " << dir_path << " (Error code: " << delete_result << ")" << std::endl;
                return false;
            }
            std::cout << "Successfully deleted directory using system command: " << dir_path << std::endl;

            // Then create empty directory without cmd.exe
            std::string create_command = "md \"" + dir_path + "\"";
            int create_result = system(create_command.c_str());
            if (create_result == 0) {
                std::cout << "Successfully created empty directory at: " << dir_path << std::endl;
                return true;
            } else {
                std::cerr << "Failed to create empty directory at: " << dir_path << " (Error code: " << create_result << ")" << std::endl;
                return false;
            }
            #else
            // On other systems, use rm -rf to delete and mkdir to create
            std::string delete_command = "rm -rf '" + dir_path + "'";
            int delete_result = system(delete_command.c_str());
            if (delete_result != 0) {
                std::cerr << "Failed to delete directory using system command: " << dir_path << " (Error code: " << delete_result << ")" << std::endl;
                return false;
            }
            std::cout << "Successfully deleted directory using system command: " << dir_path << std::endl;

            std::string create_command = "mkdir -p '" + dir_path + "'";
            int create_result = system(create_command.c_str());
            if (create_result == 0) {
                std::cout << "Successfully created empty directory at: " << dir_path << std::endl;
                return true;
            } else {
                std::cerr << "Failed to create empty directory at: " << dir_path << " (Error code: " << create_result << ")" << std::endl;
                return false;
            }
            #endif
        } catch (const std::exception& e) {
            std::cerr << "Exception while deleting directory with system command '" << dir_path << "': " << e.what() << std::endl;
            return false;
        }
    }

    // Handle oversized file
    static void handleOversizeFile(FileConfig& config, double current_size) {
        std::cout << "File exceeds size limit: " << config.path << std::endl;
        std::cout << "  Current size: " << formatFileSize(current_size)
                  << " | Limit: " << config.original_size_str
                  << " | Action: " << config.action << std::endl;

        if (config.action == "trash") {
            std::cout << "Deleting file..." << std::endl;
            if (deleteFileWithSystem(config.path)) {
                config.has_warned = true;
            }
        } else {
            // warn action, just log warning
            if (!config.has_warned) {
                std::cout << "Warning: File " << config.path << " has exceeded size limit!" << std::endl;
                config.has_warned = true;
            }
        }
    }

    // Handle oversized file or directory
    static void handleOversizePath(FileConfig& config, double current_size) {
        std::cout << "Directory exceeds size limit: " << config.path << std::endl;
        std::cout << "  Current size: " << formatFileSize(current_size)
                  << " | Limit: " << config.original_size_str
                  << " | Action: " << config.action << std::endl;

        if (config.action == "trash") {
            std::cout << "Deleting directory and creating empty directory..." << std::endl;
            if (deleteDirectoryWithSystem(config.path)) {
                config.has_warned = true;
            }
        } else {
            // warn action, just log warning
            if (!config.has_warned) {
                std::cout << "Warning: Directory " << config.path << " has exceeded size limit!" << std::endl;
                // Call calculateDirectorySize to get detailed statistics
                DirectorySizeResult result = calculateDirectorySize(config.path);
                std::cout << "  Detailed info: " << result.file_count << " files, " 
                          << result.folder_count << " folders" << std::endl;
                config.has_warned = true;
            }
        }
    }

    // Check all file sizes - process file type first
    void checkAllFiles() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);

        std::cout << "\nCheck time: " << std::ctime(&now_time);

        // Process FILE type configurations
        std::cout << "\nProcessing FILE type configurations:" << std::endl;
        for (auto& config : file_configs) {
            // Process FILE type configuration
            if (config.type != "file") {
                continue;
            }

            double current_size = getCurrentFileSize(config.path);

            if (current_size < 0) {
                // File doesn't exist or error accessing it
                config.has_warned = false; // Reset warning status
                continue;
            }

            std::cout << "File: " << config.path
                      << " | Current: " << formatFileSize(current_size)
                      << " | Limit: " << config.original_size_str
                      << " | Action: " << config.action
                      << " | Status: ";

            if (current_size > config.max_size_bytes) {
                std::cout << "EXCEEDS LIMIT!" << std::endl;
                handleOversizeFile(config, current_size);
            } else {
                double percentage = (current_size / config.max_size_bytes) * 100.0;
                std::cout << std::fixed << std::setprecision(2) << percentage << "%" << std::endl;
                config.has_warned = false; // Reset warning status
            }
        }

        // Process PATH type configurations with the same output format as FILE type
        std::cout << "\nProcessing PATH type configurations:" << std::endl;
        for (auto& config : file_configs) {
            // Process PATH type configuration
            if (config.type != "path") {
                continue;
            }

            double current_size = getDirectorySize(config.path);

            if (current_size <= 0) {
                // Directory doesn't exist or error accessing it
                config.has_warned = false; // Reset warning status
                continue;
            }

            std::cout << "Directory: " << config.path
                      << " | Current: " << formatFileSize(current_size)
                      << " | Limit: " << config.original_size_str
                      << " | Action: " << config.action
                      << " | Status: ";

            if (current_size > config.max_size_bytes) {
                std::cout << "EXCEEDS LIMIT!" << std::endl;
                handleOversizePath(config, current_size);
            } else {
                double percentage = (current_size / config.max_size_bytes) * 100.0;
                std::cout << std::fixed << std::setprecision(2) << percentage << "%" << std::endl;
                config.has_warned = false; // Reset warning status
            }
        }
    }

    // Format file size for display
    static std::string formatFileSize(double size_bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit_index = 0;
        double size = size_bytes;

        while (size >= 1024.0 && unit_index < 4) {
            size /= 1024.0;
            unit_index++;
        }

        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
        return ss.str();
    }

    // Start monitoring
    void startMonitoring(int check_interval_seconds = 5) {
        running = true;
        std::cout << "Starting file size monitoring, check interval: " << check_interval_seconds << " seconds" << std::endl;
        std::cout << "Press Ctrl+C to stop monitoring" << std::endl;

        while (running) {
            checkAllFiles();

            // Wait for specified interval
            for (int i = 0; i < check_interval_seconds && running; i++) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    }

    // Stop monitoring
    void stopMonitoring() {
        running = false;
    }
};

// Signal handler
#ifdef __linux__
#include <csignal>
FileSizeMonitor* global_monitor = nullptr;

void signalHandler(int signal) {
    if (signal == SIGINT) {
        std::cout << "\nReceived interrupt signal, stopping monitoring..." << std::endl;
        if (global_monitor) {
            global_monitor->stopMonitoring();
        }
    }
}
#endif

int main(int argc, char* argv[]) {
    std::string tsv_file = "StatList.tsv";

    // Allow specifying TSV file via command line argument
    if (argc > 1) {
        // Ensure proper handling of Chinese paths in command line arguments
        #ifdef _WIN32
        // On Windows, argv might be in ANSI encoding, need to convert to UTF-8
        int len = MultiByteToWideChar(CP_ACP, 0, argv[1], -1, nullptr, 0);
        if (len > 0) {
            std::wstring wide_path(len, 0);
            MultiByteToWideChar(CP_ACP, 0, argv[1], -1, &wide_path[0], len);
            
            // Convert from wide string to UTF-8
            FileSizeMonitor monitor_temp;
            tsv_file = FileSizeMonitor::wide_to_utf8(wide_path);
        }
        #else
        tsv_file = argv[1];
        #endif
    }

    FileSizeMonitor monitor;

    // Load configuration file
    if (!monitor.loadConfig(tsv_file)) {
        std::cerr << "Failed to load configuration file, program exiting" << std::endl;
        return 1;
    }

    // Set up signal handling
#ifdef __linux__
    global_monitor = &monitor;
    std::signal(SIGINT, signalHandler);
#endif

    std::cout << "\nSelect monitoring mode:" << std::endl;
    std::cout << "1. Regular check mode" << std::endl;
    std::cout << "2. Custom check interval" << std::endl;
    std::cout << "Enter your choice (1 or 2): ";

    int choice;
    std::cin >> choice;

    if (choice == 1) {
        // Regular check mode
        monitor.startMonitoring(5); // Check every 5 seconds
    } else if (choice == 2) {
        std::cout << "Enter check interval (seconds): ";
        int interval;
        std::cin >> interval;
        if (interval < 1) interval = 1;
        monitor.startMonitoring(interval);
    } else {
        std::cout << "Invalid choice, using default regular check mode" << std::endl;
        monitor.startMonitoring(5);
    }

    return 0;
}