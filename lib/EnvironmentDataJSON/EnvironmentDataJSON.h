#ifndef ENVIRONMENT_DATA_JSON_H
#define ENVIRONMENT_DATA_JSON_H

#include <ArduinoJson.h>
#include <HTTPClient.h>

class EnvironmentDataJSON {
public:
    /**
     * Envía datos ambientales a la API usando HTTP POST.
     * @param apiUrl URL del endpoint para enviar datos.
     * @param lightLevel Nivel de luz ambiental.
     * @param temperature Temperatura.
     * @param humidity Humedad.
     * @return true si el envío fue exitoso, false en caso contrario.
     */
    static bool IOEnvironmentData(
        const String& apiUrl, 
        float lightLevel, 
        float temperature, 
        float humidity
    );
};

#endif // ENVIRONMENT_DATA_JSON_H
