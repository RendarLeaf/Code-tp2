#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <LiquidCrystal_I2C.h>
#define printByte(args)  write(args);

byte joyStickBouton{ 4 };
byte buzzer{19};
byte MotorClockwise {13};
byte MotorCounterClockwise {14};
byte lightSensor {34};
byte nightLight {23};
byte blinker {18};
byte blinkerSwitch {35};
TickType_t blinkOn {500};
TickType_t blinkOff {250};

const int speedCheckInterval = 100; //milliseconds

const int motor_start_amount = 200; //minimum for motor spin

int testStage = 0;   //to be removed after testing
double speed = 0.0;
double accel_forward = 0.5;
double accel_backwards = 0.3;
const double deceleration = 0.1;

LiquidCrystal_I2C lcd(0x3f, 16, 2);
uint8_t lightbulb[8] = {
	0b01110, 
	0b10001, 
	0b10101, 
	0b10101, 
	0b01010, 
	0b01110, 
	0b00000, 
	0b01110
} ;


TaskHandle_t blinkTaskHandle = NULL;

void applySpeed(int joystickDirection){
  if (speed > 0.0){
    speed -= deceleration;
    if (speed < 0.0) {
      speed = 0.0;
    }
  } else if (speed < 0.0){
    speed += deceleration;
    if (speed > 0.0) {
      speed = 0.0;
    }
  }

  if (joystickDirection == 1){
    speed += (accel_forward);
  } else if (joystickDirection == -1){
    speed -= (accel_backwards);
  }

  if (speed > 100.0) {
    speed = 100.0;
  }
  if (speed < -50.0) {
    speed = -50.0;
  }

  if (abs(speed) < 0.05) {
    speed = 0.0;
  }
}

void vMotorTask(void *params) {
  for (;;) {
    int joystickInput = 0;

    /* Motor Test Logic */
    if (testStage == 0) {
      joystickInput = 1;
      if (speed >= 100.0){
        testStage = 1;
      }
    } 
    else if (testStage == 1) {
      joystickInput = -1;
      if (speed <= -50.0) {
        testStage = 2;
      }
    } 
    else if (testStage == 2) {
      joystickInput = 0;
      if (abs(speed) < 0.15){
        testStage = 0;
      }
    }
    
    applySpeed(joystickInput);

    if (speed > 0.0) {
      double speedRatio = speed / 100.0;
      if (speedRatio > 1.0) {
        speedRatio = 1.0;
      }

      int pwmVal = motor_start_amount + (int)floor(speedRatio * 55.0);
      if (pwmVal > 255){
        pwmVal = 255;
      }
      
      analogWrite(MotorClockwise, pwmVal);
      analogWrite(MotorCounterClockwise, 0);
    } 
    else if (speed < 0.0) {
      double speedRatio = abs(speed) / 100.0;
      if (speedRatio > 1.0) speedRatio = 1.0;
      int pwmVal = motor_start_amount + (int)floor(speedRatio * 55.0);
      if (pwmVal > 255){
        pwmVal = 255;
      }
      
      analogWrite(MotorCounterClockwise, pwmVal);
      analogWrite(MotorClockwise, 0);
    } 
    else {
      analogWrite(MotorClockwise, 0);
      analogWrite(MotorCounterClockwise, 0);
    }

    /* Speed Logger */
    Serial.print("Stage: ");
    Serial.print(testStage);
    Serial.print(" | Target Speed: "); 
    Serial.print(speed, 2);
    Serial.print(" | Output Signal (0-255): ");
    if (speed > 0.0) {
      Serial.println(motor_start_amount + (int)((speed / 100.0) * 55.0));
    } else if (speed < 0.0) {
      Serial.println(motor_start_amount + (int)((abs(speed) / 100.0) * 55.0));
    } else {
      Serial.println(0);
    }
    
 

    vTaskDelay(pdMS_TO_TICKS(speedCheckInterval));
  }
}



void task_blinker(void* parameter) {
  for (;;) {
    digitalWrite(blinker, LOW);
    vTaskDelay(blinkOn);
    digitalWrite(blinker, HIGH);
    vTaskDelay(blinkOff);
  }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(joyStickBouton, INPUT);
  pinMode(lightSensor, INPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(nightLight, OUTPUT);


  analogWriteResolution(MotorClockwise, 8); 
  analogWriteResolution(MotorCounterClockwise, 8); 

  analogWriteFrequency(MotorClockwise, 5000);
  analogWriteFrequency(MotorCounterClockwise, 5000);

  analogWrite(MotorClockwise, 0);
  analogWrite(MotorCounterClockwise, 0);


  pinMode(blinker, OUTPUT);
  pinMode(blinkerSwitch, INPUT);
  Serial.begin(115200);
  lcd.init(); // Initialize the LCD
	lcd.backlight(); // Turn on the backlight
	lcd.clear();
  lcd.createChar(0, lightbulb);
	lcd.home();


  xTaskCreatePinnedToCore(
    vMotorTask,
    "MotorTask",
    4096,//allocated size
    NULL,
    2, //priority
    NULL,
    1
  );

}

void loop() {
  // put your main code here, to run repeatedly:
  if (analogRead(joyStickBouton) > 0) {
    tone(buzzer, 1000);
  }
  else {
    noTone(buzzer);
  }


  if (analogRead(lightSensor) < 1000) {
    digitalWrite(nightLight, HIGH);
  }
  else {
    digitalWrite(nightLight, LOW);
  }


  if (analogRead(blinkerSwitch) > 0 && blinkTaskHandle == NULL) {
    xTaskCreate(
      task_blinker,
      "blinker",
      2048,
      &blinker,
      tskIDLE_PRIORITY + 1,
      &blinkTaskHandle
    );
    lcd.setCursor(15, 1);
	  lcd.printByte(0);
  }
  else if(analogRead(blinkerSwitch) == 0 && blinkTaskHandle != NULL) {
    vTaskDelete(blinkTaskHandle);
    digitalWrite(blinker, LOW);
    lcd.setCursor(15, 1);
    lcd.print(" ");
  }
  int currentSpeed = (int)floor(speed);

  if(currentSpeed > 0 ) {
    lcd.setCursor(8, 0);
    lcd.print("->");
    lcd.setCursor(0, 0);
    lcd.print("  ");
    lcd.setCursor(3, 0 );
    lcd.print((String)currentSpeed + " ");
  }
  else if(currentSpeed < 0) {
    lcd.setCursor(8, 0);
    lcd.print("  ");
    lcd.setCursor(0, 0);
    lcd.print("<-");
    lcd.setCursor(3, 0 );
    lcd.print((String)abs(currentSpeed) + " ");
  }
  else {
    lcd.setCursor(8, 0);
    lcd.print("  ");
    lcd.setCursor(0, 0);
    lcd.print("  ");
    lcd.setCursor(3, 0 );
    lcd.print((String)currentSpeed + " ");
  }

}
