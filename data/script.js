document.addEventListener("DOMContentLoaded", () => {
    
    // --- Referencias a elementos del DOM ---
    const form = document.getElementById("config-form");
    const statusMessage = document.getElementById("status-message");
    const logSelect = document.getElementById("log-select");
    const logContent = document.getElementById("log-content");
    const refreshLogsBtn = document.getElementById("refresh-logs-btn");

    // --- Cargar configuración inicial ---
    function loadConfig() {
        fetch("/api/config")
            .then(response => {
                if (!response.ok) {
                    throw new Error("No se pudo cargar la configuración.");
                }
                return response.json();
            })
            .then(data => {
                // Rellena el formulario con los datos recibidos
                for (const key in data) {
                    const el = document.getElementById(key);
                    if (el) {
                        el.value = data[key];
                    }
                }
            })
            .catch(error => {
                showStatus(error.message, "error");
            });
    }

    // --- Guardar configuración ---
    function saveConfig(event) {
        event.preventDefault(); // Evita que el formulario se envíe de forma tradicional
        
        const formData = new FormData(form);
        const data = {};
        
        // Convertir FormData a objeto JSON
        formData.forEach((value, key) => {
            const el = document.getElementById(key);
            // Convertir a número si el tipo de input es 'number'
            if (el && el.type === "number") {
                data[key] = parseFloat(value) || 0;
            } else {
                data[key] = value;
            }
        });

        showStatus("Guardando...", "info");

        fetch("/api/save", {
            method: "POST",
            headers: {
                "Content-Type": "application/json",
            },
            body: JSON.stringify(data),
        })
        .then(response => response.json())
        .then(result => {
            if (result.success) {
                showStatus("¡Guardado! El dispositivo se está reiniciando...", "success");
                // Desactiva el formulario mientras se reinicia
                form.style.opacity = 0.5;
                Array.from(form.elements).forEach(el => el.disabled = true);
            } else {
                throw new Error(result.error || "Falló el guardado.");
            }
        })
        .catch(error => {
            showStatus(error.message, "error");
        });
    }

    // --- Cargar lista de Logs ---
    function loadLogList() {
        logSelect.innerHTML = "<option value=''>Cargando...</option>";
        logContent.textContent = "Seleccione un archivo para ver su contenido.";

        fetch("/api/logs/list")
            .then(response => response.json())
            .then(files => {
                logSelect.innerHTML = "<option value=''>-- Seleccione un log --</option>";
                if (files.length === 0) {
                     logSelect.innerHTML = "<option value=''>-- No hay logs --</option>";
                     return;
                }
                // Rellena el select (los más nuevos primero)
                files.reverse().forEach(file => {
                    const option = document.createElement("option");
                    option.value = file;
                    option.textContent = file;
                    logSelect.appendChild(option);
                });
            })
            .catch(error => {
                logSelect.innerHTML = "<option value=''>Error al cargar</option>";
                logContent.textContent = error.message;
            });
    }

    // --- Ver un Log seleccionado ---
    function viewLog() {
        const filename = logSelect.value;
        if (!filename) {
            logContent.textContent = "Seleccione un archivo para ver su contenido.";
            return;
        }

        logContent.textContent = "Cargando...";

        fetch(`/api/logs/view?file=${encodeURIComponent(filename)}`)
            .then(response => {
                if (!response.ok) {
                    throw new Error(`Error ${response.status}: No se pudo cargar el archivo.`);
                }
                return response.text();
            })
            .then(text => {
                logContent.textContent = text || "El archivo está vacío.";
            })
            .catch(error => {
                logContent.textContent = error.message;
            });
    }

    // --- Helper para mostrar mensajes ---
    function showStatus(message, type) {
        statusMessage.textContent = message;
        statusMessage.className = type; // 'success', 'error', 'info'
    }

    // --- Asignar Eventos ---
    form.addEventListener("submit", saveConfig);
    logSelect.addEventListener("change", viewLog);
    refreshLogsBtn.addEventListener("click", loadLogList);

    // --- Carga inicial ---
    loadConfig();
    loadLogList();
});