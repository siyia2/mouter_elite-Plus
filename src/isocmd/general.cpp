// SPDX-License-Identifier: GNU General Public License v3.0 or later

#include "../headers.h"


// For storing isoFiles in RAM
std::vector<std::string> globalIsoFileList;

// Main function to select and operate on ISOs by number for umount mount cp mv and rm
void selectForIsoFiles(const std::string& operation, bool& historyPattern, int& maxDepth, bool& verbose) {
    // Calls prevent_clear_screen and tab completion
    rl_bind_key('\f', prevent_readline_keybindings);
    rl_bind_key('\t', prevent_readline_keybindings);
    
    std::set<std::string> operationFiles, skippedMessages, operationFails, uniqueErrorMessages;
    std::vector<std::string> filteredFiles, isoDirs;
    globalIsoFileList.reserve(100);
    bool isFiltered = false;
    bool needsClrScrn = true;
    bool umountMvRmBreak = false;
    
    // Determine operation color based on operation type
    std::string operationColor = (operation == "rm") ? "\033[1;91m" :
                                 (operation == "cp") ? "\033[1;92m" : 
                                 (operation == "mv") ? "\033[1;93m" :
                                 (operation == "mount") ? "\033[1;92m" : 
                                 (operation == "write") ? "\033[1;93m" :
                                 (operation == "umount") ? "\033[1;93m" : "\033[1;95m";
                                 
    std::string process = operation;
    bool isMount = (operation == "mount");
    bool isUnmount = (operation == "umount");
    bool write = (operation == "write");
    bool promptFlag = false; // PromptFlag for cache refresh, defaults to false for move and other operations
    
    // Set default location values
	atMount = false;
	atConversions = false;
	atCpMvRm = false;
	atWrite = false;
    
    if (isMount) {
		atMount = true;
	} else if (write) {
		atWrite = true;
	} else if (isUnmount) {
		
	} else {
		atCpMvRm = true;
	}
    
    while (true) {
        // Verbose output is to be disabled unless specified by progressbar function downstream
        verbose = false;

        operationFiles.clear();
        skippedMessages.clear();
        operationFails.clear();
        uniqueErrorMessages.clear();

        if (needsClrScrn && !isUnmount) {
            umountMvRmBreak = false;
            if (!clearAndLoadFiles(filteredFiles, isFiltered)) break;
            std::cout << "\n\n";
        } else if (needsClrScrn && isUnmount) {
            umountMvRmBreak = false;
            if (!loadAndDisplayMountedISOs(isoDirs, filteredFiles, isFiltered)) break;
            std::cout << "\n\n";
        }
        
        // Move the cursor up 1 line and clear them
        std::cout << "\033[1A\033[K";
        
        std::string prompt = (isFiltered ? "\001\033[1;96m\002F⊳ \001\033[1;92m\002ISO\001\033[1;94m\002 ↵ for \001" : "\001\033[1;92m\002ISO\001\033[1;94m\002 ↵ for \001")
						    + operationColor + "\002" + operation 
							+ "\001\033[1;94m\002, ? ↵ for help, ↵ to return:\001\033[0;1m\002 ";

        std::unique_ptr<char[], decltype(&std::free)> input(readline(prompt.c_str()), &std::free);
        
        // Check for EOF (Ctrl+D) or NULL input before processing
        if (!input.get()) {
            break; // Exit the loop on EOF
        }

        std::string inputString(input.get());
        
        if (inputString == "?") {
            helpSelections();
            needsClrScrn = true;
            continue;
        }

        if (inputString == "~") {
		// Set default values
		atMount = false;
		atConversions = false;
		atCpMvRm = false;
		atWrite = false;
		needsClrScrn = true;

		// Update specific flags and variables based on conditions
		if (isMount) {
			atMount = true;
			toggleFullListMount = !toggleFullListMount;
		} else if (isUnmount) {
			toggleFullListUmount = !toggleFullListUmount;
		} else if (write) {
			atWrite = true;
			toggleFullListWrite = !toggleFullListWrite;
		} else {
			atCpMvRm = true;
			toggleFullListCpMvRm = !toggleFullListCpMvRm;
		}
				
            continue;
        }

        if (inputString.empty()) {
            if (isFiltered) {
                isFiltered = false;
                continue;
            } else {
                return;
            }
        } else if (inputString == "/") {
            while (true) {
                verbose = false;
                operationFiles.clear();
                skippedMessages.clear();
                operationFails.clear();
                uniqueErrorMessages.clear();

                clear_history();
                historyPattern = true;
                loadHistory(historyPattern);
                // Move the cursor up 1 line and clear them
                std::cout << "\033[1A\033[K";

                // Generate prompt
                std::string filterPrompt = "\001\033[38;5;94m\002FilterTerms\001\033[1;94m\002 ↵ for \001" + operationColor + "\002" + operation + 
                                           "\001\033[1;94m\002, or ↵ to return: \001\033[0;1m\002";
                std::unique_ptr<char, decltype(&std::free)> searchQuery(readline(filterPrompt.c_str()), &std::free);

                if (!searchQuery || searchQuery.get()[0] == '\0' || strcmp(searchQuery.get(), "/") == 0) {
                    historyPattern = false;
                    clear_history();
                    if (isFiltered) {
                        needsClrScrn = true;
                    } else {
                        needsClrScrn = false;
                    }
                    break;
                }

                std::string inputSearch(searchQuery.get());
                
                // Decide the current list to filter
                std::vector<std::string>& currentFiles = !isUnmount 
                ? (isFiltered ? filteredFiles : globalIsoFileList)
                : (isFiltered ? filteredFiles : isoDirs);

                // Apply the filter on the current list
                auto newFilteredFiles = filterFiles(currentFiles, inputSearch);
                sortFilesCaseInsensitive(newFilteredFiles);

                if ((newFilteredFiles.size() == globalIsoFileList.size() && isMount) || (newFilteredFiles.size() == isoDirs.size() && isUnmount)) {
                    isFiltered = false;
                    break;
                }

                if (!newFilteredFiles.empty()) {
                    add_history(searchQuery.get());
                    saveHistory(historyPattern);
                    needsClrScrn = true;
                    filteredFiles = std::move(newFilteredFiles);
                    isFiltered = true;
                    historyPattern = false;
                    clear_history();
                    break;
                }
                historyPattern = false;
                clear_history();
            }
        } else if (inputString[0] == '/' && inputString.length() > 1) {
            // Directly filter the files based on the input without showing the filter prompt
            std::string inputSearch = inputString.substr(1); // Skip the '/' character

            // Decide the current list to filter
            std::vector<std::string>& currentFiles = !isUnmount 
            ? (isFiltered ? filteredFiles : globalIsoFileList)
            : (isFiltered ? filteredFiles : isoDirs);

            // Apply the filter on the current list
            auto newFilteredFiles = filterFiles(currentFiles, inputSearch);
            sortFilesCaseInsensitive(newFilteredFiles);

            if (!newFilteredFiles.empty() && !((newFilteredFiles.size() == globalIsoFileList.size() && isMount) || (newFilteredFiles.size() == isoDirs.size() && isUnmount))) {
				historyPattern = true;
                loadHistory(historyPattern);
				add_history(inputSearch.c_str()); // Save the filter pattern to history
				saveHistory(historyPattern);
                filteredFiles = std::move(newFilteredFiles);
                isFiltered = true;
                needsClrScrn = true;
                historyPattern = false;
                clear_history();
            } else {
                needsClrScrn = false;
            }
        } else {
            std::vector<std::string>& currentFiles = isFiltered 
            ? filteredFiles 
            : (!isUnmount ? globalIsoFileList : isoDirs);

            clearScrollBuffer();
            needsClrScrn = true;
            
            if (isMount && inputString == "00") {
                // Special case for mounting all files
                std::cout << "\033[0;1m";
                currentFiles = globalIsoFileList;
                processAndMountIsoFiles(inputString, currentFiles, operationFiles, skippedMessages, operationFails, uniqueErrorMessages, verbose);
            } else if (isMount){
                clearScrollBuffer();
                needsClrScrn = true;
                std::cout << "\033[0;1m";
                processAndMountIsoFiles(inputString, currentFiles, operationFiles, skippedMessages, operationFails, uniqueErrorMessages, verbose);
            } else if (isUnmount) {
                // Unmount-specific logic
                std::vector<std::string> selectedIsoDirs;
                
                if (inputString == "00") {
                    selectedIsoDirs = currentFiles;
                    umountMvRmBreak = true;
                } else {
                    umountMvRmBreak = true;
                }
                
                prepareUnmount(inputString, selectedIsoDirs, currentFiles, operationFiles, operationFails, uniqueErrorMessages, umountMvRmBreak, verbose);
                needsClrScrn = true;
            } else if (write) {
                writeToUsb(inputString, currentFiles, uniqueErrorMessages);
            } else {
                // Generic operation processing for copy, move, remove
                std::cout << "\033[0;1m\n";
                processOperationInput(inputString, currentFiles, operation, operationFiles, operationFails, uniqueErrorMessages, promptFlag, maxDepth, umountMvRmBreak, historyPattern, verbose);
            }

            // Check and print results
            if (!uniqueErrorMessages.empty() && operationFiles.empty() && skippedMessages.empty() && operationFails.empty() && isMount) {
                clearScrollBuffer();
                needsClrScrn = true;
                std::cout << "\n\033[1;91mNo valid input provided for " << operation << "\033[0;1m\n\n\033[1;32m↵ to continue...\033[0;1m";
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            } else if (verbose) {
                clearScrollBuffer();
                needsClrScrn = true;
                if (isMount){
                    verbosePrint(operationFiles, operationFails, skippedMessages, {}, uniqueErrorMessages, 2);
                } else if (isUnmount){
                    verbosePrint(operationFiles, operationFails, {}, {}, uniqueErrorMessages, 1);
                } else {
                    verbosePrint(operationFiles, operationFails, {}, {}, uniqueErrorMessages, 1);
                }
            }

            // Additional logic for non-mount operations
            if ((process == "mv" || process == "rm" || process == "umount") && isFiltered && umountMvRmBreak) {
                historyPattern = false;
                clear_history();
                isFiltered = false;
                needsClrScrn = true;
            }

            if (currentFiles.empty()) {
                clearScrollBuffer();
                needsClrScrn = true;
                std::cout << "\n\033[1;93mNo ISO available for " << operation << ".\033[0m\n\n";
                std::cout << "\n\033[1;32m↵ to continue...\033[0;1m";
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                return;
            }
        }
    }
}


// General function to tokenize input strings
void tokenizeInput(const std::string& input, std::vector<std::string>& isoFiles, std::set<std::string>& uniqueErrorMessages, std::set<int>& processedIndices) {
    std::istringstream iss(input);
    std::string token;

    std::set<std::string> invalidInputs;
    std::set<std::string> invalidIndices;
    std::set<std::string> invalidRanges;

    while (iss >> token) {
        if (startsWithZero(token)) {
            invalidIndices.insert(token);
            continue;
        }

        if (std::count(token.begin(), token.end(), '-') > 1) {
            invalidInputs.insert(token);
            continue;
        }

        size_t dashPos = token.find('-');
        if (dashPos != std::string::npos) {
            int start, end;
            try {
                start = std::stoi(token.substr(0, dashPos));
                end = std::stoi(token.substr(dashPos + 1));
            } catch (const std::invalid_argument&) {
                invalidInputs.insert(token);
                continue;
            } catch (const std::out_of_range&) {
                invalidRanges.insert(token);
                continue;
            }

            if (start < 1 || static_cast<size_t>(start) > isoFiles.size() || end < 1 || static_cast<size_t>(end) > isoFiles.size() || start == 0 || end == 0) {
                invalidRanges.insert(token);
                continue;
            }

            int step = (start <= end) ? 1 : -1;
            for (int i = start; (start <= end) ? (i <= end) : (i >= end); i += step) {
                if (i >= 1 && i <= static_cast<int>(isoFiles.size())) {
                    if (processedIndices.find(i) == processedIndices.end()) {
                        processedIndices.insert(i);
                    }
                }
            }
        } else if (isNumeric(token)) {
            int num = std::stoi(token);
            if (num >= 1 && static_cast<size_t>(num) <= isoFiles.size()) {
                if (processedIndices.find(num) == processedIndices.end()) {
                    processedIndices.insert(num);
                }
            } else {
                invalidIndices.insert(token);
            }
        } else {
            invalidInputs.insert(token);
        }
    }

    // Helper to format error messages with pluralization
    auto formatCategory = [](const std::string& singular, const std::string& plural,
                            const std::set<std::string>& items) {
        if (items.empty()) return std::string();
        std::ostringstream oss;
        oss << "\033[1;91m" << (items.size() > 1 ? plural : singular) << ": '";
        for (auto it = items.begin(); it != items.end(); ++it) {
            if (it != items.begin()) oss << " ";
            oss << *it;
        }
        oss << "'.\033[0;1m";
        return oss.str();
    };

    // Add formatted messages with conditional pluralization
    if (!invalidInputs.empty()) {
        uniqueErrorMessages.insert(formatCategory("Invalid input", "Invalid inputs", invalidInputs));
    }
    if (!invalidIndices.empty()) {
        uniqueErrorMessages.insert(formatCategory("Invalid index", "Invalid indexes", invalidIndices));
    }
    if (!invalidRanges.empty()) {
        uniqueErrorMessages.insert(formatCategory("Invalid range", "Invalid ranges", invalidRanges));
    }
}


// Function to get the total size of files
size_t getTotalFileSize(const std::vector<std::string>& files) {
    size_t totalSize = 0;
    for (const auto& file : files) {
        struct stat st;
        if (stat(file.c_str(), &st) == 0) {
            totalSize += st.st_size;
        }
    }
    return totalSize;
}


// Function to display progress bar for native operations
void displayProgressBarWithSize(std::atomic<size_t>* completedBytes, size_t totalBytes, std::atomic<size_t>* completedTasks, size_t totalTasks, std::atomic<bool>* isComplete, bool* verbose) {
    // Set up non-blocking input
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // Set stdin to non-blocking mode
    int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    
    const int barWidth = 50;
    bool enterPressed = false;
    auto startTime = std::chrono::high_resolution_clock::now();

    // Precompute formatted total bytes string if applicable
    const bool bytesTrackingEnabled = (completedBytes != nullptr);
    std::string totalBytesFormatted;
    std::stringstream ssFormatter;
    if (bytesTrackingEnabled) {
        auto formatSize = [&ssFormatter](size_t bytes) -> std::string {
            const char* units[] = {" B", " KB", " MB", " GB"};
            int unit = 0;
            double size = static_cast<double>(bytes);
            while (size >= 1024 && unit < 3) {
                size /= 1024;
                unit++;
            }
            ssFormatter.str("");
            ssFormatter.clear();
            ssFormatter << std::fixed << std::setprecision(2) << size << units[unit];
            return ssFormatter.str();
        };
        totalBytesFormatted = formatSize(totalBytes);
    }

    // Reusable components
    auto formatSize = [&ssFormatter](size_t bytes) -> std::string {
        const char* units[] = {" B", " KB", " MB", " GB"};
        int unit = 0;
        double size = static_cast<double>(bytes);
        while (size >= 1024 && unit < 3) {
            size /= 1024;
            unit++;
        }
        ssFormatter.str("");
        ssFormatter.clear();
        ssFormatter << std::fixed << std::setprecision(2) << size << units[unit];
        return ssFormatter.str();
    };

    try {
        while (!isComplete->load(std::memory_order_relaxed) || !enterPressed) {
            // Discard any input during progress update
            char ch;
            while (read(STDIN_FILENO, &ch, 1) > 0);

            // Load atomics once per iteration
            const size_t completedTasksValue = completedTasks->load(std::memory_order_relaxed);
            const size_t completedBytesValue = bytesTrackingEnabled ? completedBytes->load(std::memory_order_relaxed) : 0;

            // Calculate progress
            const double tasksProgress = static_cast<double>(completedTasksValue) / totalTasks;
            double overallProgress = tasksProgress;
            if (bytesTrackingEnabled) {
                const double bytesProgress = static_cast<double>(completedBytesValue) / totalBytes;
                overallProgress = std::max(bytesProgress, tasksProgress);
            }
            const int progressPos = static_cast<int>(barWidth * overallProgress);

            // Calculate timing and speed
            const auto currentTime = std::chrono::high_resolution_clock::now();
            const auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime);
            const double elapsedSeconds = elapsedTime.count() / 1000.0;
            const double speed = bytesTrackingEnabled ? (completedBytesValue / elapsedSeconds) : 0;

            // Build output string efficiently
            std::stringstream ss;
            ss << "\r[";
            for (int i = 0; i < barWidth; ++i) {
                ss << (i < progressPos ? "=" : (i == progressPos ? ">" : " "));
            }
            ss << "] " << std::fixed << std::setprecision(0) << (overallProgress * 100.0)
               << "% (" << completedTasksValue << "/" << totalTasks << ")";

            if (bytesTrackingEnabled) {
                ss << " (" << formatSize(completedBytesValue) << "/" << totalBytesFormatted << ") "
                   << formatSize(static_cast<size_t>(speed)) << "/s";
            }

            ss << " Time Elapsed: " << std::fixed << std::setprecision(1) << elapsedSeconds << "s\033[K";
            std::cout << ss.str() << std::flush;

            // Check completion condition
            if (completedTasksValue >= totalTasks && !enterPressed) {
                rl_bind_key('\f', prevent_readline_keybindings);
                rl_bind_key('\t', prevent_readline_keybindings);
                rl_bind_keyseq("\033[A", prevent_readline_keybindings);
                rl_bind_keyseq("\033[B", prevent_readline_keybindings);

                enterPressed = true;
                std::cout << "\n\n";
                tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                fcntl(STDIN_FILENO, F_SETFL, oldf);

                const std::string prompt = "\033[1;94mDisplay verbose output? (y/n):\033[0;1m ";
                std::unique_ptr<char, decltype(&std::free)> input(readline(prompt.c_str()), &std::free);
                
                if (input.get()) {
                    *verbose = (std::string(input.get()) == "y" || std::string(input.get()) == "Y");
                }

                rl_bind_keyseq("\033[A", rl_get_previous_history);
                rl_bind_keyseq("\033[B", rl_get_next_history);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    } catch (...) {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        fcntl(STDIN_FILENO, F_SETFL, oldf);
        throw;
    }

    std::cout << std::endl;
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
}


// Function to print all required lists
void printList(const std::vector<std::string>& items, const std::string& listType) {
    static const char* defaultColor = "\033[0m";
    static const char* bold = "\033[1m";
    static const char* reset = "\033[0m";
    static const char* red = "\033[31;1m";
    static const char* green = "\033[32;1m";
    static const char* blueBold = "\033[94;1m";
    static const char* magenta = "\033[95m";
    static const char* magentaBold = "\033[95;1m";
    static const char* orangeBold = "\033[1;38;5;208m";
    static const char* grayBold = "\033[38;5;245m";
        
    size_t maxIndex = items.size();
    size_t numDigits = std::to_string(maxIndex).length();

    // Precompute padded index strings
    std::vector<std::string> indexStrings(maxIndex);
    for (size_t i = 0; i < maxIndex; ++i) {
        indexStrings[i] = std::to_string(i + 1);
        indexStrings[i].insert(0, numDigits - indexStrings[i].length(), ' ');
    }

    std::ostringstream output;
    output << "\n"; // Initial newline for visual spacing

    for (size_t i = 0; i < items.size(); ++i) {
        const char* sequenceColor = (i % 2 == 0) ? red : green;
        std::string directory, filename, displayPath, displayHash;

        if (listType == "ISO_FILES") {
            auto [dir, fname] = extractDirectoryAndFilename(items[i]);
            directory = dir;
            filename = fname;
        } else if (listType == "MOUNTED_ISOS") {
			std::string dirName = items[i];
    
			// Find the position of the first underscore
			size_t firstUnderscorePos = dirName.find('_');
    
			// Find the position of the last tilde
			size_t lastTildePos = dirName.find_last_of('~');
    
			// Extract displayPath (after first underscore and before last tilde)
			if (firstUnderscorePos != std::string::npos && lastTildePos != std::string::npos && lastTildePos > firstUnderscorePos) {
				displayPath = dirName.substr(firstUnderscorePos + 1, lastTildePos - (firstUnderscorePos + 1));
			} else {
				// If the conditions are not met, use the entire dirName (or handle it as needed)
				displayPath = dirName;
			}
    
			// Extract displayHash (from last tilde to the end, including the last tilde)
			if (lastTildePos != std::string::npos) {
				displayHash = dirName.substr(lastTildePos); // Start at lastTildePos instead of lastTildePos + 1
			} else {
				// If no tilde is found, set displayHash to an empty string (or handle it as needed)
				displayHash = "";
			}
		} else if (listType == "IMAGE_FILES") {
            auto [dir, fname] = extractDirectoryAndFilename(items[i]);

            bool isSpecialExtension = false;
            std::string extension = fname;
            size_t dotPos = extension.rfind('.');

            if (dotPos != std::string::npos) {
                extension = extension.substr(dotPos);
                toLowerInPlace(extension);
                isSpecialExtension = (extension == ".bin" || extension == ".img" ||
                                      extension == ".mdf" || extension == ".nrg");
            }

            if (isSpecialExtension) {
                directory = dir;
                filename = fname;
                sequenceColor = orangeBold;
            }
        }

        // Build output based on listType
        if (listType == "ISO_FILES") {
            output << sequenceColor << indexStrings[i] << ". "
                   << defaultColor << bold << directory
                   << defaultColor << bold << "/"
                   << magenta << filename << defaultColor << "\n";
        } else if (listType == "MOUNTED_ISOS") {
			if (toggleFullListUmount){
            output << sequenceColor << indexStrings[i] << ". "
                   << blueBold << "/mnt/iso_"
                   << magentaBold << displayPath << grayBold << displayHash << reset << "\n";
			} else {
				output << sequenceColor << indexStrings[i] << ". "
                   << magentaBold << displayPath << "\n";
			}
        } else if (listType == "IMAGE_FILES") {
		// Alternate sequence color like in "ISO_FILES"
		const char* sequenceColor = (i % 2 == 0) ? red : green;
    
			if (directory.empty() && filename.empty()) {
				// Standard case
				output << sequenceColor << indexStrings[i] << ". "
				<< reset << bold << items[i] << defaultColor << "\n";
			} else {
				// Special extension case (keep the filename sequence as orange bold)
				output << sequenceColor << indexStrings[i] << ". "
					<< reset << bold << directory << "/"
					<< orangeBold << filename << defaultColor << "\n";
			}
        }
    }

    std::cout << output.str();
}


// Function to display how to select items from lists
void helpSelections() {
    clearScrollBuffer();
    
    // Title
    std::cout << "\n\033[1;36m===== Help Guide For Lists =====\033[0m\n" << std::endl;
    
    // Working with indices
    std::cout << "\033[1;32m1. Selecting Items:\033[0m\n"
              << "   • Single item: Enter a number (e.g., '1')\n"
              << "   • Multiple items: Separate with spaces (e.g., '1 5 6')\n"
              << "   • Range of items: Use a hyphen (e.g., '1-3')\n"
              << "   • Combine methods: '1-3 5 7-9'\n"
              << "   • Select all: Enter '00' (for mount/umount only)\n" << std::endl;
    
    // Special commands
    std::cout << "\033[1;32m2. Special Selection Commands:\033[0m\n"
              << "   • Enter \033[1;34m'/'\033[0m - Filter the current list based on a search terms (e.g., 'term' or 'term1;term2')\n"
              << "   • Enter \033[1;34m'/term1;term2'\033[0m - Directly filter the list for items containing 'term1' and 'term2'\n"
              << "   • Enter \033[1;34m'~'\033[0m - Switch between short and full paths\n"
              << "   - Note: If filtering has no matches, no message or list update is issued\n" << std::endl;
              
    // Prompt to continue
    std::cout << "\033[1;32m↵ to return...\033[0;1m";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}


// Help guide for directory prompts
void helpSearches(bool isCpMv) {
    clearScrollBuffer();
    
    // Title
    std::cout << "\n\033[1;36m===== Help Guide For FolderPath Prompts =====\033[0m\n" << std::endl;
    
    std::cout << "\033[1;32m1. Selecting FolderPaths:\033[0m\n"
              << "   • Single directory: Enter a directory (e.g., '/directory/')\n"
              << "   • Multiple directories: Separate with ; (e.g., '/directory1/;/directory2/')\n"
              << "   • Overwrite files for cp/mv: Append |^O (e.g., '/directory/ |^O' or '/directory1/;/directory2/ |^O')\n" << std::endl;
    if (!isCpMv) {
		std::cout << "\033[1;32m2. Special Cleanup Commands:\033[0m\n"
				<< "   • Enter \033[1;35m'clr'\033[0m - Clear cache:\n"
				<< "     - In Convert2ISO search prompts: Clears corresponding RAM cache\n"
				<< "     - In ImportISO search prompt: Clears on-disk ISO cache\n"
				<< "   • Enter \033[1;35m'clr_paths'\033[0m - Clears folder path history\n"
				<< "   • Enter \033[1;35m'clr_filter'\033[0m - Clears filter history\n" << std::endl;
              
		std::cout << "\033[1;32m3. Special Display Commands:\033[0m\n"
				<< "   • Enter \033[1;35m'ls'\033[0m - Lists cached image file entries (Convert2ISO search prompts only)\n"
				<< "   • Enter \033[1;35m'stats'\033[0m - View on-disk cache statistics (ImportISO search prompt only)\n" << std::endl;
              
		std::cout << "\033[1;32m4. Special Configuration Commands:\033[0m\n\n"

          << "\033[38;5;94mAuto-Update ISO Cache:\033[0m\n"
          << "   • Enter \033[1;35m'*auto_on'\033[0m  - Enable auto-update of ISO cache at startup using stored readline paths (ImportISO search prompt only)\n"
          << "   • Enter \033[1;35m'*auto_off'\033[0m - Disable ISO cache auto-update (ImportISO search prompt only)\n\n"

          << "\033[38;5;94mMount List Display Modes:\033[0m\n"
          << "   • Enter \033[1;35m'*ll_m'\033[0m - Set default display mode for mount list to long\n"
          << "   • Enter \033[1;35m'*sl_m'\033[0m - Set default display mode for mount list to short\n\n"

          << "\033[38;5;94mUnmount List Display Modes:\033[0m\n"
          << "   • Enter \033[1;35m'*ll_u'\033[0m - Set default display mode for unmount list to long\n"
          << "   • Enter \033[1;35m'*sl_u'\033[0m - Set default display mode for unmount list to short\n\n"

          << "\033[38;5;94mFile Operations List Display Modes:\033[0m\n"
          << "   • Enter \033[1;35m'*ll_fo'\033[0m - Set default display mode for cp/mv/rm list to long\n"
          << "   • Enter \033[1;35m'*sl_fo'\033[0m - Set default display mode for cp/mv/rm list to short\n\n"

          << "\033[38;5;94mWrite List Display Modes:\033[0m\n"
          << "   • Enter \033[1;35m'*ll_w'\033[0m - Set default display mode for write list to long\n"
          << "   • Enter \033[1;35m'*sl_w'\033[0m - Set default display mode for write list to short\n\n"

          << "\033[38;5;94mConversion List Display Modes:\033[0m\n"
          << "   • Enter \033[1;35m'*ll_c'\033[0m - Set default display mode for conversion lists to long\n"
          << "   • Enter \033[1;35m'*sl_c'\033[0m - Set default display mode for conversion lists to short\n"
          << std::endl;
	}
                
    // Prompt to continue
    std::cout << "\033[1;32m↵ to return...\033[0;1m";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}


// Help guide for iso and device mapping
void helpMappings() {
    clearScrollBuffer();
    
    // Title
    std::cout << "\n\033[1;36m===== Help Guide For Mappings =====\033[0m\n" << std::endl;
    
    std::cout << "\033[1;32m. Selecting Mappings:\033[0m\n"
			  << "   • Mapping = NewISOIndex>RemovableUSBDevice\n"
              << "   • Single mapping: Enter a mapping (e.g., '1>/dev/sdc')\n"
              << "   • Multiple mappings: Separate with ; (e.g., '1>/dev/sdc;2>/dev/sdd' or '1>/dev/sdc;1>/dev/sdd')\n" << std::endl;
                  
    // Prompt to continue
    std::cout << "\033[1;32m↵ to return...\033[0;1m";
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}


// For memory mapping string transformations
std::unordered_map<std::string, std::string> transformationCache;

// Function to extract directory and filename from a given path
std::pair<std::string, std::string> extractDirectoryAndFilename(std::string_view path) {
    // Use string_view for non-modifying operations
    static const std::array<std::pair<std::string_view, std::string_view>, 2> replacements = {{
        {"/home", "~"},
        {"/root", "/R"}
    }};

    // Find last slash efficiently
    auto lastSlashPos = path.find_last_of("/\\");
    if (lastSlashPos == std::string_view::npos) {
        return {"", std::string(path)};
    }

    // Early return for full list mode
    if (toggleFullListMount && atMount) {
        return {std::string(path.substr(0, lastSlashPos)), 
                std::string(path.substr(lastSlashPos + 1))};
    } else if (toggleFullListCpMvRm && atCpMvRm) {
		 return {std::string(path.substr(0, lastSlashPos)), 
                std::string(path.substr(lastSlashPos + 1))};
	} else if (toggleFullListConversions && atConversions) {
		return {std::string(path.substr(0, lastSlashPos)), 
                std::string(path.substr(lastSlashPos + 1))};
	} else if (toggleFullListWrite && atWrite) {
		return {std::string(path.substr(0, lastSlashPos)), 
                std::string(path.substr(lastSlashPos + 1))};
	}

    // Check cache first
    auto cacheIt = transformationCache.find(std::string(path));
    if (cacheIt != transformationCache.end()) {
        return {cacheIt->second, std::string(path.substr(lastSlashPos + 1))};
    }

    // Optimize directory shortening
    std::string processedDir;
    processedDir.reserve(path.length() / 2);  // More conservative pre-allocation

    size_t start = 0;
    while (start < lastSlashPos) {
        auto end = path.find_first_of("/\\", start);
        if (end == std::string_view::npos) end = lastSlashPos;

        // More efficient component truncation
        size_t componentLength = end - start;
        size_t truncatePos = std::min({
            componentLength, 
            path.find(' ', start) - start,
            path.find('-', start) - start,
            path.find('_', start) - start,
            path.find('.', start) - start,
            size_t(16)
        });

        processedDir.append(path.substr(start, truncatePos));
        processedDir.push_back('/');
        start = end + 1;
    }

    if (!processedDir.empty()) {
        processedDir.pop_back();  // Remove trailing slash

        // More efficient replacements using string_view
        for (const auto& [oldDir, newDir] : replacements) {
            size_t pos = 0;
            while ((pos = processedDir.find(oldDir, pos)) != std::string::npos) {
                processedDir.replace(pos, oldDir.length(), newDir);
                pos += newDir.length();
            }
        }
    }

    // Cache the result
    transformationCache[std::string(path)] = processedDir;

    return {processedDir, std::string(path.substr(lastSlashPos + 1))};
}
