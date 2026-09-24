
//  Ejemplo de CyberUI: 3 pantallas que rotan cada 5 s con datos de
//  mentira (para que veas todos los componentes). Sustituye los valores
//  simulados por los tuyos.
//  Necesita: TFT_eSPI configurada (User_Setup.h) y la librería CyberUI.

#include <TFT_eSPI.h>
#include <CyberUI.h>

TFT_eSPI tft;
CyberUI ui(tft);

static const uint8_t TOTAL_PAGINAS = 3;

//Pantalla 1: paneles con texto + barra segmentada
static void paginaRed(int pct) {
  const int16_t px = 4, pw = ui.ancho - 8;
  int16_t y = ui.yContenido();

  ui.panel(px, y, pw, 50, Cyber::CIAN, "// NODE_IP");
  ui.textoAjustado("192.168.1.42", px + 12, y + 24, pw - 24, Cyber::CIAN);

  y += 58;
  ui.panel(px, y, pw, 50, Cyber::MAGENTA, "// HOST.mDNS");
  ui.textoAjustado("mi-equipo-con-nombre-muy-largo.local", px + 12, y + 24, pw - 24, Cyber::BLANCO);

  y += 58;
  uint16_t color = Cyber::colorSegunPorcentaje(pct);
  ui.panel(px, y, pw, 112, color, "// MEM_CORE");
  ui.textoGlitch(String(pct) + "%", px + 12, y + 22, 5, color);
  const char* estado = Cyber::estadoSegunPorcentaje(pct);
  ui.textoDerecha(estado, px + pw - 10, y + 44, 2, color);
  ui.barraSegmentada(px + 11, y + 76, 216, 18, pct);
}

//Pantalla 2: tarjetas en columna + cifras en fila
static void paginaCifras(int a, int b, int c, int d) {
  const int16_t px = 4, pw = ui.ancho - 8;
  int16_t y = ui.yContenido();

  ui.panel(px, y, pw, 104, Cyber::CIAN, "// DOS_COLUMNAS");
  const int16_t mitad = px + 116;
  ui.divisorVertical(mitad, y + 22, 74);
  ui.tarjetaNumero(px + 12, y + 24, "ALFA", String(a), "DE 20 TOTAL", Cyber::CIAN, Cyber::MAGENTA);
  ui.tarjetaNumero(mitad + 12, y + 24, "BETA", String(b), "DE 12 TOTAL", Cyber::MAGENTA, Cyber::CIAN);

  y += 112;
  ui.panel(px, y, pw, 110, Cyber::MAGENTA, "// DOS_FILAS");
  ui.filaCifra(px + 12, y + 24, String(c), Cyber::AMARILLO, "CONTADOR C", "DESCRIPCION LINEA 2", px + 108);
  ui.lineaPunteada(px + 12, y + 62, pw - 24);
  ui.filaCifra(px + 12, y + 72, String(d), Cyber::VERDE, "CONTADOR D", "OTRA DESCRIPCION", px + 108);
}

//Pantalla 3: textos y colores disponibles
static void paginaCatalogo() {
  const int16_t px = 4, pw = ui.ancho - 8;
  int16_t y = ui.yContenido();

  ui.panel(px, y, pw, 100, Cyber::CIAN, "// PALETA");
  const uint16_t colores[] = {Cyber::CIAN, Cyber::MAGENTA, Cyber::AMARILLO, Cyber::VERDE, Cyber::ROJO, Cyber::BLANCO};
  for (int i = 0; i < 6; i++) {
    ui.pantalla().fillRect(px + 12 + i * 36, y + 26, 30, 30, colores[i]);
  }
  ui.texto("texto normal (tam 1)", px + 12, y + 68, 1, Cyber::BLANCO);
  ui.texto("texto tenue", px + 12, y + 80, 1, Cyber::TENUE);

  y += 108;
  ui.panel(px, y, pw, 100, Cyber::MAGENTA, "// GLITCH");
  ui.textoGlitch("NEON", px + 12, y + 26, 4, Cyber::CIAN);
  ui.textoGlitch("RED", px + 12, y + 62, 3, Cyber::MAGENTA, Cyber::CIAN);
  ui.textoDerecha("v1.0", px + pw - 10, y + 80, 1, Cyber::TENUE);
}

void setup() {
  ui.begin(0);                        // 0 = vertical
  ui.splash("CYBERUI", "> INICIANDO NUCLEO_");
  delay(1500);
}

void loop() {
  static uint8_t pagina = 0;

  ui.fondo();
  ui.encabezado("CYBERUI", pagina == 0 ? "//RED.Y.MEMORIA" : pagina == 1 ? "//CIFRAS" : "//CATALOGO");
  ui.indicadorRed(true, -60);          // usa WiFi.RSSI() y WiFi.status() en tu proyecto

  switch (pagina) {
    case 0: paginaRed(random(0, 101)); break;
    case 1: paginaCifras(random(0, 20), random(0, 12), random(0, 100), random(0, 100)); break;
    default: paginaCatalogo(); break;
  }

  char up[24];
  unsigned long s = millis() / 1000UL;
  snprintf(up, sizeof(up), "UP %lud %02luh %02lum", s / 86400UL, (s % 86400UL) / 3600UL, (s % 3600UL) / 60UL);
  ui.pie(">> SYNC 5s", up, pagina, TOTAL_PAGINAS);

  pagina = (pagina + 1) % TOTAL_PAGINAS;
  delay(5000);
}
