#ifndef __PSRAM_BUFFER_H
#define __PSRAM_BUFFER_H

#include "Arduino.h"
#include <esp_heap_caps.h>

// Buffer size for SD card operations (adjust as needed)
// 32KB is a good balance between performance and memory usage
#define PSRAM_BUFFER_SIZE (32 * 1024)

// Class to manage PSRAM buffer for SD card operations
class PSRAMBuffer {
private:
    uint8_t* buffer;
    size_t bufferSize;
    bool initialized;

public:
    PSRAMBuffer(size_t size = PSRAM_BUFFER_SIZE) :
        buffer(nullptr),
        bufferSize(size),
        initialized(false) {
    }

    ~PSRAMBuffer() {
        if (buffer != nullptr) {
            free(buffer);
            buffer = nullptr;
        }
    }

    bool init() {
        if (initialized) return true;

        if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > bufferSize) {
            buffer = (uint8_t*)heap_caps_malloc(bufferSize, MALLOC_CAP_SPIRAM);
            if (buffer != nullptr) {
                initialized = true;
                return true;
            }
        }

        // Fallback to regular memory if PSRAM allocation fails
        buffer = (uint8_t*)malloc(bufferSize);
        if (buffer != nullptr) {
            initialized = true;
            return true;
        }

        return false;
    }

    uint8_t* getBuffer() {
        return buffer;
    }

    size_t getSize() {
        return bufferSize;
    }

    bool isInitialized() {
        return initialized;
    }

    bool isPSRAM() {
        return heap_caps_check_integrity_addr((intptr_t)buffer, true);
    }
};

// Function to print PSRAM information
inline void printPSRAMInfo() {
    Serial.println("\n=== PSRAM Information ===");

    // Check if PSRAM is enabled
    if (psramFound()) {
        Serial.println("PSRAM is enabled");

        // Get PSRAM size and usage
        size_t psramSize = ESP.getPsramSize();
        size_t freePsram = ESP.getFreePsram();

        Serial.printf("Total PSRAM: %u bytes (%.2f KB / %.2f MB)\n",
                     psramSize, psramSize / 1024.0, psramSize / (1024.0 * 1024.0));
        Serial.printf("Free PSRAM: %u bytes (%.2f KB / %.2f MB)\n",
                     freePsram, freePsram / 1024.0, freePsram / (1024.0 * 1024.0));
        Serial.printf("Used PSRAM: %u bytes (%.2f KB / %.2f MB)\n",
                     psramSize - freePsram,
                     (psramSize - freePsram) / 1024.0,
                     (psramSize - freePsram) / (1024.0 * 1024.0));
        Serial.printf("PSRAM Usage: %.2f%%\n",
                     (psramSize - freePsram) * 100.0 / psramSize);
    } else {
        Serial.println("PSRAM is not enabled or not found");
    }

    Serial.println("=========================\n");
}

#endif // __PSRAM_BUFFER_H
