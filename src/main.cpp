/*
 ------------------------------------------------------------------------------------------------------------
 -----------                               MULTIDATA LOGGER                                ------------------
 ------------------------------------------------------------------------------------------------------------ 

 En este proyecto se realiza un DataLogger que registra los parámetros del sensor conectado al bus I2C del 
 M5STACK (ESP32). Un RTC DS3231 conectado al bus I2C del M5STACK, proporciona la fecha/hora del registro si no
 configura o dispone de red WIFI.
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
//#define BLYNK_DEBUG
//#define BLYNK_PRINT Serial

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

// Definición de Pines
#define PIN_Boton_C 37  // Pin - GPIO del Boton C en M5Stack, en el programa despertará al M5Stack
#define PIN_SDA 21      // Pin SCA I2C
#define PIN_SCL 22      // Pin SCL I2C


//  --------------  Objetos del Proyecto

// Objeto SimpleTimer para la ejecución periódica de funciones
SimpleTimer timer;
int numTimer, batTimer;

//Objeto RTC
RTC_DS3231 rtc;

// Objeto timezone
Timezone myTZ;

// Objeto para el manejo de ficheros en tarjeta SD
File myFile;

//  --------------  Objetos de sensores permitidos:
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

// ---------------------------------------------------

//  --------------  Variables de estado del Proyecto
enum Estado_Energy{     // Modos de energia del DataLogger
    Normal, Ahorro, Arranque
};
enum Estado_Registro{   // Modos de registro
    stop, start, run
};
enum Sensor_Registro{   // Sensores permitidos
    ninguno, BME280, BME680, LM75, SHT21, lux_BH1750, VEML6075, TSL2541, AM2320
};
// ---------------------------------------------------

//  --------------  Variables de contol del Proyecto
RTC_DATA_ATTR Estado_Energy mode_energy = Arranque; // Modos de energia -> 0: Normal 1: Ahorro Energia Activado 2: Arranque 1ª vez
RTC_DATA_ATTR Estado_Registro init_scan = run;      // Modos de registro de datos -> 0:startv/ 1:stop / 2:run
RTC_DATA_ATTR Sensor_Registro sensor = ninguno;     // Sensor I2C a registrar
RTC_DATA_ATTR uint32_t intervalo = 30000;           // Intervalo muestreo por defecto 30 seg.
RTC_DATA_ATTR bool red_wifi = false;                // Variable de control red wifi -> false: No conectado true: conectado
RTC_DATA_ATTR bool save_SD = true;                  // Variable de control SD save_SD -> false: No graba datos en SD true: Graba datos en SD
RTC_DATA_ATTR bool estado_BLE = false;              // Variable de control Bluetooth BLE -> false: No conectado true: conectado
RTC_DATA_ATTR bool espera_BLE = false;              // Variable de control espera conexión Bluetooth BLE -> false: No espera true: espera
bool menu = false;                                  // Variable de control para no refrescar pantalla si estoy en un menu
int battery;                                        // Nivel de bateria 100% - 75% - 50% - 25%
Estado_Energy mem_mode_energy;                      // Etado anterior Modos de energia -> 0: Normal 1: Ahorro Energia Activado 2: Arranque 1ª vez
const uint16_t refresco = 250;                      // Tiempo en mseg de refresco eventos(SimpleTimer funciones y Blynk App)
// ---------------------------------------------------

//  --------------  Variables a registrar del sensor
char registro_1 [20];
char registro_2 [20];
char registro_3 [20];
char registro_4 [20];
// ---------------------------------------------------

//  --------------  Variable control encabezados del DataLogger
////{campos[0], campos[1],campos[2], campos[3],campos[4], campos[5],
//campos[6], campos[7],campos[8], campos[9], campos[10], campos[11]}
//{encabezado reg_1, unidad reg_1, encabezado reg_2, unidad reg_2, encabezado reg_3, unidad reg_3,
// encabezado reg_4, unidad reg_4, sensor detectado, escaneo, /0}

char const * campos[10]; 
// ---------------------------------------------------

// Terminal de la app BLE BLYNK
WidgetTerminal terminal (V10);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////                  FUNIONES                 /////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Actualiza porcentaje bateria y muestra estado bateria: cargango o llena
void level_Battery(){ // Controlador de carga Bateria IP5306_I2C chip del M5STACK
    if (M5.Power.isCharging()){
        if (M5.Power.isChargeFull())
            ez.header.show("DATALOGGER  FULL"); // Datalogger cargado
        else
            ez.header.show("DATALOGGER CHARGING"); // Datalogger cargando
    }
    else{
    battery = M5.Power.getBatteryLevel();
    ez.header.show("DATALOGGER   " + (String)battery + "%");
    if (estado_BLE)
        Blynk.virtualWrite(V6,battery); 
    }
}

//Inicia Patalla: Borra, muestra botones disponibles, level bateria ...
void inicia_Pantalla(){
    ez.screen.clear();
    level_Battery();
    ez.buttons.show("SETUP # # START # SLEEP # STOP # OFF");
    ez.canvas.y(ez.canvas.height()/5);
    ez.canvas.x(15);
    ez.canvas.font(&FreeSerifBold12pt7b);
} 

// Escribe mensajes en Pantalla
void write_Pantalla(String text){
    ez.screen.clear();
    level_Battery();
    ez.buttons.show("SETUP # # START # SLEEP # STOP # OFF");
    ez.canvas.lmargin(30);
    ez.canvas.y(ez.canvas.height()/2);
    ez.canvas.font(&FreeSansBoldOblique9pt7b);
    ez.canvas.print(text);
} 

// Actualiza campos [9] = intervalo de escaneo para mostralo por pantalla
void getTimeScan(){
    uint32_t scan_intervalo = intervalo / 60000;
    if (intervalo == 30000){
        campos [9] =" 30 seg";
    }
    else{
        String scan  = " " + (String)scan_intervalo + " min"; 
        char * str = (char *)malloc(scan.length() + 1);
        scan.toCharArray(str, scan.length() + 1);
        campos [9] = str;
        #ifdef DEBUG_DATALOGGER
            Serial.print("getTimeScan()");
            Serial.println(campos [9]);
        #endif
    }
}

// Muestras los datos registrados en pantalla y actualiza porcentaje de bateria
void show_Data(){
    level_Battery();
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
    //  sensor de 2 registros: AM2320/ SHT21
    if ((sensor==BME280)||(sensor==BME680)||(sensor==AM2320)||(sensor==SHT21)){
        ez.canvas.y(3*(ez.canvas.height()/6));
        ez.canvas.x(15);
        ez.canvas.print(campos[2]);
        ez.canvas.print(": ");
        ez.canvas.print(registro_2);
        ez.canvas.print(" ");
        ez.canvas.println(campos[3]);
    }
    //  sensor de 4 registros: BME280 / BME680
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

// Recupera los últimos registros de la tarjeta SD y los almacena en las 
// variables registro_1, registro_2, registro_3 y registro_4
void leer_SD(){ 
    String cadena = "";
    int Posicion = 0; // Posición de lectura archivo datalog.csv
    myFile = SD.open("/datalog.csv", FILE_READ);//abrimos  el archivo para leer datos
    if (myFile) { 
        #ifdef DEBUG_DATALOGGER
            Serial.println("Leyendo datos de Tarjeta SD .....");
        #endif
        //Obtengo longtiud en bytes del encabezado del sensor
        int Bytes_encab = 0;
        Posicion = Bytes_encab;
        myFile.seek(Posicion); 
        while (myFile.available()) {
            char caracter = myFile.read();
            if((caracter==10)||(caracter==13)){ //ASCII de nueva de línea o Retorno de carro  
                Bytes_encab = Posicion+2;
                break;
            }
            Posicion++;
        }
        int totalBytes = myFile.size(); // Longitud Total en bytes del archivo
        #ifdef DEBUG_DATALOGGER
            Serial.println("Bytes encabezado: " + (String)Bytes_encab);
            Serial.println("TotalBytes: " + (String)totalBytes);
        #endif
        // Si hay datos los recupero (tiene que haber más datos que solo los encabezados)
        if (totalBytes>Bytes_encab){
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
        }
        myFile.close(); //cerramos el archivo
    }
    else{
        #ifdef DEBUG_DATALOGGER	
            Serial.println("Error al abrir el archivo datalog.csv");
        #endif
    }
}

// Almacena los registros del sensor en la tarjeta SD
void almacenar_SD(char Fecha_RTC[20], const char * dia){
    myFile = SD.open("/datalog.csv", FILE_APPEND);//abrimos  el archivo y añadimos datos
    if (myFile) { 
        #ifdef DEBUG_DATALOGGER
            Serial.println("Almacenando datos en Tarjeta SD .....");
        #endif
        //myFile.print((myTZ.dateTime("l, d-M-y H:i:s")));
        myFile.print((myTZ.dateTime("d-M-y H:i:s")));
        myFile.print(",");
        myFile.print(dia);       
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

// Actualización registros app BLYNK
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
    //  sensor de 2 registros: AM2320/ SHT21
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
    //  sensor de 4 registros: BME280 / BME680
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

// Salida de todos los Menus del DataLogger
void exit_menu(){
    if (timer.isEnabled(numTimer)){
        inicia_Pantalla();
        show_Data();
    }
    else
        inicia_Pantalla();
    menu = false;
}

// Menu de configuración BLE del DataLogger
void submenu_BLE(){
    ezMenu myMenu_BLE("Configuracion BLE");
    myMenu_BLE.addItem("BLE - START");
    myMenu_BLE.addItem("BLE - STOP");
    myMenu_BLE.addItem("BLE - EXIT");
    switch (myMenu_BLE.runOnce()) {
        case 1: // Se Activa BLE
            if((esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE)||
                (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_NUM)){
                btStart(); //Inicio Bluetooth
                Blynk.setDeviceName("DataLogger");
                Blynk.begin(auth); // Inicio BLYNK
                while (Blynk.connect(5000U) == false) {}
                // Si el Datalogger estaba iniciado muestra los últimos datos registrados
                if (timer.isEnabled(numTimer)){
                    Blynk.virtualWrite(V0,1); //DataLogger Activado
                    leer_SD(); // Recupera los últimos registros de la SD
                    sensor_display_Blink(); // Actualiza  V1, V2, V3 y V4 Registros 1, 2, 3 y 4
                    //Actualiza V5
                    if(intervalo == 30000)// 30 seg
                        Blynk.virtualWrite(V5,1);
                    if(intervalo == 60000)// 1 min
                        Blynk.virtualWrite(V5,2);
                    if(intervalo == 300000)// 5 min
                        Blynk.virtualWrite(V5,3);
                    if(intervalo == 90000)// 15 min
                        Blynk.virtualWrite(V5,4);
                    // Resto intervalos
                    if((intervalo != 30000)&&(intervalo != 60000)&&
                        (intervalo != 300000)&&(intervalo != 90000)){
                        String inter_min = (String)(intervalo / 60000);
                        inter_min = inter_min + " min";
                        Blynk.setProperty(V5,"labels","30 seg","1 min","5 min","15 min", inter_min);
                        Blynk.virtualWrite(V5,5);   
                    }
                    inicia_Pantalla();
                    show_Data();
                }
                // No iniciado el Datalogger se muestra pantalla inicial
                else{
                    Blynk.virtualWrite(V0,0); //DataLogger Parado
                    Blynk.virtualWrite(V1,0); //Registro_1
                    Blynk.virtualWrite(V2,0); //Registro_2
                    Blynk.virtualWrite(V3,0); //Registro_3
                    Blynk.virtualWrite(V4,0); //Registro_4
                    Blynk.setProperty(V5,"labels","30 seg","1 min","5 min","15 min", "XX min");
                    Blynk.virtualWrite(V5,1); //Escaneo inicial 30 seg
                    Blynk.virtualWrite(V7,0); //DataLogger Low Energy OFF
                    Blynk.virtualWrite(V8,0); // Espera conexión BLE
                    terminal.clear();
                }
                menu = false;
                estado_BLE = true;   
            }         
            break;
        case 2: // Se DesActiva BLE
            btStop();
            menu = false;
            estado_BLE = false;
            break;
        case 3: // EXIT
            exit_menu();
            break;
    }
}

// Menu de configuración intervalo de escaneo del DataLogger
void submenu1_SCAN(){
    ezMenu myMenu1("Configura intervalo de escaneo");
    myMenu1.addItem("30 seg");
    myMenu1.addItem("1 min");
    myMenu1.addItem("5 min");
    myMenu1.addItem("15 min");
    myMenu1.addItem("Intervalo escaneo en minutos");
    myMenu1.addItem("EXIT");
    String scaneo;

    switch (myMenu1.runOnce()) {
        case 1: // 30 seg
  	        if (timer.isEnabled(numTimer)){
                ez.msgBox("Configura intervalo de escaneo", "Antes, STOP DataLogger");
                inicia_Pantalla();
                show_Data();
            }
            else{
                intervalo = 30000;
                campos [9] =" 30 seg";
                if (estado_BLE)
                    Blynk.virtualWrite(V5,1); 
                inicia_Pantalla();
            }
            menu = false;
            break;
        case 2: // 60 seg
            if (timer.isEnabled(numTimer)){
                ez.msgBox("Configura intervalo de escaneo", "Antes, STOP DataLogger");
                inicia_Pantalla();
                show_Data();
            }
            else{
                intervalo = 60000;
                getTimeScan();
                if (estado_BLE)
                    Blynk.virtualWrite(V5,2);
                inicia_Pantalla();
            }
            menu = false;
            break;
        case 3: // 5 min
  	        if (timer.isEnabled(numTimer)){
                ez.msgBox("Configura intervalo de escaneo", "Antes, STOP DataLogger");
                inicia_Pantalla();
                show_Data();
            }
            else{
                intervalo = 300000;
                getTimeScan();
                if (estado_BLE)
                    Blynk.virtualWrite(V5,3);
                inicia_Pantalla();
            }
            menu = false;
            break;
        case 4: // 15 min
            if (timer.isEnabled(numTimer)){
                ez.msgBox("Configura intervalo de escaneo", "Antes, STOP DataLogger");
                inicia_Pantalla();
                show_Data();
            }
            else{
                intervalo = 900000;
                getTimeScan();
                if (estado_BLE)
                    Blynk.virtualWrite(V5,4);
                inicia_Pantalla();
            }
            menu = false;
            break;
        case 5: // Intervalo escaneo en seg
  	        if (timer.isEnabled(numTimer)){
                ez.msgBox("Configura intervalo de escaneo", "Antes, STOP DataLogger");
                inicia_Pantalla();
                show_Data();
            }
            else{
                scaneo = (ez.textInput("Intervalo escaneo minutos"));
                intervalo = scaneo.toInt();
                if (intervalo == 0){
                    intervalo = 30000; // 30 seg por defecto
                    ez.msgBox("Intervalo escaneo", "Error al introducir el intervalo de escaneo. Intentalo de nuevo.");
                }
                else{
                    scaneo = " " + scaneo + " min";
                    intervalo = intervalo * 60000;
                    getTimeScan();
                    if (estado_BLE){
                        Blynk.setProperty(V5,"labels","30 seg","1 min","5 min","15 min", scaneo);
                        Blynk.virtualWrite(V5,5);
                    }
                    ez.msgBox("Intervalo escaneo", "Introducido " + scaneo + " como intervalo de escaneo");
                }
                inicia_Pantalla();
            }
            menu = false;
            break;
        case 6: // EXIT
            exit_menu();
            break;
    }
}

// Borrado archivo y actualización encabezados archivo datalog.csv de la SD
void borrado_encabezado_SD(){
    if(!(SD.remove("/datalog.csv"))){ // Borrado archivo datalog.csv de la SD
        ez.msgBox("Borrar SD - SI", "Error al borrar el archivo en tarjeta SD");
        inicia_Pantalla();
        #ifdef DEBUG_DATALOGGER
            Serial.println("Error al borrar el archivo en tarjeta SD");
        #endif
    }           
    myFile = SD.open("/datalog.csv", FILE_WRITE); // Creado archivo datalog.csv en la SD
    if (myFile) {
        switch(sensor) { // Creación de encabezados archivo datalog.csv
            case BME280:  // Se crea encabezado BME280 en datalog.csv
                myFile.println("Fecha Hora WIFI,Dia,Fecha Hora RTC,Temperatura (ºC),Humedad (%),Presion (mbar),Altitud (m), Sensor BME280");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                inicia_Pantalla();
                break;
            case BME680:  // Se crea encabezado BME680 en datalog.csv
                myFile.println("Fecha Hora WIFI,Dia,Fecha Hora RTC,Temperatura (ºC),Humedad (%),Presion (mbar),Gas VOC (ohmios), Sensor BME680");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                inicia_Pantalla();
                break;
            case LM75:  // Se crea encabezado LM75 en datalog.csv
                myFile.println("Fecha Hora WIFI,Dia,Fecha Hora RTC,Temperatura (ºC), Sensor LM75");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                inicia_Pantalla();
                break;
            case lux_BH1750:  // Se crea encabezado lux_BH1750 en datalog.csv
                myFile.println("Fecha Hora WIFI,Dia,Fecha Hora RTC,Nivel Luminosidad (lux), Sensor BH1750");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                inicia_Pantalla();
                break;
            case VEML6075:  // Se crea encabezado VEML6075 en datalog.csv
                myFile.println("Fecha Hora WIFI,Dia,Fecha Hora RTC,indice UV,, Sensor VEML6075");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                inicia_Pantalla();
                break;
            case TSL2541:  // Se crea encabezado TSL2541 en datalog.csv
                myFile.println("Fecha Hora WIFI,Dia,Fecha Hora RTC,Nivel Luminosidad (lux), Sensor TSL2541");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                inicia_Pantalla();
                break;
            case SHT21:     // Se crea encabezado SHT21 en datalog.csv
                myFile.println("Fecha Hora WIFI,Dia,Fecha Hora RTC,Temperatura (ºC),Humedad (%), Sensor SHT21");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                inicia_Pantalla();
                break;
            case AM2320:    // Se crea encabezado AM2320 en datalog.csv
                myFile.println("Fecha Hora WIFI,Dia,Fecha Hora RTC,Temperatura (ºC),Humedad (%), Sensor AM2320");
                myFile.close();
                ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                inicia_Pantalla();
                break;
            case ninguno:
                break;
        }
    }
    else{
        ez.msgBox("Borrar SD - SI", "Error al crear archivo datalog.csv en tarjeta SD");
        inicia_Pantalla();
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
                inicia_Pantalla();
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
                inicia_Pantalla();
                show_Data();
            }
            else{
                ezMenu myMenu22("Grabar Datos SD");
                myMenu22.addItem("Grabar Datos - SI");
                myMenu22.addItem("Grabar Datos - NO");
                switch (myMenu22.runOnce()) {
                    case 1: // Grabar Datos - SI
                        save_SD = true;
                        inicia_Pantalla();
                        break;  
                    case 2: // Grabar Datos - NO
                        save_SD = false;
                        inicia_Pantalla();
                        break; 
                }
            }
            menu = false;
            break;
        case 3: // EXIT
            exit_menu();
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
        inicia_Pantalla();
        show_Data();
    }
    else
        inicia_Pantalla();
}

// Menu configuración RTC
void submenu4_RTC(){
    String Fecha, Hora, m;
    int mes;
    ezMenu myMenu4("Configuracion RTC");
    myMenu4.addItem("Fecha Hora Manual");
    myMenu4.addItem("Fecha Hora WIFI");
    myMenu4.addItem("EXIT");

    switch (myMenu4.runOnce()) {
        case 1: // Fijar a fecha y hora específica.   
            // Fecha = 26/12/2009 -> 26122009
            Fecha = (ez.textInput("Fecha -> ddmmaaaa"));
            m = Fecha.substring(2, 4);
            mes = m.toInt();
            switch (mes) { // Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
                case 1:
                    Fecha = "Jar " + Fecha.substring(0, 2) + " " + Fecha.substring(4);
                    break;
                case 2:
                    Fecha = "Feb " + Fecha.substring(0, 2) + " " + Fecha.substring(4);
                    break;
                case 3:
                    Fecha = "Mar " + Fecha.substring(0, 2) + " " + Fecha.substring(4);
                    break;
                case 4:
                    Fecha = "Mar " + Fecha.substring(0, 2) + " " + Fecha.substring(4);
                    break;
                case 5:
                    Fecha = "Apr " + Fecha.substring(0, 2) + " " + Fecha.substring(4);
                    break;
                case 6:
                    Fecha = "Jun " + Fecha.substring(0, 2) + " " + Fecha.substring(4);
                    break;
                case 7:
                    Fecha = "Jul " + Fecha.substring(0, 2) + " " + Fecha.substring(4);
                    break;
                case 8:
                    Fecha = "Aug " + Fecha.substring(0, 2) + " " + Fecha.substring(4);
                    break;
                case 9:
                    Fecha = "Sep " + Fecha.substring(0, 2) + " " + Fecha.substring(4);
                    break;
                case 10:
                    Fecha = "Oct " + Fecha.substring(0, 2) + " " + Fecha.substring(4);
                    break;
                case 11:
                    Fecha = "Nov " + Fecha.substring(0, 2) + " " + Fecha.substring(4);
                    break;
                case 12:
                    Fecha = "Dec " + Fecha.substring(0, 2) + " " + Fecha.substring(4);
                    break;
            }
            // ej -> Hora = 12:34 -> 1234
            Hora = (ez.textInput("Hora -> hhmm"));
            Hora = Hora.substring(0, 2) + ":" + Hora.substring(2) + ":" + "00";
            #ifdef DEBUG_DATALOGGER
                Serial.print("Fecha: ");
                Serial.print(Fecha);
                Serial.print("  Hora: ");
                Serial.println(Hora);
            #endif
            // Formato Fecha "Dec 26 2009", Formato Hora "12:34:56" 
            rtc.adjust(DateTime(Fecha.c_str(), Hora.c_str()));
            ez.msgBox("Fecha / Hora", "Fecha / Hora actualizadas satisfactoriamente");
            // Si el Datalogger estaba iniciado muestra los últimos datos registrados
            if (timer.isEnabled(numTimer)){
                inicia_Pantalla();
                leer_SD();
                show_Data();
            }
            // No iniciado el Datalogger se muestra pantalla inicial
            else{
                inicia_Pantalla();
            }
            menu = false;
            break;
        case 2: // Fijar a fecha y hora específica.   
            if(red_wifi){
                int F_year = myTZ.year();
                int F_day = myTZ.day();
                int F_month = myTZ.month();
                int F_hour = myTZ.hour();
                int F_minute = myTZ.minute();
                rtc.adjust(DateTime(F_year, F_month, F_day, F_hour, F_minute, 0)); 
            }
            // Si el Datalogger estaba iniciado muestra los últimos datos registrados
            if (timer.isEnabled(numTimer)){
                inicia_Pantalla();
                leer_SD();
                show_Data();
            }
            // No iniciado el Datalogger se muestra pantalla inicial
            else{
                inicia_Pantalla();
            }
            menu = false;
            break;
        case 3: // EXIT
            exit_menu();
            break;
    }
    
}

// Menu Principal de Configuración del DataLogger
void config_DataLogger(){
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
        case 4: // Configuracion RTC  
            submenu4_RTC();
            break;
        case 5: // EXIT
            exit_menu();
            break;
    }
}

// Gestión acciones de los botones
void scanButton() {
    String btn = ez.buttons.poll();
    if (btn == "SETUP"){
        config_DataLogger();
    }
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
uint16_t programa_Eventos(){
    timer.run(); // Inicio SimpleTimer, ejecución de funciones
    if (estado_BLE)
        Blynk.run(); // Inicio Blynk, actualización de la app
    return refresco; // retorna el intervalo para su nueva ejecución
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////                FUNCIONES SENSORES                 /////////////////////////////////////////                                 
///////////////////////////////////////////////////////////////////////////////////////////////////////////

// Configuración inicial del sensor detectado
void inicio_sensor(){
    switch(sensor) {
        case BME280:
            // Inicio BME280
            if (!bme280.begin(0x76)) {
                ez.canvas.println("No encontrado sensor");
                ez.canvas.println("  BME280 !!!");
                while (1);
            }
            #define SEALEVELPRESSURE_mbar (1010) // Estimar la altitud para una presión dada comparándola con la presión
                                    // a nivel del mar. Este ejemplo utiliza el valor predeterminado, pero 
                                    // para obtener resultados más precisos, reemplace el valor con la 
                                    // presión actual del nivel del mar en su ubicación. Lo usa BME280
            break;
        case BME680:
            // Inicio BME680
            if (!bme680.begin(0x77)) {
                ez.canvas.println("No encontrado sensor");
                ez.canvas.println("  BME680 !!!");
                while (1);
            }
            // Configuración inicial sobremuestreo filtro inicial
            bme680.setTemperatureOversampling(BME680_OS_8X);
            bme680.setHumidityOversampling(BME680_OS_2X);
            bme680.setPressureOversampling(BME680_OS_4X);
            bme680.setIIRFilterSize(BME680_FILTER_SIZE_3);
            bme680.setGasHeater(320, 150); // 320*C for 150 ms
            break;
        case LM75:
            // Inicio LM75: No necesita configuración inicial
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
        case ninguno: // default
            ez.canvas.println("No encontrado sensor");
            ez.canvas.println(" Conecte sensor al puerto I2C");
            while (1);
            break;
    }
}

// Busca las direcciones I2C del sensor conectado al DataLogger
void scanner_I2C(){
    #ifdef DEBUG_DATALOGGER
        Serial.println ();
        Serial.println ("I2C scanner. Scanning ...");
    #endif
    byte direccion_I2C = 0;
    for (byte direccion = 8; direccion < 120; direccion++){
        Wire.beginTransmission (direccion); // Comienza I2C transmision Address (direccion)
        if (Wire.endTransmission () == 0) { // Recibido 0 = success (ACK response) 
            if (((direccion)!=104)&&((direccion)!=117)){ // 0x68 RTC y 0x75 IP5306 chip display del M5STACK
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

#ifdef DEBUG_DATALOGGER     // Solo se ejecuta en modo DEBUG
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
        Serial.println((String)campos [0] + " = " + (String)registro_1);
        Serial.println((String)campos [2] + " = " + (String)registro_2);
        Serial.println((String)campos [4] + " = " + (String)registro_3);
        Serial.println((String)campos [6] + " = " + (String)registro_4);
    }
#endif

// BME280: Registra -> Temperatura, Huemdad, Presión y altitud
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

// BME280: Registra -> Temperatura, Huemdad, Presión y calidad del aire
void sensor_BME680(char Fecha_RTC[20]){
    //Leer temperatura.
    dtostrf(bme680.readTemperature(),2,1,registro_1);
    //Leer humedad.
    dtostrf(bme680.readHumidity(),2,1,registro_2);
    //Leer presion.
    dtostrf(bme680.readPressure()/ 100.0F,2,1,registro_3);
    //Leer gas VOC.
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

// LM75: Registra -> Nivel de Luminosidad
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

//SHT21: Registra -> Temperatura, Humedad
void sensor_SHT21(char Fecha_RTC[20]){
    //Leer temperatura.
    dtostrf(sht21.readTemperature(),2,1,registro_1);
    //Leer humedad.
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

// BH1750: Registra -> Nivel de Luminosidad
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

// VEML6075: Registra -> Indice ultravioleta
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

// TSL2541: Registra -> Nivel de Luminosidad
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

// AM2320: Registra -> Temperatura, Humedad
void sensor_AM2320(char Fecha_RTC[20]){
    //Leer temperatura.
    dtostrf(am2320.readTemperature(),2,1,registro_1);
    //Leer humedad.
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Gestiona la adquisición de los registros del sensor, su almacenamiento en tarjeta SD, su visialización 
// en pantalla, ahorro de energía DeepSleep y conexiones BLE y WIFI
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
    
    //Reloj RTC
    DateTime now = rtc.now();
    char buffer_Fecha[20] = "DD-MM-YYYY hh:mm:ss"; // Fecha y hora Actual
    now.toString(buffer_Fecha);
    uint8_t dia = now.dayOfTheWeek();
    const char * n_dia; // dia de la semana
    switch (dia){
        case 0: 
            n_dia = "DOMINGO";
            break;
        case 1: 
            n_dia = "LUNES";
            break;
        case 2: 
            n_dia = "MARTES";
            break;
        case 3: 
            n_dia = "MIERCOLES";
            break;
        case 4: 
            n_dia = "JUEVES";
            break;
        case 5: 
            n_dia = "VIERNES";
            break;
        case 6: 
            n_dia = "SABADO";
            break;
    }
    
    //Adquisición y registro de datos del sensor
    //Enviar Fecha app Blynk
    if (estado_BLE){
        Blynk.virtualWrite(V0,1);
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
                almacenar_SD(buffer_Fecha,n_dia);
            }
            break;
        case BME680:  
            sensor_BME680(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha,n_dia);
            }
            break;
        case LM75:  
            sensor_LM75(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha,n_dia);
            }
            break;
         case SHT21:  
            sensor_SHT21(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha,n_dia);
            }
            break;
        case lux_BH1750:  
            sensor_BH1750(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha,n_dia);
            }
            break;
        case VEML6075:  
            sensor_VEML6075(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha,n_dia);
            }
            break;
        case TSL2541:  
            sensor_TSL2541(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha,n_dia);
            }
            break;
        case AM2320:  
            sensor_AM2320(buffer_Fecha);
            if(save_SD){ // Control de grabación en SD
                almacenar_SD(buffer_Fecha,n_dia);
            }
            break;
        case ninguno:
            break;
    }
    
    if ((estado_BLE)&&(espera_BLE)){
        delay(2500); // retardo para escribir en el terminal
    }

    //Muestra en pantalla los datos recogidos en modo normal (Ahorro Energia desactivado)
    if ((!(menu))&&(mode_energy!=Ahorro)){
        Blynk.virtualWrite(V7,0); // Ahorro Energia DesActivado
        inicia_Pantalla();
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
            getTimeScan();
            Blynk.setProperty(V5,"labels","30 seg","1 min","5 min","15 min", "XX min");
            #ifdef DEBUG_DATALOGGER
                Serial.println("Blynk 30 seg");
            #endif
            break;
        case 2: // 60 seg
            intervalo = 60000;
            getTimeScan();
            Blynk.setProperty(V5,"labels","30 seg","1 min","5 min","15 min", "XX min");
            #ifdef DEBUG_DATALOGGER
                Serial.println("Blynk 60 seg");
            #endif
            break;
        case 3: // 5 min
            intervalo = 300000;
            getTimeScan();
            Blynk.setProperty(V5,"labels","30 seg","1 min","5 min","15 min", "XX min");
            #ifdef DEBUG_DATALOGGER
                Serial.println("Blynk 5 min");
            #endif
            break;
        case 4: // 15 min
            intervalo = 900000;
            getTimeScan();
            Blynk.setProperty(V5,"labels","30 seg","1 min","5 min","15 min", "XX min");
            #ifdef DEBUG_DATALOGGER
                Serial.println("Blynk 15 min");
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
        inicia_Pantalla();
    }
}

// Espera la conexión BLE incluso en ahorro de energía
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
        inicia_Pantalla();
    }
    
    // Inicio I2C
    Wire.begin(PIN_SDA, PIN_SCL);

    // Inicio Sensor
    scanner_I2C();
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
            case ninguno:
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

    // Inicio RTC DS3231
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
        inicia_Pantalla();
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
            mem_mode_energy = Ahorro; // Estado energia anterior
            mode_energy = Normal; // DESACTIVADO Ahorro de energia
            init_scan = start; // para que en loop() inicialice timer
            if (red_wifi){ // Se conectara a WIFI si antes del sueño profundo esta conectado
                WifiState_t EstadoWifi = EZWIFI_IDLE;
                while(EstadoWifi != EZWIFI_IDLE){}
                ez.clock.waitForSync();
                ez.clock.tz.setLocation(F("Europe/Madrid"));
                inicia_Pantalla();
            }
            getTimeScan();
            level_Battery(); //Actualizo nivel Batería
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
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            break;
    }

    // Espera a estar conectado a Blynk BLE
    if ((estado_BLE)&&(espera_BLE)){
            while (Blynk.connect(5000U) == false) {} 
            if (mode_energy==Ahorro)
                Blynk.virtualWrite(V7,1);
        }

    // En mode_energy "ahorro de energia" desactiva actualizar nivel bateria en pantalla
    if (mode_energy==Ahorro){ 
        timer.disable(batTimer);
    }

    //Inicio y desactivo Timer
    numTimer = timer.setInterval(intervalo, getData);
    batTimer = timer.setInterval(300000, level_Battery); // Cada 5 min actualizo nivel de bateria
    timer.disable(numTimer);

    // Eventos asociados a M5ez: SimpleTimer run y Blynk App
    ez.addEvent(programa_Eventos, refresco); 

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
            if (estado_BLE){
                Blynk.virtualWrite(V0,0);   //DataLogger Parado
                Blynk.virtualWrite(V1,0);   //Registro_1
                Blynk.virtualWrite(V2,0);   //Registro_2
                Blynk.virtualWrite(V3,0);   //Registro_3
                Blynk.virtualWrite(V4,0);   //Registro_4
                Blynk.setProperty(V5,"labels","30 seg","1 min","5 min","15 min", "XX min");
                Blynk.virtualWrite(V5,1);   //Escaneo inicial 30 seg
                Blynk.virtualWrite(V7,0);   //DataLogger Low Energy OFF
                Blynk.virtualWrite(V8,0);   //Espera conexión BLE
                terminal.clear();           //Terminal registros
            }
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
                    Serial.print("LOOP: scan = ");  Serial.println(campos [9]);
                    Serial.println("LOOP: mem_init_scan = " + (String)mem_mode_energy);
                #endif
                level_Battery(); //Actualizo nivel Batería
                timer.deleteTimer(numTimer);
                numTimer = timer.setInterval(intervalo, getData); // recoge y graba datos en SD cada intervalo
                if ((mem_mode_energy == Ahorro) && (save_SD)){ // ya estaba iniciado el DataLogger y grabando en SD
                    inicia_Pantalla();  // Borra pantalla y muestra los botones
                    leer_SD();          // Recupera los últimos registros de la SD
                    show_Data();        // Muestra los últimos registros en pantalla
                }
                else{ // iniciado DataLogger por primera vez 
                    write_Pantalla("Iniciado DataLogger " + (String)campos [8] + "       escaneando cada" + String(campos [9]));
                }
                mode_energy = Normal;
            }
            if (init_scan==stop){ // Para DataLogger
                timer.disable(numTimer);
                write_Pantalla("Parado DataLogger " + (String)campos [8]);   
                level_Battery(); //Actualizo nivel Batería
                mode_energy = Normal;
                mem_mode_energy = Normal;
            }
            init_scan = run; // run variable control DataLogger
            scanButton(); // Gestión acciones de los botones
            break;
    }

}


