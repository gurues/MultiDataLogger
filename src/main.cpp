#include <Arduino.h>
#include <M5Stack.h>
#include <ezTime.h>
#include <M5ez.h>
#include <Wire.h>     //The BME280 uses I2C comunication.
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <stdlib.h>


char temperatura [20];
char humedad [20];
double intervalo= 5000;

Adafruit_BME280 bme; // I2C


void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Escribiendo archivo: %s\n", path);
   
    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Error al abrir el archivo...");
        return;
    }
    if(file.print(message)){
        Serial.println("Escritura Ok");
    } else {
        Serial.println("Error en la escritura");
    }
}


void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("añadiendo dato al archivo: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Error al abrir el archivo para añadir... ");
        return;
    }
    if(file.print(message)){
        Serial.println("Dato añadido");
    } else {
        Serial.println("Error al añadir el dato");
    }
    file.close();
}


void setup() {
  bool status = bme.begin(); 
  if (!status) {
    Serial.println("No encontrado sensor BME280!!!");
  while (1);
  }
  //M5.begin();
  ez.begin();
  Wire.begin();
  writeFile(SD, "/Lecturas sensor bme280.txt", "Lecturas de temperatura y humedad bme280 en intervalos de 5 segundos\n ");
  appendFile(SD, "/Lecturas sensor bme280.txt", "===================================================================\n");
  ez.wifi.menu();
  ez.clock.menu();
}


void loop() {
  /*  
  int xpos = 0;
  int ypos = 100;
  int ypos1 = 130;
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(3);//Ajusto tamaño de la letra
  M5.Lcd.setTextColor(BLUE);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("Prueba bme280");
  M5.Lcd.println("=============");
  M5.Lcd.setTextColor(YELLOW);
  M5.Lcd.setCursor(xpos, ypos);
    */

    ez.screen.clear();
    ez.header.show("BASIC DATA LOGGER");
    ez.buttons.show("SETUP # INIT # STOP");

  //Leer temperatura.
  dtostrf(bme.readTemperature(),2,1,temperatura);
 
    ez.canvas.print("Temp:    ");
    ez.canvas.print(temperatura);
    ez.canvas.println(" *C ");
/*
  //Muestro datos temperatura en el LCD
  M5.Lcd.print("Temp:    ");
  M5.Lcd.print(temperatura);
  M5.Lcd.println(" *C ");

  M5.Lcd.setCursor(xpos, ypos1);
*/


  //Leer humedad.
  dtostrf(bme.readHumidity(),2,1,humedad);

    ez.canvas.print("Humedad: ");
    ez.canvas.print(humedad);
    ez.canvas.println(" % ");
 
 /*
  //Muestro datos humedad en LCD
  M5.Lcd.print("Humedad: ");
  M5.Lcd.print(humedad);
  M5.Lcd.println(" % ");
 */

 // Escribimos los datos de la temperatura en la tarjeta micro SD
  appendFile(SD, "/Lecturas sensor bme280.txt", "Temperatura = ");
  appendFile(SD, "/Lecturas sensor bme280.txt", temperatura);
  appendFile(SD, "/Lecturas sensor bme280.txt",  " ºC\n");

  // Escribimos los datos de la humedad relativa  en la tarjeta micro SD
  appendFile(SD, "/Lecturas sensor bme280.txt", "Humedad = ");
  appendFile(SD, "/Lecturas sensor bme280.txt", humedad);
  appendFile(SD, "/Lecturas sensor bme280.txt",  " %\n");


  delay(intervalo);
}


