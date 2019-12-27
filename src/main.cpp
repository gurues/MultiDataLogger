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
#include <Adafruit_Sensor.h>
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
RTC_DS1307 rtc;

// Objeto timezone
Timezone myTZ;

// Objetos de sensores permitidos:
// Objeto BME280
Adafruit_BME280 bme280; // I2C 0x76
// Objeto BME680
Adafruit_BME680 bme680; // I2C 0x77

// Objeto para el manejo de ficheros en tarjeta SD
File myFile;

// Variables de contol del Proyecto
enum estado_energy{
    Normal, Ahorro, Arranque
};
enum estado_registro{
    stop, start, reinicio
};
enum Sensor_Registro{
    ninguno, BME280, BME680
};
RTC_DATA_ATTR int mode_energy = Arranque; // Modos de energia -> 0: Normal 1: Ahorro Energia Activado 2: Arranque 1ª vez Normal
RTC_DATA_ATTR uint32_t intervalo = 30000; // intervalo muestreo por defecto 30 seg.
RTC_DATA_ATTR bool red_wifi = false; // Estado red wifi -> false: No conectado true: conectado.
RTC_DATA_ATTR bool save_SD = true; // save_SD -> false: No graba datos en SD true: Graba datos en SD.
RTC_DATA_ATTR int init_scan = reinicio; //Variable de control start/stop/reinicio registro de datos
RTC_DATA_ATTR bool estado_BLE = false; //Variable de control Bluetooth BLE -> false: No conectado true: conectado.
RTC_DATA_ATTR bool espera_BLE = false; //Variable de control espera conexión Bluetooth BLE -> false: No espera true: espera.
RTC_DATA_ATTR Sensor_Registro sensor = ninguno; // Sensor I2C a registrar
String scan =" 30 seg";  // intervalo muestreo por defecto 30 seg mmostrado en pantalla
bool menu = false; // Variable de control para no refrescar pantalla si estoy en un menu
int battery; // Nivel de bateria 100% - 75% - 50% - 25%

//variables a registrar del sensor
char registro_1 [20];
char registro_2 [20];
char registro_3 [20];
char registro_4 [20];

// Terminal de la app BLE BLYNK
WidgetTerminal terminal (V10);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////                  FUNIONES                 /////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Actualiza porcentaje bateria
void Level_Battery(){
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
    //Draw the jpeg file form TF card
    //Draw the jpeg file "p2.jpg" from TF(SD) card
    //M5.Lcd.drawJpgFile(SD, "/p2.jpg");
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
        scan =" 30 seg";
    }
    if (intervalo == 60000){
        scan =" 60 seg";
    }
    if (intervalo == 300000){
        scan =" 5 min";
    }
    if (intervalo == 900000){
        scan =" 15 min";
    }      
    if (intervalo == 1800000){
        scan =" 30 min";
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
    ez.canvas.print(scan);

    ez.canvas.y(2*(ez.canvas.height()/6));
    ez.canvas.x(15);
    ez.canvas.print("Altitud: ");
    ez.canvas.print(registro_4);
    ez.canvas.println(" m ");

    ez.canvas.y(3*(ez.canvas.height()/6));
    ez.canvas.x(15);
    ez.canvas.print("Presion: ");
    ez.canvas.print(registro_3);
    ez.canvas.println(" mbar ");

    ez.canvas.y(4*(ez.canvas.height()/6));
    ez.canvas.x(15);
    ez.canvas.print("Temperatura: ");
    ez.canvas.print(registro_1);
    ez.canvas.println(" *C ");
    
    ez.canvas.y(5*(ez.canvas.height()/6));
    ez.canvas.x(15);
    ez.canvas.print("Humedad: ");
    ez.canvas.print(registro_2);
    ez.canvas.println(" % ");
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
                scan =" 30 seg";
                if (estado_BLE)
                    Blynk.virtualWrite(V5,1); 
                Inicia_Pantalla();
            }
            menu = false;
            break;
        case 2: // 60 seg
            if (timer.isEnabled(numTimer)){
                ez.msgBox("Configura intervalo de escaneo", "Antes, STOP DataLogger");
                Inicia_Pantalla();
                show_Data();
            }
            else{
                intervalo = 60000;
                scan =" 60 seg";
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
                scan =" 5 min";
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
                scan =" 15 min";
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
                scan =" 30 min";
                if (estado_BLE)
                    Blynk.virtualWrite(V5,5);
                Inicia_Pantalla();
            }
            menu = false;
            break;
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
                if(!(SD.remove("/datalog.csv"))){ // Borrado archivo datalog.csv de la SD
                    ez.msgBox("Borrar SD - SI", "Error al borrar el archivo en tarjeta SD");
                    Inicia_Pantalla();
                    #ifdef DEBUG_DATALOGGER
                        Serial.println("Error al borrar el archivo en tarjeta SD");
                    #endif
                }           
                myFile = SD.open("/datalog.csv", FILE_WRITE); // Creado archivo datalog.csv en la SD
                if (myFile) {
                    myFile.println("Dia WIFI,Fecha Hora WIFI,Fecha Hora RTC,Temperatura (ºC),Humedad (%),Presion (mbar),Altitud (m)");
                    myFile.close();
                    ez.msgBox("Borrar SD - SI", "Datos Borrados de tarjeta SD");
                    Inicia_Pantalla();
                }
                else{
                    ez.msgBox("Borrar SD - SI", "Error al crear archivo datalog.csv en tarjeta SD");
                    Inicia_Pantalla();
                    #ifdef DEBUG_DATALOGGER
                        Serial.println("Error al crear archivo datalog.csv en tarjeta SD");
                    #endif
                }
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
    int F_year, F_day, F_month, F_hour, F_minute=0;

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
        write_Pantalla("Iniciado DataLogger escaneando cada" + scan);
        init_scan = start;
        mode_energy = Normal;
        if (estado_BLE){
            Blynk.virtualWrite(V0,init_scan);
            Blynk.virtualWrite(V9,"");
        }
    } 

    if ((btn == "STOP")&&(timer.isEnabled(numTimer))){
        write_Pantalla("Parado DataLogger");   
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
           // esp_wifi_deinit();
        }
        if (estado_BLE){// Desconectamos BLE si estaba activo
            esp_bt_controller_disable();
           // esp_bt_controller_deinit();
           // esp_bt_mem_release(ESP_BT_MODE_BTDM);
        }
        M5.Power.deepSleep(intervalo*uS_to_S_Factor);
    }

}

//Ejecución de eventos asociados
uint16_t Programa_Eventos(){
    timer.run(); // Inicio SimpleTimer para la ejecución de funciones
    if (estado_BLE)
        Blynk.run(); // Inicio Blynk
    return refresco; // retorna el intervalo para su nueva ejecución
}

void sensor_display_Blink(const char *registros){

    Blynk.virtualWrite(V1,registro_1); 
    terminal.print("    ");
    terminal.print(registros[0]);
    terminal.print(" = ");
    terminal.print(registro_1);
    terminal.print(" ");
    terminal.println(registros[1]);
    terminal.flush();
    Blynk.virtualWrite(V2,registro_2); 
    terminal.print("    Humedad = ");
    terminal.print(registro_2);
    terminal.println(" %");
    terminal.flush();
    Blynk.virtualWrite(V3,registro_3); 
    terminal.print("    Presion = ");
    terminal.print(registro_3);
    terminal.println(" mbar");
    terminal.flush();
    Blynk.virtualWrite(V4,registro_4);
    terminal.print("    Altitud = ");
    terminal.print(registro_4);
    terminal.println(" m");
    terminal.flush();
}
void sensor_BME280(char Fecha_RTC[20]){
    const char *campos[] = { "Temperatura", "ºC", "Humedad", "%", "Presion", "mBar", "Altitud", "m"};
    //Leer temperatura.
    dtostrf(bme280.readTemperature(),2,1,registro_1);
    if (estado_BLE){
        Blynk.virtualWrite(V1,registro_1); 
        terminal.print("    Temperatura = ");
        terminal.print(registro_1);
        terminal.println(" ºC");
        terminal.flush();
    }
    //Leer humedad.
    dtostrf(bme280.readHumidity(),2,1,registro_2);
    if (estado_BLE){
        Blynk.virtualWrite(V2,registro_2); 
        terminal.print("    Humedad = ");
        terminal.print(registro_2);
        terminal.println(" %");
        terminal.flush();
    }
    //Leer presion.
    dtostrf(bme280.readPressure()/ 100.0F,2,1,registro_3);
    if (estado_BLE){
        Blynk.virtualWrite(V3,registro_3); 
        terminal.print("    Presion = ");
        terminal.print(registro_3);
        terminal.println(" mbar");
        terminal.flush();
    }
    //Leer altitud.
    dtostrf(bme280.readAltitude(SEALEVELPRESSURE_mbar),2,1,registro_4);
    if (estado_BLE){
        Blynk.virtualWrite(V4,registro_4);
        terminal.print("    Altitud = ");
        terminal.print(registro_4);
        terminal.println(" m");
        terminal.flush();
    }
 
    // DEBUG DataLogger
    #ifdef DEBUG_DATALOGGER
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
        Serial.println("scan = " + (String)scan);
        Serial.println("Temperatura = " + (String)registro_1);
        Serial.println("Humedad = " + (String)registro_2);
        Serial.println("Presión = " + (String)registro_3);
        Serial.println("Altitud = " + (String)registro_4);
    #endif
    
    if(save_SD){ // Control de grabación en SD
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
            myFile.print(",");
            myFile.print(registro_2);  
            myFile.print(",");
            myFile.print(registro_3);  
            myFile.print(",");
            myFile.println(registro_4);  // Salto de linea 
            myFile.close();         //cerramos el archivo                    
        } 
        else {
            #ifdef DEBUG_DATALOGGER
                Serial.println("Error al abrir el archivo en tarjeta SD");
            #endif
        }
    }
}

void sensor_BME680(char Fecha_RTC[20]){
    const char *campos[] = { "Temperatura", "ºC", "Humedad", "%", "Presion", "mBar", "Gas VOC", "ohm"};
    //Leer temperatura.
    dtostrf(bme680.readTemperature(),2,1,registro_1);
    if (estado_BLE){
        Blynk.virtualWrite(V1,registro_1); 
        terminal.print("    Temperatura = ");
        terminal.print(registro_1);
        terminal.println(" ºC");
        terminal.flush();
    }
    //Leer humedad.
    dtostrf(bme680.readHumidity(),2,1,registro_2);
    if (estado_BLE){
        Blynk.virtualWrite(V2,registro_2); 
        terminal.print("    Humedad = ");
        terminal.print(registro_2);
        terminal.println(" %");
        terminal.flush();
    }
    //Leer presion.
    dtostrf(bme680.readPressure()/ 100.0F,2,1,registro_3);
    if (estado_BLE){
        Blynk.virtualWrite(V3,registro_3); 
        terminal.print("    Presion = ");
        terminal.print(registro_3);
        terminal.println(" mbar");
        terminal.flush();
    }
    //Leer gas VOC.uint32_t readGas(void);
    dtostrf(bme680.readGas(),2,1,registro_4);
    if (estado_BLE){
        Blynk.virtualWrite(V4,registro_4);
        terminal.print("    Gas VOC = ");
        terminal.print(registro_4);
        terminal.println(" ohm");
        terminal.flush();
    }
 
    // DEBUG DataLogger
    #ifdef DEBUG_DATALOGGER
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
        Serial.println("scan = " + (String)scan);
        Serial.println("Temperatura = " + (String)registro_1);
        Serial.println("Humedad = " + (String)registro_2);
        Serial.println("Presión = " + (String)registro_3);
        Serial.println("Altitud = " + (String)registro_4);
    #endif
    
    if(save_SD){ // Control de grabación en SD
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
            myFile.print(",");
            myFile.print(registro_2);  
            myFile.print(",");
            myFile.print(registro_3);  
            myFile.print(",");
            myFile.println(registro_4);  // Salto de linea 
            myFile.close();         //cerramos el archivo                    
        } 
        else {
            #ifdef DEBUG_DATALOGGER
                Serial.println("Error al abrir el archivo en tarjeta SD");
            #endif
        }
    }
}

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
        terminal.print("Fecha / Hora RTC: ");         
        terminal.println(buffer_Fecha);
        terminal.flush();
    }   
    
    //Adquirimos y registramos datos del sensor
    switch(sensor) {
        case BME280:  
            sensor_BME280(buffer_Fecha);
            break;
        case BME680:  
            sensor_BME680(buffer_Fecha);
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
            //esp_wifi_deinit();
            #ifdef DEBUG_DATALOGGER
                Serial.println("Desconectado WIFI");
            #endif
        }
        if (estado_BLE){// Desconectamos BLE si estaba activo
            esp_bt_controller_disable();
            //esp_bt_controller_deinit(); // Se coogaba ESP32
            //esp_bt_mem_release(ESP_BT_MODE_BTDM); // Se coogaba ESP32
            #ifdef DEBUG_DATALOGGER
                Serial.println("Desconectado BLE");
            #endif
        }
        M5.Power.deepSleep(intervalo*uS_to_S_Factor);
    } 
}

void Scanner_I2C(){
    #ifdef DEBUG_DATALOGGER
        Serial.println ();
        Serial.println ("I2C scanner. Scanning ...");
    #endif
    byte count = 0;
    byte direccion_I2C = 0;

    for (direccion_I2C = 8; direccion_I2C < 120; direccion_I2C++){
        Wire.beginTransmission (direccion_I2C); // Begin I2C transmission Address (i)
        if (Wire.endTransmission () == 0)  // Receive 0 = success (ACK response) 
        {
            if (((direccion_I2C)!=104)&&((direccion_I2C)!=117)){ // 0x68 y 0x75 IC2 del M5STACK
                #ifdef DEBUG_DATALOGGER
                    Serial.print ("Found address: ");
                    Serial.print (direccion_I2C, DEC);
                    Serial.print (" (0x");
                    Serial.print (direccion_I2C, HEX);     
                    Serial.println (")");
                #endif
                break;
            }
        count++;
        }
    }
    switch(direccion_I2C){
        case 118://0x76
            sensor = BME280;
            break;
        case 119://0x77
            sensor = BME680;
            break;
        default:
            sensor = ninguno;
            break;
    }

    #ifdef DEBUG_DATALOGGER
        Serial.print ("Found ");      
        Serial.print (count, DEC);        // numbers of devices
        Serial.println (" device(s).");
    #endif
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
        default:
            break;
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
    init_scan = param.asInt();   
}

// Controla el muestreo del DATALOGGER Blynk App
BLYNK_WRITE(V5)
{ 
    switch (param.asInt()){
        case 1: // 30seg
            intervalo = 30000;
            scan =" 30 seg";
            #ifdef DEBUG_DATALOGGER
                Serial.println("Blynk 30 seg");
            #endif
            break;
        case 2: // 60 seg
            intervalo = 60000;
            scan =" 60 seg";
            #ifdef DEBUG_DATALOGGER
                Serial.println("Blynk 60 seg");
            #endif
            break;
        case 3: // 5 min
            intervalo = 300000;
            scan =" 5 min";
            #ifdef DEBUG_DATALOGGER
                Serial.println("Blynk 5 min");
            #endif
            break;
        case 4: // 15 min
            intervalo = 900000;
            scan =" 15 min";
            #ifdef DEBUG_DATALOGGER
                Serial.println("Blynk 15 min");
            #endif
            break;
        case 5: // 30min
            intervalo = 1800000;
            scan =" 30 min";
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
    mode_energy = param.asInt();
    // No vuele a entrar en modo Ahorro energía
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
    #ifdef DEBUG_DATALOGGER
        Serial.println("LOOP: sensor = " + (String)sensor);
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
        //if (estado_BLE)
            //while (Blynk.connect() == false) {}
        Inicia_Pantalla();
    }

    // Espero a conectarme a WIFI -> Normalmente mode_energy = 1 (Ahorro de Energia) tras Deep Sleep M5STACK
    if (red_wifi){ 
        WifiState_t EstadoWifi = EZWIFI_IDLE;
        while(EstadoWifi != EZWIFI_IDLE){}
    }

    //Gestión del Ahorro de Energia mediante Deep_Sleep del ESP-32 (M5STACK)

    // Registro la pulsación del Boton C para salir del sueño profundo  
    M5.Power.setWakeupButton(PIN_Boton_C);
    // Verifico como se sale del sueño profundo
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason){
        case ESP_SLEEP_WAKEUP_EXT0: // RETORNO DEEP SLEEP -> Pulsado Boton C
            // power on the Lcd
            M5.Lcd.wakeup();
            M5.Lcd.setBrightness(50);
            Inicia_Pantalla();
            mode_energy=Normal; // DESACTIVADO Ahorro de energia
            init_scan=start; // antes era init_scan = 2 en loop()
            if (red_wifi){ // Se conectara a WIFI si antes del sueño profundo esta conectado
                WifiState_t EstadoWifi = EZWIFI_IDLE;
                while(EstadoWifi != EZWIFI_IDLE){}
                ez.clock.waitForSync();
                ez.clock.tz.setLocation(F("Europe/Madrid"));
                Inicia_Pantalla();
            }
            getTimeScan();
            ez.canvas.println("Ahorro Energia Desactivado");
            ez.canvas.println(" ");
            ez.canvas.println("   DataLogger iniciado");
            ez.canvas.println("   cada" + scan);
            #ifdef DEBUG_DATALOGGER
                Serial.println("SETUP EXT0: intervalo = " + (String)intervalo);
                Serial.println("SETUP EXT0: scan " + scan);
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
                Serial.println("SETUP TIMER: scan = " + scan);
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
        case 1: // Ahorro de energia con DataLogger inicializado
            timer.disable(batTimer);
            init_scan = start; // Para cuando se desactive el modo ahorro energia activar el timer
            getData(); // regoje y graba datos en SD cada intervalo
            break;
        case 2: // Busqueda sensor usado
            mode_energy = Normal; // Solo se ejecura 1 vez  mode_energy = 2
            break;
        default: //mode_energy = 0 (Normal)
            if (init_scan==start){ // Inicia DataLogger
                #ifdef DEBUG_DATALOGGER
                    Serial.println("LOOP: intervalo = " + (String)intervalo);
                    Serial.println("LOOP: scan = " + scan);
                #endif
                timer.deleteTimer(numTimer);
                numTimer = timer.setInterval(intervalo, getData); // regoje y graba datos en SD cada intervalo
                write_Pantalla("Iniciado DataLogger escaneando cada" + scan);
                mode_energy = Normal;
            }
            if (init_scan==stop){ // Para DataLogger
                timer.disable(numTimer);
                write_Pantalla("Parado DataLogger");   
                mode_energy = Normal;
            }
            init_scan=reinicio; // reinicio variable control DataLogger
            scanButton(); // Gestión acciones de los botones
            break;
    }
}


