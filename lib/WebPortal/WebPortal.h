#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#include <ArduinoJson.h>
#include <DNSServer.h>

// Forward declaration solo para SDManager
class SDManager;

class WebPortal {
public:
    /**
     * @brief Constructor del Portal Web.
     * @param sdMgr Referencia al SDManager (para leer logs).
     */
    WebPortal(SDManager& sdMgr);

    /**
     * @brief Inicia el portal en Modo Access Point (AP).
     */
    void beginAPMode();

    /**
     * @brief Inicia el portal en Modo Estación (STA).
     */
    void beginSTAMode();

    /**
     * @brief Detiene el servidor web.
     */
    void stop();

    /**
     * @brief Método que debe ser llamado en el loop() SOLO en modo AP.
     */
    void processDns();

private:
    AsyncWebServer server;
    DNSServer dnsServer;
    SDManager& sdManager; 

    bool _isAPMode = false;

    String getUniqueSuffix();
    void setupRoutes();

    // --- Handlers de Rutas (Endpoints) ---
    void handleCss(AsyncWebServerRequest *request);
    void handleJs(AsyncWebServerRequest *request);
    void handleGetConfig(AsyncWebServerRequest *request);
    void handleSaveConfig(AsyncWebServerRequest *request, JsonVariant &json);
    void handleListLogs(AsyncWebServerRequest *request);
    void handleViewLog(AsyncWebServerRequest *request);
    void handleNotFound(AsyncWebServerRequest *request);
};