# **MULTISENSOR I2C DATALOGGER**

## -------------------------------------------------------------------------------------------

### En este proyecto se realiza un DataLogger que registra los parámetros del sensor conectado al bus I2C del M5STACK (ESP32). Un RTC DS3231 proporciona la fecha/hora del registro si no configura o dispone de red WIFI

- Los sensores soportados son los siguientes:
  - BME280 (temperatura, humedad, presión y altitud)
  - BME680 (temperatura, humedad, presión y Gas VOC)
  - LM75 (temperatura)
  - SI7021 (temperatura y humedad)
  - INA219 (corriente,tensión y potencia en CC) - (No probado)
  - BH1750 (nivel luminosidad)
  - VEML6075 (indice UV)
  - TSL2541 (nivel luminosidad)
  - AM2320 (temperatura y humedad) - (No probado)
  
### Conexiones hardware

- El sensor a registrar se puede conectar mediante:
  - Conector GROVE I2C 5V del M5STACK (solo sensores de 5v)
  - PCB adaptador de sensor diseñado para la conexión del sensor a los pines 5V 3V3 G SDA(21) SCL(22) del M5STACK (sensores de 3V3 y 5v eligiendo el conector adecuado del adaptador)
- El RTC DS3231 está conectado al bus I2C de forma fija en el módulo PROTO del M5STACK (3V3 GND SDA-21 SCL-22)
- Adicionalmente se dispone del módulo BATTERY para aumentar la capacidad de funcionamiento autónomo del DataLogger
  
### El Datalogger realiza las siguientes funciones

- Detección automática del sensor conectado al bus I2C
- Almacena los registros realizados en una tarjeta SD en formato CSV para su posterior tratamiento y análisis
- Mediante la pantalla muestra valores de los registros realizados, fecha/hora, el estado de la bateria (porcentaje, cagando "CHARGING", llena "FULL"), conexión bluetooth y la hora actual si estas conectado a una red WIFI
- Puede comunicarse via bluetooth BLE con la aplicación Blynk para mostrar los datos y traspasar su propio control
- Mediante los botones del M5STACK se configura y controla el Datalogger mediante un despliegue de menús
  - Botón A: "SETUP"
    - Intervalo de escaneo (30 seg, 60 seg, 5 min, 15 min, Intervalo escaneo en min, Intervalo escaneo en seg)
    - Configuración SD (Borrar SD / Graba datos SI/NO)
    - Configuración General (WIFI, Reloj, Pantalla, BLE, ....)
    - Configuración RTC (configuracón manual o WIFI de fecha y hora)
    - EXIT
  - Botón B: "ON / SLEEP"
    - ON | Pulso corto: Activa Datalogger. Comienza a registrar
    - SLEEP | Pulsación larga: EL M5STACK entra en modo "deep sleep", ahorro de energia. Despierta en los intervalos configurados para realizar un registro. El modo "deep sleep", ahorro de energia, solo es configurable en el caso de que el intervalo de escaneo sea superior a 30 segundos
  - Botón C: "STOP / EXIT SLEEP"
    - STOP | Pulso corto: Para Datalogger. Detiene el registro
    - EXIT SLEEP | Pulsación larga: Saca al M5STACK del modo "deep sleep", recupera de la SD el último registro para mostralo en pantalla y contimua con los registros en los intervalos configurados
  
### La app de BLYNK realiza las siguientes funciones

- Conexión mediante BLE con el DataLogger
- Control del DataLogger:
  - Botón START/STOP DATALOGGER: Inicio y paro del registro
  - Botón WAIT BLE: En modo ahorro de energía muestra los datos en la app ya que permite la conexión BLE
  - Botón LOW ENERGY: Activa/Desactiva el modo ahorro de energia ordenando la entrada en "deep sleep" al M5STACK
  - Botones 30 SEG - 1 MIN - 5 MIN - 15 MIN - XXX. Configuración intervalos de registro. XXX muestra el intervalo en mininutos o segundos introducido por teclado (no permite su control)
  - Porcentaje de batería
  - Indicaciones de los valores de registro y sus gráficas

### Direcciones I2C de los dispositivos utilizados

- Address: 35  (0x23) BH1750
- Address: 56  (0x38) VEML6075
- Address: 57  (0x39) TSL2541
- Address: 64  (0x40) SI7021
- Address: 59  (0x49) INA219
- Address: 72  (0x48) LM75
- Address: 92  (0x5C) AM2320
- Address: 104 (0x68) RTC DS3231 - Reloj del DataLogger
- Address: 117 (0x75) IP5306 - Controlador de Bateria del M5STACK
- Address: 118 (0x76) BME-280
- Address: 119 (0x77) BME-680
  
### Código y actualización de sensores

En el archivo "main.cpp" se encuentra el código del proyecto. Para realizar debug se deben descomentadar las siguientes directivas:

- //#define BLYNK_DEBUG
- //#define BLYNK_PRINT Serial
- //#define DEBUG_DATALOGGER

Para actualizar el código con nuevos sensores I2C puedes buscar la etiqueta "//***** nuevo sensor I2C". Esta etiqueta te indica que debes introducir líenas de código, antes de la etiqueta, para definir el nuevo sensor I2C a incluir.

### Otros contenidos

- Carpeta "app BLYNK":
  - Contiene la configuración y diseño de la app BLE que se conecta el DataLogger
  
- Carpeta "Gerber_PCB_M5_SENSOR_DATALOGGER"
  - Contiene el diseño de la PCB del adaptador de sensores para su conexión a los pines 5V 3V3 G SDA(21) SCL(22) del M5STACK

- Carpeta "tools"
  - Contiene las instruciones y programas necesarios para convertir imagenes bmp, en matriz C. Estas imagenes en matrisz C pueden ser mostradas porsteriormente en la pantalla del M5STACK.

- Carpeta "Fotos"
  - Contiene fotos del DataLogger y los diversos modulos apilables que lo componen

- Archivo datalog.csv. Ejemplo de archivo de registro del Datalogger. Es necesario crear o copiar un archivo con ese nombre en la tarjeta SD para que el código del programa funcione correctamente

- Archivo user-partition.csv. Tabla de particiones personalizada (CSV) para gestionar las capacidades de memoria del M5STACK (ESP32), de acuerdo a los requisitos del proyecto

### Problemas detectados

Por alguna razón, en ocasiones, he detectado que el bus I2C del M5STACK se bloquea y no reconoce los sensoreres conectados. Para desbloquear el DataLogger es necesario desconectar todos los modulos apilables, conectarlos de nuevo y alimentar al DataLogger mediante el USB. Un vez hecho esto se puede iniciar el DataLogger, el cual detectará el sensor conectado al bus I2C

### Proyecto realizado con Platformio por gurues@2019-2020
