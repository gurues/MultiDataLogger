## Instrucciones para mostrar una imagen bmp

- 1. Imagen que deseas en formato BMP. Escalarla a 320x240 y voltéarla verticalmente.
- 2. Guarde la imagen como un BMP de 16 bits (5R6G5B). Con GIMP puede encontrar esta opción en el texto "Opciones avanzadas" en el cuadro de diálogo "Exportar imagen como BMP" justo después de asignarle un nombre al archivo.
- 3. Use el script bin22code.py de la carpeta tools para convertirlo en una matriz C.
- 4. Agregue la referencia en su código a la matriz como externa (extern const unsigned char logo[]).
- 5. Use el método M5.Lcd.drawBitmap para dibujar la imagen en la pantalla.
  
Sacado de: https://tinkerman.cat/post/m5stack-node-things-network

Modificado el archivo bin2code.py del repositorio de Xose Perez por error de compilación.