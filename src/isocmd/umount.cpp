// SPDX-License-Identifier: GNU General Public License v2.0

#include "../headers.h"
#include "../threadpool.h"
#include "../display.h"


const std::string MOUNTED_ISO_PATH = "/mnt";

bool loadAndDisplayMountedISOs(std::vector<std::string>& isoDirs, std::vector<std::string>& filteredFiles, bool& isFiltered) {
	signal(SIGINT, SIG_IGN);        // Ignore Ctrl+C
	disable_ctrl_d();
     isoDirs.clear();
        for (const auto& entry : std::filesystem::directory_iterator(MOUNTED_ISO_PATH)) {
            if (entry.is_directory() && entry.path().filename().string().find("iso_") == 0) {
                isoDirs.push_back(entry.path().string());
            }
        }
        sortFilesCaseInsensitive(isoDirs);

    // Check if ISOs exist
    if (isoDirs.empty()) {
		clearScrollBuffer();
        std::cerr << "\n\033[1;93mNo paths matching the '/mnt/iso_{name}' pattern found.\033[0m\033[0;1m\n";
        std::cout << "\n\033[1;32m↵ to return...\033[0;1m";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        return false;
    }

    // Sort ISOs case-insensitively
    sortFilesCaseInsensitive(isoDirs);

    // Display ISOs
    clearScrollBuffer();
        if (filteredFiles.size() == isoDirs.size()) {
				isFiltered = false;
		}
        printList(isFiltered ? filteredFiles : isoDirs, "MOUNTED_ISOS", "");

    return true;
}


// Function toggle between long and short vebose logging in umount
std::string modifyDirectoryPath(const std::string& dir) {
    if (displayConfig::toggleFullListUmount) {
        return dir;
    }

    // We know:
    // - "/mnt/iso_" is 9 characters
    // - The total length including '~' at the end is 6 characters from the start
    
    // First check if string is long enough
    if (dir.length() < 9) {  // Must be at least as long as "/mnt/iso_"
        return dir;
    }
    
    // Verify the '_' is where we expect it
    if (dir[8] != '_') {
        return dir;
    }
    
    // Find the last '~'
    size_t lastTildePos = dir.find_last_of('~');
    if (lastTildePos == std::string::npos) {
        return dir;
    }
    
    // Extract everything between the known '_' position and the '~'
    return dir.substr(9, lastTildePos - 9);
}


// Function to unmount ISO files asynchronously
void unmountISO(const std::vector<std::string>& isoDirs, std::set<std::string>& unmountedFiles, std::set<std::string>& unmountedErrors, std::atomic<size_t>* completedTasks, std::atomic<size_t>* failedTasks) {
    std::atomic<bool> g_CancelledMessageAdded{false};

    // Early exit if cancelled before starting
    if (g_operationCancelled.load()) return;

    // Pre-define format strings to avoid repeated string constructions
    const std::string rootErrorPrefix = "\033[1;91mFailed to unmount: \033[1;93m'";
    const std::string rootErrorSuffix = "\033[1;93m'\033[1;91m.\033[0;1m {needsRoot}";
    
    const std::string successPrefix = "\033[0;1mUnmounted: \033[1;92m'";
    const std::string successSuffix = "\033[1;92m'\033[0m.";
    
    const std::string errorPrefix = "\033[1;91mFailed to unmount: \033[1;93m'";
    const std::string errorSuffix = "'\033[1;91m.\033[0;1m {notAnISO}";

    // Pre-allocate containers with estimated capacity
    const size_t estimatedSize = isoDirs.size();
    std::vector<std::pair<std::string, int>> unmountResults;
    std::vector<std::string> errorMessages;
    std::vector<std::string> successMessages;
    std::vector<std::string> removalMessages;
    
    unmountResults.reserve(estimatedSize);
    errorMessages.reserve(estimatedSize);
    successMessages.reserve(estimatedSize);
    removalMessages.reserve(estimatedSize);
    
    // Create a reusable string buffer
    std::string outputBuffer;
    outputBuffer.reserve(512);  // Reserve space for a typical message
    
    // Root check with cancellation awareness
    bool hasRoot = (geteuid() == 0);

    if (!hasRoot) {
        for (const auto& isoDir : isoDirs) {
            if (g_operationCancelled.load()) break;
            
            std::string modifiedDir = modifyDirectoryPath(isoDir);
            
            // Use string append operations instead of stringstream
            outputBuffer.clear();
            outputBuffer.append(rootErrorPrefix)
                       .append(modifiedDir)
                       .append(rootErrorSuffix);
            
            errorMessages.push_back(outputBuffer);
            failedTasks->fetch_add(1, std::memory_order_acq_rel);
        }
        
        // Lock and insert all Root related error messages
        if (!errorMessages.empty()) {
            std::lock_guard<std::mutex> lock(globalSetsMutex);
            unmountedErrors.insert(errorMessages.begin(), errorMessages.end());
        }
        
        // If no root, skip unmount operations entirely
        return;
    }

    // Perform unmount operations and record results
    for (const auto& isoDir : isoDirs) {
        if (g_operationCancelled.load()) {
            break;
        }
        
        int result = umount2(isoDir.c_str(), MNT_DETACH);
        unmountResults.emplace_back(isoDir, result);
    }

    // Process results only if not cancelled
    if (!g_operationCancelled.load()) {
        std::vector<std::string> successfulUnmounts;
        std::vector<std::string> failedUnmounts;
        
        successfulUnmounts.reserve(estimatedSize);
        failedUnmounts.reserve(estimatedSize);
        
        // Categorize unmount results
        for (const auto& [dir, result] : unmountResults) {
            bool isEmpty = isDirectoryEmpty(dir);
            if (result == 0 || isEmpty) {
                successfulUnmounts.push_back(dir);
            } else {
                failedUnmounts.push_back(dir);
            }
        }

        // Handle successful unmounts
        for (const auto& dir : successfulUnmounts) {
            if (isDirectoryEmpty(dir) && rmdir(dir.c_str()) == 0) {
                std::string modifiedDir = modifyDirectoryPath(dir);
                
                outputBuffer.clear();
                outputBuffer.append(successPrefix)
                           .append(modifiedDir)
                           .append(successSuffix);
                
                successMessages.push_back(outputBuffer);
                // Increment completed tasks for each success
                completedTasks->fetch_add(1, std::memory_order_acq_rel);
            }
        }

        // Handle failures
        for (const auto& dir : failedUnmounts) {
            std::string modifiedDir = modifyDirectoryPath(dir);
            
            outputBuffer.clear();
            outputBuffer.append(errorPrefix)
                       .append(modifiedDir)
                       .append(errorSuffix);
            
            errorMessages.push_back(outputBuffer);
            // Increment failed tasks for each failure
            failedTasks->fetch_add(1, std::memory_order_acq_rel);
        }
    }

    // Lock and insert all messages at once to reduce contention
    {
        std::lock_guard<std::mutex> lock(globalSetsMutex); // Protect the set
        
        // Use batch insertion for better performance
        if (!successMessages.empty()) {
            unmountedFiles.insert(successMessages.begin(), successMessages.end());
        }
        
        if (!removalMessages.empty()) {
            unmountedFiles.insert(removalMessages.begin(), removalMessages.end());
        }
        
        if (!errorMessages.empty()) {
            unmountedErrors.insert(errorMessages.begin(), errorMessages.end());
        }
    }
}


// Main function to send ISOs for unmount
void prepareUnmount(const std::string& input, std::vector<std::string>& currentFiles, std::set<std::string>& operationFiles, std::set<std::string>& operationFails, std::set<std::string>& uniqueErrorMessages, bool& umountMvRmBreak, bool& verbose) {
    // Setup signal handler
    setupSignalHandlerCancellations();
    
    g_operationCancelled.store(false);
    
    std::set<int> indicesToProcess;

    // Handle input ("00" = all files, else parse input)
    if (input == "00") {
        for (size_t i = 0; i < currentFiles.size(); ++i)
            indicesToProcess.insert(i + 1);
    } else {
        tokenizeInput(input, currentFiles, uniqueErrorMessages, indicesToProcess);
        if (indicesToProcess.empty()) {
            umountMvRmBreak = false;
            return;
        }
    }

    // Create selected files vector from indices
    std::vector<std::string> selectedMountpoints;
    selectedMountpoints.reserve(indicesToProcess.size());
    for (int index : indicesToProcess)
        selectedMountpoints.push_back(currentFiles[index - 1]);

    clearScrollBuffer();
    std::cout << "\n\033[0;1m Processing \033[1;93mumount\033[0;1m operations... (\033[1;91mCtrl + c\033[0;1m:cancel)\n";

    // Thread pool setup
    unsigned int numThreads = std::min(static_cast<unsigned int>(selectedMountpoints.size()), maxThreads);
    const size_t chunkSize = std::min(size_t(100), selectedMountpoints.size()/numThreads + 1);
    std::vector<std::vector<std::string>> chunks;

    // Split work into chunks
    for (size_t i = 0; i < selectedMountpoints.size(); i += chunkSize) {
        auto end = std::min(selectedMountpoints.begin() + i + chunkSize, selectedMountpoints.end());
        chunks.emplace_back(selectedMountpoints.begin() + i, end);
    }

    ThreadPool pool(numThreads);
    std::vector<std::future<void>> unmountFutures;
    std::atomic<size_t> completedTasks(0);
    std::atomic<size_t> failedTasks(0);
    std::atomic<bool> isProcessingComplete(false);

    // Start progress thread
    std::thread progressThread(
        displayProgressBarWithSize, 
        nullptr,
        static_cast<size_t>(0),
        &completedTasks,
        &failedTasks,
        selectedMountpoints.size(),
        &isProcessingComplete,
        &verbose
    );

    // Enqueue chunk tasks
    for (const auto& chunk : chunks) {
        unmountFutures.emplace_back(pool.enqueue([&, chunk]() {
            if (g_operationCancelled.load()) return;
            unmountISO(chunk, operationFiles, operationFails, &completedTasks, &failedTasks);
        }));
    }

    // Wait for completion or cancellation
    for (auto& future : unmountFutures) {
        future.wait();
        if (g_operationCancelled.load()) break;
    }

    // Cleanup
    isProcessingComplete.store(true);
    progressThread.join();
}

