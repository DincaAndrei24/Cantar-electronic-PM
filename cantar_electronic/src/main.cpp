#include <HX711_ADC.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <avr/io.h>
#include <avr/interrupt.h> 

const int HX711_dout = 2; 
const int HX711_sck  = 3; 
const float FACTOR_CALIBRARE_FIX = 22626.79; 
const int PRAG_BUTON_APASAT = 1000;

HX711_ADC LoadCell(HX711_dout, HX711_sck);
LiquidCrystal_I2C lcd(0x27, 16, 2); 

enum StareSistem { STARE_NORMAL, STARE_TARA, STARE_SALVARE, STARE_TREZIRE };
StareSistem stareCurenta = STARE_NORMAL;

unsigned long timpAnteriorAfisare = 0;
unsigned long timpMesajTemporar = 0;
unsigned long timpOprireBuzzer = 0;
unsigned long timpUltimulDebounce = 0;

int pretKilogram = 0;              
boolean modulSleep = false;        
boolean tastaApasataAnterior = false;

volatile boolean buzzerActiv = false; 
volatile unsigned long sistemTimpMs = 0;
volatile uint8_t contorJumatatiMs = 0;

ISR(TIMER2_COMPA_vect) {
    if (buzzerActiv) {
        PIND |= (1 << PIND5);
    }

    contorJumatatiMs++;
    if (contorJumatatiMs >= 2) {
        sistemTimpMs++;
        contorJumatatiMs = 0;
    }
}


unsigned long citesteTimpulNostru() {
    unsigned long timpCurent;
    cli();
    timpCurent = sistemTimpMs;
    sei();
    return timpCurent;
}

void emiteBeepNonBlocant(int durataMs) {
    buzzerActiv = true;
    timpOprireBuzzer = citesteTimpulNostru() + durataMs;
}

void opresteBuzzerHardware() {
    buzzerActiv = false;
    PORTD &= ~(1 << PORTD5); 
}

void USART_init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void USART_transmitChar(unsigned char data) {
    while (!(UCSR0A & (1 << UDRE0))); 
    UDR0 = data;
}

void USART_print(const char* str) {
    while (*str) USART_transmitChar(*str++);
}

void USART_printFloat(float val, int zecimale) {
    char buffer[10];
    dtostrf(val, 4, zecimale, buffer);
    USART_print(buffer);
}

void USART_printInt(int val) {
    char buffer[7];
    itoa(val, buffer, 10);
    USART_print(buffer);
}

void ADC_init() {
    ADMUX = (1 << REFS0); 
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); 
}

uint16_t ADC_read() {
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

int citesteTastaADC() {
    unsigned long timpCurent = citesteTimpulNostru(); 
    uint16_t valoareBruta = ADC_read();
    
    if (valoareBruta > PRAG_BUTON_APASAT) {
        if (tastaApasataAnterior) {
            timpUltimulDebounce = timpCurent; 
            tastaApasataAnterior = false;
        }
        return -1; 
    }
    
    if (!tastaApasataAnterior && (timpCurent - timpUltimulDebounce > 200)) { 
        tastaApasataAnterior = true;
        
        int butonFizic = 0;
        if (valoareBruta <= 20)                        butonFizic = 1;
        else if (valoareBruta >= 70  && valoareBruta <= 120)  butonFizic = 2;
        else if (valoareBruta >= 150 && valoareBruta <= 195)  butonFizic = 3;
        else if (valoareBruta >= 215 && valoareBruta <= 260)  butonFizic = 4;
        else if (valoareBruta >= 275 && valoareBruta <= 315)  butonFizic = 5;
        else if (valoareBruta >= 325 && valoareBruta <= 365)  butonFizic = 6;
        else if (valoareBruta >= 370 && valoareBruta <= 405)  butonFizic = 7;
        else if (valoareBruta >= 410 && valoareBruta <= 445)  butonFizic = 8;
        else if (valoareBruta >= 446 && valoareBruta <= 475)  butonFizic = 9;
        else if (valoareBruta >= 476 && valoareBruta <= 505)  butonFizic = 10;
        else if (valoareBruta >= 506 && valoareBruta <= 535)  butonFizic = 11;
        else if (valoareBruta >= 536 && valoareBruta <= 555)  butonFizic = 12;
        else if (valoareBruta >= 556 && valoareBruta <= 575)  butonFizic = 13;
        else if (valoareBruta >= 576 && valoareBruta <= 600)  butonFizic = 14;

        if (butonFizic >= 1 && butonFizic <= 9) return butonFizic; 
        if (butonFizic == 10) return 0;   
        if (butonFizic == 11) return 88;  
        if (butonFizic == 12) return 99;  
        if (butonFizic == 13) return 77;  
        if (butonFizic == 14) return 55;  
    }
    return -1;
}

void setup() {
  USART_init(103); 
  ADC_init(); 

  DDRD |= (1 << DDD4) | (1 << DDD5);
  PORTD &= ~(1 << PORTD4); 
  PORTD &= ~(1 << PORTD5);


  TCCR2A = (1 << WGM21);
  TCCR2B = (1 << CS22);
  OCR2A = 124;
  TIMSK2 |= (1 << OCIE2A);
  sei();

  emiteBeepNonBlocant(80);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Cantar Inteligent");
  lcd.setCursor(0, 1);
  lcd.print("Auto-Tara...");

  LoadCell.begin();
  LoadCell.start(2000, true); 
  LoadCell.setCalFactor(FACTOR_CALIBRARE_FIX);
  
  PORTD |= (1 << PORTD4); 
  lcd.clear();
  
  emiteBeepNonBlocant(100);
}


void loop() {
  unsigned long timpCurent = citesteTimpulNostru();

  if (buzzerActiv && timpCurent >= timpOprireBuzzer) {
      opresteBuzzerHardware();
  }

  if (stareCurenta != STARE_NORMAL && timpCurent >= timpMesajTemporar) {
      stareCurenta = STARE_NORMAL;
      lcd.clear();
  }

  if (!modulSleep) {
    LoadCell.update();
  }

  if (!modulSleep && stareCurenta == STARE_NORMAL) {
    if (timpCurent - timpAnteriorAfisare > 200) {
      float greutate = LoadCell.getData();
      if (greutate < 0.03 && greutate > -0.03) greutate = 0.00;

      float pretTotal = greutate * pretKilogram;
      if (pretTotal < 0) pretTotal = 0.00;

      lcd.setCursor(0, 0);
      lcd.print(greutate, 2);
      lcd.print(" kg             ");

      lcd.setCursor(0, 1);
      lcd.print("P:");
      lcd.print(pretKilogram);
      lcd.print(" T:");
      lcd.print(pretTotal, 2);
      lcd.print("        "); 
      
      timpAnteriorAfisare = timpCurent;
    }
  }

  int tasta = citesteTastaADC();
  if (tasta != -1) {
    emiteBeepNonBlocant(60); 

    if (modulSleep) {
      if (tasta == 55) {
        modulSleep = false;
        stareCurenta = STARE_TREZIRE;
        timpMesajTemporar = timpCurent + 800;
        
        lcd.backlight();
        lcd.clear();
        lcd.print("Trezire sistem..");
        LoadCell.tareNoDelay(); 
      }
    } 
    else {
      if (tasta >= 0 && tasta <= 9) {
        if (pretKilogram < 10) { 
          pretKilogram = (pretKilogram * 10) + tasta;
        }
      } 
      else if (tasta == 88) {
        stareCurenta = STARE_TARA;
        timpMesajTemporar = timpCurent + 600;
        lcd.clear();
        lcd.print("Setare Tara...");
        LoadCell.tareNoDelay();
      }
      else if (tasta == 99) {
        pretKilogram = 0;
      }
      else if (tasta == 77) {
        float greutateSalvare = LoadCell.getData();
        if (greutateSalvare < 0.03 && greutateSalvare > -0.03) greutateSalvare = 0.00;
        float totalSalvare = greutateSalvare * pretKilogram;
        if (totalSalvare < 0) totalSalvare = 0.00;

        stareCurenta = STARE_SALVARE;
        timpMesajTemporar = timpCurent + 1200;
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Trimitere Date..");
        lcd.setCursor(0, 1);
        lcd.print("P: "); lcd.print(pretKilogram);
        lcd.print(" Total: "); lcd.print(totalSalvare, 2);

        USART_print("-------------------------\r\n");
        USART_print("PRODUS SALVAT -> ");
        USART_printFloat(greutateSalvare, 2);
        USART_print(" kg | ");
        USART_printInt(pretKilogram);
        USART_print(" RON/kg | TOTAL: ");
        USART_printFloat(totalSalvare, 2);
        USART_print(" RON\r\n");
        USART_print("-------------------------\r\n");
      }
      else if (tasta == 55) {
        modulSleep = true;
        lcd.clear();
        lcd.noBacklight(); 
      }
    }
  }
}