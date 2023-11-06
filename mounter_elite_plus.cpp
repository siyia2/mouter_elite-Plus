#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <condition_variable>
#include <dirent.h>
#include <filesystem>
#include <functional>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <mutex>
#include <omp.h>
#include <readline/readline.h>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <unordered_set>
#include <vector>
#include <queue>

// Cache Varables \\

const std::string cacheDirectory = std::string(std::getenv("HOME")) + "/.cache"; // Construct the full path to the cache directory
const std::string cacheFileName = "iso_cache.txt";;
const uintmax_t maxCacheSize = 50 * 1024 * 1024; // 50MB
std::vector<std::string> binImgFilesCache; // Memory cached binImgFiles here
std::vector<std::string> mdfMdsFilesCache; // Memory cached mdfImgFiles here



//	SANITISATION AND STRING STUFF	\\

std::string shell_escape(const std::string& s) {
    // Estimate the maximum size of the escaped string
    size_t max_size = s.size() * 4;  // Assuming every character might need escaping
    std::string escaped_string;
    escaped_string.reserve(max_size);

    for (char c : s) {
        if (c == '\'') {
            escaped_string += "'\\''";
        } else {
            escaped_string += c;
        }
    }

    return "'" + escaped_string + "'";
}

// Function to read a line of input using readline
std::string readInputLine(const std::string& prompt) {
    char* input = readline(prompt.c_str());

    if (input) {
        std::string inputStr(input);
        free(input); // Free the allocated memory for the input
        return inputStr;
    }

    return ""; // Return an empty string if readline fails
}

// MULTITHREADING STUFF
std::mutex mountMutex; // Mutex for thread safety
std::mutex mtx;

namespace fs = std::filesystem;

//	Function prototypes	\\

//	stds
std::vector<std::string> findBinImgFiles(const std::string& directory);
std::vector<std::string> findMdsMdfFiles(const std::string& directory);
std::string chooseFileToConvert(const std::vector<std::string>& files);
//	bools
bool directoryExists(const std::string& path);
bool iequals(const std::string& a, const std::string& b);
bool allSelectedFilesExistOnDisk(const std::vector<std::string>& selectedFiles);
bool isMdf2IsoInstalled();
bool fileExists(const std::string& path);

//	voids
void convertMDFToISO(const std::string& inputPath);
void convertMDFsToISOs(const std::vector<std::string>& inputPaths, int numThreads);
void processMDFFilesInRange(int start, int end);
void select_and_convert_files_to_iso_mdf();
void processMdfMdsFilesInRange(const std::vector<std::string>& mdfMdsFiles, int start, int end);
void mountISO(const std::vector<std::string>& isoFiles);
void listAndMountISOs();
void unmountISO(const std::string& isoDir);
void listMountedISOs();
void unmountISOs();
void cleanAndUnmountAllISOs();
void convertBINsToISOs();
void listMode();
void select_and_mount_files_by_number();
void select_and_convert_files_to_iso();
void print_ascii();
void screen_clear();
void print_ascii();
void parallelTraverse(const std::filesystem::path& path, std::vector<std::string>& isoFiles, std::mutex& mtx);
void refreshCacheForDirectory(const std::string& path, std::vector<std::string>& allIsoFiles);
void manualRefreshCache();
void removeNonExistentPathsFromCacheWithOpenMP();

std::string directoryPath;					// Declare directoryPath here

int main() {
    bool exitProgram = false;
    std::string choice;

    while (!exitProgram) {
        bool returnToMainMenu = false;
        std::system("clear");
        print_ascii();
        // Display the main menu options
        std::cout << "Menu Options:" << std::endl;
        std::cout << "1. Mount" << std::endl;
        std::cout << "2. Unmount" << std::endl;
        std::cout << "3. Clean&Unmount" << std::endl;
        std::cout << "4. Conversion Tools" << std::endl;
        std::cout << "5. Refresh ISO Cache" << std::endl;
        std::cout << "6. List Mountpoints" << std::endl;
        std::cout << "7. Exit the Program" << std::endl;

        // Prompt for the main menu choice
        //std::cin.clear();

        std::cout << " " << std::endl;
        char* input = readline("\033[94mEnter a choice:\033[0m ");
        std::cout << " " << std::endl;
        if (!input) {
            break; // Exit the program if readline returns NULL (e.g., on EOF or Ctrl+D)
        }

        std::string choice(input);
        free(input);

        if (choice == "1") {
            std::system("clear");
            select_and_mount_files_by_number();
            std::system("clear");
        } else {
            switch (choice[0]) {
            case '2':
                std::system("clear");
                unmountISOs();
                std::cout << "Press Enter to continue...";
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::system("clear");
                break;
            case '3':
                cleanAndUnmountAllISOs();
                std::cout << "Press Enter to continue...";
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::system("clear");
                break;
            case '4':
                while (!returnToMainMenu) {
                    std::cout << "1. Convert to ISO (BIN2ISO)" << std::endl;
                    std::cout << "2. Convert to ISO (MDF2ISO)" << std::endl;
                    std::cout << "3. Back to Main Menu" << std::endl;
                    std::cout << " " << std::endl;
                    char* submenu_input = readline("\033[94mEnter a choice:\033[0m ");

                    if (!submenu_input) {
                        break; // Exit the submenu if readline returns NULL
                    }

                    std::string submenu_choice(submenu_input);
                    free(submenu_input);

                    switch (submenu_choice[0]) {
                    case '1':
                        std::system("clear");
                        select_and_convert_files_to_iso();
                        break;
                    case '2':
                        std::system("clear");
                        select_and_convert_files_to_iso_mdf();
                        break;
                    case '3':
                        returnToMainMenu = true;  // Set the flag to return to the main menu
                        break; // Go back to the main menu
                    default:
                        std::cout << "\033[31mInvalid choice. Please enter 1, 2, or 3.\033[0m" << std::endl;
                        break;
                    }
                }
                break;
            case '5':
				removeNonExistentPathsFromCacheWithOpenMP();
                manualRefreshCache();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::system("clear");
                break;
            case '6':
                listMountedISOs();
                std::cout << "Press Enter to continue...";
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::system("clear");
                break;
            case '7':
                exitProgram = true; // Exit the program
                std::cout << "Exiting the program..." << std::endl;
                break;
            default:
                std::cout << "\033[31mInvalid choice. Please enter 1, 2, 3, 4, 5, or 6.\033[0m" << std::endl;
                break;
            }
        }
    }

    return 0;
}

// ... Function definitions ...

void print_ascii() {
    // Display ASCII art \\

    const char* greenColor = "\x1B[32m";
    const char* resetColor = "\x1B[0m"; // Reset color to default

    std::cout << greenColor << R"( _____            ___  _____  _____     ___  __   __   ___   _____  _____     __   __   ___  __   __  _   _  _____  _____  ____         ____
|  ___)    /\    (   )(_   _)|  ___)   (   )|  \ /  | / _ \ |  ___)|  ___)   |  \ /  | / _ \(_ \ / _)| \ | |(_   _)|  ___)|  _ \       (___ \     _      _   
| |_      /  \    | |   | |  | |_       | | |   v   || |_| || |    | |_      |   v   || | | | \ v /  |  \| |  | |  | |_   | |_) )  _  __ __) )  _| |_  _| |_ 
|  _)    / /\ \   | |   | |  |  _)      | | | |\_/| ||  _  || |    |  _)     | |\_/| || | | |  | |   |     |  | |  |  _)  |  __/  | |/ // __/  (_   _)(_   _)
| |___  / /  \ \  | |   | |  | |___     | | | |   | || | | || |    | |___    | |   | || |_| |  | |   | |\  |  | |  | |___ | |     | / /| |___    |_|    |_|  
|_____)/_/    \_\(___)  |_|  |_____)   (___)|_|   |_||_| |_||_|    |_____)   |_|   |_| \___/   |_|   |_| \_|  |_|  |_____)|_|     |__/ |_____)               
                                                                                                                                                             
                                                                                                                                                                       
                                                                                                                                         )" << resetColor << '\n';

}

//	CACHE STUFF \\


// Function to check if a file or directory exists
bool fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

// Function to remove non-existent paths from the cache with OpenMP (up to 4 cores)
void removeNonExistentPathsFromCacheWithOpenMP() {
    std::string cacheFilePath = std::string(getenv("HOME")) + "/.cache/iso_cache.txt";
    std::vector<std::string> cache;
    std::ifstream cacheFile(cacheFilePath);
    std::string line;

    if (!cacheFile) {
        std::cerr << "Error: Unable to open cache file." << std::endl;
        return;
    }

    while (std::getline(cacheFile, line)) {
        cache.push_back(line);
    }

    cacheFile.close();

    // Set the number of threads to a maximum of 4
    omp_set_num_threads(4);

    std::vector<std::string> retainedPaths;

    #pragma omp parallel for
    for (int i = 0; i < cache.size(); i++) {
        if (fileExists(cache[i])) {
            #pragma omp critical
            retainedPaths.push_back(cache[i]);
        }
    }

    std::ofstream updatedCacheFile(cacheFilePath);

    if (!updatedCacheFile) {
        std::cerr << "Error: Unable to open cache file for writing." << std::endl;
        return;
    }

    for (const std::string& path : retainedPaths) {
        updatedCacheFile << path << std::endl;
    }

    updatedCacheFile.close();
}


// Set default cache dir
std::string getHomeDirectory() {
    const char* homeDir = getenv("HOME");
    if (homeDir) {
        return std::string(homeDir);
    }
    return "";
}

// Load cache
std::vector<std::string> loadCache() {
    std::vector<std::string> isoFiles;
    std::string cacheFilePath = getHomeDirectory() + "/.cache/iso_cache.txt";
    std::ifstream cacheFile(cacheFilePath);

    if (cacheFile.is_open()) {
        std::string line;
        while (std::getline(cacheFile, line)) {
            // Check if the line is not empty
            if (!line.empty()) {
                isoFiles.push_back(line);
            }
        }
        cacheFile.close();

        // Remove duplicates from the loaded cache
        std::sort(isoFiles.begin(), isoFiles.end());
        isoFiles.erase(std::unique(isoFiles.begin(), isoFiles.end()), isoFiles.end());
    }

    return isoFiles;
}
// Save cache
void saveCache(const std::vector<std::string>& isoFiles) {
    std::filesystem::path cachePath = cacheDirectory;
    cachePath /= cacheFileName;

    // Load the existing cache
    std::vector<std::string> existingCache = loadCache();

    // Append new and unique entries to the existing cache
    for (const std::string& iso : isoFiles) {
        if (std::find(existingCache.begin(), existingCache.end(), iso) == existingCache.end()) {
            existingCache.push_back(iso);
        }
    }

    // Open the cache file in write mode (truncating it)
    std::ofstream cacheFile(cachePath);
    if (cacheFile.is_open()) {
        for (const std::string& iso : existingCache) {
            cacheFile << iso << "\n";
        }
        cacheFile.close();
    } else {
        std::cerr << "Error: Could not open cache file for writing." << std::endl;
    }
}

// Check if all selected files are still present on disk
bool allSelectedFilesExistOnDisk(const std::vector<std::string>& selectedFiles) {
    for (const std::string& file : selectedFiles) {
        if (!std::filesystem::exists(file)) {
            return false;
        }
    }
    return true;
}

// Function to refresh the cache for a single directory
void refreshCacheForDirectory(const std::string& path, std::vector<std::string>& allIsoFiles) {
    std::cout << "Processing directory path: " << path << std::endl;
    std::vector<std::string> newIsoFiles;
    
    // Perform the cache refresh for the directory (e.g., using parallelTraverse)
    parallelTraverse(path, newIsoFiles, mtx);
    
    // Lock the mutex to protect the shared 'allIsoFiles' vector
    std::lock_guard<std::mutex> lock(mtx);
    
    // Append the new entries to the shared vector
    allIsoFiles.insert(allIsoFiles.end(), newIsoFiles.begin(), newIsoFiles.end());
    
    std::cout << "Cache refreshed for directory: " << path << std::endl;
}

// Cache refresh function
void manualRefreshCache() {
    std::string inputLine = readInputLine("\033[94mEnter directory paths to manually refresh the cache (separated by spaces), or simply press enter to cancel:\033[0m ");
    
    if (inputLine.empty()) {
        std::cout << "Cache refresh canceled." << std::endl;
        return;
    }

    std::istringstream iss(inputLine);
    std::string path;
    std::vector<std::string> allIsoFiles; // Create a vector to combine results

    std::vector<std::thread> threads;
    
    while (iss >> path) {
        threads.emplace_back(std::thread(refreshCacheForDirectory, path, std::ref(allIsoFiles)));
    }
    
    // Wait for all threads to finish
    for (std::thread& t : threads) {
        t.join();
    }

    // Now, save the combined cache
    saveCache(allIsoFiles);
    std::cout << "Cache refreshed successfully." << std::endl;
}

//	MOUNT STUFF	\\

bool directoryExists(const std::string& path) {
    return fs::is_directory(path);
}

void mountIsoFile(const std::string& isoFile, std::map<std::string, std::string>& mountedIsos) {
    // Check if the ISO file is already mounted
    std::lock_guard<std::mutex> lock(mountMutex); // Lock to protect access to mountedIsos
    if (mountedIsos.find(isoFile) != mountedIsos.end()) {
        std::cout << "\e[1;31mALREADY MOUNTED\e[0m: ISO file '" << isoFile << "' is already mounted at '" << mountedIsos[isoFile] << "'.\033[0m" << std::endl;
        std::cout << "Press Enter to continue...";
        std::cin.get(); // Wait for the user to press Enter
        return;
    }

    // Use the filesystem library to extract the ISO file name
    fs::path isoPath(isoFile);
    std::string isoFileName = isoPath.stem().string(); // Remove the .iso extension

    std::string mountPoint = "/mnt/iso_" + isoFileName; // Use the modified ISO file name in the mount point with "iso_" prefix

    // Check if the mount point directory doesn't exist, create it
    if (!directoryExists(mountPoint)) {
        // Create the mount point directory
        std::string mkdirCommand = "sudo mkdir " + shell_escape(mountPoint);
        if (system(mkdirCommand.c_str()) != 0) {
            std::perror("\033[33mFailed to create mount point directory\033[0m");
            return;
        }

        // Mount the ISO file to the mount point
        std::string mountCommand = "sudo mount -o loop " + shell_escape(isoFile) + " " + shell_escape(mountPoint) + " 1> /dev/null ";
        if (system(mountCommand.c_str()) != 0) {
            std::perror("\033[31mFailed to mount ISO file\033[0m");
            std::cout << "Press Enter to continue...";
			std::cin.get(); // Wait for the user to press Enter
			std::system("clear");

            // Cleanup the mount point directory
            std::string cleanupCommand = "sudo rmdir " + shell_escape(mountPoint);
            if (system(cleanupCommand.c_str()) != 0) {
                std::perror("\033[33mFailed to clean up mount point directory\033[0m");
            }

            //std::cout << "Press Enter to continue...";
            //std::cin.get(); // Wait for the user to press Enter
            return;
        } else {
            //std::cout << "\033[32mISO file '" << isoFile << "' mounted at '" << mountPoint << "'.\033[0m" << std::endl;
            // Store the mount point in the map
            mountedIsos[isoFile] = mountPoint;
            
        }
    } else {
        // The mount point directory already exists, so the ISO is considered mounted
        std::cout << "\033[33mISO file '" << isoFile << "' is already mounted at '" << mountPoint << "'.\033[m" << std::endl;
        mountedIsos[isoFile] = mountPoint;
        std::cout << "Press Enter to continue...";
        std::cin.get(); // Wait for the user to press Enter
        std::system("clear");
    }
}

void mountISO(const std::vector<std::string>& isoFiles) {
    std::map<std::string, std::string> mountedIsos;
    int count = 1;
    std::vector<std::thread> threads;

    for (const std::string& isoFile : isoFiles) {
        // Limit the number of threads to a maximum of 4
        if (threads.size() >= 4) {
            for (auto& thread : threads) {
                thread.join();
            }
            threads.clear();
        }

        std::string IsoFile = (isoFile);
        threads.emplace_back(mountIsoFile, IsoFile, std::ref(mountedIsos));
    }

    // Join any remaining threads
    for (auto& thread : threads) {
        thread.join();
    }

}

bool fileExistsOnDisk(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
}

bool ends_with_iso(const std::string& str) {
    // Convert the string to lowercase for a case-insensitive comparison
    std::string lowercase = str;
    std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(), ::tolower);
    return lowercase.size() >= 4 && lowercase.compare(lowercase.size() - 4, 4, ".iso") == 0;
}

void select_and_mount_files_by_number() {
    std::vector<std::string> isoFiles = loadCache();

    if (isoFiles.empty()) {
        std::system("clear");
        std::cout << "\033[33mCache is empty. Please refresh the cache from the main menu.\033[0m" << std::endl;
        std::cout << "Press Enter to continue...";
        std::cin.get(); // Wait for the user to press Enter
        return;
    }

    // Filter isoFiles to include entries with ".iso" or ".ISO" extensions
    isoFiles.erase(std::remove_if(isoFiles.begin(), isoFiles.end(), [](const std::string& iso) {
        return !ends_with_iso(iso);
    }), isoFiles.end());

    if (isoFiles.empty()) {
        std::cout << "\033[33mNo more unmounted .iso files in the cache. Please refresh the cache from the main menu.\033[0m" << std::endl;
        return;
    }

    std::vector<std::string> mountedISOs;
    std::unordered_set<std::string> mountedSet(mountedISOs.begin(), mountedISOs.end());

    while (true) {
        // Remove already mounted files from the selection list
        isoFiles.erase(std::remove_if(isoFiles.begin(), isoFiles.end(), [&mountedSet](const std::string& iso) {
            return mountedSet.find(iso) != mountedSet.end();
        }), isoFiles.end());

        if (isoFiles.empty()) {
            std::cout << "\033[33mNo more unmounted .iso files in the cache. Please refresh the cache from the main menu.\033[0m" << std::endl;
            break;
        }

        std::cout << "\033[33m! IF EXPECTED ISO FILE IS NOT ON THE LIST, UPDATE CACHE FROM MAIN MENU !\n\033[0m" << std::endl;
        for (int i = 0; i < isoFiles.size(); i++) {
            std::cout << i + 1 << ". " << isoFiles[i] << std::endl;
        }
		
        std::string input;
        std::cout << "\033[94mChoose .iso files to mount (enter numbers 1 2 or ranges like '1-3', '00' to mount all, or press Enter to return):\033[0m ";
        std::getline(std::cin, input);
        std::system("clear");

        if (input.empty()) {
            std::cout << "Press Enter to Return" << std::endl;
            break;
        }

        if (input == "00") {
            // Mount all listed files
            for (const std::string& iso : isoFiles) {
                if (mountedSet.find(iso) == mountedSet.end()) {
                    // Check if the file exists on disk before mounting
                    if (fileExistsOnDisk(iso)) {
                        mountISO({iso});
                        mountedSet.insert(iso);
                    } else {
						// Clear the existing cache
						isoFiles.clear();
						parallelTraverse(directoryPath, isoFiles, mtx);
						saveCache(isoFiles);
                        std::cout << "\033[33mISO file '" << iso << "' does not exist on disk. Please refresh the cache from the main menu.\033[0m" << std::endl;
                        
                    }
                } else {
                    std::cout << "\033[33mISO file '" << iso << "' is already mounted.\033[0m" << std::endl;
                }
            }
        } else {
            std::istringstream iss(input);
            std::string token;

            while (std::getline(iss, token, ' ')) {
                if (token.find('-') != std::string::npos) {
                    // Handle a range
                    size_t dashPos = token.find('-');
                    int startRange = std::stoi(token.substr(0, dashPos));
                    int endRange = std::stoi(token.substr(dashPos + 1));

                    for (int i = startRange; i <= endRange; i++) {
                        int selectedNumber = i - 1;

                        if (selectedNumber >= 0 && selectedNumber < isoFiles.size()) {
                            std::string selectedISO = isoFiles[selectedNumber];

                            if (mountedSet.find(selectedISO) == mountedSet.end()) {
                                // Check if the file exists on disk before mounting
                                if (fileExistsOnDisk(selectedISO)) {
                                    mountISO({selectedISO});
                                    mountedSet.insert(selectedISO);
                                } else {
                                    std::cout << "\033[33mISO file '" << selectedISO << "' does not exist on disk. Please refresh the cache from the main menu.\033[0m" << std::endl;
                                }
                            } else {
                                std::cout << "\033[33mISO file '" << selectedISO << "' is already mounted.\033[0m" << std::endl;
                            }
                        } else {
                            std::cout << "\033[31mInvalid range: " << token << ". Please try again.\033[0m" << std::endl;
                        }
                    }
                } else {
                    int selectedNumber = std::stoi(token);
                    if (selectedNumber >= 1 && selectedNumber <= isoFiles.size()) {
                        std::string selectedISO = isoFiles[selectedNumber - 1];

                        if (mountedSet.find(selectedISO) == mountedSet.end()) {
                            // Check if the file exists on disk before mounting
                            if (fileExistsOnDisk(selectedISO)) {
                                mountISO({selectedISO});
                                mountedSet.insert(selectedISO);
                            } else {
                                std::cout << "\033[33mISO file '" << selectedISO << "' does not exist on disk. Please refresh the cache from the main menu.\033[0m" << std::endl;
                            }
                        } else {
                            std::cout << "\033[33mISO file '" << selectedISO << "' is already mounted.\033[0m" << std::endl;
                        }
                    } else {
                        std::cout << "\033[31mInvalid number: " << selectedNumber << ". Please try again.\033[0m" << std::endl;
                    }
                }
            }
        }
    }
}

bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }

    bool equal = true;

    // Set the number of threads to a maximum of 4
    omp_set_num_threads(4);

    #pragma omp parallel for
    for (int i = 0; i < a.size(); i++) {
        if (!equal) continue; // Skip further work if a mismatch has been found

        char lhs = std::tolower(a[i]);
        char rhs = std::tolower(b[i]);

        #pragma omp critical
        {
            if (lhs != rhs) {
                equal = false;
            }
        }
    }

    return equal;
}

void parallelTraverse(const std::filesystem::path& path, std::vector<std::string>& isoFiles, std::mutex& mtx) {
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                const std::filesystem::path& filePath = entry.path();

                // Check if the file size is zero or the ISO name ends in ".bin.iso"
                if (std::filesystem::file_size(filePath) == 0 || iequals(std::string(filePath.stem()), ".bin")) {
                    continue; // Skip zero-byte files and files ending in ".bin.iso"
                }

                std::string extensionStr = filePath.extension().string();
                std::string_view extension = extensionStr;

                if (iequals(std::string(extension), ".iso")) {
                    std::lock_guard<std::mutex> lock(mtx);
                    isoFiles.push_back(filePath.string());
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void processPath(const std::string& path, std::vector<std::string>& allIsoFiles) {
    std::cout << "Processing directory path: " << path << std::endl;
    std::vector<std::string> newIsoFiles;
    parallelTraverse(path, newIsoFiles, mtx);

    // Lock the mutex to safely update the shared vector
    std::lock_guard<std::mutex> lock(mtx);
    allIsoFiles.insert(allIsoFiles.end(), newIsoFiles.begin(), newIsoFiles.end());

    std::cout << "Cache refreshed for directory: " << path << std::endl;
}



// UMOUNT FUNCTIONS	\\

void listMountedISOs() {
    const std::string isoPath = "/mnt";
    std::vector<std::string> isoDirs;

    // Find and store directories with the name "iso_*" in /mnt
    DIR* dir;
    struct dirent* entry;

    if ((dir = opendir(isoPath.c_str())) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_DIR && std::string(entry->d_name).find("iso_") == 0) {
                std::string fullDirPath = isoPath + "/" + entry->d_name;
                std::string isoName = entry->d_name + 4; // Remove "/mnt/iso_" part
                isoDirs.push_back(isoName);
            }
        }
        closedir(dir);
    } else {
        std::cerr << "Error opening the /mnt directory." << std::endl;
    }

    // Display a list of mounted ISOs with ISO names in bold and white "List of mounted ISOs" text
    if (!isoDirs.empty()) {
        std::cout << "\033[37;1mList of mounted ISOs:\033[0m" << std::endl; // White and bold
        for (size_t i = 0; i < isoDirs.size(); ++i) {
            std::cout << i + 1 << ". \033[1m\033[35m" << isoDirs[i] << "\033[0m" << std::endl; // Bold and magenta
        }
    } else {
        std::cerr << "\033[31mNO ISOS MOUNTED\n\033[0m" << std::endl;
 }
}

void unmountISO(const std::string& isoDir) {
    // Unmount the ISO and suppress logs
    std::string unmountCommand = "sudo umount -l " + shell_escape(isoDir) + " > /dev/null 2>&1";
    int result = system(unmountCommand.c_str());

    if (result == 0) {
        // Omitted log for unmounting success
    } else {
        // Omitted log for unmounting failure
    }

    // Check if the directory is empty before removing it
    if (std::filesystem::is_empty(isoDir)) {
        std::string removeDirCommand = "sudo rmdir -p " + shell_escape(isoDir) + " 2>/dev/null";
        int removeDirResult = system(removeDirCommand.c_str());

        if (removeDirResult == 0) {
            // Omitted log for directory removal
        }
    } else {
        std::cout << "\033[31mDIRECTORY NOT EMPTY, SKIPPING PROBABLY NOT AN ISO.\033[0m" << std::endl;
        // Handle the case where the directory is not empty, e.g., log an error or take appropriate action.
    }
}

void unmountISOs() {
    listMountedISOs(); // Display the initial list of mounted ISOs

    const std::string isoPath = "/mnt";

    while (true) {
        std::vector<std::string> isoDirs;

        // Find and store directories with the name "iso_*" in /mnt
        DIR* dir;
        struct dirent* entry;

        if ((dir = opendir(isoPath.c_str())) != NULL) {
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_DIR && std::string(entry->d_name).find("iso_") == 0) {
                    isoDirs.push_back(isoPath + "/" + entry->d_name);
                }
            }
            closedir(dir);
        } else {
            std::cerr << "\031[33mError opening the /mnt directory.\033[0m" << std::endl;
        }

        if (isoDirs.empty()) {
            std::cout << "\033[33mLIST IS EMPTY, NOTHING TO DO.\n\033[0m";
            return;
        }

        // Prompt for unmounting input
        std::cout << "\033[94mEnter the range of ISOs to unmount (e.g., 1, 1-3, 1 to 3, or individual numbers like 1 2 3), '00' to unmount all, or press Enter to return:\033[0m ";
        std::string input;
        std::getline(std::cin, input);
        std::system("clear");
        if (input == "") {
            std::cout << "Exiting the unmounting tool." << std::endl;
            break;  // Exit the loop
        }

        if (input == "00") {
            // Unmount all ISOs
            for (const std::string& isoDir : isoDirs) {
                std::lock_guard<std::mutex> lock(mtx); // Lock the critical section
                unmountISO(isoDir);
            }
            listMountedISOs(); // Display the updated list of mounted ISOs after unmounting all
            continue;  // Restart the loop
        }

        // Split the input into tokens
        std::istringstream iss(input);
        std::vector<int> unmountIndices;

        std::string token;
        while (iss >> token) {
            if (std::regex_match(token, std::regex("^\\d+$"))) {
                // Individual number
                int number = std::stoi(token);
                if (number >= 1 && static_cast<size_t>(number) <= isoDirs.size()) {
                    unmountIndices.push_back(number);
                } else {
                    std::cerr << "\033[31mInvalid index. Please try again.\n\033[0m" << std::endl;
                    continue;  // Restart the loop
                }
            } else if (std::regex_match(token, std::regex("^(\\d+)-(\\d+)$"))) {
                // Range input (e.g., "1-3")
                std::smatch match;
                std::regex_match(token, match, std::regex("^(\\d+)-(\\d+)$"));
                int startRange = std::stoi(match[1]);
                int endRange = std::stoi(match[2]);
                if (startRange >= 1 && endRange >= startRange && static_cast<size_t>(endRange) <= isoDirs.size()) {
                    for (int i = startRange; i <= endRange; ++i) {
                        unmountIndices.push_back(i);
                    }
                } else {
                    std::cerr << "\033[31mInvalid range. Please try again.\n\033[0m" << std::endl;
                }
            } else {
                std::cerr << "\033[31mInvalid input format. Please try again.\n\033[0m" << std::endl;
            }
        }

        if (unmountIndices.empty()) {
            std::cerr << "\033[31mNo valid indices provided. Please try again.\n\033[0m" << std::endl;
            continue;  // Restart the loop
        }

        // Determine the number of available CPU cores
        const unsigned int numCores = std::thread::hardware_concurrency();

        // Create a vector of threads to perform unmounting and directory removal concurrently
        std::vector<std::thread> threads;

        for (int index : unmountIndices) {
            const std::string& isoDir = isoDirs[index - 1];

            // Use a thread for each ISO to be unmounted
            threads.emplace_back([&]() {
                std::lock_guard<std::mutex> lock(mtx); // Lock the critical section
                unmountISO(isoDir);
            });
        }

        // Join the threads to wait for them to finish
        for (auto& thread : threads) {
            thread.join();
        }

        listMountedISOs(); // Display the updated list of mounted ISOs after unmounting
    }
}

void unmountAndCleanISO(const std::string& isoDir) {
    std::string unmountCommand = "sudo umount -l " + shell_escape(isoDir) + " 2>/dev/null";
    int result = std::system(unmountCommand.c_str());

    // if (result != 0) {
    //     std::cerr << "Failed to unmount " << isoDir << " with sudo." << std::endl;
    // }

    // Remove the directory after unmounting
    std::string removeDirCommand = "sudo rmdir " + shell_escape(isoDir);
    int removeDirResult = std::system(removeDirCommand.c_str());

    if (removeDirResult != 0) {
        std::cerr << "\033[31mFailed to remove directory\033[0m" << isoDir << std::endl;
    }
}

void cleanAndUnmountISO(const std::string& isoDir) {
    std::lock_guard<std::mutex> lock(mtx);

    // Construct a shell-escaped ISO directory path
    std::string IsoDir = (isoDir);

    unmountAndCleanISO(IsoDir);
}

void cleanAndUnmountAllISOs() {
    std::cout << "\n";
    std::cout << "Clean and Unmount All ISOs function." << std::endl;
    const std::string isoPath = "/mnt";
    std::vector<std::string> isoDirs;

    DIR* dir;
    struct dirent* entry;

    if ((dir = opendir(isoPath.c_str())) != NULL) {
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_DIR && std::string(entry->d_name).find("iso_") == 0) {
                isoDirs.push_back(isoPath + "/" + entry->d_name);
            }
        }
        closedir(dir);
    }

    if (isoDirs.empty()) {
        std::cout << "\033[33mNO ISOS LEFT TO BE CLEANED\n\033[0m" << std::endl;
        return;
    }

    std::vector<std::thread> threads;

    for (const std::string& isoDir : isoDirs) {
        // Construct a shell-escaped path
        std::string IsoDir = (isoDir);
        threads.emplace_back(cleanAndUnmountISO, IsoDir);
        if (threads.size() >= 4) {
            for (std::thread& thread : threads) {
                thread.join();
            }
            threads.clear();
        }
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    std::cout << "\033[32mALL ISOS CLEANED\n\033[0m" << std::endl;
}

// BIN/IMG CONVERSION FUNCTIONS	\\

// Function to list and prompt the user to choose a file for conversion
std::string chooseFileToConvert(const std::vector<std::string>& files) {
    std::cout << "\033[32mFound the following .bin and .img files:\033[0m\n";
    for (size_t i = 0; i < files.size(); ++i) {
        std::cout << i + 1 << ": " << files[i] << "\n";
    }

    int choice;
    std::cout << "\033[94mEnter the number of the file you want to convert:\033[0m ";
    std::cin >> choice;

    if (choice >= 1 && choice <= static_cast<int>(files.size())) {
        return files[choice - 1];
    } else {
        std::cout << "\033[31mInvalid choice. Please choose a valid file.\033[31m\n";
        return "";
    }
}

std::vector<std::string> findBinImgFiles(const std::string& directory) {
    // Check if the cache is already populated
    if (!binImgFilesCache.empty()) {
        return binImgFilesCache;
    }

    std::vector<std::string> fileNames;

    try {
        std::vector<std::future<void>> futures;
        std::mutex mutex; // Mutex for protecting the shared data
        const int maxThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4; // Maximum number of worker threads

        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {
                    return std::tolower(c);
                }); // Convert extension to lowercase

                if ((ext == ".bin" || ext == ".img") && (entry.path().filename().string().find("data") == std::string::npos) && (entry.path().filename().string() != "terrain.bin") && (entry.path().filename().string() != "blocklist.bin")) {
                    if (std::filesystem::file_size(entry) >= 10'000'000) {
                        // Ensure the number of active threads doesn't exceed maxThreads
                        while (futures.size() >= maxThreads) {
                            // Wait for at least one thread to complete
                            auto it = std::find_if(futures.begin(), futures.end(),
                            [](const std::future<void>& f) {
                                return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                            });
                            if (it != futures.end()) {
                                it->get();
                                futures.erase(it);
                            }
                        }

                        // Create a task to process the file
                        futures.push_back(std::async(std::launch::async, [entry, &fileNames, &mutex] {
                            std::string fileName = entry.path().string();

                            // Lock the mutex before modifying the shared data
                            std::lock_guard<std::mutex> lock(mutex);

                            // Add the file name to the shared vector without shell escaping
                            fileNames.push_back(fileName);
                        }));
                    }
                }
            }
        }

        // Wait for the remaining tasks to complete
        for (auto& future : futures) {
            future.get();
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    // After collecting the results, update the cache
    binImgFilesCache = fileNames;
    return fileNames;
}

bool isCcd2IsoInstalled() {
    if (std::system("which ccd2iso > /dev/null 2>&1") == 0) {
        return true;
    } else {
        return false;
    }
}

void convertBINToISO(const std::string& inputPath) {
    // Check if the input file exists
    if (!std::ifstream(inputPath)) {
        std::cout << "\033[31mThe specified input file '" << inputPath << "' does not exist.\033[0m" << std::endl;
        return;
    }

    // Define the output path for the ISO file with only the .iso extension
    std::string outputPath = inputPath.substr(0, inputPath.find_last_of(".")) + ".iso";

    // Check if the output ISO file already exists
    if (std::ifstream(outputPath)) {
        std::cout << "\033[33mThe output ISO file '" << outputPath << "' already exists. Skipping conversion.\033[0m" << std::endl;
    } else {
        // Execute the conversion using ccd2iso, with shell-escaped paths
        std::string conversionCommand = "ccd2iso " + shell_escape(inputPath) + " " + shell_escape(outputPath);
        int conversionStatus = std::system(conversionCommand.c_str());
        if (conversionStatus == 0) {
            std::cout << "\033[32mImage file converted to ISO:\033[0m " << outputPath << std::endl;
        } else {
            std::cout << "\033[31mConversion of " << inputPath << " failed.\033[0m" << std::endl;
        }
    }
}

void convertBINsToISOs(const std::vector<std::string>& inputPaths, int numThreads) {
    if (!isCcd2IsoInstalled()) {
        std::cout << "\033[31mccd2iso is not installed. Please install it before using this option.\033[0m" << std::endl;
        return;
    }

    // Create a thread pool with a limited number of threads
    std::vector<std::thread> threads;
    int numCores = std::min(numThreads, static_cast<int>(std::thread::hardware_concurrency()));

    for (const std::string& inputPath : inputPaths) {
        if (inputPath == "") {
            break; //
        } else {
            // Construct the shell-escaped input path
            std::string escapedInputPath = shell_escape(inputPath);

            // Create a new thread for each conversion
            threads.emplace_back(convertBINToISO, escapedInputPath);
            if (threads.size() >= numCores) {
                // Limit the number of concurrent threads to the number of available cores
                for (auto& thread : threads) {
                    thread.join();
                }
                threads.clear();
            }
        }
    }

    // Join any remaining threads
    for (auto& thread : threads) {
        thread.join();
    }
}

void processFilesInRange(int start, int end) {
	std::vector<std::string> binImgFiles;
    int numThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4; // Determine the number of threads based on CPU cores
    std::vector<std::string> selectedFiles;
    for (int i = start; i <= end; i++) {
        selectedFiles.push_back(binImgFiles[i - 1]);
    }

    // Construct the shell-escaped file paths
    std::vector<std::string> escapedSelectedFiles;
    for (const std::string& file : selectedFiles) {
        escapedSelectedFiles.push_back(shell_escape(file));
    }

    // Call convertBINsToISOs with shell-escaped file paths
    convertBINsToISOs(escapedSelectedFiles, numThreads);
}

void select_and_convert_files_to_iso() {
	std::vector<std::string> binImgFiles;
    std::string directoryPath = readInputLine("\033[94mEnter the directory path to search for .bin .img files or simply press enter to return:\033[0m ");

    if (directoryPath.empty()) {
        std::cout << "Path input is empty. Exiting." << std::endl;
        return;
    }

    // Call the findBinImgFiles function to populate the cache
    binImgFiles = findBinImgFiles(directoryPath);

    if (binImgFiles.empty()) {
        std::cout << "\033[33mNo .bin or .img files found in the specified directory and its subdirectories or all files are under 10MB.\n\033[0m";
    } else {
        for (int i = 0; i < binImgFiles.size(); i++) {
            std::cout << i + 1 << ". " << binImgFiles[i] << std::endl;
        }

        std::string input;

        while (true) {
            std::cout << "\033[94mChoose a file to process (enter the number or range e.g., 1-5 or 1 or simply press Enter to return):\033[0m ";
            std::getline(std::cin, input);

            if (input.empty()) {
                std::cout << "Exiting..." << std::endl;
                break;
            }

            std::istringstream iss(input);
            std::string token;
            std::vector<std::thread> threads;

            while (iss >> token) {
                std::istringstream tokenStream(token);
                int start, end;
                char dash;

                if (tokenStream >> start) {
                    if (tokenStream >> dash && dash == '-' && tokenStream >> end) {
                        // Range input (e.g., 1-5)
                        if (start >= 1 && start <= binImgFiles.size() && end >= start && end <= binImgFiles.size()) {
                            for (int i = start; i <= end; i++) {
                                std::string selectedFile = binImgFiles[i - 1]; // No need to shell-escape
                                threads.emplace_back(convertBINToISO, selectedFile);
                            }
                        } else {
                            std::cout << "\033[31mInvalid range. Please try again.\033[0m" << std::endl;
                        }
                    } else if (start >= 1 && start <= binImgFiles.size()) {
                        // Single number input
                        int selectedIndex = start - 1;
                        std::string selectedFile = binImgFiles[selectedIndex]; // No need to shell-escape
                        threads.emplace_back(convertBINToISO, selectedFile);
                    } else {
                        std::cout << "\033[31mInvalid number: " << start << ". Please try again.\033[0m" << std::endl;
                    }
                } else {
                    std::cout << "\033[31mInvalid input format: " << token << ". Please try again.\033[0m" << std::endl;
                }
            }

            // Wait for all threads to complete
            for (auto& thread : threads) {
                thread.join();
            }
        }
    }
}

// MDF/MDS CONVERSION FUNCTIONS	\\

std::vector<std::string> findMdsMdfFiles(const std::string& directory) {
    // Check if the cache is already populated
    if (!mdfMdsFilesCache.empty()) {
        return mdfMdsFilesCache;
    }

    std::vector<std::string> fileNames;

    try {
        std::vector<std::future<void>> futures;
        std::mutex mutex; // Mutex for protecting the shared data
        const int maxThreads = 4; // Maximum number of worker threads

        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](char c) {
                    return std::tolower(c);
                }); // Convert extension to lowercase

                if (ext == ".mdf") {
                    if (std::filesystem::file_size(entry) >= 10'000'000) {
                        // Ensure the number of active threads doesn't exceed maxThreads
                        while (futures.size() >= maxThreads) {
                            // Wait for at least one thread to complete
                            auto it = std::find_if(futures.begin(), futures.end(),
                            [](const std::future<void>& f) {
                                return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                            });
                            if (it != futures.end()) {
                                it->get();
                                futures.erase(it);
                            }
                        }

                        // Create a task to process the file
                        futures.push_back(std::async(std::launch::async, [entry, &fileNames, &mutex] {
                            std::string fileName = entry.path().string();

                            // Lock the mutex before modifying the shared data
                            std::lock_guard<std::mutex> lock(mutex);

                            // Add the file name to the shared vector
                            fileNames.push_back(fileName);
                        }));
                    }
                }
            }
        }

        // Wait for the remaining tasks to complete
        for (auto& future : futures) {
            future.get();
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
    }

    // After collecting the results, update the cache
    mdfMdsFilesCache = fileNames;
    return fileNames;
}

bool isMdf2IsoInstalled() {
    std::string command = "which " + shell_escape("mdf2iso");
    if (std::system((command + " > /dev/null 2>&1").c_str()) == 0) {
        return true;
    } else {
        return false;
    }
}

void convertMDFToISO(const std::string& inputPath) {
    // Check if the input file exists
    if (!std::ifstream(inputPath)) {
        std::cout << "\033[31mThe specified input file '" << inputPath << "' does not exist.\033[0m" << std::endl;
        return;
    }

    // Escape the inputPath before using it in shell commands
    std::string escapedInputPath = shell_escape(inputPath);

    // Define the output path for the ISO file with only the .iso extension
    std::string outputPath = inputPath.substr(0, inputPath.find_last_of(".")) + ".iso";

    // Escape the outputPath before using it in shell commands
    std::string escapedOutputPath = shell_escape(outputPath);

    // Execute the conversion using mdf2iso
    std::string conversionCommand = "mdf2iso " + escapedInputPath + " " + escapedOutputPath;

    // Capture the output of the mdf2iso command
    FILE* pipe = popen(conversionCommand.c_str(), "r");
    if (!pipe) {
        std::cout << "\033[31mFailed to execute conversion command\033[0m" << std::endl;
        return;
    }

    char buffer[128];
    std::string conversionOutput;
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        conversionOutput += buffer;
    }

    int conversionStatus = pclose(pipe);

    if (conversionStatus == 0) {
        // Check if the conversion output contains the "already ISO9660" message
        if (conversionOutput.find("already ISO") != std::string::npos) {
            std::cout << "\033[31mThe selected file '" << inputPath << "' is already in ISO format. Skipping conversion.\033[0m" << std::endl;
        } else {
            std::cout << "\033[33mImage file converted to ISO:\033[0m " << outputPath << std::endl;
        }
    } else {
        std::cout << "\033[31mConversion of " << inputPath << " failed.\033[0m" << std::endl;
    }
}

void convertMDFsToISOs(const std::vector<std::string>& inputPaths, int numThreads) {
    if (!isMdf2IsoInstalled()) {
        std::cout << "\033[31mmdf2iso is not installed. Please install it before using this option.\033[0m";
        return;
    }

    // Create a thread pool with a limited number of threads
    std::vector<std::thread> threads;
    int numCores = std::min(numThreads, static_cast<int>(std::thread::hardware_concurrency()));

    for (const std::string& inputPath : inputPaths) {
        if (inputPath == "") {
            break; // Exit the loop
        } else {
            // No need to escape the file path, as we'll handle it in the convertMDFToISO function
            std::string escapedInputPath = inputPath;

            // Create a new thread for each conversion
            threads.emplace_back(convertMDFToISO, escapedInputPath);
            if (threads.size() >= numCores) {
                // Limit the number of concurrent threads to the number of available cores
                for (auto& thread : threads) {
                    thread.join();
                }
                threads.clear();
            }
        }
    }

    // Join any remaining threads
    for (auto& thread : threads) {
        thread.join();
    }
}

void processMDFFilesInRange(int start, int end) {
	std::vector<std::string> mdfImgFiles;	// Declare mdfImgFiles here
    int numThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4; // Determine the number of threads based on CPU cores
    std::vector<std::string> selectedFiles;
    for (int i = start; i <= end; i++) {
        std::string FilePath = (mdfImgFiles[i - 1]);
        selectedFiles.push_back(FilePath);
    }
    convertMDFsToISOs(selectedFiles, numThreads);
}

void select_and_convert_files_to_iso_mdf() {
    int numThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4;
    std::string directoryPath = readInputLine("\033[94mEnter the directory path to search for .mdf files or simply press enter to return:\033[0m ");

    if (directoryPath.empty()) {
        std::cout << "\033[33mPath input is empty. Exiting.\033[0m" << std::endl;
        return;
    }

    // Call the findMdsMdfFiles function to populate the cache
    std::vector<std::string> mdfMdsFiles = findMdsMdfFiles(directoryPath);

    if (mdfMdsFiles.empty()) {
        std::cout << "\033[31mNo .mdf files found in the specified directory and its subdirectories or all files are under 10MB.\n\033[0m";
        return;
    }

    while (true) {
        // Display the list of available files with numbers
        std::cout << "Select files to convert to ISO:\n";
        for (int i = 0; i < mdfMdsFiles.size(); i++) {
            std::cout << i + 1 << ". " << mdfMdsFiles[i] << std::endl;
        }

        // Ask the user to choose files
        std::string input;
        std::cout << "\033[94mEnter the numbers of the files to convert (e.g., '1-2' or '1 2', or simply press enter to return):\033[0m ";
        std::getline(std::cin, input);

        if (input.empty()) {
            std::cout << "Exiting..." << std::endl;
            break;
        }

        // Parse the user input to extract file numbers
        std::vector<int> selectedFileIndices;
        std::istringstream iss(input);
        std::string token;
        while (iss >> token) {
            if (token.find('-') != std::string::npos) {
                // Handle a range (e.g., "1-2")
                size_t dashPos = token.find('-');
                int startRange = std::stoi(token.substr(0, dashPos));
                int endRange = std::stoi(token.substr(dashPos + 1));
                for (int i = startRange; i <= endRange; i++) {
                    if (i >= 1 && i <= mdfMdsFiles.size()) {
                        selectedFileIndices.push_back(i - 1);
                    }
                }
            } else {
                // Handle individual numbers (e.g., "1")
                int selectedFileIndex = std::stoi(token);
                if (selectedFileIndex >= 1 && selectedFileIndex <= mdfMdsFiles.size()) {
                    selectedFileIndices.push_back(selectedFileIndex - 1);
                }
            }
        }

        if (!selectedFileIndices.empty()) {
            // Convert the selected files
            std::vector<std::string> selectedFiles;
            for (int index : selectedFileIndices) {
                selectedFiles.push_back(mdfMdsFiles[index]);
            }
            convertMDFsToISOs(selectedFiles, numThreads);
        } else {
            std::cout << "Invalid choice. Please enter valid file numbers or 'exit'." << std::endl;
        }
    }
}
