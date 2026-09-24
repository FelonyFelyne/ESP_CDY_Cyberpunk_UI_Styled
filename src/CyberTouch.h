#pragma once

//  CyberTouch -- entrada táctil y widgets para CyberUI
//  Por: Alejandro Varela SciFi  ->  @alejandro.varela.scifi
//   github.com/FelonyFelyne
//
//  Incluye:
//    - CyberTouch    driver del XPT2046 (CYD): lectura filtrada, eventos
//                    inicio / fin del toque y calibración guardada en flash
//    - Widgets       CyberBoton, CyberCheckbox, CyberInterruptor,
//                    CyberSlider, CyberSelector, CyberStepper
//    - CyberGUI      administra los widgets: reparte los toques y redibuja
//                    SOLO los widgets que cambiaron
//
//  Dependencias: TFT_eSPI, XPT2046_Touchscreen (Paul Stoffregen) y CyberUI.h
//
//  Los pines del touch usan por defecto los de la CYD (ESP32-2432S028).
//  Para cambiarlos, define las macros ANTES del #include:
//    #define CYBER_TOUCH_CS 33   (IRQ 36, MOSI 32, MISO 39, CLK 25)



#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include <XPT2046_Touchscreen.h>
#include <functional>
#include <math.h>
#include "CyberUI.h"

#ifndef CYBER_TOUCH_IRQ
#define CYBER_TOUCH_IRQ 36  // T_IRQ
#endif
#ifndef CYBER_TOUCH_MOSI
#define CYBER_TOUCH_MOSI 32  // T_DIN
#endif
#ifndef CYBER_TOUCH_MISO
#define CYBER_TOUCH_MISO 39  // T_OUT
#endif
#ifndef CYBER_TOUCH_CLK
#define CYBER_TOUCH_CLK 25  // T_CLK
#endif
#ifndef CYBER_TOUCH_CS
#define CYBER_TOUCH_CS 33  // T_CS
#endif

// Presión mínima (0-4095) para considerar que hay un dedo. Si te detecta
// toques fantasma, súbela; si no detecta toques suaves, bájala.
#ifndef CYBER_TOUCH_Z_MIN
#define CYBER_TOUCH_Z_MIN 250
#endif
// Milisegundos sin señal antes de dar el toque por terminado (anti-rebote).
#ifndef CYBER_TOUCH_SOLTAR_MS
#define CYBER_TOUCH_SOLTAR_MS 40
#endif
// Cantidad máxima de widgets que administra un CyberGUI.
#ifndef CYBER_MAX_WIDGETS
#define CYBER_MAX_WIDGETS 24
#endif

//  Estado del toque (ya en coordenadas de pantalla)
struct CyberToque {
  bool activo = false;    // hay un dedo apoyado
  bool inicio = false;    // true SOLO en el primer ciclo del toque
  bool fin = false;       // true SOLO en el ciclo en que se soltó
  int16_t x = 0;          // último punto válido
  int16_t y = 0;
  uint32_t desde = 0;     // millis() en que empezó el toque
};

//  Driver del touch + calibración
class CyberTouch {
 public:
  CyberTouch() : spi(VSPI), ts(CYBER_TOUCH_CS, CYBER_TOUCH_IRQ) {}

  // Inicia el touch. Llama antes a ui.begin(rotacion): usa sus medidas y su
  // rotación. Devuelve true si ya hay una calibración válida (guardada en
  // flash, o los valores de fábrica de la CYD en horizontal, rotación 1).
  // Si devuelve false, llama a calibrar().
  bool begin(CyberUI& ui) {
    ancho = ui.ancho;
    alto = ui.alto;
    rot = ui.rotacion;
    spi.begin(CYBER_TOUCH_CLK, CYBER_TOUCH_MISO, CYBER_TOUCH_MOSI, CYBER_TOUCH_CS);
    ts.begin(spi);
    ts.setRotation(rot);

    valoresPorDefecto();
    calibrado = (rot == 1);  // los valores por defecto solo son fiables en rotación 1
    if (cargarCalibracion()) calibrado = true;
    return calibrado;
  }

  // Lee el touch. Llamar UNA vez por vuelta de loop() (CyberGUI ya lo hace).
  const CyberToque& leer() {
    t.inicio = false;
    t.fin = false;
    const uint32_t ahora = millis();

    bool tocado = false;
    int16_t nx = 0, ny = 0;
    if (tocadoAhora()) {
      TS_Point p = ts.getPoint();
      if (p.z >= CYBER_TOUCH_Z_MIN) {
        tocado = true;
        nx = aplicar(ejeX, p.x, p.y, ancho);
        ny = aplicar(ejeY, p.x, p.y, alto);
      }
    }

    if (tocado) {
      ultimaSenal = ahora;
      if (!t.activo) {
        t.activo = true;
        t.inicio = true;
        t.x = nx;
        t.y = ny;
        t.desde = ahora;
      } else {
        t.x = (t.x + nx) / 2;  // suavizado sencillo
        t.y = (t.y + ny) / 2;
      }
    } else if (t.activo && (ahora - ultimaSenal) > CYBER_TOUCH_SOLTAR_MS) {
      t.activo = false;
      t.fin = true;
    }
    return t;
  }

  const CyberToque& actual() const { return t; }
  bool estaCalibrado() const { return calibrado; }

  //Calibración
  // Pega aquí lo que imprime calibrar() por el monitor serie para no tener
  // que calibrar en cada arranque (o usa la copia guardada en flash).
  void setCalibracion(bool xUsaY, float x0, float x1, float y0, float y1) {
    ejeX = {xUsaY, x0, x1};
    ejeY = {!xUsaY, y0, y1};
    calibrado = true;
  }

  // Asistente de 3 puntos: pide tocar 3 miras (esquina sup. izquierda, sup.
  // derecha e inf. derecha). Detecta solo si hay ejes intercambiados o
  // invertidos. Bloquea hasta terminar y guarda el resultado en flash.
  void calibrar(CyberUI& ui, bool guardar = true) {
    const int16_t m = 24;
    const int16_t px[3] = {m, (int16_t)(ancho - m), (int16_t)(ancho - m)};
    const int16_t py[3] = {m, m, (int16_t)(alto - m)};
    float rx[3], ry[3];

    while (true) {
      bool ok = true;
      for (int i = 0; i < 3 && ok; i++) ok = capturarPunto(ui, i, px[i], py[i], rx[i], ry[i]);
      if (!ok) continue;

      // Eje X de pantalla: cambia entre el punto 0 y el 1.
      const bool xUsaY = fabsf(ry[1] - ry[0]) > fabsf(rx[1] - rx[0]);
      // Eje Y de pantalla: cambia entre el punto 1 y el 2.
      const bool yUsaY = fabsf(ry[2] - ry[1]) > fabsf(rx[2] - rx[1]);
      const float ax = xUsaY ? ry[0] : rx[0], bx = xUsaY ? ry[1] : rx[1];
      const float by = yUsaY ? ry[1] : rx[1], cy = yUsaY ? ry[2] : rx[2];

      if (xUsaY == yUsaY || fabsf(bx - ax) < 300 || fabsf(cy - by) < 300) {
        mensaje(ui, "LECTURA INVALIDA", "INTENTA DE NUEVO");
        continue;
      }

      // Extrapola de las miras (a m px del borde) a los bordes 0 y dim-1.
      const float kx = (bx - ax) / (float)(px[1] - px[0]);
      const float ky = (cy - by) / (float)(py[2] - py[1]);
      ejeX = {xUsaY, ax + (0 - px[0]) * kx, ax + ((ancho - 1) - px[0]) * kx};
      ejeY = {yUsaY, by + (0 - py[1]) * ky, by + ((alto - 1) - py[1]) * ky};
      calibrado = true;
      break;
    }

    if (guardar) guardarCalibracion();
    Serial.printf("touch.setCalibracion(%s, %.1f, %.1f, %.1f, %.1f);\n",
                  ejeX.usaY ? "true" : "false", ejeX.raw0, ejeX.raw1, ejeY.raw0, ejeY.raw1);
    mensaje(ui, "CALIBRACION OK", "");
  }

  //Borra la calibración guardada en flash (vuelve a pedirla al arrancar).
  void borrarCalibracion() {
    Preferences p;
    if (p.begin("cyberui", false)) {
      p.clear();
      p.end();
    }
    valoresPorDefecto();
    calibrado = (rot == 1);
  }

 private:
  struct EjeCal {
    bool usaY;    // ¿este eje de pantalla se lee del eje Y crudo del touch?
    float raw0;   // valor crudo que corresponde al borde 0 de la pantalla
    float raw1;   // valor crudo que corresponde al borde opuesto (dim - 1)
  };

  SPIClass spi;
  XPT2046_Touchscreen ts;
  CyberToque t;
  uint32_t ultimaSenal = 0;
  int16_t ancho = 240, alto = 320;
  uint8_t rot = 0;
  bool calibrado = false;
  EjeCal ejeX{false, 200, 3700};
  EjeCal ejeY{true, 240, 3800};

  bool tocadoAhora() { return ts.tirqTouched() && ts.touched(); }

  //  Valores de fábrica de la CYD en horizontal (rotación 1).
  void valoresPorDefecto() {
    ejeX = {false, 200, 3700};
    ejeY = {true, 240, 3800};
  }

  static int16_t aplicar(const EjeCal& e, float rx, float ry, int16_t dim) {
    const float raw = e.usaY ? ry : rx;
    float s = (raw - e.raw0) * (dim - 1) / (e.raw1 - e.raw0);
    if (s < 0) s = 0;
    if (s > dim - 1) s = dim - 1;
    return (int16_t)(s + 0.5f);
  }

  bool cargarCalibracion() {
    Preferences p;
    if (!p.begin("cyberui", true)) return false;  // si aún no existe
    bool ok = p.getBool("cal", false) && p.getUShort("w", 0) == (uint16_t)ancho &&
              p.getUShort("h", 0) == (uint16_t)alto && p.getUChar("rot", 255) == rot;
    if (ok) {
      const bool xUsaY = p.getBool("xy", false);
      ejeX = {xUsaY, p.getFloat("x0", 200), p.getFloat("x1", 3700)};
      ejeY = {!xUsaY, p.getFloat("y0", 240), p.getFloat("y1", 3800)};
    }
    p.end();
    return ok;
  }

  void guardarCalibracion() {
    Preferences p;
    if (!p.begin("cyberui", false)) return;
    p.putBool("cal", true);
    p.putUShort("w", (uint16_t)ancho);
    p.putUShort("h", (uint16_t)alto);
    p.putUChar("rot", rot);
    p.putBool("xy", ejeX.usaY);
    p.putFloat("x0", ejeX.raw0);
    p.putFloat("x1", ejeX.raw1);
    p.putFloat("y0", ejeY.raw0);
    p.putFloat("y1", ejeY.raw1);
    p.end();
  }

  void mensaje(CyberUI& ui, const char* l1, const char* l2) {
    ui.fondo();
    ui.textoCentrado(l1, alto / 2 - 12, 2, Cyber::CIAN);
    ui.textoCentrado(l2, alto / 2 + 12, 1, Cyber::TENUE);
    delay(900);
  }

  // Dibuja una mira y espera un toque; promedia las lecturas crudas.
  bool capturarPunto(CyberUI& ui, int n, int16_t sx, int16_t sy, float& rx, float& ry) {
    ui.fondo();
    ui.textoCentrado("CALIBRAR TOUCH", alto / 2 - 20, 2, Cyber::CIAN);
    char msg[24];
    snprintf(msg, sizeof(msg), "TOCA LA MIRA %d DE 3", n + 1);
    ui.textoCentrado(msg, alto / 2 + 4, 1, Cyber::TENUE);
    TFT_eSPI& tft = ui.pantalla();
    tft.drawCircle(sx, sy, 10, Cyber::CIAN);
    tft.drawFastHLine(sx - 18, sy, 36, Cyber::MAGENTA);
    tft.drawFastVLine(sx, sy - 18, 36, Cyber::MAGENTA);
    tft.fillRect(sx - 1, sy - 1, 3, 3, Cyber::AMARILLO);

    while (tocadoAhora()) delay(10);  // que suelte el toque anterior
    delay(150);
    while (!tocadoAhora()) delay(10);

    float sumX = 0, sumY = 0;
    int cuenta = 0;
    const uint32_t t0 = millis();
    while (tocadoAhora() && (millis() - t0) < 1500) {
      TS_Point p = ts.getPoint();
      if (p.z >= CYBER_TOUCH_Z_MIN) {
        sumX += p.x;
        sumY += p.y;
        cuenta++;
      }
      delay(8);
    }
    if (cuenta < 5) return false;
    rx = sumX / cuenta;
    ry = sumY / cuenta;
    return true;
  }

  friend class CyberGUI;
};


//  Widgets
// Base de todos los widgets. Reglas comunes:
//  * `fondo` es el color de lo que hay DETRÁS del widget (por defecto el de
//    un panel). Si lo pones sobre el fondo de pantalla usa Cyber::FONDO.
//  * Cada widget se redibuja solo cuando cambia (bandera `sucio`).
//  * Las etiquetas son punteros: usa literales o buffers que sigan vivos.
class CyberWidget {
 public:
  int16_t x, y, w, h;
  uint16_t fondo = Cyber::PANEL;
  bool habilitado = true;   // deshabilitado: se ve apagado y no responde
  int8_t margen = 3;        // píxeles extra alrededor que también cuentan como toque
  bool sucio = true;        // pendiente de redibujar

  CyberWidget(int16_t x, int16_t y, int16_t w, int16_t h) : x(x), y(y), w(w), h(h) {}
  virtual ~CyberWidget() {}

  virtual void dibujar(CyberUI& ui) = 0;
  // Se llama en cada ciclo mientras el dedo está sobre este widget (y en el
  // ciclo de soltar). Los widgets lo usan para reaccionar.
  virtual void tocar(const CyberToque& t) = 0;

  bool contiene(int16_t px, int16_t py) const {
    return px >= x - margen && px < x + w + margen && py >= y - margen && py < y + h + margen;
  }
  void habilitar(bool v) {
    if (habilitado != v) {
      habilitado = v;
      sucio = true;
    }
  }
  void marcar() { sucio = true; }
};

//Botón
class CyberBoton : public CyberWidget {
 public:
  CyberBoton(int16_t x, int16_t y, int16_t w, int16_t h, const char* texto,
             uint16_t color = Cyber::CIAN)
      : CyberWidget(x, y, w, h), texto(texto), color(color) {}

  void setTexto(const char* t) {
    texto = t;
    sucio = true;
  }
  void setColor(uint16_t c) {
    color = c;
    sucio = true;
  }
  // Se ejecuta al soltar el dedo DENTRO del botón.
  void alPulsar(std::function<void()> f) { cbPulsar = f; }
  // Alternativa sin callbacks: true UNA vez tras cada pulsación.
  bool pulsado() {
    bool r = flag;
    flag = false;
    return r;
  }

  void dibujar(CyberUI& ui) override {
    const uint16_t col = habilitado ? color : Cyber::TENUE;
    const bool inv = presionado && habilitado;
    ui.formaCortada(x, y, w, h, inv ? col : fondo, col, fondo, 7);
    if (!inv) ui.pantalla().fillRect(x, y + 4, 3, h - 12, col);  // barra de acento
    const uint8_t tam = Cyber::tamAuto(strlen(texto), w - 14, 2);
    ui.textoEnCaja(texto, x, y, w, h, tam, inv ? Cyber::FONDO : col, tam >= 2 && !inv && habilitado);
  }

  void tocar(const CyberToque& t) override {
    if (t.inicio) presionado = true, sucio = true;
    if (t.activo) {
      const bool dentro = contiene(t.x, t.y);
      if (dentro != presionado) presionado = dentro, sucio = true;
    }
    if (t.fin) {
      if (presionado) {
        flag = true;
        if (cbPulsar) cbPulsar();
      }
      presionado = false;
      sucio = true;
    }
  }

 private:
  const char* texto;
  uint16_t color;
  bool presionado = false;
  bool flag = false;
  std::function<void()> cbPulsar;
};

//Checkbox
class CyberCheckbox : public CyberWidget {
 public:
  CyberCheckbox(int16_t x, int16_t y, int16_t w, int16_t h, const char* etiqueta,
                bool marcado = false, uint16_t color = Cyber::CIAN)
      : CyberWidget(x, y, w, h), etiqueta(etiqueta), color(color), valor(marcado) {}

  bool marcado() const { return valor; }
  // Cambia el valor desde el programa (NO dispara el callback).
  void poner(bool v) {
    if (valor != v) valor = v, sucio = true;
  }
  void alCambiar(std::function<void(bool)> f) { cb = f; }

  void dibujar(CyberUI& ui) override {
    TFT_eSPI& tft = ui.pantalla();
    tft.fillRect(x, y, w, h, fondo);
    const int16_t bs = 20, bx = x + 2, by = y + (h - bs) / 2;
    const uint16_t col = habilitado ? color : Cyber::TENUE;
    ui.formaCortada(bx, by, bs, bs, fondo, col, fondo, 5);
    if (valor) {
      // Marca con sombra magenta desplazada (efecto glitch)
      for (int pasada = 0; pasada < 2; pasada++) {
        const uint16_t c = pasada == 0 ? Cyber::MAGENTA : col;
        const int16_t o = pasada == 0 ? 1 : 0;
        for (int d = 0; d < 2; d++) {
          tft.drawLine(bx + 4 + d + o, by + 10 + o, bx + 8 + d + o, by + 14 + o, c);
          tft.drawLine(bx + 8 + d + o, by + 14 + o, bx + 15 + d + o, by + 5 + o, c);
        }
      }
    }
    const int16_t xt = bx + bs + 8;
    const uint8_t tam = Cyber::tamAuto(strlen(etiqueta), x + w - xt, 2);
    ui.texto(etiqueta, xt, y + (h - 8 * tam) / 2, tam,
             !habilitado ? Cyber::TENUE : (valor ? Cyber::BLANCO : Cyber::TENUE));
  }

  void tocar(const CyberToque& t) override {
    if (t.fin && contiene(t.x, t.y)) {
      valor = !valor;
      sucio = true;
      if (cb) cb(valor);
    }
  }

 private:
  const char* etiqueta;
  uint16_t color;
  bool valor;
  std::function<void(bool)> cb;
};

//Interruptor (ON / OFF)
class CyberInterruptor : public CyberWidget {
 public:
  CyberInterruptor(int16_t x, int16_t y, int16_t w, int16_t h, const char* etiqueta,
                   bool encendido = false, uint16_t color = Cyber::VERDE)
      : CyberWidget(x, y, w, h), etiqueta(etiqueta), color(color), valor(encendido) {}

  bool encendido() const { return valor; }
  void poner(bool v) {
    if (valor != v) valor = v, sucio = true;
  }
  void alCambiar(std::function<void(bool)> f) { cb = f; }

  void dibujar(CyberUI& ui) override {
    TFT_eSPI& tft = ui.pantalla();
    tft.fillRect(x, y, w, h, fondo);
    const int16_t sw = 46, sh = 22, sx = x + w - sw - 2, sy = y + (h - sh) / 2;
    const uint16_t col = !habilitado ? Cyber::TENUE : (valor ? color : Cyber::TENUE);

    const uint8_t tam = Cyber::tamAuto(strlen(etiqueta), sx - x - 8, 2);
    ui.texto(etiqueta, x + 2, y + (h - 8 * tam) / 2, tam, valor && habilitado ? Cyber::BLANCO : Cyber::TENUE);

    ui.formaCortada(sx, sy, sw, sh, fondo, col, fondo, 5);
    const int16_t kx = valor ? sx + sw - 4 - 20 : sx + 4;
    tft.fillRect(kx, sy + 4, 20, sh - 8, col);
    if (valor) ui.texto("ON", sx + 7, sy + 7, 1, col);
    else ui.texto("OFF", sx + 26, sy + 7, 1, col);
  }

  void tocar(const CyberToque& t) override {
    if (t.fin && contiene(t.x, t.y)) {
      valor = !valor;
      sucio = true;
      if (cb) cb(valor);
    }
  }

 private:
  const char* etiqueta;
  uint16_t color;
  bool valor;
  std::function<void(bool)> cb;
};

//Slider (arrastrar)
// Alto recomendado: 34 px. Etiqueta y valor arriba, barra segmentada abajo.
class CyberSlider : public CyberWidget {
 public:
  CyberSlider(int16_t x, int16_t y, int16_t w, int16_t h, int minimo, int maximo, int valor,
              const char* etiqueta = nullptr, uint16_t color = Cyber::CIAN,
              const char* unidad = "", int paso = 1)
      : CyberWidget(x, y, w, h), minimo(minimo), maximo(maximo), valor_(valor),
        etiqueta(etiqueta), unidad(unidad), color(color), paso(paso > 0 ? paso : 1) {
    margen = 8;  // más fácil de agarrar
  }

  int valor() const { return valor_; }
  void poner(int v) {
    v = limitar(v);
    if (v != valor_) valor_ = v, sucio = true;
  }
  // Se llama cada vez que el valor cambia mientras arrastras.
  void alCambiar(std::function<void(int)> f) { cb = f; }

  void dibujar(CyberUI& ui) override {
    TFT_eSPI& tft = ui.pantalla();
    tft.fillRect(x, y, w, h, fondo);
    const uint16_t col = habilitado ? color : Cyber::TENUE;

    if (etiqueta) ui.texto(etiqueta, x + 2, y, 1, Cyber::TENUE);
    char buf[24];
    snprintf(buf, sizeof(buf), "%d%s", valor_, unidad);
    ui.textoDerecha(buf, x + w - 2, y, 1, col);

    const int16_t ty = y + 14, th = h - 16;
    const int n = w / 8;
    const long rango = (long)maximo - minimo;
    const int llenos = rango > 0 ? (int)(((long)(valor_ - minimo) * n + rango / 2) / rango) : 0;
    for (int i = 0; i < n; i++) {
      tft.fillRect(x + i * 8, ty + 4, 6, th - 8, i < llenos ? col : Cyber::SEG_VACIO);
    }
    const int16_t mx = x + (rango > 0 ? (int16_t)(((long)(valor_ - minimo) * (w - 6)) / rango) : 0);
    tft.fillRect(mx + 1, ty - 1, 6, th + 2, col);       // marcador con sombra glitch
    tft.fillRect(mx, ty - 1, 6, th + 2, Cyber::MAGENTA);
    tft.fillRect(mx + 2, ty + 1, 2, th - 2, Cyber::BLANCO);
  }

  void tocar(const CyberToque& t) override {
    if (!t.activo) return;  // (inicio o mantener; el dedo puede salirse por arriba/abajo)
    const long rango = (long)maximo - minimo;
    long v = minimo + ((long)(t.x - x - 3) * rango + (w - 6) / 2) / (w - 6);
    v = minimo + ((v - minimo + paso / 2) / paso) * paso;
    poner((int)v, true);
  }

 private:
  int minimo, maximo, valor_;
  const char* etiqueta;
  const char* unidad;
  uint16_t color;
  int paso;
  std::function<void(int)> cb;

  int limitar(int v) const { return v < minimo ? minimo : (v > maximo ? maximo : v); }
  void poner(int v, bool avisar) {
    v = limitar(v);
    if (v != valor_) {
      valor_ = v;
      sucio = true;
      if (avisar && cb) cb(valor_);
    }
  }
};

//Selector (opciones excluyentes, tipo pestañas)
class CyberSelector : public CyberWidget {
 public:
  CyberSelector(int16_t x, int16_t y, int16_t w, int16_t h, const char* const* opciones,
                uint8_t cantidad, uint8_t seleccion = 0, uint16_t color = Cyber::CIAN)
      : CyberWidget(x, y, w, h), opciones(opciones), n(cantidad), sel(seleccion), color(color) {}

  uint8_t seleccion() const { return sel; }
  void poner(uint8_t i) {
    if (i < n && i != sel) sel = i, sucio = true;
  }
  void alCambiar(std::function<void(uint8_t)> f) { cb = f; }

  void dibujar(CyberUI& ui) override {
    TFT_eSPI& tft = ui.pantalla();
    tft.fillRect(x, y, w, h, fondo);
    const uint16_t col = habilitado ? color : Cyber::TENUE;
    const int16_t sw = w / n;
    size_t maxLen = 1;
    for (uint8_t i = 0; i < n; i++) {
      const size_t l = strlen(opciones[i]);
      if (l > maxLen) maxLen = l;
    }
    const uint8_t tam = Cyber::tamAuto(maxLen, sw - 8, 2);

    for (uint8_t i = 0; i < n; i++) {
      const int16_t sx = x + i * sw;
      const int16_t ancho = (i == n - 1) ? (w - i * sw) : sw - 2;
      if (i == sel) {
        tft.fillRect(sx, y, ancho, h, col);
        ui.textoEnCaja(opciones[i], sx, y, ancho, h, tam, Cyber::FONDO);
      } else {
        tft.drawRect(sx, y, ancho, h, Cyber::SEG_VACIO);
        ui.textoEnCaja(opciones[i], sx, y, ancho, h, tam, Cyber::TENUE);
      }
    }
  }

  void tocar(const CyberToque& t) override {
    if (!t.fin || !contiene(t.x, t.y)) return;
    int i = (t.x - x) / (w / n);
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    if (i != sel) {
      sel = (uint8_t)i;
      sucio = true;
      if (cb) cb(sel);
    }
  }

 private:
  const char* const* opciones;
  uint8_t n;
  uint8_t sel;
  uint16_t color;
  std::function<void(uint8_t)> cb;
};

//Stepper  [-]  valor  [+]  (con autorrepetición al mantener)
class CyberStepper : public CyberWidget {
 public:
  CyberStepper(int16_t x, int16_t y, int16_t w, int16_t h, int minimo, int maximo, int valor,
               int paso = 1, const char* unidad = "", uint16_t color = Cyber::CIAN)
      : CyberWidget(x, y, w, h), minimo(minimo), maximo(maximo), valor_(valor), paso(paso),
        unidad(unidad), color(color) {}

  int valor() const { return valor_; }
  void poner(int v) {
    v = v < minimo ? minimo : (v > maximo ? maximo : v);
    if (v != valor_) valor_ = v, sucio = true;
  }
  void alCambiar(std::function<void(int)> f) { cb = f; }

  void dibujar(CyberUI& ui) override {
    TFT_eSPI& tft = ui.pantalla();
    tft.fillRect(x, y, w, h, fondo);
    const uint16_t col = habilitado ? color : Cyber::TENUE;
    const int16_t bw = h;

    for (int lado = -1; lado <= 1; lado += 2) {
      const int16_t bx = lado < 0 ? x : x + w - bw;
      const bool pres = (lado_ == lado) && habilitado;
      ui.formaCortada(bx, y, bw, h, pres ? col : fondo, col, fondo, 5);
      const uint16_t tinta = pres ? Cyber::FONDO : col;
      const int16_t cx = bx + bw / 2, cy = y + h / 2;
      tft.fillRect(cx - 6, cy - 1, 12, 3, tinta);              // "-"
      if (lado > 0) tft.fillRect(cx - 1, cy - 6, 3, 12, tinta);  // "+"
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "%d%s", valor_, unidad);
    const uint8_t tam = Cyber::tamAuto(strlen(buf), w - 2 * bw - 6, 2);
    ui.textoEnCaja(buf, x + bw, y, w - 2 * bw, h, tam, col, habilitado);
  }

  void tocar(const CyberToque& t) override {
    if (t.inicio) {
      lado_ = t.x < x + h ? -1 : (t.x >= x + w - h ? 1 : 0);
      ultimo = t.desde;
      if (lado_) mover(lado_);
      sucio = true;
    } else if (t.activo && lado_) {
      const uint32_t ahora = millis();
      if (ahora - t.desde > 450 && ahora - ultimo > 110) {  // autorrepetición
        mover(lado_);
        ultimo = ahora;
      }
    }
    if (t.fin) lado_ = 0, sucio = true;
  }

 private:
  int minimo, maximo, valor_, paso;
  const char* unidad;
  uint16_t color;
  int8_t lado_ = 0;
  uint32_t ultimo = 0;
  std::function<void(int)> cb;

  void mover(int dir) {
    int v = valor_ + dir * paso;
    v = v < minimo ? minimo : (v > maximo ? maximo : v);
    if (v != valor_) {
      valor_ = v;
      sucio = true;
      if (cb) cb(valor_);
    }
  }
};

//CyberGUI -- administra los widgets de la pantalla actual
class CyberGUI {
 public:
  CyberGUI(CyberUI& ui, CyberTouch& touch) : ui(ui), touch(touch) {}

  // Registra un widget (el objeto debe seguir vivo: globales o static).
  // Devuelve false si ya se alcanzó CYBER_MAX_WIDGETS.
  bool agregar(CyberWidget& w) {
    if (n >= CYBER_MAX_WIDGETS) return false;
    lista[n++] = &w;
    w.sucio = true;
    return true;
  }

  // Olvida todos los widgets (úsalo al cambiar de pantalla).
  void limpiar() {
    n = 0;
    activo = nullptr;
  }

  //Dibuja todos los widgets registrados (después de pintar paneles y fondo).
  void dibujarTodo() {
    for (uint8_t i = 0; i < n; i++) {
      lista[i]->dibujar(ui);
      lista[i]->sucio = false;
    }
  }

  //Llamar en CADA vuelta de loop(): lee el touch, reparte los toques y
  // redibuja solo lo que cambió. Devuelve true si algún widget recibió el toque.
  bool actualizar() {
    const CyberToque& t = touch.leer();
    bool manejado = false;

    if (t.inicio) {
      activo = nullptr;
      for (int i = (int)n - 1; i >= 0; i--) {  // el último agregado queda "encima"
        if (lista[i]->habilitado && lista[i]->contiene(t.x, t.y)) {
          activo = lista[i];
          break;
        }
      }
    }
    if (activo && (t.activo || t.fin)) {
      activo->tocar(t);
      manejado = true;
    }
    if (t.fin) activo = nullptr;

    for (uint8_t i = 0; i < n; i++) {
      if (lista[i]->sucio) {
        lista[i]->dibujar(ui);
        lista[i]->sucio = false;
      }
    }
    return manejado;
  }

  const CyberToque& toque() const { return touch.actual(); }

 private:
  CyberUI& ui;
  CyberTouch& touch;
  CyberWidget* lista[CYBER_MAX_WIDGETS];
  uint8_t n = 0;
  CyberWidget* activo = nullptr;
};
