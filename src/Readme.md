# **DATALOGGER: REGISTRADOR .............**

### En este proyecto se realiza un DataLogger que registra temperatura, humedad, presión y altitud mediante los siguientes componentes:

- ESP32 M5STACK
- BME280
- RTC 1307

### El Datalogger realiza las siguientes funciones:
- Almacena los registros realizados en una tarjeta SD en formato CSV para su posterior tratamiento y analisis
- Puede comunicarse via bluetooth BLE con la aplicación Blynk para mostrar los datos y traspasar su propio control
- Mediante los botones del M5STACK se configura el Datalogger mediante el despliegue de menús
  - Botón A: "SETUP"
    - Intervalo de esaneo (30 seg, 60 seg, 5 min, 15 min, 30 min)
    - Configuración SD (Borrar SD / Graba datos)
    - Configuración General (WIFI, Reloj, Pantalla, BLE, ....)
    - Configuración RTC (configuracón manual de fecha y hora)
    - EXIT
  - Botón B: "START / SLEEP" 
    - Pulso corto: Activa Datalogger, comienza a registrar
    - Pulsación larga: EL M5STACK entra en modo "deep sleep" y despierta en los intervalos configurados para realizar un registro
  - Botón C: "STOP / EXIT SLEEP"
    - Pulso corto: Para Datalogger, detiene el registro
    - Pulsación larga: EL M5STACK del modo "deep sleep" y contimua con los registros despierta en los intervalos configurados



I2C scanner. Scanning ...
Found address: 104 (0x68) GROVE M5STACK
Found address: 117 (0x75) IP5306 Controlador de Bateria del M5STACK 
Found address: 118 (0x76) BME-280
Found address: 119 (0x77) BME-280