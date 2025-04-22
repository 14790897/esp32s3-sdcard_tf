#include "sd_read_write.h"

// Global PSRAM buffer for file operations
PSRAMBuffer g_psramBuffer;

void listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
  Serial.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if (!root)
  {
    Serial.println("Failed to open directory");
    return;
  }
  if (!root.isDirectory())
  {
    Serial.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file)
  {
    if (file.isDirectory())
    {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels)
      {
        listDir(fs, file.path(), levels - 1);
      }
    }
    else
    {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

bool removeDir(fs::FS &fs, const char *path)
{
  return fs.rmdir(path); // rmdir 返回 bool
}
bool createDir(fs::FS &fs, const char *path)
{
  return fs.mkdir(path); // mkdir 返回 bool
}

void readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while (file.available())
  {
    Serial.write(file.read());
  }
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
}

void appendFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }
  if (file.print(message))
  {
    Serial.println("Message appended");
  }
  else
  {
    Serial.println("Append failed");
  }
}

void renameFile(fs::FS &fs, const char *path1, const char *path2)
{
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2))
  {
    Serial.println("File renamed");
  }
  else
  {
    Serial.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char *path)
{
  Serial.printf("Deleting file: %s\n", path);
  if (fs.remove(path))
  {
    Serial.println("File deleted");
  }
  else
  {
    Serial.println("Delete failed");
  }
}

void testFileIO(fs::FS &fs, const char *path)
{
  File file = fs.open(path);
  static uint8_t buf[512];
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file)
  {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len)
    {
      size_t toRead = len;
      if (toRead > 512)
      {
        toRead = 512;
      }
      file.read(buf, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms\r\n", flen, end);
    file.close();
  }
  else
  {
    Serial.println("Failed to open file for reading");
  }

  file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t i;
  start = millis();
  for (i = 0; i < 2048; i++)
  {
    file.write(buf, 512);
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms\n", 2048 * 512, end);
  file.close();
}

// Enhanced file I/O functions using PSRAM buffer
void readFile_PSRAM(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file with PSRAM buffer: %s\n", path);

  // Initialize PSRAM buffer if not already done
  if (!g_psramBuffer.isInitialized())
  {
    if (!g_psramBuffer.init())
    {
      Serial.println("Failed to initialize PSRAM buffer, falling back to standard function");
      readFile(fs, path);
      return;
    }
    Serial.printf("PSRAM buffer initialized: %u bytes\n", g_psramBuffer.getSize());
    Serial.printf("Buffer is in %s\n", g_psramBuffer.isPSRAM() ? "PSRAM" : "regular memory");
  }

  File file = fs.open(path);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  uint8_t *buffer = g_psramBuffer.getBuffer();
  size_t bufferSize = g_psramBuffer.getSize();
  size_t bytesRead = 0;
  size_t totalBytesRead = 0;
  uint32_t startTime = millis();

  Serial.print("Read from file: ");
  while ((bytesRead = file.read(buffer, bufferSize)) > 0)
  {
    totalBytesRead += bytesRead;
    // For demonstration, we'll just print the first 100 bytes
    if (totalBytesRead <= 100)
    {
      for (size_t i = 0; i < min(bytesRead, (size_t)100); i++)
      {
        Serial.write(buffer[i]);
      }
    }
  }

  uint32_t endTime = millis();
  Serial.printf("\nRead %u bytes in %u ms (%.2f KB/s)\n",
                totalBytesRead,
                endTime - startTime,
                totalBytesRead / (float)(endTime - startTime));

  file.close();
}

void writeFile_PSRAM(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file with PSRAM buffer: %s\n", path);

  // Initialize PSRAM buffer if not already done
  if (!g_psramBuffer.isInitialized())
  {
    if (!g_psramBuffer.init())
    {
      Serial.println("Failed to initialize PSRAM buffer, falling back to standard function");
      writeFile(fs, path, message);
      return;
    }
    Serial.printf("PSRAM buffer initialized: %u bytes\n", g_psramBuffer.getSize());
    Serial.printf("Buffer is in %s\n", g_psramBuffer.isPSRAM() ? "PSRAM" : "regular memory");
  }

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }

  size_t messageLen = strlen(message);
  uint8_t *buffer = g_psramBuffer.getBuffer();
  size_t bufferSize = g_psramBuffer.getSize();

  // Copy message to PSRAM buffer
  size_t bytesToWrite = min(messageLen, bufferSize);
  memcpy(buffer, message, bytesToWrite);

  uint32_t startTime = millis();
  bool success = file.write(buffer, bytesToWrite) == bytesToWrite;
  uint32_t endTime = millis();

  if (success)
  {
    Serial.printf("File written: %u bytes in %u ms (%.2f KB/s)\n",
                  bytesToWrite,
                  endTime - startTime,
                  bytesToWrite / (float)(endTime - startTime));
  }
  else
  {
    Serial.println("Write failed");
  }

  file.close();
}

void appendFile_PSRAM(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Appending to file with PSRAM buffer: %s\n", path);

  // Initialize PSRAM buffer if not already done
  if (!g_psramBuffer.isInitialized())
  {
    if (!g_psramBuffer.init())
    {
      Serial.println("Failed to initialize PSRAM buffer, falling back to standard function");
      appendFile(fs, path, message);
      return;
    }
    Serial.printf("PSRAM buffer initialized: %u bytes\n", g_psramBuffer.getSize());
    Serial.printf("Buffer is in %s\n", g_psramBuffer.isPSRAM() ? "PSRAM" : "regular memory");
  }

  File file = fs.open(path, FILE_APPEND);
  if (!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }

  size_t messageLen = strlen(message);
  uint8_t *buffer = g_psramBuffer.getBuffer();
  size_t bufferSize = g_psramBuffer.getSize();

  // Copy message to PSRAM buffer
  size_t bytesToWrite = min(messageLen, bufferSize);
  memcpy(buffer, message, bytesToWrite);

  uint32_t startTime = millis();
  bool success = file.write(buffer, bytesToWrite) == bytesToWrite;
  uint32_t endTime = millis();

  if (success)
  {
    Serial.printf("Message appended: %u bytes in %u ms (%.2f KB/s)\n",
                  bytesToWrite,
                  endTime - startTime,
                  bytesToWrite / (float)(endTime - startTime));
  }
  else
  {
    Serial.println("Append failed");
  }

  file.close();
}

void testFileIO_PSRAM(fs::FS &fs, const char *path)
{
  // Initialize PSRAM buffer if not already done
  if (!g_psramBuffer.isInitialized())
  {
    if (!g_psramBuffer.init())
    {
      Serial.println("Failed to initialize PSRAM buffer, falling back to standard function");
      testFileIO(fs, path);
      return;
    }
    Serial.printf("PSRAM buffer initialized: %u bytes\n", g_psramBuffer.getSize());
    Serial.printf("Buffer is in %s\n", g_psramBuffer.isPSRAM() ? "PSRAM" : "regular memory");
  }

  uint8_t *buffer = g_psramBuffer.getBuffer();
  size_t bufferSize = g_psramBuffer.getSize();

  // Read test
  File file = fs.open(path);
  size_t len = 0;
  uint32_t start = millis();
  uint32_t end = start;
  if (file)
  {
    len = file.size();
    size_t flen = len;
    start = millis();
    while (len)
    {
      size_t toRead = len;
      if (toRead > bufferSize)
      {
        toRead = bufferSize;
      }
      file.read(buffer, toRead);
      len -= toRead;
    }
    end = millis() - start;
    Serial.printf("%u bytes read for %u ms (%.2f KB/s)\r\n",
                  flen, end, flen / (float)end);
    file.close();
  }
  else
  {
    Serial.println("Failed to open file for reading");
  }

  // Write test
  file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }

  // Fill buffer with random data
  for (size_t i = 0; i < bufferSize; i++)
  {
    buffer[i] = random(0, 255);
  }

  size_t i;
  start = millis();
  // Write 2MB of data in chunks of bufferSize
  size_t totalBytes = 2 * 1024 * 1024;
  size_t bytesWritten = 0;
  while (bytesWritten < totalBytes)
  {
    size_t toWrite = min(bufferSize, totalBytes - bytesWritten);
    file.write(buffer, toWrite);
    bytesWritten += toWrite;
  }
  end = millis() - start;
  Serial.printf("%u bytes written for %u ms (%.2f KB/s)\n",
                bytesWritten, end, bytesWritten / (float)end);
  file.close();
}