# ArandanoIRTFirmware

> **Firmware embebido para un nodo de monitoreo agronómico multimodal basado en ESP32-S3.**
> Desarrollado como componente central del sistema *Arandano IRT*, proyecto de grado en Ingeniería de Sistemas.

---

## Descripción General

Este repositorio contiene el firmware de producción para el nodo sensor inteligente del sistema **Arandano IRT**, una plataforma distribuida de monitoreo agrícola de precisión orientada al cultivo de arándanos (*Vaccinium* spp.). El dispositivo adquiere, procesa y transmite de forma autónoma múltiples flujos de datos ambientales y de imagen hacia un backend centralizado, habilitando análisis continuos para la toma de decisiones agronómicas basada en datos.

El firmware está construido sobre el framework **Arduino para ESP32** usando **PlatformIO** como entorno de desarrollo, e implementa un ciclo de operación continuo (siempre activo) basado en una máquina de estados no bloqueante que garantiza la estabilidad de la conexión WiFi y la resiliencia ante fallos de comunicación.

---

## Tabla de Contenidos

- [Descripción General](#descripción-general)
- [Arquitectura del Firmware](#arquitectura-del-firmware)
- [Sensores y Hardware](#sensores-y-hardware)
- [Módulos de Software](#módulos-de-software)
- [Características Técnicas Destacadas](#características-técnicas-destacadas)
- [Estructura del Repositorio](#estructura-del-repositorio)
- [Dependencias](#dependencias)
- [Configuración del Dispositivo](#configuración-del-dispositivo)
- [Compilación y Despliegue](#compilación-y-despliegue)
- [Ciclo de Operación](#ciclo-de-operación)
- [Seguridad](#seguridad)
- [Licencia](#licencia)

---

## Arquitectura del Firmware

El firmware sigue un diseño modular basado en capas de responsabilidad:

```
┌─────────────────────────────────────────────────────────┐
│                     main.cpp (Orquestador)              │
│         setup() → loop() [State-Machine No-Bloqueante]  │
└──────────────┬─────────────────────────────┬────────────┘
               │                             │
     ┌─────────▼──────────┐      ┌───────────▼───────────┐
     │  src/ (Módulos de  │      │  lib/ (Drivers y      │
     │  Orquestación)     │      │  Servicios Reutiliz.) │
     │                    │      │                        │
     │ • SystemInit       │      │ • API (Authn + HTTPS)  │
     │ • CycleController  │      │ • MLX90640Sensor       │
     │ • EnvironmentTasks │      │ • OV2640Sensor         │
     │ • ImageTasks       │      │ • BME280Sensor         │
     │ • ConfigManager    │      │ • BH1750Sensor         │
     └────────────────────┘      │ • DS18B20Sensor        │
                                 │ • SDManager            │
                                 │ • TimeManager          │
                                 │ • WiFiManager          │
                                 │ • WebPortal            │
                                 │ • ErrorLogger          │
                                 │ • LEDStatus            │
                                 └────────────────────────┘
```

El flujo de inicialización es completamente secuencial y con manejo de fallos críticos: cualquier fallo irrecuperable en periféricos esenciales (SD, WiFi, NTP, sensores) detiene la ejecución y lo reporta mediante el sistema de LEDs de estado y el `ErrorLogger`.

---

## Sensores y Hardware

| Sensor / Componente | Modelo | Variable Medida | Interfaz |
|---|---|---|---|
| Cámara Térmica | MLX90640 | Temperatura superficial (mapa 32×24 px) | I2C |
| Cámara Visual RGB | OV2640 | Imagen JPEG del cultivo | DVP / SCCB |
| Temp., Hum. y Presión | BME280 | Temperatura ambiental, Humedad relativa, Presión atmosférica | I2C |
| Luminosidad | BH1750 | Nivel de luz (lux) | I2C |
| Temperatura Interna | DS18B20 | Temperatura del gabinete/procesador | 1-Wire |
| Microcontrolador | Freenove ESP32-S3 WROOM | — | — |
| Almacenamiento | Tarjeta MicroSD | Persistencia y cola offline | SPI |

**Placa objetivo:** `freenove_esp32_s3_wroom`
**Framework:** Arduino (PlatformIO / espressif32)
**Sistema de archivos embebido:** LittleFS (configuración), MicroSD (datos y logs)

---

## Módulos de Software

### `src/` — Orquestadores del Ciclo Principal

| Módulo | Responsabilidad |
|---|---|
| `main.cpp` | Punto de entrada, bucle principal con máquina de estados no-bloqueante |
| `SystemInit` | Inicialización robusta de hardware y servicios (Serial, I2C, WiFi, NTP) |
| `CycleController` | Lógica de ciclo: autenticación API, control de LED, limpieza de buffers |
| `EnvironmentTasks` | Lectura de sensores ambientales (BH1750, BME280) y envío al backend |
| `ImageTasks` | Captura y envío de imágenes térmica (MLX90640) y visual (OV2640) |
| `ConfigManager` | Carga y exposición de la configuración desde `config.json` (LittleFS) |

### `lib/` — Drivers y Servicios

| Librería | Responsabilidad |
|---|---|
| `API` | Cliente HTTPS con activación, autenticación JWT y refresco de tokens |
| `SDManager` | Gestión de archivos en MicroSD: datos, logs, cola offline y estado cifrado |
| `TimeManager` | Sincronización NTP y provisión de timestamps precisos |
| `WiFiManager` | Conexión WiFi robusta con reconexión automática |
| `WebPortal` | Portal web embebido para configuración (modo AP) y diagnóstico (modo STA) |
| `MLX90640Sensor` | Wrapper para cámara térmica: lectura de frames de 768 puntos (32×24) |
| `OV2640Sensor` | Captura JPEG en PSRAM desde la cámara visual |
| `BME280Sensor` | Lectura de temperatura, humedad y presión ambiental |
| `BH1750Sensor` | Medición de luminosidad ambiental en lux |
| `DS18B20Sensor` | Temperatura interna del dispositivo por protocolo 1-Wire |
| `ErrorLogger` | Logging estructurado: local (SD) y remoto (API) con niveles INFO/WARNING/ERROR |
| `LEDStatus` | Indicación visual del estado del sistema mediante LED RGB |
| `MultipartDataSender` | Empaquetado y envío de payloads multipart (JSON + JPEG) al backend |

---

## Características Técnicas Destacadas

- **Ciclo no-bloqueante**: El `loop()` principal no usa `delay()` para controlar el ciclo de datos; en su lugar, compara el tiempo actual (epoch Unix) con el tiempo programado para la siguiente ejecución, permitiendo tareas continuas (e.g., monitoreo de temperatura interna) entre ciclos.

- **Cola offline con persistencia en SD**: Si el backend no está disponible, los datos capturados se serializan en formato JSON y se almacenan en un directorio `pending/` en la MicroSD. En el próximo ciclo exitoso, el `SDManager` procesa y reintenta el envío de la cola pendiente.

- **Autenticación segura con token JWT**: La clase `API` implementa la activación por código de único uso, la obtención de *access token* y *refresh token*, y el refresco automático ante errores `HTTP 401`, sin requerir intervención humana.

- **Cifrado AES-256-GCM en reposo**: El estado de autenticación de la API (tokens, estado de activación) se almacena cifrado en la MicroSD usando AES-256 en modo GCM. La clave se genera aleatoriamente en el primer arranque y se persiste de forma segura en la partición NVS del ESP32.

- **Portal de configuración embebido (Captive Portal)**: En ausencia de configuración WiFi válida, el dispositivo levanta un punto de acceso (AP) con un portal web servido desde LittleFS que permite configurar credenciales WiFi, URL de la API y rutas de endpoints, sin necesidad de reflashear el firmware.

- **Sincronización NTP periódica**: La precisión temporal es crítica para la correlación de datos. El firmware sincroniza el reloj con NTP al arrancar y verifica periódicamente el estado de sincronización durante la operación, realizando re-sincronizaciones automáticas si detecta desviación.

- **Gestión activa del almacenamiento**: El `SDManager` monitorea el porcentaje de uso de la MicroSD y envía alertas al backend cuando supera el 90%, así como la resolución del estado de alerta cuando baja del 85%.

- **Captura de imagen condicionada por luminosidad**: La imagen visual RGB solo se captura cuando el nivel de luz (BH1750) supera un umbral configurable, evitando imágenes oscuras e inútiles durante la noche.

---

## Estructura del Repositorio

```
ArandanoIRTFirmware/
│
├── src/                        # Módulos de orquestación del ciclo principal
│   ├── main.cpp                # Punto de entrada y loop principal
│   ├── SystemInit.{h,cpp}      # Inicialización robusta de hardware y servicios
│   ├── CycleController.{h,cpp} # Gestión del ciclo: auth, LED, cleanup
│   ├── EnvironmentTasks.{h,cpp}# Lectura de sensores ambientales y envío
│   ├── ImageTasks.{h,cpp}      # Captura y envío de imágenes
│   └── ConfigManager.{h,cpp}   # Carga de configuración desde LittleFS
│
├── lib/                        # Drivers y servicios reutilizables
│   ├── API/                    # Cliente HTTPS con autenticación JWT
│   ├── SDManager/              # Gestión de MicroSD y cola offline
│   ├── TimeManager/            # Sincronización NTP y timestamps
│   ├── WiFiManager/            # Conexión WiFi con reconexión automática
│   ├── WebPortal/              # Portal web embebido (AP y STA mode)
│   ├── MLX90640Sensor/         # Driver cámara térmica (32×24 px)
│   ├── OV2640Sensor/           # Driver cámara visual (JPEG / PSRAM)
│   ├── BME280Sensor/           # Driver sensor Temp/Hum/Presión
│   ├── BH1750Sensor/           # Driver sensor de luminosidad
│   ├── DS18B20Sensor/          # Driver sensor temperatura interna (1-Wire)
│   ├── ErrorLogger/            # Sistema de logging local y remoto
│   ├── LEDStatus/              # Control de LED RGB de estado
│   ├── MultipartDataSender/    # Envío de payloads multipart (JSON + JPEG)
│   └── EnvironmentDataJSON/    # Serialización de datos ambientales
│
├── data/                       # Sistema de archivos LittleFS (flasheado al dispositivo)
│   ├── config.template.json    # Plantilla de configuración del dispositivo
│   ├── index.html              # UI del portal de configuración
│   ├── style.css               # Estilos del portal de configuración
│   └── script.js               # Lógica del portal de configuración
│
├── test/                       # Tests unitarios (PlatformIO native)
├── experimental/               # Código experimental y prototipos
├── platformio.ini              # Configuración del proyecto PlatformIO
├── .gitignore
└── LICENSE
```

---

## Dependencias

Las dependencias se declaran en `platformio.ini` y son gestionadas automáticamente por PlatformIO:

| Librería | Propósito |
|---|---|
| `adafruit/Adafruit MLX90640` | Driver base para la cámara térmica |
| `claws/BH1750` (GitHub) | Driver para el sensor de luminosidad |
| `adafruit/Adafruit BME280 Library` | Driver para el sensor ambiental BME280 |
| `milesburton/DallasTemperature` | Driver para el sensor DS18B20 (1-Wire) |
| `PaulStoffregen/OneWire` | Protocolo 1-Wire para DS18B20 |
| `adafruit/Adafruit BusIO` | Abstracción de comunicación SPI/I2C |
| `adafruit/Adafruit Unified Sensor` | Abstracción unificada de sensores Adafruit |
| `me-no-dev/ESPAsyncWebServer` (GitHub) | Servidor web asíncrono para el WebPortal |
| `me-no-dev/AsyncTCP` (GitHub) | TCP asíncrono requerido por ESPAsyncWebServer |
| `bblanchon/ArduinoJson` | Serialización/deserialización JSON |

**Nota:** Las librerías del ESP-IDF (`mbedtls`, `nvs_flash`, `esp_sntp`, etc.) son provistas directamente por la plataforma `espressif32` y no requieren declaración explícita.

---

## Configuración del Dispositivo

El firmware requiere un archivo `config.json` en la partición LittleFS del dispositivo. En el primer arranque (o si el archivo no existe), el dispositivo entra automáticamente en **Modo de Configuración (AP)**.

### Modo de Configuración (AP)

1. Busca la red WiFi llamada **`ArandanoIR_Config`** (sin contraseña) desde cualquier dispositivo.
2. En un navegador web, navega a **`http://192.168.4.1`** (o el portal cautivo se abrirá automáticamente).
3. Completa el formulario con las credenciales WiFi y los parámetros de la API.
4. Presiona **"Guardar y Reiniciar"**. El dispositivo aplicará la configuración y reiniciará en modo normal.

### Parámetros de Configuración (`config.json`)

A partir de la plantilla `data/config.template.json`:

```json
{
    "wifi_ssid": "NombreDeTuRed",
    "wifi_pass": "ContraseñaDeTuRed",
    "FIRMWARE_DEVICE_ID": 1,
    "FIRMWARE_ACTIVATION_CODE": "CODIGO-DE-ACTIVACION",
    "api_base_url": "https://tu-backend.com",
    "api_activate_path": "/api/devices/activate",
    "api_auth_path": "/api/auth/check",
    "api_refresh_token_path": "/api/auth/refresh",
    "api_log_path": "/api/logs",
    "api_ambient_data_path": "/api/observations/ambient",
    "api_capture_data_path": "/api/observations/capture",
    "data_interval_minutes": 15
}
```

| Campo | Descripción |
|---|---|
| `wifi_ssid` / `wifi_pass` | Credenciales de la red WiFi |
| `FIRMWARE_DEVICE_ID` | ID del dispositivo registrado en el backend |
| `FIRMWARE_ACTIVATION_CODE` | Código de activación de único uso asignado por el backend |
| `api_base_url` | URL base del servidor de la API |
| `api_*_path` | Rutas relativas de cada endpoint de la API |
| `data_interval_minutes` | Intervalo mínimo de colección de datos (puede ser sobreescrito por la API) |

---

## Compilación y Despliegue

### Prerrequisitos

- **PlatformIO** instalado (extensión de VS Code o CLI)
- Acceso a la placa **Freenove ESP32-S3 WROOM** via USB
- (Opcional) Monitor serie a 115200 baudios para depuración

### Compilar y Cargar

```bash
# Compilar el firmware
pio run

# Compilar y cargar el firmware al dispositivo
pio run --target upload

# Cargar el sistema de archivos LittleFS (portal web y plantilla de config)
pio run --target uploadfs

# Monitorear la salida serie
pio device monitor --baud 115200
```

### Flags de Compilación

| Flag | Descripción |
|---|---|
| `BOARD_HAS_PSRAM` | Habilita el uso de PSRAM para buffers de imagen grandes |
| `-mfix-esp32-psram-cache-issue` | Mitiga el problema conocido de caché con PSRAM |
| `FS_LITTLEFS` | Selecciona LittleFS como sistema de archivos flash |
| `ENABLE_DEBUG_SERIAL` | Habilita logs detallados por puerto serie (desactivar en producción) |

> **Nota:** Para compilar en modo de producción (sin logs de depuración), comentar la línea `-D ENABLE_DEBUG_SERIAL` en `platformio.ini`.

---

## Ciclo de Operación

```
Arranque (setup())
    │
    ├─► Inicializar Serial, NVS, LittleFS
    ├─► Cargar config.json
    ├─► Inicializar SD Card
    ├─► Instanciar objeto API
    │
    ├─► ¿WiFi configurado?
    │       SÍ ──► Conectar WiFi ──► Sincronizar NTP ──► Inicializar Sensores
    │               └──► Modo STA (Operación Normal)
    │       NO ──► Levantar AP ──► Modo Configuración (Captive Portal)
    │
    └──────────────────────────────────────────────────────────┐
                                                               ▼
Bucle Principal (loop()) ── Modo STA ───────────────────────────
    │
    ├─► [Continuo] Leer DS18B20 (temp. interna)
    │
    └─► [¿Es hora del ciclo?] ────────────────────────────────────
            │
            ├─► Verificar autenticación API (con reintentos)
            │       └─► Si no activado: Activar con código
            │       └─► Si token inválido (401): Refrescar token
            │       └─► Si backend offline (5xx): Continuar con cola offline
            │
            ├─► Capturar datos ambientales (BH1750 + BME280)
            │       └─► Enviar a /api/observations/ambient
            │       └─► Si falla: serializar y guardar en SD (pending/)
            │
            ├─► Capturar imagen térmica (MLX90640, 32×24 px)
            ├─► [Si luz suficiente] Capturar imagen visual (OV2640, JPEG)
            │       └─► Enviar ambas a /api/observations/capture (multipart)
            │       └─► Si falla: serializar y guardar en SD (pending/)
            │
            ├─► Procesar cola de envíos pendientes (SD → API)
            ├─► Gestionar almacenamiento SD (rotación, alertas)
            ├─► Verificar estado NTP (re-sincronizar si necesario)
            │
            └─► Programar siguiente ciclo (alineado a intervalo configurado)
```

---

## Seguridad

El firmware implementa múltiples capas de seguridad para proteger las credenciales y el estado del dispositivo:

- **Cifrado en reposo (AES-256-GCM):** Los tokens JWT de acceso y refresco se almacenan cifrados en la MicroSD. La clave AES-256 es generada de forma aleatoria en el primer arranque y persistida en la partición **NVS** (Non-Volatile Storage) del ESP32, que es un almacenamiento separado y protegido.

- **Autenticación por token de único uso:** El proceso de activación utiliza un código de activación de único uso asociado al `FIRMWARE_DEVICE_ID`, garantizando que solo dispositivos autorizados por el sistema backend puedan incorporarse a la red de sensores.

- **Comunicación HTTPS:** Toda la comunicación con el backend se realiza sobre HTTPS, garantizando la confidencialidad e integridad de los datos en tránsito.

- **Refresco automático de tokens:** El mecanismo de `refresh token` evita el uso prolongado del `access token`, reduciendo la ventana de exposición en caso de interceptación.

---

## Licencia

Distribuido bajo la **Licencia MIT**. Consulta el archivo [`LICENSE`](LICENSE) para más detalles.

---

*Proyecto de Grado — Ingeniería de Sistemas.*
*Sistema de Monitoreo Agronómico de Precisión para Cultivos de Arándano.*