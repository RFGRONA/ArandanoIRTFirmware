#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h> // Servidor web asíncrono
#include <AsyncJson.h>         // Manejo de JSON asíncrono
#include <ArduinoJson.h>       // Librería de JSON
#include <DNSServer.h>         // Para el portal cautivo

// Declaración anticipada (Forward declaration)
class SDManager;

/**
 * @class WebPortal
 * @brief Gestiona el servidor web (Access Point y Modo Estación)
 * para configuración y visualización de logs.
 */
class WebPortal {
public:
    /**
     * @brief Constructor del Portal Web.
     * @param sdMgr Referencia al SDManager (para leer logs de la SD).
     */
    WebPortal(SDManager& sdMgr);

    /**
     * @brief Inicia el portal en Modo Access Point (AP).
     * Crea un AP, un servidor DNS (portal cautivo) y mDNS.
     */
    void beginAPMode();

    /**
     * @brief Inicia el portal en Modo Estación (STA).
     * Se une a un WiFi existente e inicia mDNS.
     */
    void beginSTAMode();

    /**
     * @brief Detiene los servicios (servidor web, DNS, mDNS).
     */
    void stop();

    /**
     * @brief Procesa las peticiones DNS (requerido para el portal cautivo en modo AP).
     * Debe llamarse en el loop() principal *solo* si está en modo AP.
     */
    void processDns();

private:
    AsyncWebServer server; ///< Instancia del servidor web asíncrono.
    DNSServer dnsServer;   ///< Servidor DNS para el portal cautivo.
    SDManager& sdManager;  ///< Referencia al gestor de la SD (para logs).

    bool _isAPMode = false; ///< Flag: true si está en Modo AP, false en Modo STA.

    /**
     * @brief Obtiene un sufijo único para el hostname y SSID del AP.
     * (Usa deviceId o los últimos 3 bytes de la MAC como fallback).
     * @return String con el sufijo (ej: "123" o "A1B2C3").
     */
    String getUniqueSuffix();

    /**
     * @brief Configura todos los endpoints (rutas) del servidor web.
     */
    void setupRoutes();

    // --- Handlers (Manejadores de Rutas) ---
    void handleCss(AsyncWebServerRequest *request);
    void handleJs(AsyncWebServerRequest *request);
    void handleGetConfig(AsyncWebServerRequest *request);
    void handleSaveConfig(AsyncWebServerRequest *request, JsonVariant &json);
    void handleListLogs(AsyncWebServerRequest *request);
    void handleViewLog(AsyncWebServerRequest *request);
    void handleNotFound(AsyncWebServerRequest *request);
};