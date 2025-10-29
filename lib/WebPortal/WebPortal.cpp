#include "WebPortal.h"
#include "ConfigManager.h" 
#include "SDManager.h"
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <WiFi.h>

// Definimos el nombre del AP para el modo de configuración
#define AP_SSID "Arandano-Config"

WebPortal::WebPortal(SDManager& sdMgr)
    : server(80), 
      sdManager(sdMgr) {
}

void WebPortal::beginAPMode() {
    _isAPMode = true;

    // --- MODIFICADO: Usar nombre de red único ---
    String uniqueSuffix = getUniqueSuffix();
    String apSSID = "ArandanoIRT-" + uniqueSuffix;
    String hostname = "arandanoirt-" + uniqueSuffix;

    WiFi.setHostname(hostname.c_str());
    
    // Iniciar el Access Point
    WiFi.softAP(apSSID.c_str()); // Usamos el nuevo SSID
    IPAddress apIP = WiFi.softAPIP();
    Serial.print("[WebPortal] AP Iniciado. Conéctese a '");
    Serial.print(apSSID);
    Serial.print("' en la IP: ");
    Serial.println(apIP);

    // Iniciar el DNS Server para el portal cautivo
    dnsServer.start(53, "*", apIP);

    // --- NUEVO: Iniciar mDNS también en modo AP ---
    if (MDNS.begin(hostname.c_str())) {
        MDNS.addService("http", "tcp", 80);
        Serial.print("[WebPortal] mDNS (Modo AP) iniciado. Acceda en: http://");
        Serial.print(hostname);
        Serial.println(".local");
    }

    // Configurar rutas y arrancar servidor
    setupRoutes();
    server.begin();
}

void WebPortal::beginSTAMode() {
    _isAPMode = false;

    // --- MODIFICADO: Usar el nuevo formato de hostname ---
    String hostname = "arandanoirt-" + getUniqueSuffix();

    WiFi.setHostname(hostname.c_str());

    // Iniciar mDNS
    if (MDNS.begin(hostname.c_str())) {
        MDNS.addService("http", "tcp", 80);
        Serial.print("[WebPortal] mDNS (Modo STA) iniciado. Acceda en: http://");
        Serial.print(hostname);
        Serial.println(".local");
    } else {
        Serial.println("[WebPortal] Error iniciando mDNS");
    }

    // Configurar rutas y arrancar servidor
    setupRoutes();
    server.begin();
}

void WebPortal::stop() {
    server.end();
    dnsServer.stop();
    MDNS.end();
}

void WebPortal::processDns() {
    if (_isAPMode) {
        dnsServer.processNextRequest();
    }
}

void WebPortal::setupRoutes() {
    
    // --- Rutas estáticas ---
    server.on("/style.css", HTTP_GET, std::bind(&WebPortal::handleCss, this, std::placeholders::_1));
    server.on("/script.js", HTTP_GET, std::bind(&WebPortal::handleJs, this, std::placeholders::_1));

    // --- Rutas API ---
    server.on("/api/config", HTTP_GET, std::bind(&WebPortal::handleGetConfig, this, std::placeholders::_1));
    server.on("/api/logs/list", HTTP_GET, std::bind(&WebPortal::handleListLogs, this, std::placeholders::_1));
    server.on("/api/logs/view", HTTP_GET, std::bind(&WebPortal::handleViewLog, this, std::placeholders::_1));

    AsyncCallbackJsonWebHandler* saveHandler = new AsyncCallbackJsonWebHandler(
        "/api/save", 
        std::bind(&WebPortal::handleSaveConfig, this, std::placeholders::_1, std::placeholders::_2)
    );
    server.addHandler(saveHandler);


    if (_isAPMode) {
        // --- INICIO DE LA LÓGICA DE PORTAL CAUTIVO ROBUSTA ---
        // 1. Interceptar las URLs de "chequeo" del SO y servir el index.
        
        // Usado por Android/Chrome
        server.on("/generate_204", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
        
        // Usado por Android (gconnectivitycheck.gstatic.com)
        server.on("/gconnectivitycheck.gstatic.com", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
        server.on("/connectivitycheck.gstatic.com", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });

        // Usado por Microsoft (fwlink, msftconnecttest)
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

        // Usado por Apple (hotspot-detect.html)
         server.on("/hotspot-detect.html", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
        
        // *** NUEVO: Usado por Linux (Ubuntu, Fedora, etc. con NetworkManager) ***
        server.on("/connectivitycheck.ubuntu.com", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
        server.on("/connectivitycheck.gnome.org", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
         server.on("/nm-check.txt", HTTP_GET, [this](AsyncWebServerRequest *request){
            request->send(LittleFS, "/index.html", "text/html");
        });
        // --- FIN DE LA LÓGICA DE PORTAL CAUTIVO ---
    }


    // 2. Manejador específico SOLO para la raíz "/"
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html", "text/html");
    });

    // 3. onNotFound se encargará de redirigir CUALQUIER OTRA COSA
    //    (Ej. si el usuario escribe "google.com" en la barra del navegador)
    server.onNotFound(std::bind(&WebPortal::handleNotFound, this, std::placeholders::_1));
}

// --- Implementación de Handlers ---

void WebPortal::handleCss(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/style.css", "text/css");
}

void WebPortal::handleJs(AsyncWebServerRequest *request) {
    request->send(LittleFS, "/script.js", "text/javascript");
}

void WebPortal::handleGetConfig(AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc; // Ajustar tamaño si tu config es más grande

    // Leemos la 'config' global
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

void WebPortal::handleSaveConfig(AsyncWebServerRequest *request, JsonVariant &json) {
    StaticJsonDocument<512> doc = json.as<JsonObject>();
    String output;
    serializeJson(doc, output);

    bool success = saveConfiguration(output);

    if (success) {
        request->send(200, "application/json", "{\"success\":true}");
        
        // Agendar reinicio después de 1 segundo para que la respuesta se envíe
        request->onDisconnect([](){
            Serial.println("[WebPortal] Configuración guardada. Reiniciando en 1s...");
            delay(1000);
            ESP.restart();
        });
    } else {
        request->send(500, "application/json", "{\"success\":false, \"error\":\"No se pudo guardar en LittleFS\"}");
    }
}

void WebPortal::handleListLogs(AsyncWebServerRequest *request) {
    
    // --- ESTE MÉTODO AÚN NO EXISTE ---
    // Lo implementaremos en Fase 3
    String jsonList = sdManager.listLogFilesJSON(LOG_DIR);
    // ----------------------------------

    request->send(200, "application/json", jsonList);
}

void WebPortal::handleViewLog(AsyncWebServerRequest *request) {
    if (!request->hasParam("file")) {
        request->send(400, "text/plain", "Error: Falta el parámetro 'file'");
        return;
    }

    String filename = request->getParam("file")->value(); 

    if (filename.indexOf("..") != -1) {
        request->send(400, "text/plain", "Error: Nombre de archivo no válido (..)");
        return;
    }

    if (!filename.endsWith(".txt")) { 
        request->send(400, "text/plain", "Error: Archivo no es .txt");
        return;
    }

    String path = String(LOG_DIR) + "/" + filename;

    File logFile = sdManager.getLogFile(path); 
    
    if (logFile) {

        AsyncWebServerResponse *response = request->beginResponse(logFile, path, "text/plain");

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

void WebPortal::handleNotFound(AsyncWebServerRequest *request) {
    if (_isAPMode) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[WebPortal] Captive Portal: Petición a '%s' no encontrada. Sirviendo index.html\n", request->url().c_str());
        #endif
        request->send(LittleFS, "/index.html", "text/html");

    } else {
        request->send(404, "text/plain", "404: No encontrado");
    }
}

/**
 * @brief Obtiene el sufijo único para el dispositivo.
 * Usa el deviceId si es > 0, o los últimos 3 bytes de la MAC como fallback.
 * @return String con el sufijo (ej: "123" o "A1B2C3").
 */
String WebPortal::getUniqueSuffix() {
    // Usamos la 'config' global cargada por ConfigManager
    if (config.deviceId > 0) {
        return String(config.deviceId);
    } else {
        // Fallback: usar los últimos 3 bytes de la MAC
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macStr[7]; // 3 bytes * 2 chars/byte + 1 null terminator
        sprintf(macStr, "%02X%02X%02X", mac[3], mac[4], mac[5]);
        return String(macStr);
    }
}