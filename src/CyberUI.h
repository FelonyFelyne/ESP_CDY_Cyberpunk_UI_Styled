#pragma once


//  CyberUI v2 -- mini librería de interfaz "cyberpunk" para TFT_eSPI
//
//   Por: Alejandro Varela SciFi  ->  @alejandro.varela.scifi
//   github.com/FelonyFelyne
//
//  (los widgets táctiles viven en CyberTouch.h)
//
//  Header-only: copia este archivo junto a tu sketch (o a la carpeta
//  libraries/CyberUI/) y haz  #include "CyberUI.h".
//
//  Qué incluye
//    - Paleta neón (namespace Cyber)
//    - Fondo con líneas de escaneo + rejilla
//    - Texto con "glitch" (doble color: sombra desplazada + principal)
//    - Paneles con esquinas cortadas, barra de acento y etiqueta
//    - Barra segmentada (cian -> amarillo -> rojo según el %)
//    - Encabezado con título glitch, franja de glitch aleatoria e
//        indicador de red (barras de señal + ONLINE / NO LINK)
//    - Pie con texto izquierdo/derecho e indicador de página
//    - Tarjetas de cifra grande (columna y fila), línea punteada, splash
//
//  Requisitos
//    - Librería TFT_eSPI (con tu User_Setup.h ya configurado).
//    - Usa SOLO la fuente por defecto (GLCD) con distintos tamaños, así
//      que no hace falta activar ningún LOAD_FONTx. Ojo: esa fuente no
//      dibuja acentos ni eñes -- escribe los textos sin ellos.
//    - No usa sprites: todo se dibuja directo al panel (un sprite a pantalla
//      completa no cabe en un ESP32 sin PSRAM).
//
//  Medidas de la fuente GLCD: cada carácter mide 6 x 8 px por unidad de
//  "tam" (tam 1 = 6x8, tam 2 = 12x16, tam 5 = 30x40 ...).
//
//  Uso mínimo
//    TFT_eSPI tft;  CyberUI ui(tft);
//    setup():  ui.begin(0);            // 0 = vertical, 1 = horizontal...
//    dibujo :  ui.fondo();
//              ui.encabezado("CYBER_GUI", "//MI.SUBTITULO");
//              ui.panel(4, 58, 232, 50, Cyber::CIAN, "// NODE_IP");
//              ui.textoAjustado("192.168.1.20", 16, 82, 208, Cyber::CIAN);
//              ui.pie(">> SYNC 60s", "UP 0d 01h");



#include <Arduino.h>
#include <TFT_eSPI.h>

namespace Cyber {

// RGB888 -> RGB565 en tiempo de compilación.
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

//Paleta neón (cámbiala aquí y cambia toda la interfaz)
constexpr uint16_t FONDO      = rgb(6, 4, 16);      //casi negro, tinte violeta
constexpr uint16_t SCAN       = rgb(13, 9, 28);     // líneas de escaneo
constexpr uint16_t REJILLA    = rgb(20, 14, 44);    //rejilla vertical
constexpr uint16_t PANEL      = rgb(9, 7, 24);      // fondo de paneles
constexpr uint16_t ENCABEZADO = rgb(18, 6, 36);     // fondo del encabezado
constexpr uint16_t CIAN       = rgb(0, 225, 255);
constexpr uint16_t MAGENTA    = rgb(255, 0, 140);
constexpr uint16_t AMARILLO   = rgb(252, 238, 10);
constexpr uint16_t VERDE      = rgb(57, 255, 20);
constexpr uint16_t ROJO       = rgb(255, 32, 64);
constexpr uint16_t TENUE      = rgb(120, 96, 190);  // etiquetas secundarias
constexpr uint16_t SEG_VACIO  = rgb(28, 20, 58);    // segmentos apagados
constexpr uint16_t BLANCO     = rgb(235, 245, 255);

// Ancho en píxeles de un texto de n caracteres con la fuente GLCD.
inline int16_t anchoTexto(size_t caracteres, uint8_t tam) {
  return (int16_t)(caracteres * 6 * tam);
}

// Mayor tamaño de texto (<= tamMax) con el que `caracteres` caben en
// `anchoDisponible` px. Nunca devuelve menos de 1.
inline uint8_t tamAuto(size_t caracteres, int16_t anchoDisponible, uint8_t tamMax = 2) {
  uint8_t tam = tamMax;
  while (tam > 1 && anchoTexto(caracteres, tam) > anchoDisponible) tam--;
  return tam;
}

// Color neón según un porcentaje: verde < 75 <= amarillo < 90 <= rojo.
inline uint16_t colorSegunPorcentaje(int pct) {
  if (pct >= 90) return ROJO;
  if (pct >= 75) return AMARILLO;
  return VERDE;
}

// Palabra de estado según un porcentaje (sin acentos: fuente GLCD).
inline const char* estadoSegunPorcentaje(int pct) {
  if (pct >= 90) return "CRITICO";
  if (pct >= 75) return "ALERTA";
  return "NOMINAL";
}

// Nivel de señal WiFi (0-4) a partir del RSSI en dBm.
inline uint8_t nivelSenal(int rssi) {
  if (rssi > -55) return 4;
  if (rssi > -67) return 3;
  if (rssi > -78) return 2;
  return 1;
}

}  //namespace Cyber

class CyberUI {
 public:
  explicit CyberUI(TFT_eSPI& t) : tft(t) {}

 //Medidas de la pantalla (se actualizan en begin() / medir()).
  int16_t ancho = 240;
  int16_t alto = 320;
  uint8_t rotacion = 0;  //rotación actual del TFT (la usa CyberTouch)

  // Inicializa el panel. rotacion: 0/2 vertical, 1/3 horizontal.
  // Si ya llamaste tft.init() y setRotation() por tu cuenta, usa solo medir().
  void begin(uint8_t rotacion = 0) {
    tft.init();
    tft.setRotation(rotacion);
    medir();
  }

  void medir() {
    ancho = tft.width();
    alto = tft.height();
    rotacion = tft.getRotation();
  }

  // Acceso directo al TFT por si quieres dibujar algo a mano.
  TFT_eSPI& pantalla() { return tft; }

  // Zona útil entre encabezado y pie.
  int16_t yContenido() const { return 58; }
  int16_t yPie() const { return alto - 20; }


//Fondo
  // Borra la pantalla y pinta líneas de escaneo + rejilla vertical tenue.
  void fondo(bool escaneo = true, bool rejilla = true) {
    tft.fillScreen(Cyber::FONDO);
    if (escaneo) {
      for (int16_t y = 0; y < alto; y += 3) tft.drawFastHLine(0, y, ancho, Cyber::SCAN);
    }
    if (rejilla) {
      for (int16_t x = 20; x < ancho; x += 40) tft.drawFastVLine(x, 0, alto, Cyber::REJILLA);
    }
  }


//Texto (fondo transparente: respeta el escaneo del fondo)
  void texto(const char* txt, int16_t x, int16_t y, uint8_t tam, uint16_t color) {
    tft.setTextSize(tam);
    tft.setTextColor(color);
    tft.setCursor(x, y);
    tft.print(txt);
  }

  void texto(const String& txt, int16_t x, int16_t y, uint8_t tam, uint16_t color) {
    texto(txt.c_str(), x, y, tam, color);
  }

  // Texto con glitch: una copia desplazada (+2,+1) en `sombra` debajo y el
  // texto principal encima. Es el efecto de "doble color".
  void textoGlitch(const char* txt, int16_t x, int16_t y, uint8_t tam,
                   uint16_t color, uint16_t sombra = Cyber::MAGENTA) {
    texto(txt, x + 2, y + 1, tam, sombra);
    texto(txt, x, y, tam, color);
  }

  void textoGlitch(const String& txt, int16_t x, int16_t y, uint8_t tam,
                   uint16_t color, uint16_t sombra = Cyber::MAGENTA) {
    textoGlitch(txt.c_str(), x, y, tam, color, sombra);
  }

  // Texto alineado a la derecha: `xDerecha` es el borde derecho.
  void textoDerecha(const char* txt, int16_t xDerecha, int16_t y, uint8_t tam, uint16_t color) {
    texto(txt, xDerecha - Cyber::anchoTexto(strlen(txt), tam), y, tam, color);
  }

  // Texto centrado horizontalmente en la pantalla.
  void textoCentrado(const char* txt, int16_t y, uint8_t tam, uint16_t color) {
    texto(txt, (ancho - Cyber::anchoTexto(strlen(txt), tam)) / 2, y, tam, color);
  }

  // Texto de tamaño 2 que se achica solo a tamaño 1 si no cabe en `anchoMax`.
  // Si se achica, baja 4 px para quedar centrado en la misma línea.
  void textoAjustado(const String& txt, int16_t x, int16_t y, int16_t anchoMax,
                     uint16_t color, uint8_t tamMax = 2) {
    uint8_t tam = tamMax;
    while (tam > 1 && Cyber::anchoTexto(txt.length(), tam) > anchoMax) tam--;
    texto(txt, x, y + (tamMax - tam) * 4, tam, color);
  }


//  Paneles y líneas
// Panel con esquinas cortadas (arriba-derecha y abajo-izquierda), barra
 // de acento gruesa a la izquierda y etiqueta tipo consola (opcional).
  void panel(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color,
             const char* etiqueta = nullptr) {
    const int16_t c = 10;  // tamaño del corte

    tft.fillRect(x, y, w, h, Cyber::PANEL);
    // Recorta los triángulos de las esquinas cortadas
    tft.fillTriangle(x + w - c, y, x + w, y, x + w, y + c, Cyber::FONDO);
    tft.fillTriangle(x, y + h - c, x, y + h, x + c, y + h, Cyber::FONDO);

    // Contorno
    tft.drawFastHLine(x, y, w - c, color);
    tft.drawLine(x + w - c, y, x + w, y + c, color);
    tft.drawFastVLine(x + w, y + c, h - c, color);
    tft.drawFastHLine(x + c, y + h, w - c, color);
    tft.drawLine(x, y + h - c, x + c, y + h, color);
    tft.drawFastVLine(x, y, h - c, color);

    // Barra de acento
    tft.fillRect(x, y + 4, 3, h - c - 8, color);

    if (etiqueta) {
      tft.fillRect(x + 10, y + 7, 5, 5, Cyber::MAGENTA);
      texto(etiqueta, x + 20, y + 6, 1, Cyber::TENUE);
    }
  }

  // Línea horizontal punteada (separador dentro de un panel).
  void lineaPunteada(int16_t x, int16_t y, int16_t w, uint16_t color = Cyber::SEG_VACIO) {
    for (int16_t i = 0; i < w; i += 6) tft.drawFastHLine(x + i, y, 3, color);
  }

  // Divisor vertical sólido (separador de columnas dentro de un panel).
  void divisorVertical(int16_t x, int16_t y, int16_t h, uint16_t color = Cyber::SEG_VACIO) {
    tft.drawFastVLine(x, y, h, color);
  }

  // Rectángulo con esquinas cortadas (arriba-derecha y abajo-izquierda),
  // relleno y con contorno. `fondoDetras` es el color de lo que hay detrás
  // (para "borrar" los triángulos de las esquinas). Es la base de botones,
  // casillas e interruptores.
  void formaCortada(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t relleno,
                    uint16_t contorno, uint16_t fondoDetras, int16_t corte = 6) {
    tft.fillRect(x, y, w, h, relleno);
    tft.fillTriangle(x + w - 1 - corte, y, x + w - 1, y, x + w - 1, y + corte, fondoDetras);
    tft.fillTriangle(x, y + h - 1 - corte, x, y + h - 1, x + corte, y + h - 1, fondoDetras);

    tft.drawFastHLine(x, y, w - corte, contorno);
    tft.drawLine(x + w - 1 - corte, y, x + w - 1, y + corte, contorno);
    tft.drawFastVLine(x + w - 1, y + corte, h - corte, contorno);
    tft.drawFastHLine(x + corte, y + h - 1, w - corte, contorno);
    tft.drawLine(x, y + h - 1 - corte, x + corte, y + h - 1, contorno);
    tft.drawFastVLine(x, y, h - corte, contorno);
  }

  // Texto centrado dentro de una caja (horizontal y verticalmente).
  void textoEnCaja(const char* txt, int16_t x, int16_t y, int16_t w, int16_t h,
                   uint8_t tam, uint16_t color, bool glitch = false,
                   uint16_t sombra = Cyber::MAGENTA) {
    const int16_t tx = x + (w - Cyber::anchoTexto(strlen(txt), tam)) / 2;
    const int16_t ty = y + (h - 8 * tam) / 2;
    if (glitch) textoGlitch(txt, tx, ty, tam, color, sombra);
    else texto(txt, tx, ty, tam, color);
  }

//Barra segmentada
  //Barra de `segmentos` bloques que ocupa `w` px. Cada bloque tiene color
  //por su posición: cian, amarillo desde el 75%, rojo desde el 90%.
  // Con marcas = true dibuja "0 / 50 / 100" debajo.
  void barraSegmentada(int16_t x, int16_t y, int16_t w, int16_t h, int pct,
                       uint8_t segmentos = 18, bool marcas = true) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    const int16_t paso = w / segmentos;
    const int16_t segAncho = paso - 2;
    int llenos = (pct * segmentos + 50) / 100;
    if (pct > 0 && llenos == 0) llenos = 1;

    for (int i = 0; i < segmentos; i++) {
      int posicion = ((i + 1) * 100) / segmentos;
      uint16_t color = (posicion >= 90) ? Cyber::ROJO
                       : (posicion >= 75) ? Cyber::AMARILLO
                                          : Cyber::CIAN;
      tft.fillRect(x + i * paso, y, segAncho, h, i < llenos ? color : Cyber::SEG_VACIO);
    }
    if (marcas) {
      const int16_t yMarcas = y + h + 4;
      texto("0", x, yMarcas, 1, Cyber::TENUE);
      texto("50", x + (segmentos * paso) / 2 - 6, yMarcas, 1, Cyber::TENUE);
      texto("100", x + segmentos * paso - 18, yMarcas, 1, Cyber::TENUE);
    }
  }

//Cifras grandes
  // Tarjeta en COLUMNA: etiqueta pequeña arriba, cifra grande con glitch y
  // una línea de detalle debajo. Ocupa unos (14 + 8*tam + 16) px de alto.
  void tarjetaNumero(int16_t x, int16_t y, const char* etiqueta, const String& valor,
                     const char* detalle, uint16_t color, uint16_t sombra = Cyber::MAGENTA,
                     uint8_t tam = 5) {
    texto(etiqueta, x, y, 1, Cyber::TENUE);
    textoGlitch(valor, x, y + 14, tam, color, sombra);
    if (detalle) texto(detalle, x, y + 14 + 8 * tam + 8, 1, Cyber::TENUE);
  }

  //Cifra en FILA: número (tam 4) a la izquierda y dos líneas de texto a su
  // derecha, empezando en xTexto. Ocupa 32 px de alto.
  void filaCifra(int16_t x, int16_t y, const String& valor, uint16_t color,
                 const char* linea1, const char* linea2, int16_t xTexto) {
    textoGlitch(valor, x, y, 4, color);
    texto(linea1, xTexto, y + 6, 1, color);
    texto(linea2, xTexto, y + 18, 1, Cyber::TENUE);
  }

//encabezado / pie
  // Encabezado de 48 px: franja magenta arriba, línea cian abajo, título
  // con glitch (máx. 8 caracteres a tamaño 3 en una pantalla de 240 px),
  // subtítulo y una franja de glitch aleatoria (cambia en cada llamada).
  void encabezado(const char* titulo, const char* subtitulo = nullptr) {
    tft.fillRect(0, 0, ancho, 46, Cyber::ENCABEZADO);
    tft.fillRect(0, 0, ancho, 2, Cyber::MAGENTA);
    tft.drawFastHLine(0, 46, ancho, Cyber::CIAN);
    tft.fillRect(0, 46, 60, 2, Cyber::CIAN);  // muesca gruesa

    textoGlitch(titulo, 10, 8, 3, Cyber::CIAN);
    if (subtitulo) texto(subtitulo, 10, 35, 1, Cyber::TENUE);
    franjaGlitch(51);
  }

//Segmentos cortos aleatorios (cian/magenta) para el efecto glitch.
  void franjaGlitch(int16_t y, uint8_t cantidad = 4) {
    for (uint8_t i = 0; i < cantidad; i++) {
      int16_t gx = random(10, ancho - 40);
      int16_t gw = random(8, 34);
      tft.fillRect(gx, y, gw, 2, (i % 2) ? Cyber::MAGENTA : Cyber::CIAN);
    }
  }

  // Indicador de red para la esquina del encabezado: 4 barras de señal y
  // "ONLINE" / "NO LINK". rssi = WiFi.RSSI() (ignorado si !conectado).
  void indicadorRed(bool conectado, int rssi = -60) {
    const int16_t x = ancho - 44, yBase = 30;
    const uint8_t nivel = conectado ? Cyber::nivelSenal(rssi) : 0;
    for (uint8_t i = 0; i < 4; i++) {
      int16_t h = 4 + i * 4;
      tft.fillRect(x + i * 7, yBase - h, 5, h, i < nivel ? Cyber::CIAN : Cyber::SEG_VACIO);
    }
    texto(conectado ? "ONLINE" : "NO LINK", conectado ? x : x - 6, yBase + 4, 1,
          conectado ? Cyber::VERDE : Cyber::ROJO);
  }

  // Pie de 20 px: línea magenta, texto a la izquierda, texto a la derecha
  // y, si totalPaginas > 1, un indicador de página al centro.
  void pie(const char* izquierda = nullptr, const char* derecha = nullptr,
           uint8_t pagina = 0, uint8_t totalPaginas = 1) {
    const int16_t y = yPie();
    tft.drawFastHLine(0, y, ancho, Cyber::MAGENTA);
    tft.fillRect(ancho - 60, y - 1, 60, 2, Cyber::MAGENTA);

    if (izquierda) texto(izquierda, 8, y + 8, 1, Cyber::CIAN);
    if (derecha) textoDerecha(derecha, ancho - 8, y + 8, 1, Cyber::TENUE);

    if (totalPaginas > 1) {
      const int16_t total = totalPaginas * 16 - 6;
      const int16_t x0 = (ancho - total) / 2;
      for (uint8_t i = 0; i < totalPaginas; i++) {
        tft.fillRect(x0 + i * 16, y + 9, 10, 4, i == pagina ? Cyber::MAGENTA : Cyber::SEG_VACIO);
      }
    }
  }

//Splash
  //Pantalla de arranque: título glitch centrado y un mensaje debajo.
  void splash(const char* titulo, const char* mensaje = "> INICIANDO NUCLEO_") {
    fondo();
    const int16_t y = alto / 2 - 40;
    const int16_t x = (ancho - Cyber::anchoTexto(strlen(titulo), 3)) / 2;
    textoGlitch(titulo, x, y, 3, Cyber::CIAN);
    tft.drawFastHLine(x, y + 32, Cyber::anchoTexto(strlen(titulo), 3), Cyber::MAGENTA);
    textoCentrado(mensaje, y + 42, 1, Cyber::TENUE);
  }

 private:
  TFT_eSPI& tft;
};
