
//  Demo_Tactil: todos los widgets de CyberTouch en 3 pantallas (CYD,
//  vertical). Cambias de pantalla con el selector de arriba.
//
//   Por: Alejandro Varela SciFi  ->  @alejandro.varela.scifi
//   github.com/FelonyFelyne
//
//    CONTROL  checkboxes (LED RGB de la CYD) + botones TODO ON / TODO OFF
//    AJUSTES  slider, stepper, interruptor y selector de modo
//    SISTEMA  botones: recalibrar touch y reiniciar
//  
//    LIBRERÍA REQUERIDA:
//    Necesita: - TFT_eSPI (by Bodmer) (User_Setup.h de la CYD) 
//              - XPT2046  Touchscreen.

#include <TFT_eSPI.h>
#include <CyberUI.h>
#include <CyberTouch.h>

// LED RGB de la CYD (activo en LOW). Si tu placa no lo tiene, ignóralo.
#define LED_R 4
#define LED_G 16
#define LED_B 17

TFT_eSPI tft;
CyberUI ui(tft);
CyberTouch touch;
CyberGUI gui(ui, touch);

//Widgets (globales: tienen que seguir vivos)
// Coordenadas pensadas para 240x320 vertical. Paneles: x=4, ancho 232.
static const char* PAGINAS[] = {"CONTROL", "AJUSTES", "SISTEMA"};
CyberSelector selPagina(4, 58, 232, 30, PAGINAS, 3, 0);

// Pantalla CONTROL
CyberCheckbox cbRojo(16, 122, 208, 26, "LED ROJO", false, Cyber::ROJO);
CyberCheckbox cbVerde(16, 150, 208, 26, "LED VERDE", false, Cyber::VERDE);
CyberCheckbox cbAzul(16, 178, 208, 26, "LED AZUL", false, Cyber::CIAN);
CyberBoton btnOn(16, 252, 100, 34, "TODO ON", Cyber::VERDE);
CyberBoton btnOff(124, 252, 100, 34, "TODO OFF", Cyber::ROJO);

// Pantalla AJUSTES
CyberSlider slBrillo(16, 122, 208, 34, 0, 100, 60, "BRILLO", Cyber::CIAN, "%", 5);
CyberStepper stTiempo(16, 166, 208, 30, 5, 60, 15, 5, " min");
CyberInterruptor swNoche(16, 232, 208, 28, "MODO NOCHE", false, Cyber::MAGENTA);
static const char* MODOS[] = {"ECO", "NORMAL", "MAX"};
CyberSelector selModo(16, 264, 208, 26, MODOS, 3, 1, Cyber::AMARILLO);

// Pantalla SISTEMA
CyberBoton btnCal(16, 130, 208, 40, "RECALIBRAR TOUCH", Cyber::CIAN);
CyberBoton btnReiniciar(16, 190, 208, 40, "REINICIAR", Cyber::ROJO);

uint8_t pagina = 0;
bool reconstruir = true;   // se activa en los callbacks; se atiende en loop()

static void ponerLed(int pin, bool encendido) { digitalWrite(pin, encendido ? LOW : HIGH); }


//Dibujo de cada pantalla
static void construirPagina() {
  const int16_t px = 4, pw = 232;
  gui.limpiar();
  ui.fondo();
  ui.encabezado("CYBER-GUI", pagina == 0 ? "//DEMO.CONTROL" : pagina == 1 ? "//DEMO.AJUSTES" : "//DEMO.SISTEMA");

  selPagina.fondo = Cyber::FONDO;   // este widget va directo sobre el fondo
  gui.agregar(selPagina);

  if (pagina == 0) {
    ui.panel(px, 96, pw, 118, Cyber::CIAN, "// LED_RGB");
    ui.panel(px, 222, pw, 72, Cyber::MAGENTA, "// ATAJOS");
    gui.agregar(cbRojo);
    gui.agregar(cbVerde);
    gui.agregar(cbAzul);
    gui.agregar(btnOn);
    gui.agregar(btnOff);
  } else if (pagina == 1) {
    ui.panel(px, 96, pw, 108, Cyber::CIAN, "// PARAMETROS");
    ui.panel(px, 212, pw, 84, Cyber::MAGENTA, "// SISTEMA");
    gui.agregar(slBrillo);
    gui.agregar(stTiempo);
    gui.agregar(swNoche);
    gui.agregar(selModo);
  } else {
    ui.panel(px, 96, pw, 158, Cyber::CIAN, "// MANTENIMIENTO");
    gui.agregar(btnCal);
    gui.agregar(btnReiniciar);
  }

  ui.pie(">> TOUCH DEMO", "v2.0", pagina, 3);
  gui.dibujarTodo();
}



void setup() {
  Serial.begin(115200);
  pinMode(LED_R, OUTPUT); pinMode(LED_G, OUTPUT); pinMode(LED_B, OUTPUT);
  ponerLed(LED_R, false); ponerLed(LED_G, false); ponerLed(LED_B, false);

  ui.begin(0);                                   // vertical
  ui.splash("CYBER-GUI", "> INICIANDO TOUCH_");
  if (!touch.begin(ui)) touch.calibrar(ui);      // solo la primera vez (se guarda en flash)

//Callbacks
  // Regla: dentro de un callback NO reconstruyas la pantalla; levanta una
  // bandera y hazlo en loop() (aquí: `reconstruir`).
  selPagina.alCambiar([](uint8_t i) { pagina = i; reconstruir = true; });

  cbRojo.alCambiar([](bool v) { ponerLed(LED_R, v); });
  cbVerde.alCambiar([](bool v) { ponerLed(LED_G, v); });
  cbAzul.alCambiar([](bool v) { ponerLed(LED_B, v); });
  btnOn.alPulsar([]() {
    cbRojo.poner(true); cbVerde.poner(true); cbAzul.poner(true);   // poner() no dispara callbacks
    ponerLed(LED_R, true); ponerLed(LED_G, true); ponerLed(LED_B, true);
  });
  btnOff.alPulsar([]() {
    cbRojo.poner(false); cbVerde.poner(false); cbAzul.poner(false);
    ponerLed(LED_R, false); ponerLed(LED_G, false); ponerLed(LED_B, false);
  });

  slBrillo.alCambiar([](int v) { Serial.printf("brillo = %d\n", v); });
  stTiempo.alCambiar([](int v) { Serial.printf("tiempo = %d min\n", v); });
  swNoche.alCambiar([](bool v) { Serial.printf("modo noche = %d\n", v); });
  selModo.alCambiar([](uint8_t i) { Serial.printf("modo = %s\n", MODOS[i]); });

  btnCal.alPulsar([]() { touch.calibrar(ui); reconstruir = true; });
  btnReiniciar.alPulsar([]() { ESP.restart(); });
}

void loop() {
  gui.actualizar();
  if (reconstruir) {
    reconstruir = false;
    construirPagina();
  }
  delay(5);
}
