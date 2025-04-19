/**********************************************************************
  Filename    : SDMMC Remote Access
  Description : The SD card is accessed remotely via WiFi
  Based on    : www.freenove.com
  **********************************************************************/
#include "Arduino.h"
#include "sd_read_write.h"
#include "SD_MMC.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <secrets.h>  // 重要: 您需要创建此文件，包含WiFi凭据 (见下方说明)
#include <esp_task_wdt.h>  // Add watchdog timer support
#include <ESPmDNS.h>

/* 重要说明: 
 * 请在src目录下创建一个名为"secrets.h"的文件，内容如下:
 * 
 * #ifndef SECRETS_H
 * #define SECRETS_H
 * 
 * // WiFi连接凭据
 * const char* ssid = "您的WiFi名称";
 * const char* password = "您的WiFi密码";
 * 
 * #endif
 */

#define SD_MMC_CMD 38 // Please do not modify it.
#define SD_MMC_CLK 39 // Please do not modify it.
#define SD_MMC_D0 40  // Please do not modify it.

#define STATUS_LED 2  // Built-in LED on most ESP32 boards

// 创建Web服务器，端口80
AsyncWebServer server(80);

// HTML页面
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <title>ESP32 SD卡文件管理</title>
  <!-- 网站图标，使用在线网址 -->
  <link rel="icon" href="https://cdn.jsdelivr.net/gh/twitter/twemoji@14.0.2/assets/72x72/1f4be.png">
  <style>
    :root {
      --primary-color: #4a89dc;
      --secondary-color: #5cb85c;
      --accent-color: #f0ad4e;
      --danger-color: #d9534f;
      --light-bg: #f8f9fa;
      --dark-text: #333;
      --border-radius: 8px;
      --box-shadow: 0 2px 5px rgba(0,0,0,0.1);
      --transition: all 0.3s ease;
    }
    
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }
    
    body { 
      font-family: 'Segoe UI', Arial, sans-serif; 
      text-align: center; 
      margin: 0 auto;
      padding: 15px;
      background-color: #f5f7fa;
      color: var(--dark-text);
      line-height: 1.6;
      max-width: 1200px;
    }
    
    h1, h2, h3 {
      margin-bottom: 20px;
      color: var(--primary-color);
    }
    
    h1 {
      margin-top: 20px;
      font-size: 2.2em;
      border-bottom: 2px solid var(--primary-color);
      padding-bottom: 10px;
      display: inline-block;
    }
    
    .container {
      padding: 20px;
      margin-bottom: 30px;
      background-color: white;
      border-radius: var(--border-radius);
      box-shadow: var(--box-shadow);
    }
    
    .button {
      padding: 10px 20px;
      font-size: 16px;
      margin: 5px;
      cursor: pointer;
      background-color: var(--primary-color);
      color: white;
      border: none;
      border-radius: var(--border-radius);
      transition: var(--transition);
    }
    
    .button:hover {
      background-color: #3a79cc;
      transform: translateY(-2px);
    }
    
    .button-green {
      background-color: var(--secondary-color);
    }
    
    .button-green:hover {
      background-color: #4cae4c;
    }
    
    .button-orange {
      background-color: var(--accent-color);
    }
    
    .button-orange:hover {
      background-color: #ec971f;
    }
    
    .button-danger {
      background-color: var(--danger-color);
    }
    
    .button-danger:hover {
      background-color: #c9302c;
    }
    
    /* 下载按钮特殊样式 */
    .button-download {
      background-color: #5bc0de;  /* 使用不同于普通按钮的颜色 */
      color: white;  /* 确保文本是白色，与蓝色背景形成对比 */
      font-weight: bold;
    }
    
    .button-download:hover {
      background-color: #46b8da;
    }
    
    input[type="text"], input[type="file"] {
      padding: 10px;
      margin: 10px 0;
      border: 1px solid #ddd;
      border-radius: var(--border-radius);
      width: 100%;
      max-width: 400px;
    }
    
    .file {
      background-color: var(--light-bg);
      margin: 10px 0;
      padding: 15px;
      border-radius: var(--border-radius);
      text-align: left;
      display: flex;
      justify-content: space-between;
      align-items: center;
      box-shadow: var(--box-shadow);
      transition: var(--transition);
    }
    
    .file:hover {
      transform: translateX(5px);
      background-color: #e9ecef;
    }
    
    .file-content {
      flex-grow: 1;
    }
    
    .file-actions {
      display: flex;
      gap: 10px;
    }
    
    .file a {
      text-decoration: none;
      color: var(--primary-color);
      font-weight: bold;
    }
    
    .dir {
      background-color: #e8f0fe;
      border-left: 4px solid var(--primary-color);
    }
    
    .dir:hover {
      background-color: #d8e5fd;
    }
    
    .upload-form {
      margin: 20px 0;
      padding: 20px;
      border: 1px solid #ddd;
      border-radius: var(--border-radius);
      background-color: white;
      box-shadow: var(--box-shadow);
    }
    
    .server-info {
      background-color: #fff8e1;
      padding: 20px;
      border-radius: var(--border-radius);
      margin-bottom: 30px;
      box-shadow: var(--box-shadow);
      border-left: 4px solid var(--accent-color);
    }
    
    .qrcode {
      margin: 20px auto;
      padding: 10px;
      background-color: white;
      display: inline-block;
      border-radius: var(--border-radius);
      box-shadow: var(--box-shadow);
    }
    
    .qrcode img {
      max-width: 100%;
      border-radius: calc(var(--border-radius) - 4px);
    }
    
    /* 进度条样式 */
    .progress-container {
      width: 100%;
      background-color: #eee;
      border-radius: 20px;
      margin: 15px 0;
      padding: 3px;
      display: none;
      box-shadow: inset 0 1px 3px rgba(0,0,0,0.1);
    }
    
    .progress-bar {
      height: 24px;
      border-radius: 20px;
      background: linear-gradient(90deg, var(--secondary-color), #7ac77a);
      width: 0%;
      text-align: center;
      line-height: 24px;
      color: white;
      transition: width 0.5s ease;
      font-weight: bold;
      box-shadow: 0 1px 2px rgba(0,0,0,0.1);
    }
    
    #uploadStatus {
      margin-top: 10px;
      font-weight: bold;
    }
    
    /* 响应式设计 */
    @media (max-width: 768px) {
      .file {
        flex-direction: column;
        align-items: flex-start;
      }
      
      .file-actions {
        margin-top: 10px;
        width: 100%;
        justify-content: flex-start;
      }
      
      .button {
        padding: 8px 16px;
        font-size: 14px;
      }
    }
    
    /* 顶部导航栏 */
    .navbar {
      background-color: var(--primary-color);
      padding: 15px;
      margin: -15px -15px 20px -15px;
      color: white;
      box-shadow: 0 2px 5px rgba(0,0,0,0.2);
    }
    
    .navbar h1 {
      margin: 0;
      padding: 0;
      border: none;
      color: white;
    }
    
    /* 页脚 */
    .footer {
      margin-top: 40px;
      padding: 20px;
      text-align: center;
      color: #6c757d;
      font-size: 14px;
      border-top: 1px solid #ddd;
    }
  </style>
</head>
<body>
  <div class="navbar">
    <h1>ESP32 SD卡文件浏览器</h1>
  </div>
  
  <div class="server-info container">
    <h3>服务器信息</h3>
    <p>IP地址: <strong id="serverIP">正在获取...</strong></p>
    <div class="qrcode" id="qrcode"></div>
    <p>扫描二维码或在浏览器中访问上面的地址来连接到此服务器</p>
  </div>
  
  <div id="currentPath" class="container"></div>
  <div id="fileList" class="container"></div>
  
  <div class="upload-form container">
    <h3>上传文件</h3>
    <form id="uploadForm" enctype="multipart/form-data">
      <input type="file" name="file" id="file" class="button">
      <br>
      <input type="button" value="上传" onclick="uploadFile()" class="button button-green">
    </form>
    <div class="progress-container" id="progressContainer">
      <div class="progress-bar" id="progressBar">0%</div>
    </div>
    <div id="uploadStatus"></div>
  </div>

  <div id="createDir" class="upload-form container">
    <h3>创建文件夹</h3>
    <input type="text" id="dirName" placeholder="文件夹名称">
    <br>
    <button onclick="createDirectory()" class="button button-orange">创建</button>
  </div>

  <div class="footer">
    ESP32 SD卡文件管理器 &copy; liuweiqing@2025
  </div>

  <script>
    let currentPath = "/";
    
    // 页面加载时获取文件列表和服务器IP
    window.onload = function() {
      loadFileList(currentPath);
      fetchServerIP();
    };
    
    // 获取服务器IP
    function fetchServerIP() {
      fetch('/serverinfo')
        .then(response => response.json())
        .then(data => {
          const serverIP = data.ip;
          document.getElementById('serverIP').innerText = serverIP;
          generateQR(serverIP);
        })
        .catch(error => {
          console.error('Error fetching server IP:', error);
          document.getElementById('serverIP').innerText = 'IP获取失败';
        });
    }
    
    // 加载指定路径下的文件列表
    function loadFileList(path) {
      currentPath = path;
      document.getElementById('currentPath').innerHTML = '<h2>当前路径: ' + currentPath + '</h2>';
      
      if(currentPath != "/") {
        document.getElementById('currentPath').innerHTML += 
          '<button onclick="loadFileList(\'' + getParentDirectory(currentPath) + '\')" class="button">返回上级目录</button>';
      }
      
      fetch('/list?dir=' + encodeURIComponent(path))
        .then(response => response.json())
        .then(data => {
          let html = '';
          
          // 添加目录
          data.directories.forEach(dir => {
            html += '<div class="file dir">';
            html += '<div class="file-content">';
            html += '<a href="#" onclick="loadFileList(\'' + (currentPath == '/' ? currentPath + dir : currentPath + '/' + dir) + '\')">';
            html += '<strong>📁 ' + dir + '</strong>';
            html += '</a>';
            html += '</div>';
            html += '<div class="file-actions">';
            html += '<button onclick="deleteItem(\'' + (currentPath == '/' ? currentPath + dir : currentPath + '/' + dir) + '\', true)" class="button button-danger">删除</button>';
            html += '</div>';
            html += '</div>';
          });
          
          // 添加文件
          data.files.forEach(file => {
            html += '<div class="file">';
            html += '<div class="file-content">';
            html += '<strong>📄 ' + file.name + '</strong> (' + formatBytes(file.size) + ')';
            html += '</div>';
            html += '<div class="file-actions">';
            html += '<a href="/download?path=' + encodeURIComponent((currentPath == '/' ? currentPath + file.name : currentPath + '/' + file.name)) + '" class="button button-download">下载</a> ';
            html += '<button onclick="deleteItem(\'' + (currentPath == '/' ? currentPath + file.name : currentPath + '/' + file.name) + '\', false)" class="button button-danger">删除</button>';
            html += '</div>';
            html += '</div>';
          });
          
          if (html === '') {
            html = '<p>此文件夹为空</p>';
          }
          
          document.getElementById('fileList').innerHTML = html;
        })
        .catch(error => {
          console.error('Error loading file list:', error);
          document.getElementById('fileList').innerHTML = '<p>无法加载文件列表</p>';
        });
    }

    // 获取上级目录路径
    function getParentDirectory(path) {
      if (path === '/' || !path.includes('/')) return '/';
      const pathWithoutTrailingSlash = path.endsWith('/') ? path.slice(0, -1) : path;
      const parentDir = pathWithoutTrailingSlash.substring(0, pathWithoutTrailingSlash.lastIndexOf('/'));
      return parentDir === '' ? '/' : parentDir;
    }

    // 上传文件
    function uploadFile() {
      const fileInput = document.getElementById('file');
      const file = fileInput.files[0];
      if (!file) {
        document.getElementById('uploadStatus').textContent = '请选择文件';
        return;
      }

      // 记录当前路径和文件信息
      const uploadPath = currentPath;
      console.log(`Uploading to directory: ${uploadPath}`);
      
      const formData = new FormData();
      formData.append('file', file);
      formData.append('path', uploadPath); // 明确添加当前路径参数

      document.getElementById('uploadStatus').textContent = `准备上传到 ${uploadPath}...`;
      
      // 显示进度条
      const progressContainer = document.getElementById('progressContainer');
      const progressBar = document.getElementById('progressBar');
      progressContainer.style.display = 'block';
      progressBar.style.width = '0%';
      progressBar.textContent = '0%';
      
      // 创建 XHR 对象以跟踪进度
      const xhr = new XMLHttpRequest();
      
      // 进度事件监听
      xhr.upload.addEventListener('progress', (event) => {
        if (event.lengthComputable) {
          const percentComplete = Math.round((event.loaded / event.total) * 100);
          progressBar.style.width = percentComplete + '%';
          progressBar.textContent = percentComplete + '%';
          document.getElementById('uploadStatus').textContent = `上传中: ${formatBytes(event.loaded)} / ${formatBytes(event.total)}`;
        }
      });
      
      xhr.addEventListener('load', () => {
        if (xhr.status === 200) {
          document.getElementById('uploadStatus').textContent = '上传成功!';
          console.log(`File uploaded to: ${uploadPath}`);
          setTimeout(() => {
            loadFileList(currentPath); // 刷新文件列表
          }, 1000);
        } else {
          document.getElementById('uploadStatus').textContent = '上传失败: ' + xhr.statusText;
        }
      });
      
      xhr.addEventListener('error', () => {
        document.getElementById('uploadStatus').textContent = '上传错误，请检查网络连接';
      });
      
      xhr.addEventListener('abort', () => {
        document.getElementById('uploadStatus').textContent = '上传已取消';
      });
      
      // 添加调试信息到URL
      xhr.open('POST', '/upload?path=' + encodeURIComponent(uploadPath));
      xhr.send(formData);
    }

    // 删除文件或目录
    function deleteItem(path, isDirectory) {
      if (confirm('确定要删除 ' + path + ' 吗?')) {
        fetch('/delete', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
          },
          body: 'path=' + encodeURIComponent(path) + '&isDirectory=' + isDirectory
        })
        .then(response => response.text())
        .then(result => {
          alert(result);
          loadFileList(currentPath); // 刷新文件列表
        })
        .catch(error => {
          alert('删除失败: ' + error);
        });
      }
    }

    // 创建目录
    function createDirectory() {
      const dirName = document.getElementById('dirName').value;
      if (!dirName) {
        alert('请输入文件夹名称');
        return;
      }

      fetch('/mkdir', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded',
        },
        body: 'path=' + encodeURIComponent(currentPath) + '&dirname=' + encodeURIComponent(dirName)
      })
      .then(response => response.text())
      .then(result => {
        alert(result);
        document.getElementById('dirName').value = '';
        loadFileList(currentPath); // 刷新文件列表
      })
      .catch(error => {
        alert('创建文件夹失败: ' + error);
      });
    }

    // 格式化文件大小显示
    function formatBytes(bytes) {
      if (bytes === 0) return '0 Bytes';
      const k = 1024;
      const sizes = ['Bytes', 'KB', 'MB', 'GB'];
      const i = Math.floor(Math.log(bytes) / Math.log(k));
      return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    // 生成QR码
    function generateQR() {
      const serverIP = document.getElementById('serverIP').innerText;
      const qrUrl = `https://api.qrserver.com/v1/create-qr-code/?size=150x150&data=http://${serverIP}/`;
      document.getElementById('qrcode').innerHTML = `<img src="${qrUrl}" alt="Server QR Code">`;
    }
  </script>
</body>
</html>
)rawliteral";

// 初始化SD卡
bool initSDCard() {
  Serial.println("  - Begin SD_MMC mounting...");
  
  // Try to initialize with minimal settings first
  if (!SD_MMC.begin("/sdcard", true)) {  // 1-bit mode
    Serial.println("  - Basic mount failed, trying with detailed parameters...");
    
    // More detailed initialization with all parameters
    if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5)) {
      Serial.println("  - Detailed mount also failed");
      return false;
    }
  }
  
  Serial.println("  - SD card mount point created");

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("  - No card detected though mount succeeded");
    return false;
  }

  Serial.println("Card Mounted Successfully");
  Serial.print("SD_MMC Card Type: ");
  if (cardType == CARD_MMC) {
    Serial.println("MMC");
  } else if (cardType == CARD_SD) {
    Serial.println("SDSC");
  } else if (cardType == CARD_SDHC) {
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
  Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
  return true;
}

// 获取文件类型
String getContentType(String filename) {
  if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".jpeg")) return "image/jpeg";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/pdf";
  else if (filename.endsWith(".zip")) return "application/zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  else if (filename.endsWith(".json")) return "application/json";
  return "text/plain";
}

void blinkIP(IPAddress ip) {
  Serial.println("\n\n********************************************");
  Serial.println("*                                          *");
  Serial.print("*       SERVER IP: ");
  Serial.print(ip);
  Serial.println("           *");
  Serial.println("*                                          *");
  Serial.println("********************************************\n");
  
  // Visual indication with LED
  pinMode(STATUS_LED, OUTPUT);
  
  // Signal system ready with 3 quick blinks
  for (int i = 0; i < 3; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(100);
    digitalWrite(STATUS_LED, LOW);
    delay(100);
  }
  
  delay(1000); // Pause before IP signal
}

void setup() {
    // Initialize serial first thing
    Serial.begin(115200);
    
    // Long delay to make sure monitor connects
    for(int i=0; i<5; i++) {
        Serial.print(".");
        delay(500);
    }
    
    Serial.println("\n\n=== ESP32-S3 SD Card Server Starting ===");
    
    // Basic pin check
    Serial.println("Using pins:");
    Serial.printf("CMD: %d, CLK: %d, D0: %d\n", SD_MMC_CMD, SD_MMC_CLK, SD_MMC_D0);
    
    // Try watchdog setup
    Serial.println("Setting up watchdog...");
    try {
        esp_task_wdt_init(15, true); // 15 seconds timeout, panic on timeout
        esp_task_wdt_add(NULL);      // Add current thread to WDT watch
        Serial.println("Watchdog initialized successfully");
    } catch(...) {
        Serial.println("Watchdog initialization failed, continuing without it");
    }
    
    // 初始化SD卡
    Serial.println("Setting SD_MMC pins...");
    SD_MMC.setPins(SD_MMC_CLK, SD_MMC_CMD, SD_MMC_D0);
    Serial.println("Pins set, now initializing SD card...");
    
    // Initialize SD with proper error handling
    bool sdInitialized = false;
    for(int retry=0; retry<3; retry++) {
        Serial.printf("SD init attempt %d/3...\n", retry+1);
        if (initSDCard()) {
            sdInitialized = true;
            break;
        }
        delay(1000);
    }
    
    if (!sdInitialized) {
        Serial.println("SD card initialization failed after retries");
        Serial.println("System will continue without SD card capabilities");
        // Continue with WiFi setup anyway
    }
    
    // 设置WiFi接入点模式
    Serial.println("Setting up WiFi access point...");
    // WiFi.softAP(ssid, password);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    // IPAddress IP = WiFi.softAPIP();
    
    // Visual indication of the IP address
    // blinkIP(IP);
    blinkIP(WiFi.localIP());
    if (MDNS.begin("esp32"))
    {
      Serial.println("mDNS 已启动，访问：http://esp32.local");
    }
    else
    {
      Serial.println("mDNS 启动失败");
    }
    // 设置Web服务器路由
    Serial.println("Setting up web server routes...");
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        Serial.println("Serving index page");
        request->send(200, "text/html", index_html);
    });

    // 添加获取服务器IP的端点
    server.on("/serverinfo", HTTP_GET, [](AsyncWebServerRequest *request){
        DynamicJsonDocument doc(256);
        String ip;
        
        if (WiFi.status() == WL_CONNECTED) {
            ip = WiFi.localIP().toString();
        } else {
            ip = WiFi.softAPIP().toString();
        }
        
        doc["ip"] = ip;
        doc["hostname"] = "esp32.local";
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // 列出目录内容
    server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
        String dirPath = "/";
        if (request->hasParam("dir")) {
            dirPath = request->getParam("dir")->value();
        }

        File root = SD_MMC.open(dirPath);
        if (!root) {
            request->send(404, "text/plain", "Directory not found");
            return;
        }

        if (!root.isDirectory()) {
            request->send(400, "text/plain", "Not a directory");
            return;
        }

        DynamicJsonDocument doc(8192);
        JsonArray directories = doc["directories"].to<JsonArray>();
        JsonArray files = doc["files"].to<JsonArray>();

        File file = root.openNextFile();
        while (file) {
            if (file.isDirectory()) {
                String dirName = String(file.name());
                dirName = dirName.substring(dirName.lastIndexOf('/') + 1);
                directories.add(dirName);
            } else {
              JsonObject fileObj = files.add<JsonObject>();
              String fileName = String(file.name());
              fileName = fileName.substring(fileName.lastIndexOf('/') + 1);
              fileObj["name"] = fileName;
              fileObj["size"] = file.size();
            }
            file = root.openNextFile();
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // 下载文件
    server.on("/download", HTTP_GET, [](AsyncWebServerRequest *request){
        if (!request->hasParam("path")) {
            request->send(400, "text/plain", "Missing file path");
            return;
        }

        String path = request->getParam("path")->value();
        if (!SD_MMC.exists(path)) {
            request->send(404, "text/plain", "File not found");
            return;
        }

        File file = SD_MMC.open(path);
        if (!file) {
            request->send(500, "text/plain", "Failed to open file for reading");
            return;
        }

        String fileName = path;
        if (path.lastIndexOf('/') >= 0) {
            fileName = path.substring(path.lastIndexOf('/') + 1);
        }

        AsyncWebServerResponse *response = request->beginResponse(SD_MMC, path, getContentType(fileName));
        response->addHeader("Content-Disposition", "attachment; filename=" + fileName);
        request->send(response);
    });

    // 上传文件
    server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200);
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        static File uploadFile;
        static String uploadPath;
        
        if (!index) {
            // 获取上传路径参数，两个地方都尝试获取
            String path = "/";
            
            // 从URL查询参数获取
            if (request->hasParam("path")) {
                path = request->getParam("path")->value();
                Serial.println("Path from URL param: " + path);
            }
            
            // 从表单数据获取，优先级更高
            if (request->hasParam("path", true)) {
                path = request->getParam("path", true)->value();
                Serial.println("Path from form data: " + path);
            }
            
            // 修复: 确保路径以斜杠结尾，除非是根目录
            if (path != "/" && !path.endsWith("/")) {
                path += "/";
            }
            
            Serial.print("Final upload directory: ");
            Serial.println(path);
            
            // 构建完整文件路径
            uploadPath = path + filename;
            
            Serial.print("Upload Start: ");
            Serial.println(uploadPath);
            
            // 确保目录存在
            if (path != "/" && !SD_MMC.exists(path)) {
                if (createDir(SD_MMC, path.c_str())) {
                    Serial.println("Created directory: " + path);
                } else {
                    Serial.println("Failed to create directory: " + path);
                }
            }
            
            // 打开文件进行写入
            uploadFile = SD_MMC.open(uploadPath, FILE_WRITE);
            
            if (!uploadFile) {
                Serial.println("Failed to open file for writing: " + uploadPath);
            }
        }
        
        if (uploadFile) {
            uploadFile.write(data, len);
        }
        
        if (final) {
            if (uploadFile) {
                uploadFile.close();
                request->send(200, "text/plain", "File uploaded successfully to " + uploadPath);
                Serial.println("Upload Complete: " + uploadPath);
            } else {
                request->send(500, "text/plain", "Could not create file on SD card");
                Serial.println("Upload Failed");
            }
        }
    });

    // 删除文件或目录
    server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!request->hasParam("path", true)) {
            request->send(400, "text/plain", "Missing path");
            return;
        }
        
        String path = request->getParam("path", true)->value();
        bool isDirectory = false;
        
        if (request->hasParam("isDirectory", true)) {
            isDirectory = request->getParam("isDirectory", true)->value() == "true";
        }

        bool success = false;
        if (isDirectory) {
            success = removeDir(SD_MMC, path.c_str());
        } else {
            success = SD_MMC.remove(path.c_str());
        }
        
        if (success) {
            request->send(200, "text/plain", "Deleted successfully");
        } else {
            request->send(500, "text/plain", "Failed to delete");
        }
    });

    // 创建目录
    server.on("/mkdir", HTTP_POST, [](AsyncWebServerRequest *request){
        if (!request->hasParam("path", true) || !request->hasParam("dirname", true)) {
            request->send(400, "text/plain", "Missing path or directory name");
            return;
        }
        
        String path = request->getParam("path", true)->value();
        String dirname = request->getParam("dirname", true)->value();
        
        if (path != "/" && !path.endsWith("/")) path += "/";
        String fullPath = path + dirname;
        
        if (createDir(SD_MMC, fullPath.c_str())) {
            request->send(200, "text/plain", "Directory created");
        } else {
            request->send(500, "text/plain", "Failed to create directory");
        }
    });

    // 开始Web服务器
    Serial.println("Starting web server...");
    server.begin();
    Serial.println("HTTP server started successfully");
    Serial.println("System is now running!");
}

void loop() {
    // Reset the watchdog timer regularly to prevent timeout
    try {
        esp_task_wdt_reset();
    } catch(...) {
        // Ignore errors if watchdog wasn't initialized properly
    }
    
    // 主循环保持空闲，Web服务器在后台运行
    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 10000) { // Print status every 10 seconds
        lastMsg = millis();
        Serial.print(".");  // Minimalist heartbeat indicator
    }
    delay(100); // Small delay to prevent watchdog issues
}