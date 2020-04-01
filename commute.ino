#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <DHT.h>
#include <math.h>
#include <Keypad.h>


// Board requirements
//
// Total:               4 PWM Pins, 15 Digital Pins
//
// LED:                 3 Pin(s) (ideally PWM)
// DHT22 Temp/Humidity: 1 Pin(s) (PWM)
// Buttons:             3 Pin(s)
// Keypad:              8 Pin(s)
// MicroSD:             4 Pin(s)
// LCD: SCL/SCA
// DS3231 Date/Time: SCL/SCA


// LED
const int BLUE_LED_PIN = 2;
const int GREEN_LED_PIN = 3;
const int RED_LED_PIN = 4;

// Buttons
const int GREEN_BUTTON_PIN = 5;
const int RED_BUTTON_PIN = 6;
const int BLACK_BUTTON_PIN = 7;
int priorButtonValue[8];

// Temperature and Humidity Sensor
const int DHT22_PIN = 8;
DHT dht22(DHT22_PIN, DHT22);
int temperatureLcd;
int humidityLcd;

// LCD
LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 20, 4);

// Date/Time Sensor
RTC_DS3231 ds3231;
char dateTimeLcd[21];

// Keypad
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {{ '1', '2', '3', 'A' },
                         { '4', '5', '6', 'B' },
                         { '7', '8', '9', 'C' },
                         { '*', '0', '#', 'D' }};
byte rowPins[ROWS] = { 38, 39, 40, 41 };
byte colPins[COLS] = { 42, 43, 44, 45 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);


// States of the application
const int STOPPED = 1;
const int RECORDING = 2;
const int PAUSED = 3;
int state = STOPPED;


// State transitions of the application
const int NONE = 0;
const int START = 1;
const int PAUSE = 2;
const int RESUME = 3;
const int CANCEL = 4;
const int FINISH = 5;
int recentTransition = NONE;


char keypadEntry[3];
char keypadEntryIndex = 0;

const int INIT = 0;
const int CADEN = 1;
const int DAD = 2;
const int COMPLETE = 3;
int guessState = INIT;

int cadenGuess;
int dadGuess;
char guessLcd[21];


int loopDelay = 10;
int aggregateLoopDelay = 0;


uint32_t startTime;
uint32_t endTime;
uint32_t elapsedMilliseconds;
char elapsedTimeLcd[21];

uint32_t pauseStartTime;
uint32_t aggregatePauseTime;


void setup() {
    Serial.begin(9600);

    // Initialize the LCD
    lcd.init();
    lcd.backlight();

    // Initialize rtc library
    ds3231.begin();
    if (ds3231.lostPower()) {
        // Use compilation date/time to reset
        Serial.println("RTC lost power.");
        ds3231.adjust(DateTime(F(__DATE__), F(__TIME__)));
    } else {
        ds3231.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // Initialize the DHT22 temperature and humidity sensor
    dht22.begin();

    // Initialize the buttons
    pinMode(GREEN_BUTTON_PIN, INPUT_PULLUP);
    pinMode(RED_BUTTON_PIN, INPUT_PULLUP);
    pinMode(BLACK_BUTTON_PIN, INPUT_PULLUP);

    // Initialize the LED
    pinMode(BLUE_LED_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(RED_LED_PIN, OUTPUT);

    // Initialize the buttons to unpressed
    priorButtonValue[BLUE_LED_PIN] = HIGH;
    priorButtonValue[GREEN_LED_PIN] = HIGH;
    priorButtonValue[RED_LED_PIN] = HIGH;

    // Initial delay
    delay(500);
}


void loop() {


    switch (state) {


    case STOPPED:

        switch (getSelectedButton()) {

        case GREEN_BUTTON_PIN:
            start();
            break;

        default:
            switch (recentTransition) {
            case NONE:
                red();
                break;
            case FINISH:
                toggleLed(HIGH, LOW, LOW);
                break;
            case CANCEL:
                toggleLed(LOW, LOW, HIGH);
                break;
            }
        }

        break;


    case RECORDING:

        switch (getSelectedButton()) {

        case GREEN_BUTTON_PIN:
            pause();
            break;

        case BLACK_BUTTON_PIN:
            cancel();
            break;

        case RED_BUTTON_PIN:
            finish();
            break;

        default:
            // Every second, get the elapsed recording time
            if (seconds(1)) {
                getElapsedTime();
            }

            // Set the LED
            switch (recentTransition) {
            case NONE:
                green();
                break;
            case START:
            case RESUME:
                toggleLed(LOW, HIGH, LOW);
                break;
            }

            if (guessState > INIT) {
                getGuesses();
            }
            break;
        }

        break;


    case PAUSED:

        switch (getSelectedButton()) {

        case GREEN_BUTTON_PIN:
            resume();
            break;

        case BLACK_BUTTON_PIN:
            resume();
            cancel();
            break;

        case RED_BUTTON_PIN:
            resume();
            finish();
            break;

        default:
            // Set the LED
            switch (recentTransition) {
            case NONE:
                yellow();
                break;
            case PAUSE:
                toggleLed(HIGH, HIGH, LOW);
                break;
            }

            if (guessState > INIT) {
                getGuesses();
            }
            break;
        }

        break;
    }


    // Every 1 second, get the date/time, and update the LCD
    // Every 5 seconds, get the date/time, temp/humdity, and update the LCD
    if (seconds(1)) {
        getDateTime();
        if (seconds(5)) {
            getTemperatureHumidity();
        }
        updateLcd();
    }


    delay(loopDelay);
    aggregateLoopDelay += loopDelay;
    if (aggregateLoopDelay > 5000) {
        aggregateLoopDelay = 0;
        recentTransition = NONE;
    }
}


void resetGuess() {
    guessState = CADEN;
    keypadEntryIndex = 0;
    sprintf(keypadEntry, "  ");
}


void getGuesses() {

    char ch = keypad.getKey();
    if (ch) {
        switch (ch) {

        case '#':
            if (keypadEntryIndex < 2) {
                Serial.println("ERROR: Finish entering guess");
                break;
            }

            if (guessState == CADEN) {
                cadenGuess = atoi(keypadEntry);
                guessState = DAD;
            } else {
                dadGuess = atoi(keypadEntry);
                guessState = COMPLETE;
            }

            keypadEntryIndex = 0;
            sprintf(keypadEntry, "  ");
            Serial.print("cadenGuess: ");
            Serial.print(cadenGuess);
            Serial.print("  dadGuess: ");
            Serial.print(dadGuess);
            Serial.print("  keypadEntry: [");
            Serial.print(keypadEntry);
            Serial.println("]");
            break;

        case '*':
            if (keypadEntryIndex > 0) {
                Serial.print("keypadEntry before: [");
                Serial.print(keypadEntry);
                Serial.print("]");
                keypadEntryIndex--;
                keypadEntry[keypadEntryIndex] = ' ';
                Serial.print("  keypadEntry after: [");
                Serial.print(keypadEntry);
                Serial.println("]");
            } else {
                Serial.println("ERROR: Nothing to delete");
            }
            break;

        case 'A':
            resetGuess();
            break;

        default:
            if (keypadEntryIndex < 2) {
                keypadEntry[keypadEntryIndex] = ch;
                keypadEntryIndex++;
                Serial.print("keypadEntry: [");
                Serial.print(keypadEntry);
                Serial.println("]");
            } else {
                Serial.println("ERROR: Entry ignored, use * or #");
            }
            break;
        }
    }


    switch (guessState) {

    case CADEN:
        sprintf(guessLcd, "Caden: [%-2s]        ", keypadEntry);
        break;
    case DAD:
        sprintf(guessLcd, "Caden: %2d Dad: [%-2s]", cadenGuess, keypadEntry);
        break;
    case COMPLETE:
        sprintf(guessLcd, "Caden: %2d Dad: %02d  ", cadenGuess, dadGuess);
    }
}


void start() {
    Serial.println("start...");
    recentTransition = START;
    state = RECORDING;
    startTime = millis();
    aggregatePauseTime = 0;
    pauseStartTime = 0;
    aggregateLoopDelay = 0;
    guessState = CADEN;
}


void pause() {
    Serial.println("pause...");
    recentTransition = PAUSE;
    state = PAUSED;
    pauseStartTime = millis();
    aggregateLoopDelay = 0;
}


void resume() {
    Serial.println("resumed...");
    recentTransition = RESUME;
    state = RECORDING;
    uint32_t pauseEndTime = millis();
    aggregatePauseTime += pauseEndTime - pauseStartTime;
    aggregateLoopDelay = 0;
}


void cancel() {
    Serial.println("cancel...");
    recentTransition = CANCEL;
    state = STOPPED;
    aggregateLoopDelay = 0;
    resetGuess();
}


void finish() {
    Serial.println("finish...");
    recentTransition = FINISH;
    state = STOPPED;
    // Write information to the MicroSD Card here ...
    aggregateLoopDelay = 0;
    resetGuess();
}


int getSelectedButton() {
    if (wasButtonJustPushed(RED_BUTTON_PIN)) {
        return RED_BUTTON_PIN;
    }

    if (wasButtonJustPushed(GREEN_BUTTON_PIN)) {
        return GREEN_BUTTON_PIN;
    }

    if (wasButtonJustPushed(BLACK_BUTTON_PIN)) {
        return BLACK_BUTTON_PIN;
    }

    return 0;
}


boolean wasButtonJustPushed(int buttonPin) {

    // Read the current value of the button
    int buttonValue = digitalRead(buttonPin);

    // The button was previously up, and is still up, it is unchanged
    if (buttonValue == HIGH && priorButtonValue[buttonPin] == HIGH) {
        return false;
    }

    // The button was previously down, and is still down, it is unchanged
    if (buttonValue == LOW && priorButtonValue[buttonPin] == LOW) {
        return false;
    }

    // The button was just released (back in the up position)
    if (buttonValue == HIGH && priorButtonValue[buttonPin] == LOW) {
        priorButtonValue[buttonPin] = HIGH;
        return false;
    }

    // The button was just pressed (back in the down position)
    if (buttonValue == LOW && priorButtonValue[buttonPin] == HIGH) {
        priorButtonValue[buttonPin] = LOW;
        return true;
    }
}


void toggleLed(int red, int green, int blue) {
    if (aggregateLoopDelay % 500 < 250) {
        led(red, green, blue);
        return;
    }

    off();
}


void off() {
    led(LOW, LOW, LOW);
}


void red() {
    led(HIGH, LOW, LOW);
}


void green() {
    led(LOW, HIGH, LOW);
}


void blue() {
    led(LOW, LOW, HIGH);
}


void yellow() {
    led(254, 254, LOW);
}


void led(int red, int green, int blue) {
    if (red == LOW || red == HIGH) {
        digitalWrite(RED_LED_PIN, red);
    } else {
        analogWrite(RED_LED_PIN, red);
    }

    if (green == LOW || green == HIGH) {
        digitalWrite(GREEN_LED_PIN, green);
    } else {
        analogWrite(GREEN_LED_PIN, green);
    }

    if (blue == LOW || blue == HIGH) {
        digitalWrite(BLUE_LED_PIN, blue);
    } else {
        analogWrite(BLUE_LED_PIN, blue);
    }
}


void getDateTime() {
    DateTime now = ds3231.now();
    char daysOfTheWeek[7][12] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    int hour = now.hour();
    char ampm = 'a';
    if (hour > 12) {
        hour -= 12;
        ampm = 'p';
    }
    sprintf(dateTimeLcd, "%s %02d/%02d %02d:%02d:%02d%c", daysOfTheWeek[now.dayOfTheWeek()], now.month(), now.day(), hour, now.minute(), now.second(), ampm);
}


void getTemperatureHumidity() {
    temperatureLcd = (int) round(dht22.readTemperature(true));
    humidityLcd = (int) round(dht22.readHumidity());
}


void getElapsedTime() {
    elapsedMilliseconds = millis();
    int minutes = ((elapsedMilliseconds - startTime - aggregatePauseTime) / 1000) / 60;
    int seconds = ((elapsedMilliseconds - startTime - aggregatePauseTime) / 1000) % 60;
    sprintf(elapsedTimeLcd, "%02d:%02d", minutes, seconds);
}


boolean seconds(int n) {
    if (aggregateLoopDelay % (n * 1000) == 0) {
        return true;
    }
    return false;
}


void updateLcd() {
    /* lcd.clear(); */

    lcd.setCursor(0, 0);
    lcd.print(dateTimeLcd);

    lcd.setCursor(0, 1);
    lcd.print(temperatureLcd);
    lcd.print((char) 223);

    lcd.setCursor(4, 1);
    lcd.print(humidityLcd);

    if (state == STOPPED) {
        lcd.setCursor(0, 2);
        lcd.print("                  ");

        lcd.setCursor(0, 3);
        lcd.print("                  ");
    } else {
        lcd.setCursor(0, 2);
        lcd.print(guessLcd);

        lcd.setCursor(0, 3);
        lcd.print(elapsedTimeLcd);
    }
}
