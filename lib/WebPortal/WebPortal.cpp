#include "WebPortal.h"
#include "ConfigManager.h" // Para acceder a la 'config' global
#include "SDManager.h"     // Para acceder a sdManager
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <WiFi.h>

/**
 * @brief Constructor. Inicializa la referencia al servidor y al SDManager.
 */
WebPortal::WebPortal(SDManager& sdMgr)
    : server(80), 
      sdManager(sdMgr) {
}

/**
 * @brief Inicia el modo Access Point (AP) para configuración.
 */
void WebPortal::beginAPMode() {
    _isAPMode = true;

    // Genera un nombre de host y SSID único
    String uniqueSuffix = getUniqueSuffix();
    String apSSID = "ArandanoIRT-" + uniqueSuffix;
    String hostname = "arandanoirt-" + uniqueSuffix;

    WiFi.setHostname(hostname.c_str());
    
    // Iniciar el Access Point
    WiFi.softAP(apSSID.c_str()); 
    IPAddress apIP = WiFi.softAPIP();
    
    // --- Salidas de terminal en Inglés ---
    Serial.print("[WebPortal] AP Started. Connect to '");
    Serial.print(apSSID);
    Serial.print("' on IP: ");
    Serial.println(apIP);

    // Iniciar el DNS Server para el portal cautivo
    // (Redirige todas las peticiones a nuestra IP)
    dnsServer.start(53, "*", apIP);

    // Iniciar mDNS (para .local)
    if (MDNS.begin(hostname.c_str())) {
        MDNS.addService("http", "tcp", 80);
        Serial.print("[WebPortal] mDNS (AP Mode) started. Access at: http://");
        Serial.print(hostname);
        Serial.println(".local");
    }

    // Configurar rutas y arrancar servidor
    setupRoutes();
    server.begin();
}

/**
 * @brief Inicia el modo Estación (STA) para operación normal.
 */
void WebPortal::beginSTAMode() {
    _isAPMode = false;

    // Configura el hostname
    String hostname = "arandanoirt-" + getUniqueSuffix();
    WiFi.setHostname(hostname.c_str());

    // Iniciar mDNS
    if (MDNS.begin(hostname.c_str())) {
        MDNS.addService("http", "tcp", 80);
        Serial.print("[WebPortal] mDNS (STA Mode) started. Access at: http://");
        Serial.print(hostname);
        Serial.println(".local");
    } else {
        Serial.println("[WebPortal] Error starting mDNS");
    }

    // Configurar rutas y arrancar servidor
    setupRoutes();
    server.begin();
}

/**
 * @brief Detiene todos los servicios de red del portal.
 */
void WebPortal::stop() {
    server.end();
    dnsServer.stop();
    MDNS.end();
}

/**
 * @brief Procesa las peticiones DNS (solo en modo AP).
 */
void WebPortal::processDns() {
    if (_isAPMode) {
        dnsServer.processNextRequest();
    }
}

/**
 * @brief Configura todas las rutas (endpoints) del servidor web.
 */
void WebPortal::setupRoutes() {
    
    // --- Rutas estáticas (archivos CSS/JS) ---
    server.on("/style.css", HTTP_GET, std::bind(&WebPortal::handleCss, this, std::placeholders::_1));
    server.on("/script.js", HTTP_GET, std::bind(&WebPortal::handleJs, this, std::placeholders::_1));

    // --- Rutas API (JSON) ---
    server.on("/api/config", HTTP_GET, std::bind(&WebPortal::handleGetConfig, this, std::placeholders::_1));
    server.on("/api/logs/list", HTTP_GET, std::bind(&WebPortal::handleListLogs, this, std::placeholders::_1));
    server.on("/api/logs/view", HTTP_GET, std::bind(&WebPortal::handleViewLog, this, std::placeholders::_1));

    // Handler para guardar la configuración (recibe JSON)
    AsyncCallbackJsonWebHandler* saveHandler = new AsyncCallbackJsonWebHandler(
        "/api/save", 
        std::bind(&WebPortal::handleSaveConfig, this, std::placeholders::_1, std::placeholders::_2)
    );
    server.addHandler(saveHandler);


    if (_isAPMode) {
        // --- LÓGICA DE PORTAL CAUTIVO ---
        // Intercepta las URLs de "chequeo de conectividad" de los
        // sistemas operativos (Android, iOS, Windows, Linux) y
        // les sirve el index.html para abrir el portal automáticamente.
        
        // Android/Chrome
        server.on("/generate_204", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
        server.on("/gconnectivitycheck.gstatic.com", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
        server.on("/connectivitycheck.gstatic.com", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });

        // Microsoft
        server.on("/fwlink", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
        server.on("/fwlink/", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
        server.on("/connecttest.txt", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
         server.on("/msftconnecttest/connect.txt", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });

        // Apple
         server.on("/hotspot-detect.html", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
        
        // Linux (NetworkManager)
        server.on("/connectivitycheck.ubuntu.com", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
        server.on("/connectivitycheck.gnome.org", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
         server.on("/nm-check.txt", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
        // --- FIN LÓGICA PORTAL CAUTIVO ---
    }

    // Ruta raíz (/)
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });

    // Manejador 404 (Not Found)
    server.onNotFound(std::bind(&WebPortal::handleNotFound, this, std::placeholders::_1));
}

// --- Implementación de Handlers ---

void WebPortal::handleCss(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/style.css", "text/css");
}

void WebPortal::handleJs(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/script.js", "text/javascript");
}

/**
 * @brief Maneja GET /api/config. Devuelve la configuración actual.
 */
void WebPortal::handleGetConfig(AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc; // Ajustar tamaño si la config crece

    // Lee desde la variable 'config' global
    doc["wifi_ssid"] = config.wifi_ssid;
    doc["wifi_pass"] = config.wifi_pass;
    doc["deviceId"] = config.deviceId;
    doc["activationCode"] = config.activationCode;
    doc["apiBaseUrl"] = config.apiBaseUrl;
    doc["apiActivatePath"] = config.apiActivatePath;
    doc["apiAuthPath"] = config.apiAuthPath;
    doc["apiRefreshTokenPath"] = config.apiRefreshTokenPath;
    doc["apiLogPath"] = config.apiLogPath;
    doc["apiAmbientDataPath"] = config.apiAmbientDataPath;
    doc["apiCaptureDataPath"] = config.apiCaptureDataPath;
    doc["data_interval_minutes"] = config.data_interval_minutes;
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
}

/**
 * @brief Maneja POST /api/save. Guarda la config y reinicia.
 */
void WebPortal::handleSaveConfig(AsyncWebServerRequest *request, JsonVariant &json) {
    StaticJsonDocument<512> doc = json.as<JsonObject>();
    String output;
    serializeJson(doc, output); // Convierte el JSON recibido a String

    // Llama al ConfigManager para guardar en LittleFS
    bool success = saveConfiguration(output);

    if (success) {
        request->send(200, "application/json", "{\"success\":true}");
        
        // Agenda el reinicio cuando el cliente se desconecte
        request->onDisconnect([](){
            Serial.println("[WebPortal] Configuration saved. Rebooting in 1s...");
            delay(1000);
            ESP.restart();
        });
    } else {
        // Mantenemos el error en español para el cliente web
        request->send(500, "application/json", "{\"success\":false, \"error\":\"No se pudo guardar en LittleFS\"}");
    }
}

/**
 * @brief Maneja GET /api/logs/list. Devuelve la lista de archivos de log.
 */
void WebPortal::handleListLogs(AsyncWebServerRequest *request) {
    // Llama al SDManager para obtener la lista de archivos en formato JSON
    String jsonList = sdManager.listLogFilesJSON(LOG_DIR);
    request->send(200, "application/json", jsonList);
}

/**
 * @brief Maneja GET /api/logs/view. Hace streaming de un archivo de log.
 */
void WebPortal::handleViewLog(AsyncWebServerRequest *request) {
    // Valida que el parámetro "file" exista
    if (!request->hasParam("file")) {
        request->send(400, "text/plain", "Error: Falta el parámetro 'file'");
        return;
    }

    String filename = request->getParam("file")->value(); 

    // Medidas de seguridad básicas
    if (filename.indexOf("..") != -1) {
        request->send(400, "text/plain", "Error: Nombre de archivo no válido (..)");
        return;
    }
    if (!filename.endsWith(".txt")) { 
        request->send(400, "text/plain", "Error: Archivo no es .txt");
        return;
    }

    String path = String(LOG_DIR) + "/" + filename;

    // Obtiene el archivo desde el SDManager
    File logFile = sdManager.getLogFile(path); 
    
    if (logFile) {
        // Inicia una respuesta de streaming
        AsyncWebServerResponse *response = request->beginResponse(logFile, path, "text/plain");

        // Callback para cerrar el archivo cuando el cliente se desconecte
        request->onDisconnect([logFile]() mutable {
            logFile.close(); 
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[WebPortal] Log file stream finished and file closed.");
            #endif
        });

        request->send(response);

    } else {
        request->send(404, "text/plain", "Error 404: Archivo no encontrado en " + path);
    }
}

/**
 * @brief Manejador 404.
 */
void WebPortal::handleNotFound(AsyncWebServerRequest *request) {
    if (_isAPMode) {
        // Modo AP (Portal Cautivo): Redirige cualquier URL desconocida al index.
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[WebPortal] Captive Portal: Request to '%s' not found. Serving index.html\n", request->url().c_str());
        #endif
        request->send(LittleFS, "/index.html", "text/html");

    } else {
        // Modo STA: Es un 404 real.
        request->send(404, "text/plain", "404: No encontrado");
    }
}

/**
 * @brief Obtiene el sufijo único para el dispositivo.
 */
String WebPortal::getUniqueSuffix() {
    // Usa el deviceId de la config global si es válido
    if (config.deviceId > 0) {
        return String(config.deviceId);
    } else {
        // Fallback: usar los últimos 3 bytes de la MAC
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macStr[7]; // 6 chars + 1 null
        sprintf(macStr, "%02X%02X%02X", mac[3], mac[4], mac[5]);
        return String(macStr);
    }
}