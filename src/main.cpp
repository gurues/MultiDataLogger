#include <Arduino.h>
#include <M5Stack.h>
#include <ezTime.h>
#include <M5ez.h>
#include <Wire.h>     //The BME280 uses I2C comunication.
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "RTClib.h"
#include <SimpleTimer.h>
#include <stdlib.h>

// Descomenta para relizar DEBUG por puerto serie
#define DEBUG_DATALOGGER

// Definición de Constantes
#define uS_to_S_Factor 1000         // Factor para DeepSleep en segundos
#define SEALEVELPRESSURE_HPA (1023.5) // Estimar la altitud para una presión dada comparándola con la presión
                                    // a nivel del mar. Este ejemplo utiliza el valor predeterminado, pero 
                                    // para obtener resultados más precisos, reemplace el valor con la 
                                    // presión actual del nivel del mar en su ubicación

// Definición de Pines
#define PIN_Boton_C 37  // Pin - GPIO del Boton C en M5Stack, en el programa despertará al M5Stack
#define PIN_SDA 21      // Pin SCA I2C
#define PIN_SCL 22      // Pin SCL I2C

// Objeto SimpleTimer para la ejecución periodica de funciones
SimpleTimer timer;
int numTimer, batTimer;

//Objeto RTC
RTC_DS1307 rtc;

// Objeto timezone
Timezone myTZ;

// Objeto BME280
Adafruit_BME280 bme; // I2C

// Objeto para el manejo de ficheros en tarjeta SD
File myFile;

// Variables de contol del Proyecto
RTC_DATA_ATTR int mode_energy = 2; // Modos de energia -> 0: Normal 1: Ahorro Energia Activado 2: Arranque 1ª vez Normal
RTC_DATA_ATTR uint32_t intervalo = 30000; // intervalo muestreo por defecto 30 seg.
RTC_DATA_ATTR bool red_wifi = false; // Estado red wifi -> false: No conectado true: conectado.
int init_scan = 2; //Variable de control start/stop registro de datos
String scan =" 30 seg";  // intervalo muestreo por defecto 30 seg mmostrado en pantalla
uint16_t refresco = 1000; // refresco eventos cada 1 seg (SimpleTimer funciones)
//int init_scan = 2; //Variable de control start/stop registro de datos
int menu = 0; // Variable de control para no refrescar pantalla si estoy en un menu
int battery; // Nivel de bateria 100% - 75% - 50% - 25%

//variables a registrar BME280
char temperatura [20];
char humedad [20];
char presion [20];
char altitud [20];


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////                  FUNIONES                 /////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Actualiza porcentaje bateria
void Level_Battery(){
    battery = M5.Power.getBatteryLevel();
    ez.header.show("DATA LOGGER   " + (String)battery + "%");
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
    ez.canvas.print(altitud);
    ez.canvas.println(" m ");

    ez.canvas.y(3*(ez.canvas.height()/6));
    ez.canvas.x(15);
    ez.canvas.print("Presion: ");
    ez.canvas.print(presion);
    ez.canvas.println(" hPa ");

    ez.canvas.y(4*(ez.canvas.height()/6));
    ez.canvas.x(15);
    ez.canvas.print("Temperatura: ");
    ez.canvas.print(temperatura);
    ez.canvas.println(" *C ");
    
    ez.canvas.y(5*(ez.canvas.height()/6));
    ez.canvas.x(15);
    ez.canvas.print("Humedad: ");
    ez.canvas.print(humedad);
    ez.canvas.println(" % ");
}

// Menu de configuración intervalo de escaneo del DataLogger
void submenu1(){
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
                Inicia_Pantalla();
            }
            menu=0;
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
                Inicia_Pantalla();
            }
            menu=0;
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
                Inicia_Pantalla();
            }
            menu=0;
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
                Inicia_Pantalla();
            }
            menu=0;
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
                Inicia_Pantalla();
            }
            menu=0;
            break;
    }
}

// Menu Borrar SD
void submenu2(){
    ezMenu myMenu2("Eliminar Datos SD");
    myMenu2.addItem("Borrar SD - SI");
    myMenu2.addItem("Borrar SD - NO");
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
                    myFile.println("Dia WIFI,Fecha Hora WIFI,Fecha Hora RTC,Temperatura (ºC),Humedad (%),Presion (hPa),Altitud (m)");
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
            menu=0;
            break;
        case 2: // No se Borra fichero con datos
            if (timer.isEnabled(numTimer)){
                Inicia_Pantalla();
                show_Data();
            }
            else
                Inicia_Pantalla();
            menu=0;
            break;
    }
}

// Menu Configuración General DAtaLoggger
void submenu3(){
    ez.settings.menu();
    ez.clock.tz.setLocation(F("Europe/Madrid"));
    ez.clock.waitForSync();
    Inicia_Pantalla();
}

// Menu Principal de Configuración del DataLogger
void Config_DataLogger(){
    menu=1;
    ezMenu myMenu("Configuracion DataLogger");
    myMenu.addItem("Intervalo de escaneo");
    myMenu.addItem("Eliminar Datos SD");
    myMenu.addItem("Configuracion General");
    myMenu.addItem ("EXIT");
    switch (myMenu.runOnce()) {
        case 1: // Intervalo de escaneo   
            submenu1();
            break;
        case 2: // Eliminar Datos SD  
            submenu2();
            break;
        case 3: // Configuracion General  
            submenu3();
            break;
        case 4: // EXIT
            if (timer.isEnabled(numTimer)){
                Inicia_Pantalla();
                show_Data();
            }
            else
                Inicia_Pantalla();
            menu=0;
            break;
    }
}

// Gestión acciones de los botones
void scanButton() {
    String btn = ez.buttons.poll();

    if (btn == "SETUP") Config_DataLogger();

    if ((btn == "START")&&(!(timer.isEnabled(numTimer)))){
        write_Pantalla("Iniciado DataLogger escaneando cada" + scan);
        init_scan = 1;
        mode_energy = 0;
    } 

    if ((btn == "STOP")&&(timer.isEnabled(numTimer))){
        write_Pantalla("Parado DataLogger");   
        init_scan = 0;
        mode_energy = 0;
    } 

    if (btn == "OFF"){
        M5.Power.powerOFF();
    }

    if ((btn == "SLEEP")&&(timer.isEnabled(numTimer))){
        mode_energy = 1;
        M5.Power.deepSleep(intervalo*uS_to_S_Factor);
    }

}

//Ejecución de eventos asociados
uint16_t Programa_Eventos(){
    timer.run(); // Inicio SimpleTimer para la ejecución de funciones
    return refresco; // retorna el intervalo para su nueva ejecución
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
        if (mode_energy==1)
            ez.clock.waitForSync();
        myTZ.setLocation(F("Europe/Madrid"));
    }
    //RTC:
    DateTime now = rtc.now();
    String Fecha = (String)now.day() + "/"+ (String)now.month() + "/"+ (String)now.year() 
                    + " " + (String)now.hour() + ":"+ (String)now.minute() + ":"+ (String)now.second();

    //Adquisición de datos del sensor
    //Leer temperatura.
    dtostrf(bme.readTemperature(),2,1,temperatura);
    //Leer humedad.
    dtostrf(bme.readHumidity(),2,1,humedad);
    //Leer presion.
    dtostrf(bme.readPressure()/ 100.0F,2,1,presion);
    //Leer altitud.
    dtostrf(bme.readAltitude(SEALEVELPRESSURE_HPA),2,1,altitud);

    //Muestra en pantalla los datos recogidos
    if ((menu==0)&&(mode_energy!=1)){
        Inicia_Pantalla();
        show_Data();
    }

    // DEBUG DataLogger
    #ifdef DEBUG_DATALOGGER
        Serial.println("*************************************************************");
        Serial.println("Fecha / Hora WIFI: " + myTZ.dateTime("l, d-M-y H:i:s"));
        Serial.println("Fecha / Hora RTC: " + Fecha);
        if(red_wifi) Serial.println("Red Wifi = True");
        else Serial.println("Red Wifi = False");
        Serial.println("Modo Energia = " + (String)mode_energy);
        Serial.println("intervalo = " + (String)intervalo);
        Serial.println("scan = " + (String)scan);
        Serial.println("Temperatura = " + (String)temperatura);
        Serial.println("Humedad = " + (String)humedad);
        Serial.println("Presión = " + (String)presion);
        Serial.println("Altitud = " + (String)altitud);
    #endif
    
    //Almacenamos los datos en la SD
    myFile = SD.open("/datalog.csv", FILE_APPEND);//abrimos  el archivo y añadimos datos
    if (myFile) { 
        #ifdef DEBUG_DATALOGGER
            Serial.println("Almacenando datos en Tarjeta SD .....");
        #endif
        myFile.print((ez.clock.tz.dateTime("l, d-M-y H:i:s")));
        myFile.print(",");
        myFile.print(Fecha);
        myFile.print(",");
        myFile.print(temperatura);
        myFile.print(",");
        myFile.print(humedad);  
        myFile.print(",");
        myFile.print(presion);  
        myFile.print(",");
        myFile.println(altitud);  // Salto de linea 
        myFile.close();         //cerramos el archivo                    
    } 
    else {
  	    #ifdef DEBUG_DATALOGGER
            Serial.println("Error al abrir el archivo en tarjeta SD");
        #endif
    }
    
    // Si estamos ejectando el DataLogger en mode_energy = Ahorro de energia dormimos el Datalogger
    // hasta la siguiente lesctura para ahorra bateria
    if (mode_energy==1){
        M5.Power.deepSleep(intervalo*uS_to_S_Factor);
    } 
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
  if (mode_energy!=1){ // En todos los mode_energy menos en ahorra de energia
    Inicia_Pantalla();
  }
  
  // Inicio I2C
  Wire.begin(PIN_SDA, PIN_SCL);

  // Inicio BME280
  if (!bme.begin()) {
    ez.canvas.println("No encontrado sensor");
    ez.canvas.println("  BME280 !!!");
    while (1);
  }

  // Inicio RTC DS1307
  if (!rtc.begin()) {
    ez.canvas.println("No encontrado RTC");
    while (1);
   }
   if (mode_energy==2){
      // Fijar a fecha y hora de compilacion por defecto
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
   }

  // Inicio tarjeta SD
  if (!SD.begin()) {
    ez.canvas.println("Fallo SD o SD no insertada");
    while (1);
  }

  // Registro la pulsación del Boton C para salir del sueño profundo  
  M5.Power.setWakeupButton(PIN_Boton_C);
  // Verifico como se sale del sueño profundo
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  switch (wakeup_reason){
    case ESP_SLEEP_WAKEUP_EXT0: //Pulsado Boton C
        Inicia_Pantalla();
        mode_energy=0; // DESACTIVADO Ahorro de energia
        init_scan=1;
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
    case ESP_SLEEP_WAKEUP_TIMER: //Timer cada intervalo
        // ACTIVADO Ahorro de energia
        #ifdef DEBUG_DATALOGGER
            getTimeScan();
            Serial.println("SETUP TIMER: intervalo = " + (String)intervalo);
            Serial.println("SETUP TIMER: scan = " + scan);
        #endif
        break;
  }

  // M5ez -> Configuración inicial WIFI y reloj 
  if (mode_energy==2){
    ez.settings.menu();
    red_wifi = ez.wifi.autoConnect; // Compruebo si me tengo que conecta al WIFI
    if (red_wifi){
        WifiState_t EstadoWifi = EZWIFI_IDLE;
        while(EstadoWifi != EZWIFI_IDLE){}  // Espero a conectarme al WIFI
        ez.clock.waitForSync();
        ez.clock.tz.setLocation(F("Europe/Madrid"));
    }
    Inicia_Pantalla();
  }

  // Conexión WIFI en mode_energy = 1
  if (red_wifi){ 
    WifiState_t EstadoWifi = EZWIFI_IDLE;
    while(EstadoWifi != EZWIFI_IDLE){}
  }

  //Inicio y desactivo Timer
  numTimer = timer.setInterval(intervalo, getData);
  batTimer = timer.setInterval(300000, Level_Battery); // Cada 5 min actualizo nivel de bateria
  timer.disable(numTimer);
  if (mode_energy==1){ // En mode_energy "ahorro de energia" desactiva actualizar nivel bateria
    timer.disable(batTimer);
  }

}

void loop() {

    red_wifi = ez.wifi.autoConnect; // Compruebo WIFI
    switch (mode_energy) {
        case 1: // Ahorro de energia con DataLogger inicializado
            timer.disable(batTimer);
            getData(); // regoje y graba datos en SD cada intervalo
        default:
            if (init_scan==1){ // Inicia DataLogger
                #ifdef DEBUG_DATALOGGER
                    Serial.println("LOOP: intervalo = " + (String)intervalo);
                    Serial.println("LOOP: scan = " + scan);
                #endif
                timer.deleteTimer(numTimer);
                numTimer = timer.setInterval(intervalo, getData); // regoje y graba datos en SD cada intervalo
                ez.addEvent(Programa_Eventos, refresco); // Eventos asociados a M5ez: SimpleTimer run
            }
            if (init_scan==0){ // Para DataLogger
                timer.disable(numTimer);
                ez.removeEvent(Programa_Eventos);
            }
            init_scan=2; // reinicio variable control DataLogger
            scanButton(); // Gestión acciones de los botones
            break;
    }
}


