
// Calibrar_Touch: fuerza el asistente de calibración y luego te deja
// dibujar con el dedo para comprobar que el touch coincide con lo que
// ves. Abre el monitor serie a 115200: al terminar imprime una línea
// `touch.setCalibracion(...)` que puedes pegar en tus sketches.


#include <TFT_eSPI.h>
#include <CyberUI.h>
#include <CyberTouch.h>

TFT_eSPI tft;
CyberUI ui(tft);
CyberTouch touch;

void setup() {
  Serial.begin(115200);
  ui.begin(0);          // 0 = vertical; prueba 1 para horizontal
  touch.begin(ui);
  touch.calibrar(ui);   // 3 toques en las miras

  ui.fondo();
  ui.encabezado("TOUCH", "//DIBUJA.CON.EL.DEDO");
  ui.pie(">> PRUEBA", "TOCA LA PANTALLA");
}

void loop() {
  const CyberToque& t = touch.leer();
  if (t.activo) {
    ui.pantalla().fillCircle(t.x, t.y, 3, Cyber::MAGENTA);
    ui.pantalla().fillCircle(t.x, t.y, 1, Cyber::CIAN);
  }
  delay(5);
}
