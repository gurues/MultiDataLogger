/*
 ------------------------------------------------------------------------------------------------------------
 -----------                               MULTIDATA LOGGER                                ------------------
 ------------------------------------------------------------------------------------------------------------ 

 En este proyecto se realiza un DataLogger que registra los parámetros del sensor conectado al bus I2C del 
 M5STACK (ESP32). Un RTC DS3231 proporciona la fecha/hora del registro si no configura o dispone de red WIFI.
 El DataLogger es capaz de detectar el sensor conectado al bus I2C y predisponerse a registrar sus parámetros.

 Sensores soportados: 
 BME280 (temperatura, humedad, presión y altitud) - BME680 (temperatura, humedad, presión y Gas VOC)
 LM75 (temperatura) - SHT21 (temperatura y humedad) - BH1750 (nivel luminosidad) - VEML6075 (indice UV)
 TSL2541 (nivel luminosidad) - AM2320 (temperatura y humedad)
 
 Para más información lee el archivo Readme.md

 Realizado por gurues@2019-2020

 ------------------------------------------------------------------------------------------------------------
*/

//Descomenta para DEBUG Blynk por puerto serie
#define BLYNK_DEBUG
#define BLYNK_PRINT Serial

// Descomenta para relizar DEBUG por puerto serie
#define DEBUG_DATALOGGER

// Librerias del programa
#include <Arduino.h>
#include <M5Stack.h>
#include <ezTime.h>
#include <M5ez.h>
#include <Wire.h>     
#include <BH1750.h>
#include <Temperature_LM75_Derived.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_AM2320.h"
#include <Adafruit_TSL2561_U.h>
#include "Adafruit_HTU21DF.h"
#include <Adafruit_I2CDevice.h>
#include "Adafruit_VEML6075.h"
#include <Adafruit_BME280.h>
#include "Adafruit_BME680.h"
#include "RTClib.h"
#include <SimpleTimer.h>
#include <stdlib.h>
#include <BlynkSimpleEsp32_BLE.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include "esp_wifi.h"

// Conexión BLE Blynk
#define BLYNK_USE_DIRECT_CONNECT

//Token app Blynk
char auth[] = "votRVhHKSuTYsnu9cupBwNq-yD0scAZK";

//matriz C externa del logo
extern const unsigned char logo[];

// Definición de Constantes
#define uS_to_S_Factor 1000         // Factor para DeepSleep en segundos
#define SEALEVELPRESSURE_mbar (1010) // Estimar la altitud para una presión dada comparándola con la presión
                                    // a nivel del mar. Este ejemplo utiliza el valor predeterminado, pero 
                                    // para obtener resultados más precisos, reemplace el valor con la 
                                    // presión actual del nivel del mar en su ubicación

// Definición de Pines
#define PIN_Boton_C 37  // Pin - GPIO del Boton C en M5Stack, en el programa despertará al M5Stack
#define PIN_SDA 21      // Pin SCA I2C
#define PIN_SCL 22      // Pin SCL I2C

// Refresco eventos cada 250 mseg (SimpleTimer funciones y Blynk App)
const uint16_t refresco = 250; 

// Objeto SimpleTimer para la ejecución periodica de funciones
SimpleTimer timer;
int numTimer, batTimer;

//Objeto RTC
RTC_DS3231 rtc;

// Objeto timezone
Timezone myTZ;

// Objetos de sensores permitidos:
// Objeto BH1750
BH1750 bh1750; // I2C 0x23
// Objeto VEML6075
Adafruit_VEML6075 veml6075 = Adafruit_VEML6075(); // I2C 0x38 y 0x39 No funciona
// Objeto TSL2541
Adafruit_TSL2561_Unified tsl2541 = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345); // I2C 0x39
// Objeto SHT21
Adafruit_HTU21DF sht21 = Adafruit_HTU21DF(); // I2C 0x40
// Objeto CJMCU-75 -> LM75
Generic_LM75 lm75; // I2C 0x48
// Objeto AM2320
Adafruit_AM2320 am2320 = Adafruit_AM2320(); // I2C 0x5C
// Objeto BME280
Adafruit_BME280 bme280; // I2C 0x76
// Objeto BME680
Adafruit_BME680 bme680; // I2C 0x77


// Objeto para el manejo de ficheros en tarjeta SD
File myFile;

// Variables de estado del Proyecto
enum Estado_Energy{     // Modos de energia del DataLogger
    Normal, Ahorro, Arranque
};
enum Estado_Registro{   // Modos de registro
    stop, start, run
};
enum Sensor_Registro{   // Sensores permitidos
    ninguno, BME280, BME680, LM75, SHT21, lux_BH1750, VEML6075, TSL2541, AM2320
};

// Variables de contol del Proyecto
RTC_DATA_ATTR Estado_Energy mode_energy = Arranque; // Modos de energia -> 0: Normal 1: Ahorro Energia Activado 2: Arranque 1ª vez Normal
RTC_DATA_ATTR Estado_Registro init_scan = run;      // Modos de registro de datos -> start/stop/run
RTC_DATA_ATTR Sensor_Registro sensor = ninguno;     // Sensor I2C a registrar
RTC_DATA_ATTR uint32_t intervalo = 30000;           // Intervalo muestreo por defecto 30 seg.
RTC_DATA_ATTR bool red_wifi = false;                // Variable de control red wifi -> false: No conectado true: conectado.
RTC_DATA_ATTR bool save_SD = true;                  // Variable de control SD save_SD -> false: No graba datos en SD true: Graba datos en SD
RTC_DATA_ATTR bool estado_BLE = false;              // Variable de control Bluetooth BLE -> false: No conectado true: conectado
RTC_DATA_ATTR bool espera_BLE = false;              // Variable de control espera conexión Bluetooth BLE -> false: No espera true: espera
bool menu = false;                                  // Variable de control para no refrescar pantalla si estoy en un menu
int battery;                                        // Nivel de bateria 100% - 75% - 50% - 25%
Estado_Energy mem_mode_energy;                      // Etado anterior Modos de energia -> 0: Normal 1: Ahorro Energia Activado 2: Arranque 1ª vez Normal

//variables a registrar del sensor
char registro_1 [20];
char registro_2 [20];
char registro_3 [20];
char registro_4 [20];
                         
//Variable control encabezados DataLogger
////{campos[0], campos[1],campos[2], campos[3],campos[4], campos[5],
//campos[6], campos[7],campos[8], campos[9]}
//{encabezado reg_1, unidad reg_1,encabezado reg_2, unidad reg_2,encabezado reg_3, unidad reg_3,
// encabezado reg_4, unidad reg_4,sensor detectado, escaneo}

char const *campos[10]; 

// Terminal de la app BLE BLYNK
WidgetTerminal terminal (V10);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////                  FUNIONES                 /////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Actualiza porcentaje bateria
void Level_Battery(){ // Controlador de carga Bateria IP5306_I2C chip del M5STACK
    battery = M5.Power.getBatteryLevel();
    ez.header.show("DATA LOGGER   " + (String)battery + "%");
    if (estado_BLE)
        Blynk.virtualWrite(V6,battery); 
}

// Inicio Pantalla Vacia
void Inicia_Pantalla(){
    ez.screen.clear();
    Level_Battery();
    ez.buttons.show("SETUP # # START # SLEEP # STOP # OFF");
    ez.canvas.y(ez.canvas.height()/5);
    ez.canvas.x(15);
    ez.canvas.font(&FreeSerifBold12pt7b);
} 

// Escribe mensajes en Pantalla
void write_Pantalla(String text){
    ez.screen.clear();
    Level_Battery();
    ez.buttons.show("SETUP # # START # SLEEP # STOP # OFF");
    ez.canvas.lmargin(30);
    ez.canvas.y(ez.canvas.height()/2);
    ez.canvas.font(&FreeSansBoldOblique9pt7b);
    ez.canvas.print(text);
} 

// Actualiza el intervalo de escaneo para mostralo por pantalla
void getTimeScan(){
    if (intervalo == 30000){
        campos [9] =" 30 seg";
    }
    if (intervalo == 60000){
        campos [9] =" 60 seg";
    }
    if (intervalo == 300000){
        campos [9] =" 5 min";
    }
    if (intervalo == 900000){
        campos [9] =" 15 min";
    }      
    if (intervalo == 1800000){
        campos [9] =" 30 min";
    }
}

// Muestras los datos registrados en pantalla y actualiza porcentaje de bateria
void show_Data(){
    Level_Battery();
    ez.canvas.clear();
    ez.canvas.font(&FreeMonoBold12pt7b);
    ez.canvas.y(ez.canvas.height()/6);
    ez.canvas.x(50);
    ez.canvas.print("Escaneo");
    ez.canvas.print(campos [9]);
    // sensor de 1 registro: LM75 / lux_BH1750 / VEML6075 / TSL2541
    if ((sensor==BME280)||(sensor==BME680)||(sensor==AM2320)||(sensor==SHT21)||
        (sensor==LM75)||(sensor==lux_BH1750)||(sensor==VEML6075)||(sensor==TSL2541)){
        ez.canvas.y(2*(ez.canvas.height()/6));
        ez.canvas.x(15);
        ez.canvas.print(campos[0]);
        ez.canvas.print(": ");
        ez.canvas.print(registro_1);
        ez.canvas.print(" ");
        ez.canvas.println(campos[1]);
    }
    //  sensor de 2 registro: AM2320/ SHT21
    if ((sensor==BME280)||(sensor==BME680)||(sensor==AM2320)||(sensor==SHT21)){
        ez.canvas.y(3*(ez.canvas.height()/6));
        ez.canvas.x(15);
        ez.canvas.print(campos[2]);
        ez.canvas.print(": ");
        ez.canvas.print(registro_2);
        ez.canvas.print(" ");
        ez.canvas.println(campos[3]);
    }
    //  sensor de 4 registro: BME280 / BME680
    if ((sensor==BME280)||(sensor==BME680)){
        ez.canvas.y(4*(ez.canvas.height()/6));
        ez.canvas.x(15);
        ez.canvas.print(campos[4]);
        ez.canvas.print(": ");
        ez.canvas.print(registro_3);
        ez.canvas.print(" ");
        ez.canvas.print(campos[5]);
        
        ez.canvas.y(5*(ez.canvas.height()/6));
        ez.canvas.x(15);
        ez.canvas.print(campos[6]);
        ez.canvas.print(": ");
        ez.canvas.print(registro_4);
        ez.canvas.print(" ");
        ez.canvas.print(campos[7]);
    }
}

void submenu_BLE(){
    ezMenu myMenu_BLE("Configuracion BLE");
    myMenu_BLE.addItem("BLE - START");
    myMenu_BLE.addItem("BLE - STOP");
    switch (myMenu_BLE.runOnce()) {
        case 1: // Se Activa BLE
            if((esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE)||
                (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_NUM)){
                btStart(); //Inicio Bluetooth
                Blynk.setDeviceName("DataLogger");
                Blynk.begin(auth); // Inicio BLYNK
                while (Blynk.connect(5000U) == false) {}
                Blynk.virtualWrite(V0,0); //DataLogger Parado
                Blynk.virtualWrite(V5,1); //Escaneo inicial 30 seg
                Blynk.virtualWrite(V7,0); //DataLogger Low Energy OFF
                Blynk.virtualWrite(V8,0); // Espera conexión BLE
                terminal.clear();
                menu = false;
                estado_BLE = true;   
            }         
            break;
        case 2: // Se DesActiva BLE
            btStop();
            menu = false;
            estado_BLE = false;
            break;
    }
}

// Menu de configuración intervalo de escaneo del DataLogger
void submenu1_SCAN(){
    ezMenu myMenu1("Configura intervalo de escaneo");
    myMenu1.addItem("30 seg");
    myMenu1.addItem("60 seg");
    myMenu1.addItem("5 min");
    myMenu1.addItem("15 min");
    myMenu1.addItem("30 min");
    switch (myMenu1.runOnce()) {
        case 1: // 30 seg
  	        if (timer.isEnabled(numTimer)){
                ez.msgBox("Configura intervalo de escaneo", "Antes, STOP DataLogger");
                Inicia_Pantalla();
                show_Data();
            }
            else{
                intervalo = 30000;
                campos [9] =" 30 seg";
                if (estado_BLE)
                    Blynk.virtualWrite(V5,1); 
                Inicia_Pantalla();
            }
            menu = false;
            break;
        case 2: // 60 seg
            if (timer.isEnabled(numTimer)){
                ez.msgBox("Configura intervalo de ecampos escaneo", "Antes, STOP DataLogger");
                Inicia_Pantalla();
                show_Data();
            }
            else{
                intervalo = 60000;
                campos [9] =" 60 seg";
                if (estado_BLE)
                    Blynk.virtualWrite(V5,2);
                Inicia_Pantalla();
            }
            menu = false;
            break;
        case 3: // 5 min
  	        if (timer.isEnabled(numTimer)){
                ez.msgBox("Configura intervalo de escaneo", "Antes, STOP DataLogger");
                Inicia_Pantalla();
                show_Data();
            }
            else{
                intervalo = 300000;
                campos [9] =" 5 min";
                if (estado_BLE)
                    Blynk.virtualWrite(V5,3);
                Inicia_Pantalla();
            }
            menu = false;
            break;
        case 4: // 15 min
            if (timer.isEnabled(numTimer)){
                ez.msgBox("Configura intervalo de escaneo", "Antes, STOP DataLogger");
                Inicia_Pantalla();
                show_Data();
            }
            else{
                intervalo = 900000;
                campos [9] =" 15 min";
                if (estado_BLE)
                    Blynk.virtualWrite(V5,4);
                Inicia_Pantalla();
            }
            menu = false;
            break;
        case 5: // 30 min
  	        if (timer.isEnabled(numTimer)){
                ez.msgBox("Configura intervalo de escaneo", "Antes, STOP DataLogger");
                Inicia_Pantalla();
                show_Data();
            }
            else{
                intervalo = 1800000;
                campos [9] =" 30 min";
                if (estado_BLE)
                    Blynk.virtualWrite(V5,5);
                Inicia_Pantalla();
            }
            menu = false;
            break;
    }
}

void borrado_encabezado_SD(){
    if(!(SD.remove("/datalog.csv"))){ // Borrado archivo datalog.csv de la SD
        ez.msgBox("Borrar SD - SI", "Error al borrar el archivo en tarjeta SD");
        Inicia_Pantalla();
        #ifdef DEBUG_DATALOGGER
            Serial.println("Error al borrar el archivo en tarjeta SD");
        #endif
    }           
    myFile = SD.open("/datalog.csv", FILE_WRITE); // Creado archivo datalog.csv en la SD
    if (myFile) {
        switch(sensor) {
            case BME280:  // Se crea encabezado BME280 en datalog.csv
                myFile.println("Dia WIFI,Fecha Hora WIFI,Fecha Hora RTC,Temperatura (ºC),Humedad (%),Presion (mbar),Altitud (m), Sensor BME280");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                Inicia_Pantalla();
                break;
            case BME680:  // Se crea encabezado BME680 en datalog.csv
                myFile.println("Dia WIFI,Fecha Hora WIFI,Fecha Hora RTC,Temperatura (ºC),Humedad (%),Presion (mbar),Gas VOC (ohmios), Sensor BME680");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                Inicia_Pantalla();
                break;
            case LM75:  // Se crea encabezado LM75 en datalog.csv
                myFile.println("Dia WIFI,Fecha Hora WIFI,Fecha Hora RTC,Temperatura (ºC), Sensor LM75");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                Inicia_Pantalla();
                break;
            case lux_BH1750:  // Se crea encabezado lux_BH1750 en datalog.csv
                myFile.println("Dia WIFI,Fecha Hora WIFI,Fecha Hora RTC,Nivel Luminosidad (lux), Sensor BH1750");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                Inicia_Pantalla();
                break;
            case VEML6075:  // Se crea encabezado BMe680 en datalog.csv
                myFile.println("Dia WIFI,Fecha Hora WIFI,Fecha Hora RTC,indice UV,, Sensor VEML6075");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                Inicia_Pantalla();
                break;
            case TSL2541:  // Se crea encabezado BMe680 en datalog.csv
                myFile.println("Dia WIFI,Fecha Hora WIFI,Fecha Hora RTC,Nivel Luminosidad (lux), Sensor TSL2541");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                Inicia_Pantalla();
                break;
            case SHT21:
                myFile.println("Dia WIFI,Fecha Hora WIFI,Fecha Hora RTC,Temperatura (ºC),Humedad (%), Sensor SHT21");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                Inicia_Pantalla();
                break;
            case AM2320:
                myFile.println("Dia WIFI,Fecha Hora WIFI,Fecha Hora RTC,Temperatura (ºC),Humedad (%), Sensor AM2320");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                Inicia_Pantalla();
                break;
        }
    }
    else{
        ez.msgBox("Borrar SD - SI", "Error al crear archivo datalog.csv en tarjeta SD");
        Inicia_Pantalla();
        #ifdef DEBUG_DATALOGGER
            Serial.println("Error al crear archivo datalog.csv en tarjeta SD");
        #endif
    }
}

// Menu Borrar SD
void submenu2_SD(){
    ezMenu myMenu2("Configuracion SD");
    myMenu2.addItem("Borrar SD - SI");
    myMenu2.addItem("Grabar Datos SD SI/NO");
    myMenu2.addItem("EXIT");
    switch (myMenu2.runOnce()) {
        case 1: // Se Borra fichero con datos y se crea de nuevo
            if (timer.isEnabled(numTimer)){
                ez.msgBox("Borrar SD - SI", "Antes, STOP DataLogger");
                Inicia_Pantalla();
                show_Data();
            }
            else{
                // Borrado archivo datalog.csv y actulización encabezados de la SD
                borrado_encabezado_SD();
            }
            menu = false;
            break;
        case 2: // Grabar Datos en SD SI/NO
            if (timer.isEnabled(numTimer)){
                ez.msgBox("Grabar Datos en SD SI/NO", "Antes, STOP DataLogger");
                Inicia_Pantalla();
                show_Data();
            }
            else{
                ezMenu myMenu22("Grabar Datos SD");
                myMenu22.addItem("Grabar Datos - SI");
                myMenu22.addItem("Grabar Datos - NO");
                switch (myMenu22.runOnce()) {
                    case 1: // Grabar Datos - SI
                        save_SD = true;
                        Inicia_Pantalla();
                        break;  
                    case 2: // Grabar Datos - NO
                        save_SD = false;
                        Inicia_Pantalla();
                        break; 
                }
            }
            menu = false;
            break;
        case 3: // EXIT
            if (timer.isEnabled(numTimer)){
                Inicia_Pantalla();
                show_Data();
            }
            else
                Inicia_Pantalla();
            menu = false;
            break;
    }
}

// Menu Configuración General DAtaLoggger
void submenu3_CONFIG(){
    ez.settings.menu();
    red_wifi = ez.wifi.autoConnect; // Compruebo WIFI
    if(red_wifi){
        ez.clock.tz.setLocation(F("Europe/Madrid"));
        ez.clock.waitForSync();
    } 
    if (timer.isEnabled(numTimer)){ //Iniciado DataLogger
        Inicia_Pantalla();
        show_Data();
    }
    else
        Inicia_Pantalla();
}

void submenu4_RTC(){
    int F_year=2020, F_day=1, F_month=1, F_hour=0, F_minute=0;
    if(red_wifi){
        F_year=myTZ.year();
        F_day=myTZ.day();
        F_month=myTZ.month();
        F_hour=myTZ.hour();
        F_minute=myTZ.minute();
    }
    ezMenu myMenu4("Configuracion RTC");
    myMenu4.addItem("Day");
    myMenu4.addItem("Month");
    myMenu4.addItem("Year");
    myMenu4.addItem("Hour");
    myMenu4.addItem("Minute");
    myMenu4.addItem("RTC_OK");
    myMenu4.addItem("EXIT");
    String dato;
    switch (myMenu4.runOnce()) {
        case 1: // Dia   
            dato = (ez.textInput("Day", (String)F_day));
            F_day = dato.toInt();
            submenu4_RTC();
            break;
        case 2: // Mes  
            dato = (ez.textInput("Month", (String)F_month));
            F_month = dato.toInt();
            submenu4_RTC();
            break;
        case 3: // Año  
            dato = (ez.textInput("Year", (String)F_year));
            F_year = dato.toInt();
            submenu4_RTC();
            break;
        case 4: // Hora  
            dato = (ez.textInput("Hour", (String)F_hour));
            F_hour = dato.toInt();
            submenu4_RTC();
            break;
        case 5: // Minuto  
            dato = (ez.textInput("Minute", (String)F_minute));
            F_minute = dato.toInt();
            submenu4_RTC();
            break;
        case 6: // RTC_OK
            // Fijar a fecha y hora específica. En el ejemplo, 21 de Enero de 2016 a las 03:00:00
            // rtc.adjust(DateTime(2016, 1, 21, 3, 0, 0));
            rtc.adjust(DateTime(F_year, F_month, F_day, F_hour, F_minute, 0)); 
            Inicia_Pantalla();
            menu = false;
            break;
        case 7: // EXIT
            Inicia_Pantalla();
            menu = false;
            break;
    }
    
}

// Menu Principal de Configuración del DataLogger
void Config_DataLogger(){
    menu = true;
    ezMenu myMenu("Configuracion DataLogger");
    myMenu.addItem("Intervalo de escaneo");
    myMenu.addItem("Configuracion SD");
    myMenu.addItem("Configuracion General");
    myMenu.addItem("Configuracion RTC");
    myMenu.addItem ("EXIT");
    switch (myMenu.runOnce()) {
        case 1: // Intervalo de escaneo   
            submenu1_SCAN();
            break;
        case 2: // Eliminar Datos SD  
            submenu2_SD();
            break;
        case 3: // Configuracion General  
            submenu3_CONFIG();
            break;
        case 4: // Configuracion General  
            submenu4_RTC();
            break;
        case 5: // EXIT
            if (timer.isEnabled(numTimer)){
                Inicia_Pantalla();
                show_Data();
            }
            else
                Inicia_Pantalla();
            menu = false;
            break;
    }
}

// Gestión acciones de los botones
void scanButton() {
    String btn = ez.buttons.poll();

    if (btn == "SETUP") Config_DataLogger();

    if ((btn == "START")&&(!(timer.isEnabled(numTimer)))){
        write_Pantalla("Iniciado DataLogger " + (String)campos [8] + "       escaneando cada" + (String)campos [9]);
        init_scan = start;
        mode_energy = Normal;
        if (estado_BLE){
            Blynk.virtualWrite(V0,init_scan);
            Blynk.virtualWrite(V9,"");
        }
    } 

    if ((btn == "STOP")&&(timer.isEnabled(numTimer))){
        write_Pantalla("Parado DataLogger " + (String)campos [8]);   
        init_scan = stop;
        mode_energy = Normal;
        if (estado_BLE)
            Blynk.virtualWrite(V0,init_scan);
    } 

    if (btn == "OFF"){
        M5.Power.powerOFF();
    }

    if ((btn == "SLEEP")&&(timer.isEnabled(numTimer))){
        mode_energy = Ahorro;
        if (estado_BLE)
            Blynk.virtualWrite(V7,mode_energy);
        //Sueño profundo ESP32 del M5STACK
        if (red_wifi){ // Desconectamos WIFI si esta activo
            esp_wifi_disconnect();
            esp_wifi_stop();
        }
        if (estado_BLE){// Desconectamos BLE si estaba activo
            esp_bt_controller_disable();
        }
        M5.Power.deepSleep(intervalo*uS_to_S_Factor);
    }
    if (btn == "Yes") {
        // Borrado archivo datalog.csv y actulización encabezados de la SD
        borrado_encabezado_SD();
        write_Pantalla("Datalogger y tarjeta SD                preparados para sensor " + (String)campos [8]); 
    }
    if (btn == "No") {
        // Borrado archivo datalog.csv y actulización encabezados de la SD
        write_Pantalla("Datalogger preparado para           sensor " + (String)campos [8]); 
    }
}

//Ejecución de eventos asociados
uint16_t Programa_Eventos(){
    timer.run(); // Inicio SimpleTimer para la ejecución de funciones
    if (estado_BLE)
        Blynk.run(); // Inicio Blynk
    return refresco; // retorna el intervalo para su nueva ejecución
}

void sensor_display_Blink(){
    // sensor de 1 registro: LM75 / lux_BH1750 / VEML6075 / TSL2541
    if ((sensor==BME280)||(sensor==BME680)||(sensor==AM2320)||(sensor==SHT21)||
        (sensor==LM75)||(sensor==lux_BH1750)||(sensor==VEML6075)||(sensor==TSL2541)){
        //Actualizo Registro_1 en Blink app
        Blynk.virtualWrite(V1,registro_1); 
        terminal.print("    ");
        terminal.print(campos[0]);
        terminal.print(" = ");
        terminal.print(registro_1);
        terminal.print(" ");
        terminal.print(campos[1]);
        terminal.flush();
    }
    //  sensor de 2 registro: AM2320/ SHT21
    if ((sensor==BME280)||(sensor==BME680)||(sensor==AM2320)||(sensor==SHT21)){
        //Actualizo Registro_2 en Blink app
        Blynk.virtualWrite(V2,registro_2); 
        terminal.print("    ");
        terminal.print(campos[2]);
        terminal.print(" = ");
        terminal.print(registro_2);
        terminal.print(" ");
        terminal.print(campos[3]);
        terminal.flush();
    }
    //  sensor de 4 registro: BME280 / BME680
    if ((sensor==BME280)||(sensor==BME680)){
        //Actualizo Registro_3 en Blink app
        Blynk.virtualWrite(V3,registro_3);
        terminal.print("    ");
        terminal.print(campos[4]);
        terminal.print(" = ");
        terminal.print(registro_3);
        terminal.print(" ");
        terminal.print(campos[5]);
        terminal.flush();
        //Actualizo Registro_4 en Blink app
        Blynk.virtualWrite(V4,registro_4);
        terminal.print("    ");
        terminal.print(campos[6]);
        terminal.print(" = ");
        terminal.print(registro_4);
        terminal.print(" ");
        terminal.print(campos[7]);
        terminal.flush();
    }
}
void leer_SD(){
    String cadena = "";
    int Posicion = 0; // Posición de lectura archivo datalog.csv

    myFile = SD.open("/datalog.csv", FILE_READ);//abrimos  el archivo y añadimos datos
    if (myFile) { 
        #ifdef DEBUG_DATALOGGER
            Serial.println("Leyendo datos de Tarjeta SD .....");
        #endif
        int totalBytes = myFile.size();
        #ifdef DEBUG_DATALOGGER
            Serial.print("totalBytes: ");
            Serial.println((String)totalBytes);
        #endif
        // Busco cuantos bytes ocupan los últimos registros
        // Me posiciono en la última posición con datos del archivo datalog.csv
        Posicion = totalBytes - 3;
        myFile.seek(Posicion); 
        while (myFile.available()) {
            char caracter = myFile.read();
            if((caracter==00)||(caracter==10)||(caracter==13)){ //ASCII de nueva de línea  o Retorno de carro       
                break;
            }
            else{
                Posicion = Posicion - 1;
                myFile.seek(Posicion);
            }
        }
        #ifdef DEBUG_DATALOGGER
            Serial.print("Bytes a Recuperar:");
            Serial.println((String)Posicion);
        #endif
        //Leemos los ultimos registros del archivo datalog.csv
        Posicion = Posicion + 1;
        myFile.seek(Posicion);
        String cadena;
        int i=0;
        while (myFile.available()) {
            char caracter = myFile.read();
            if(caracter==44){ //ASCII: ',' Sa busca para encortrar los registros a recuperar
                switch (i) { // DIA,FECHA WIFI,FECHA RTC,registro_1,registro_2,registro_3,registro_4
                    case 3: 
                        cadena.toCharArray(registro_1, sizeof(registro_1));
                        #ifdef DEBUG_DATALOGGER
                            Serial.print("Registros_1 recuperados SD:");
                            Serial.println(registro_1);
                        #endif
                        break;  
                    case 4: 
                        cadena.toCharArray(registro_2, sizeof(registro_2));
                        #ifdef DEBUG_DATALOGGER
                            Serial.print("Registros_2 recuperados SD:");
                            Serial.println(registro_2);
                        #endif
                        break;
                    case 5: 
                        cadena.toCharArray(registro_3, sizeof(registro_3));
                        #ifdef DEBUG_DATALOGGER
                            Serial.print("Registros_3 recuperados SD:");
                            Serial.println(registro_3);
                        #endif
                        break;
                    case 6: 
                        cadena.toCharArray(registro_4, sizeof(registro_4));
                        #ifdef DEBUG_DATALOGGER
                            Serial.print("Registros_4 recuperados SD:");
                            Serial.println(registro_4);
                        #endif
                        break;
                }
                #ifdef DEBUG_DATALOGGER
                    Serial.print("i:");
                    Serial.println(i);
                #endif
                i++;
                cadena = "";
            }
            else{
                cadena = cadena + caracter;
            }
            if(myFile.position()==(totalBytes)){
                break;
            }
        }
        myFile.close(); //cerramos el archivo
    }
    else{
        #ifdef DEBUG_DATALOGGER	
            Serial.println("Error al abrir el archivo datalog.csv");
        #endif
    }
}

void almacenar_SD(char Fecha_RTC[20]){
    //Almacenamos los datos en la SD
    myFile = SD.open("/datalog.csv", FILE_APPEND);//abrimos  el archivo y añadimos datos
    if (myFile) { 
        #ifdef DEBUG_DATALOGGER
            Serial.println("Almacenando datos en Tarjeta SD .....");
        #endif
        myFile.print((myTZ.dateTime("l, d-M-y H:i:s")));
        myFile.print(",");
        myFile.print(Fecha_RTC);
        myFile.print(",");
        myFile.print(registro_1);
        if ((sensor==BME280)||(sensor==BME680)||(sensor==AM2320)||(sensor==SHT21)){ // sensor de 2 registros
            myFile.print(",");
            myFile.print(registro_2);  
        }
        if ((sensor==BME280)||(sensor==BME680)){ // sensor de 4 registros
            myFile.print(",");
            myFile.print(registro_3);  
            myFile.print(",");
            myFile.print(registro_4);  
        }
        myFile.println(",");  // Salto de linea
        myFile.close();    //cerramos el archivo                    
    } 
    else {
        #ifdef DEBUG_DATALOGGER
            Serial.println("Error al abrir el archivo en tarjeta SD");
        #endif
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////                FUNCIONES SENSORES                 /////////////////////////////////////////                                 
///////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG_DATALOGGER
    void debug_Sensor(char Fecha_RTC[20]){
    // DEBUG DataLogger
        Serial.println("*************************************************************");
        Serial.println("Fecha / Hora WIFI: " + myTZ.dateTime("l, d-M-y H:i:s"));
        Serial.print("Fecha / Hora RTC: "); Serial.println(Fecha_RTC);
        if(red_wifi) Serial.println("Red Wifi = True");
        else Serial.println("Red Wifi = False");
        if(estado_BLE) Serial.println("estado_BLE = True");
        else Serial.println("estado_BLE = False");
        if(espera_BLE) Serial.println("espera_BLE = True");
        else Serial.println("espera_BLE = False");
        Serial.println("Modo Energia = " + (String)mode_energy);
        Serial.println("scan = " + (String)campos [9]);
        Serial.println("Temperatura = " + (String)registro_1);
        Serial.println("Humedad = " + (String)registro_2);
        Serial.println("Presión = " + (String)registro_3);
        Serial.println("Altitud = " + (String)registro_4);
    }
#endif

void sensor_BME280(char Fecha_RTC[20]){  
    //Leer temperatura.
    dtostrf(bme280.readTemperature(),2,1,registro_1);
    //Leer humedad.
    dtostrf(bme280.readHumidity(),2,1,registro_2);
    //Leer presion.
    dtostrf(bme280.readPressure()/ 100.0F,2,1,registro_3);
    //Leer altitud.
    dtostrf(bme280.readAltitude(SEALEVELPRESSURE_mbar),2,1,registro_4);
    //Se actulizan datos en app Blynk
    if (estado_BLE){
        Blynk.setProperty(V1,"label", "                         Temperatura");
        Blynk.setProperty(V2,"label", "                         Humedad");
        Blynk.setProperty(V3,"label", "                         Presion");
        Blynk.setProperty(V4,"label", "                         Altidud");
        sensor_display_Blink();
    }
    // DEBUG DataLogger
    #ifdef DEBUG_DATALOGGER
        debug_Sensor(Fecha_RTC);
    #endif
}

void sensor_BME680(char Fecha_RTC[20]){
    //Leer temperatura.
    dtostrf(bme680.readTemperature(),2,1,registro_1);
    //Leer humedad.
    dtostrf(bme680.readHumidity(),2,1,registro_2);
    //Leer presion.
    dtostrf(bme680.readPressure()/ 100.0F,2,1,registro_3);
    //Leer gas VOC.uint32_t readGas(void);
    dtostrf(bme680.readGas(),2,1,registro_4);
    //Se actulizan datos en app Blynk
    if (estado_BLE){
        Blynk.setProperty(V1,"label", "                         Temperatura");
        Blynk.setProperty(V2,"label", "                         Humedad");
        Blynk.setProperty(V3,"label", "                         Presion");
        Blynk.setProperty(V4,"label", "                         Gas VOC");
        sensor_display_Blink();
    }
    // DEBUG DataLogger
    #ifdef DEBUG_DATALOGGER
        debug_Sensor(Fecha_RTC);
    #endif
}

void sensor_LM75(char Fecha_RTC[20]){
    //Leer nivel luminusidad.
    dtostrf(lm75.readTemperatureC(),2,1,registro_1);
    //Registro 2.
    dtostrf(0.0,2,1,registro_2);
    //Registro 3.
    dtostrf(0.0,2,1,registro_3);
    //Registro 4.
    dtostrf(0.0,2,1,registro_4);
    //Se actulizan datos en app Blynk
    if (estado_BLE){
        Blynk.setProperty(V1,"label", "                         Temperatura");
        Blynk.setProperty(V2,"label", " ");
        Blynk.setProperty(V3,"label", " ");
        Blynk.setProperty(V4,"label", " ");
        sensor_display_Blink();
    }
    // DEBUG DataLogger
    #ifdef DEBUG_DATALOGGER
        debug_Sensor(Fecha_RTC);
    #endif
}

void sensor_SHT21(char Fecha_RTC[20]){
    //Leer temperatura.
    dtostrf(sht21.readTemperature(),2,1,registro_1);
    //Leer humrdad.
    dtostrf(sht21.readHumidity(),2,1,registro_2);
    //Registro 3.
    dtostrf(0.0,2,1,registro_3);
    //Registro 4.
    dtostrf(0.0,2,1,registro_4);
    //Se actulizan datos en app Blynk
    if (estado_BLE){
        Blynk.setProperty(V1,"label", "                         Temperatura");
        Blynk.setProperty(V1,"label", "                         Humedad");
        Blynk.setProperty(V3,"label", " ");
        Blynk.setProperty(V4,"label", " ");
        sensor_display_Blink();
    }
    // DEBUG DataLogger
    #ifdef DEBUG_DATALOGGER
        debug_Sensor(Fecha_RTC);
    #endif
}

void sensor_BH1750(char Fecha_RTC[20]){
    //Leer luminosidad.
    dtostrf(bh1750.readLightLevel(),2,1,registro_1);
    //Registro 2.
    dtostrf(0.0,2,1,registro_2);
    //Registro 3.
    dtostrf(0.0,2,1,registro_3);
    //Registro 4.
    dtostrf(0.0,2,1,registro_4);
    //Se actulizan datos en app Blynk
    if (estado_BLE){
        Blynk.setProperty(V1,"label", "                         Luminosidad");
        Blynk.setProperty(V2,"label", " ");
        Blynk.setProperty(V3,"label", " ");
        Blynk.setProperty(V4,"label", " ");
        sensor_display_Blink();
    }
    // DEBUG DataLogger
    #ifdef DEBUG_DATALOGGER
        debug_Sensor(Fecha_RTC);
    #endif
}

void sensor_VEML6075(char Fecha_RTC[20]){
    //Leer indice UV.
    dtostrf(veml6075.readUVI(),2,1,registro_1);
    //Registro 2.
    dtostrf(0.0,2,1,registro_2);
    //Registro 3.
    dtostrf(0.0,2,1,registro_3);
    //Registro 4.
    dtostrf(0.0,2,1,registro_4);
    //Se actulizan datos en app Blynk
    if (estado_BLE){
        Blynk.setProperty(V1,"label", "                        Indice UV");
        Blynk.setProperty(V2,"label", " ");
        Blynk.setProperty(V3,"label", " ");
        Blynk.setProperty(V4,"label", " ");
        sensor_display_Blink();
    }
    // DEBUG DataLogger
    #ifdef DEBUG_DATALOGGER
        debug_Sensor(Fecha_RTC);
    #endif
}

void sensor_TSL2541(char Fecha_RTC[20]){
    //Leer luminosidad.
    sensors_event_t event;
    tsl2541.getEvent(&event);
    /* Display the results (light is measured in lux) */
    dtostrf(event.light,2,1,registro_1);
    //Registro 2.
    dtostrf(0.0,2,1,registro_2);
    //Registro 3.
    dtostrf(0.0,2,1,registro_3);
    //Registro 4.
    dtostrf(0.0,2,1,registro_4);
    //Se actulizan datos en app Blynk
    if (estado_BLE){
        Blynk.setProperty(V1,"label", "                         Luminosidad");
        Blynk.setProperty(V2,"label", " ");
        Blynk.setProperty(V3,"label", " ");
        Blynk.setProperty(V4,"label", " ");
        sensor_display_Blink();
    }
    // DEBUG DataLogger
    #ifdef DEBUG_DATALOGGER
        debug_Sensor(Fecha_RTC);
    #endif
}

void sensor_AM2320(char Fecha_RTC[20]){
    //Leer temperatura.
    dtostrf(am2320.readTemperature(),2,1,registro_1);
    //Leer humrdad.
    dtostrf(am2320.readHumidity(),2,1,registro_2);
    //Registro 3.
    dtostrf(0.0,2,1,registro_3);
    //Registro 4.
    dtostrf(0.0,2,1,registro_4);
    //Se actulizan datos en app Blynk
    if (estado_BLE){
        Blynk.setProperty(V1,"label", "                         Temperatura");
        Blynk.setProperty(V1,"label", "                         Humedad");
        Blynk.setProperty(V3,"label", " ");
        Blynk.setProperty(V4,"label", " ");
        sensor_display_Blink();
    }
    // DEBUG DataLogger
    #ifdef DEBUG_DATALOGGER
        debug_Sensor(Fecha_RTC);
    #endif
}

void Scanner_I2C(){
    #ifdef DEBUG_DATALOGGER
        Serial.println ();
        Serial.println ("I2C scanner. Scanning ...");
    #endif
    byte direccion_I2C = 0;

    for (byte direccion = 8; direccion < 120; direccion++){
        Wire.beginTransmission (direccion); // Comienza I2C transmision Address (direccion)
        if (Wire.endTransmission () == 0) { // Recibido 0 = success (ACK response) 
            if (((direccion)!=104)&&((direccion)!=117)){ // 0x68 RTC y 0x75 IP5306 chip del M5STACK
                direccion_I2C = direccion;
                break;
            }
        }
    }
    #ifdef DEBUG_DATALOGGER
        Serial.print ("Found address: ");
        Serial.print (direccion_I2C, DEC);
        Serial.print (" (0x");
        Serial.print (direccion_I2C, HEX);     
        Serial.println (")");
    #endif
    switch(direccion_I2C){
        case 35://0x23
            sensor = lux_BH1750;
            //Se actulizan datos del encabezado
            campos[0] = "Luminosidad";
            campos[1] = "lux";
            campos[2] = "-";
            campos[3] = "-";
            campos[4] = "-";
            campos[5] = "-";
            campos[6] = "-";
            campos[7] = "-";
            break;
        case 56://0x38
            sensor = VEML6075;
            //Se actulizan datos del encabezado
            campos[0] = "Indice UV";
            campos[1] = " ";
            campos[2] = "-";
            campos[3] = "-";
            campos[4] = "-";
            campos[5] = "-";
            campos[6] = "-";
            campos[7] = "-";
            break;
        case 57://0x39
            sensor = TSL2541;
            //Se actulizan datos del encabezado
            campos[0] = "Luminosidad";
            campos[1] = "lux";
            campos[2] = "-";
            campos[3] = "-";
            campos[4] = "-";
            campos[5] = "-";
            campos[6] = "-";
            campos[7] = "-";
            break;
        case 58://0x40
            sensor = SHT21;
            //Se actulizan datos del encabezado
            campos[0] = "Temperatura";
            campos[1] = "ºC";
            campos[2] = "Humedad";
            campos[3] = "%";
            campos[4] = "-";
            campos[5] = "-";
            campos[6] = "-";
            campos[7] = "-";
            break;
        case 72://0x48
            sensor = LM75;
            //Se actulizan datos del encabezado
            campos[0] = "Temperatura";
            campos[1] = "ºC";
            campos[2] = "-";
            campos[3] = "-";
            campos[4] = "-";
            campos[5] = "-";
            campos[6] = "-";
            campos[7] = "-";
            break;
        case 92://0x5C
            sensor = AM2320;
            //Se actulizan datos del encabezado
            campos[0] = "Temperatura";
            campos[1] = "ºC";
            campos[2] = "Humedad";
            campos[3] = "%";
            campos[4] = "-";
            campos[5] = "-";
            campos[6] = "-";
            campos[7] = "-";
            break;
        case 118://0x76
            sensor = BME280;
            //Se actulizan datos datos del encabezado
            campos[0] = "Temperatura";
            campos[1] = "ºC";
            campos[2] = "Humedad";
            campos[3] = "%";
            campos[4] = "Presion";
            campos[5] = "mBar";
            campos[6] = "Altitud";
            campos[7] = "m";
            break;
        case 119://0x77
            sensor = BME680;
            //Se actulizan datos datos del encabezado
            campos[0] = "Temperatura";
            campos[1] = "ºC";
            campos[2] = "Humedad";
            campos[3] = "%";
            campos[4] = "Presion";
            campos[5] = "mBar";
            campos[6] = "Gas VOC";
            campos[7] = "ohm";
            break;
        default:
            sensor = ninguno;
            break;
    }
}

void inicio_sensor(){
    switch(sensor) {
        case BME280:
            // Inicio BME280
            if (!bme280.begin(0x76)) {
                ez.canvas.println("No encontrado sensor");
                ez.canvas.println("  BME280 !!!");
                while (1);
            }
            break;
        case BME680:
            // Inicio BME680
            if (!bme680.begin(0x77)) {
                ez.canvas.println("No encontrado sensor");
                ez.canvas.println("  BME680 !!!");
                while (1);
            }
            // Set up oversampling and filter initialization
            bme680.setTemperatureOversampling(BME680_OS_8X);
            bme680.setHumidityOversampling(BME680_OS_2X);
            bme680.setPressureOversampling(BME680_OS_4X);
            bme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
            bme680.setGasHeater(320, 150); // 320*C for 150 ms
            break;
        case LM75:
            break;
        case SHT21:
            // Inicio SHT21
            if (!sht21.begin()) {
                ez.canvas.println("No encontrado sensor");
                ez.canvas.println("  SHT21 !!!");
                while (1);
            }
            break;
        case lux_BH1750:
            // Inicio BH1750
            if (!bh1750.begin(BH1750::ONE_TIME_HIGH_RES_MODE)) {
                ez.canvas.println("No encontrado sensor");
                ez.canvas.println("  BH1750 !!!");
                while (1);
            }
            break; 
        case VEML6075:
            // Inicio VEML6075
            if (!veml6075.begin()) {
                ez.canvas.println("No encontrado sensor");
                ez.canvas.println("  VEML6075 !!!");
                while (1);
            }
            break; 
        case TSL2541:
            // Inicio TSL2541
            if (!tsl2541.begin()) {
                ez.canvas.println("No encontrado sensor");
                ez.canvas.println("  TSL2541 !!!");
                while (1);
            }
            tsl2541.enableAutoRange(true);
            tsl2541.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);
            break; 
        case AM2320:
            // Inicio AM2320
            if (!am2320.begin()) {
                ez.canvas.println("No encontrado sensor");
                ez.canvas.println("  AM2320 !!!");
                while (1);
            }
            break;
        default:
            ez.canvas.println("No encontrado sensor");
            ez.canvas.println(" Conecte sensor al puerto I2C");
            while (1);
            break;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Recoge los datos requeridos los muestra en pantlla y los alamacena en la tarjeta SD
void  getData(){
    // Fecha y Hora -> WIFI y RTC
    //WIFI: Provide official timezone names: https://en.wikipedia.org/wiki/List_of_tz_database_time_zones
    if(red_wifi){
        WifiState_t EstadoWifi = EZWIFI_IDLE;
        while(EstadoWifi != EZWIFI_IDLE){
            ez.wifi.loop();
        }
        if (mode_energy==Ahorro)
            ez.clock.waitForSync();
        myTZ.setLocation(F("Europe/Madrid"));
    }
    
    //RTC:
    DateTime now = rtc.now();
    char buffer_Fecha[20] = "DD-MM-YYYY hh:mm:ss";
    now.toString(buffer_Fecha);
    
    //Adquisición y registro de datos del sensor
    //Enviar Fecha app Blynk
    if (estado_BLE){
        Blynk.virtualWrite(V9,buffer_Fecha); 
        terminal.flush();
        terminal.print("Fecha / Hora RTC: ");         
        terminal.print(buffer_Fecha);
        terminal.flush();
    }   
    //Adquirimos y registramos datos del sensor
    switch(sensor) {
        case BME280:  
            sensor_BME280(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha);
            }
            break;
        case BME680:  
            sensor_BME680(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha);
            }
            break;
        case LM75:  
            sensor_LM75(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha);
            }
            break;
         case SHT21:  
            sensor_SHT21(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha);
            }
            break;
        case lux_BH1750:  
            sensor_BH1750(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha);
            }
            break;
        case VEML6075:  
            sensor_VEML6075(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha);
            }
            break;
        case TSL2541:  
            sensor_TSL2541(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha);
            }
            break;
        case AM2320:  
            sensor_AM2320(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha);
            }
            break;
    }
    
    if ((estado_BLE)&&(espera_BLE)){
        delay(2500); // retardo para escribir en el terminal
    }

    //Muestra en pantalla los datos recogidos en modo normal (Ahorro Energia desactivado)
    if ((!(menu))&&(mode_energy!=Ahorro)){
        Blynk.virtualWrite(V7,0); // Ahorro Energia DesActivado
        Inicia_Pantalla();
        show_Data();
    }
    
    // Sincronizo Blynk
    if (estado_BLE)
        Blynk.syncAll();

    // Si estamos ejectando el DataLogger en mode_energy = Ahorro de energia dormimos el Datalogger
    // hasta la siguiente lectura para ahorra bateria Sueño profundo ESP32 del M5STACK
    if (mode_energy==Ahorro){// Desconectamos WIFI si estaba activo
        if (red_wifi){ 
            esp_wifi_disconnect();
            esp_wifi_stop();
            #ifdef DEBUG_DATALOGGER
                Serial.println("Desconectado WIFI");
            #endif
        }
        if (estado_BLE){// Desconectamos BLE si estaba activo
            esp_bt_controller_disable();
            #ifdef DEBUG_DATALOGGER
                Serial.println("Desconectado BLE");
            #endif
        }
        M5.Power.deepSleep(intervalo*uS_to_S_Factor);
    } 
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//             Funciones BLYNK APP
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Se ejecuta cuando la conexión a Blynk se establece
BLYNK_CONNECTED() {
  //Sincroniza todos los pines virtuales con el último dato almacenado en el servidor de Blynk
    Blynk.syncAll();
}

// Controla PULSADOR ON / OFF (BUTTON) DATALOGGER Blynk App
BLYNK_WRITE(V0)
{ 
    init_scan = (Estado_Registro)param.asInt();   
}

// Controla el muestreo del DATALOGGER Blynk App
BLYNK_WRITE(V5)
{ 
    switch (param.asInt()){
        case 1: // 30seg
            intervalo = 30000;
            campos [9] =" 30 seg";
            #ifdef DEBUG_DATALOGGER
                Serial.println("Blynk 30 seg");
            #endif
            break;
        case 2: // 60 seg
            intervalo = 60000;
            campos [9] =" 60 seg";
            #ifdef DEBUG_DATALOGGER
                Serial.println("Blynk 60 seg");
            #endif
            break;
        case 3: // 5 min
            intervalo = 300000;
            campos [9] =" 5 min";
            #ifdef DEBUG_DATALOGGER
                Serial.println("Blynk 5 min");
            #endif
            break;
        case 4: // 15 min
            intervalo = 900000;
            campos [9] =" 15 min";
            #ifdef DEBUG_DATALOGGER
                Serial.println("Blynk 15 min");
            #endif
            break;
        case 5: // 30min
            intervalo = 1800000;
            campos [9] =" 30 min";
            #ifdef DEBUG_DATALOGGER
                Serial.println("Blynk 30 min");
            #endif
            break;
        }
    //Actualizo el intervalo de escaneo si Blynk app lo cambia
    if (timer.isEnabled(numTimer)){
        timer.deleteTimer(numTimer);
        numTimer = timer.setInterval(intervalo, getData);
    }  
}

// Controla PULSADOR ON / OFF (BUTTON) LOW ENERGY DATALOGGER Blynk App
BLYNK_WRITE(V7)
{ 
    mode_energy = (Estado_Energy)param.asInt();
    // No vuelve a entrar en modo Ahorro energía
    if (mode_energy==Normal){
        // power on the Lcd
        M5.Lcd.wakeup();
        M5.Lcd.setBrightness(50);
        timer.enable(batTimer);
        Inicia_Pantalla();
    }
}

BLYNK_WRITE(V8)
{ 
    espera_BLE = param.asInt(); 
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

    #ifdef DEBUG_DATALOGGER
        Serial.begin(115200);
        Serial.println("Iniciando DATALOGGER ...");
    #endif
    
    // Inicio M5STACK
    ez.begin();
    delay(50);
    if (mode_energy!=Ahorro){ // En todos los mode_energy menos en ahorro de energia
        Inicia_Pantalla();
    }
    
    // Inicio I2C
    Wire.begin(PIN_SDA, PIN_SCL);

    // Inicio Sensor
    Scanner_I2C();
        switch(sensor) {
            case BME280:  
                campos [8]="BME280";
                break;
            case BME680:  
                campos [8]="BME680";
                break;
            case LM75:  
                campos [8]="LM75";
                break;
            case SHT21:  
                campos [8]="SHT21";
                break;
            case lux_BH1750:  
                campos [8]="BH1750";
                break;
            case VEML6075:  
                campos [8]="VEML6075";
                break;
            case TSL2541:  
                campos [8]="TSL2541";
                break;
            case AM2320:  
                campos [8]="AM2320";
                break;
        }
    #ifdef DEBUG_DATALOGGER
        Serial.println("SETUP: Sensor Detectado = " + (String)campos [8]);
    #endif
    inicio_sensor();

    // Inicio tarjeta SD
    if (!SD.begin()) {
        ez.canvas.println("Fallo SD o SD no insertada");
        while (1);
    }

    // Inicio RTC DS1307
    if (!rtc.begin()) {
        ez.canvas.println("No encontrado RTC");
        while (1);
    }

    // Inicio Bluetooth BLE (no se ejecuta en el arranque inicial estado_BLE=false, solo si hay Deep Sleep ESP32)
    if (estado_BLE){
        btStart(); //Inicio Bluetooth
        ez.settings.menuObj.addItem("Bluetooth BLE", submenu_BLE);
        Blynk.setDeviceName("DataLogger");
        Blynk.begin(auth); // Inicio BLYNK
    }
    else{
        btStop();
        delay(10);
    }
    
    // M5ez -> Configuración inicial WIFI, reloj y Blynk App (Solo se ejecuta 1 vez en primer arranque)
    if (mode_energy==Arranque){
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // Fecha y Hora de compilacion para el RTC
        ez.settings.menuObj.addItem("Bluetooth BLE", submenu_BLE);
        ez.settings.menu();
        red_wifi = ez.wifi.autoConnect; // Compruebo si me tengo que conecta al WIFI
        if (red_wifi){
            WifiState_t EstadoWifi = EZWIFI_IDLE;
            while(EstadoWifi != EZWIFI_IDLE){}  // Espero a conectarme al WIFI
            ez.clock.waitForSync();
            ez.clock.tz.setLocation(F("Europe/Madrid"));
            // Fecha y Hora del servidor WIFI para el RTC
            rtc.adjust(DateTime(ez.clock.tz.year(), ez.clock.tz.month(), 
                                ez.clock.tz.day(), ez.clock.tz.hour(), 
                                ez.clock.tz.minute(), 0)
                        );
        }
        Inicia_Pantalla();
    }

    // Espero a conectarme a WIFI -> Normalmente mode_energy = 1 (Ahorro de Energia) tras Deep Sleep M5STACK
    if (red_wifi){ 
        WifiState_t EstadoWifi = EZWIFI_IDLE;
        while(EstadoWifi != EZWIFI_IDLE){}
    }

    //------ Gestión del Ahorro de Energia mediante Deep_Sleep del ESP-32 (M5STACK)
    
    // Registro la pulsación del Boton C para salir del sueño profundo  
    M5.Power.setWakeupButton(PIN_Boton_C);
    // Verifico como se sale del sueño profundo
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason){
        case ESP_SLEEP_WAKEUP_EXT0: // RETORNO DEEP SLEEP -> Pulsado Boton C
            // power on the Lcd
            M5.Lcd.wakeup();
            M5.Lcd.setBrightness(50);
            //leer_SD();
            //Inicia_Pantalla();
            mem_mode_energy = Ahorro; // Estado energia anterior
            mode_energy = Normal; // DESACTIVADO Ahorro de energia
            init_scan = start; // para que en loop() inicialice timer
            if (red_wifi){ // Se conectara a WIFI si antes del sueño profundo esta conectado
                WifiState_t EstadoWifi = EZWIFI_IDLE;
                while(EstadoWifi != EZWIFI_IDLE){}
                ez.clock.waitForSync();
                ez.clock.tz.setLocation(F("Europe/Madrid"));
                Inicia_Pantalla();
            }
            getTimeScan();
            //ez.canvas.println("Ahorro Energia Desactivado");
            //ez.canvas.println(" ");
            //ez.canvas.println("   DataLogger iniciado");
            //ez.canvas.println("   cada" + (String)campos [9]);
            #ifdef DEBUG_DATALOGGER
                Serial.println("SETUP EXT0: intervalo = " + (String)intervalo);
                Serial.println("SETUP EXT0: scan " + (String)campos [9]);
            #endif
            break;
        case ESP_SLEEP_WAKEUP_TIMER: // RETORNO DEEP SLEEP -> Timer cada intervalo
            // ACTIVADO Ahorro de energia
            // power off the Lcd
            M5.Lcd.setBrightness(0);
            M5.Lcd.sleep();
            #ifdef DEBUG_DATALOGGER
                getTimeScan();
                Serial.println("SETUP TIMER: intervalo = " + (String)intervalo);
                Serial.println("SETUP TIMER: scan = " + (String)campos [9]);
            #endif
            break;
    }

    // Espera a estar conectado a Blynk BLE
    if ((estado_BLE)&&(espera_BLE)){
            while (Blynk.connect(5000U) == false) {} //yield();
            if (mode_energy==Ahorro)
                Blynk.virtualWrite(V7,1);
        }

    // En mode_energy "ahorro de energia" desactiva actualizar nivel bateria en pantalla
    if (mode_energy==Ahorro){ 
        timer.disable(batTimer);
    }

    //Inicio y desactivo Timer
    numTimer = timer.setInterval(intervalo, getData);
    batTimer = timer.setInterval(300000, Level_Battery); // Cada 5 min actualizo nivel de bateria
    timer.disable(numTimer);

    // Eventos asociados a M5ez: SimpleTimer run y Blynk App
    ez.addEvent(Programa_Eventos, refresco); 

}

void loop() {

    red_wifi = ez.wifi.autoConnect; // Compruebo WIFI
    switch (mode_energy) {
        case Ahorro: // Ahorro de energia con DataLogger inicializado
            timer.disable(batTimer); // Desactivo actualización porcentaje bateria
            init_scan = start; // Para cuando se desactive el modo ahorro energia activar el timer
            getData(); // recoge y graba datos en SD cada intervalo
            break;
        case Arranque: // Borrado y actualizado encabezados SD -> Solo se ejecura 1 vez
            M5.Lcd.drawBitmap(0, 0, 320, 240, (uint16_t *) logo); // Muestra en pantalla logo
            delay(3000);
            campos [9] = " 30 seg"; // Intervalo muestreo por defecto 30 seg mostrado en pantalla
            ez.msgBox("Inicio DataLogger", "¿Quieres borrar y actualizar encabezados tarjeta SD con el sensor " 
                        + (String)campos [8] + "?", "No # # Yes",false);
            mem_mode_energy = Arranque; 
            mode_energy = Normal; // 
            break;
        case Normal: // Funcionamiento normal a la espera de iteración con usuario
            if (init_scan==start){ // Inicia DataLogger
                #ifdef DEBUG_DATALOGGER
                    Serial.println("LOOP: intervalo = " + (String)intervalo);
                    Serial.println("LOOP: scan = " + (String)campos [9]);
                    Serial.println("LOOP: mem_init_scan = " + (String)mem_mode_energy);
                #endif
                timer.deleteTimer(numTimer);
                numTimer = timer.setInterval(intervalo, getData); // recoge y graba datos en SD cada intervalo
                if (mem_mode_energy == Ahorro){ // ya estaba iniciado el DataLogger
                    Inicia_Pantalla();
                    leer_SD();
                    show_Data();
                }
                else{ // iniciado DataLogger por primera vez 
                    write_Pantalla("Iniciado DataLogger " + (String)campos [8] + "       escaneando cada" + (String)campos [9]);
                }
                mode_energy = Normal;
            }
            if (init_scan==stop){ // Para DataLogger
                timer.disable(numTimer);
                write_Pantalla("Parado DataLogger " + (String)campos [8]);   
                mode_energy = Normal;
            }
            init_scan = run; // run variable control DataLogger
            scanButton(); // Gestión acciones de los botones
            break;
    }
}


