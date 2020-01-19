# **MULTISENSOR I2C DATALOGGER**

## -------------------------------------------------------------------------------------------

### En este proyecto se realiza un DataLogger que registra los parámetros del sensor conectado al bus I2C del M5STACK (ESP32). Un RTC DS3231 proporciona la fecha/hora del registro si no configura o dispone de red WIFI

- Los sensores soportados son los siguientes:
  - BME280 (temperatura, humedad, presión y altitud)
  - BME680 (temperatura, humedad, presión y Gas VOC)
  - LM75 (temperatura)
  - SHT21 (temperatura y humedad)
  - BH1750 (nivel luminosidad)
  - VEML6075 (indice UV) (No funciona actualmente)
  - TSL2541 (nivel luminosidad)
  - AM2320 (temperatura y humedad)
  
### Conexiones hardware

- El sensor a registrar se puede conectar mediante:
  - Conector GROVE I2C 5V del M5STACK (solo sensores de 5v)
  - PCB adaptador de sensor diseñado para la conexión del sensor a los pines 5V 3V3 G SDA(21) SCL(22) del M5STACK (sensores de 3V3 y 5v eligiendo el conector adecuado del adaptador)
- El RTC DS3231 está conectado al bus I2C de forma fija en el módulo PROTO del M5STACK (3V3 GND SDA-21 SCL-22)
- Adicionalmente se dispone del módulo BATTERY para aumentar la capacidad de funcionamiento autónomo del DataLogger
  
### El Datalogger realiza las siguientes funciones

- Detección automática del sensor conectado al bus I2C
- Almacena los registros realizados en una tarjeta SD en formato CSV para su posterior tratamiento y análisis
- Puede comunicarse via bluetooth BLE con la aplicación Blynk para mostrar los datos y traspasar su propio control
- Mediante los botones del M5STACK se configura el Datalogger mediante un despliegue de menús
  - Botón A: "SETUP"
    - Intervalo de escaneo (30 seg, 60 seg, 5 min, 15 min, Intervalo escaneo en min)
    - Configuración SD (Borrar SD / Graba datos SI/NO)
    - Configuración General (WIFI, Reloj, Pantalla, BLE, ....)
    - Configuración RTC (configuracón manual o WIFI de fecha y hora)
    - EXIT
  - Botón B: "START / SLEEP"
    - Pulso corto: Activa Datalogger. Comienza a registrar
    - Pulsación larga: EL M5STACK entra en modo "deep sleep", ahorro de energia. Despierta en los intervalos configurados para realizar un registro
  - Botón C: "STOP / EXIT SLEEP"
    - Pulso corto: Para Datalogger. Detiene el registro
    - Pulsación larga: Saca al M5STACK del modo "deep sleep", recupera de la SD el último registro para mostralo en pantalla y contimua con los registros en los intervalos configurados
  
### La app de BLYNK realiza las siguientes funciones

- Conexión mediante BLE con el DataLogger
- Control del DataLogger:
  - Botón START/STOP DATALOGGER: Inicio y paro del registro
  - Botón WAIT BLE: En modo ahorro de energía muestra los datos en la app ya que permite la conexión BLE
  - Botón LOW ENERGY: Activa/Desactiva el modo ahorro de energia ordenando la entrada en "deep sleep" al M5STACK
  - Botones 30SEG - 60 SEG - 5MIN - 15MIN - XX MIN. Configuración intervalos de registro. XX MIN muestra el intervalo en min introducido por teclado (no permite su control)
  - Porcentaje de batería
  - Indicaciones de los valores de registro y sus gráficas

### Direcciones I2C de los dispositivos utilizados

- Address: 35  (0x23) BH1750
- Address: 56  (0x38) 57 (0x39) VEML6075 (Actualmente no funciona)
- Address: 57  (0x39) TSL2541
- Address: 58  (0x40) SHT21
- Address: 72  (0x48) LM75
- Address: 92  (0x5C) AM2320
- Address: 104 (0x68) RTC DS3231
- Address: 117 (0x75) IP5306 Controlador de Bateria del M5STACK
- Address: 118 (0x76) BME-280
- Address: 119 (0x77) BME-680
  
### Otros contenidos

- Carpeta "app BLYNK":
  - Contiene la configuración y diseño de la app BLE que se conecta la DataLogger
  
- Carpeta "Gerber_PCB_M5_SENSOR_DATALOGGER"
  - Contiene el diseño de la PCB del adaptador de sensores para su conexión a los pines 5V 3V3 G SDA(21) SCL(22) del M5STACK

- Carpeta "tools"
  - Contiene las instruciones y programas necesarios para convertir imagenes bmp, en matriz C. Estas imagenes en matrisz C pueden ser mostradas porsteriormente en la pantalla del M5STACK.

- Archivo datalog.csv. Ejemplo de archivo de registro del Datalogger. Es necesario crear o copiar un archivo con ese nombre en la tarjeta SD para que el código del programa funcione correctamente

- Archivo user-partition.csv. Tabla de particiones personalizada (CSV) para gestionar las capacidades de memoria del M5STACK (ESP32), de acuerdo a los requisitos del proyecto

### Proyecto realizado por gurues@2019-2020
