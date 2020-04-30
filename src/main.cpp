#include <Arduino.h>
#include <Servo.h>

// Macro utils
#define ARRAY_SIZE(foo) (sizeof(foo) / sizeof(foo[0]))

// Debug messages ini console
#define DEBUG

//init ESC
#define INIT_ESC

// Pin definitions
#ifdef NANO
#define LED_pin LED_BUILTIN
#define OUT_pin 9
#elif YUN
#define LED_pin LED_BUILTIN
#define OUT_pin 9
#elif F411
#define LED_pin LED_BUILTIN
#define OUT_pin PC13
#endif

// servo control def
Servo servo;

// string/serial buffer
#define INPUT_SIZE 127
char buffer[INPUT_SIZE + 1];

// params the output function
boolean shutdown = false;
unsigned int Ti = 3000;
unsigned int RR = 10;
unsigned int PEEP = 10;
unsigned int Pinsp = 35;
unsigned int fraq[] = {14, 2, 5};

// piecewise time(ms) and values(0-100) of output function
unsigned int values[] = {0, 0, 0};
unsigned int times[] = {0, 0, 0};
unsigned int fanValue = 0; // actual value of the fan (to send to UI)

// calculate piecewise function based in output params
void changeParams()
{
  int f = fraq[1] + fraq[2];
  times[1] = Ti * fraq[1] / f;
  times[2] = Ti * fraq[2] / f;

  times[0] = 100 * 60 / RR * 10 - Ti;

  values[0] = PEEP;
  values[1] = 1.05 * Pinsp;
  values[2] = Pinsp;
}

// read Serial data function (from UI)
void readData()
{
  //TODO: fix detect burst of values
  if (Serial.available() >= 1)
  {
    byte size = Serial.readBytesUntil('\n', buffer, INPUT_SIZE);
    buffer[size] = 0;

    if (buffer != "")
    {
      char *command = strtok(buffer, ",");
      while (command != 0)
      {
        // Serial.println(command);
        char *sep = strchr(command, '=');
        *sep = 0;
        int a = atoi(command);
        ++sep;
        int b = atoi(sep);
        if (a != 0 && a < ARRAY_SIZE(times))
        {
          values[a] = b;
        }
        else
        {
          switch (*command)
          {
          case 'T':
            Ti = b;
            break;
          case 'R':
            RR = b;
            break;
          case 'P':
            Pinsp = b;
            break;
          case 'E':
            PEEP = b;
            break;
          case 'H':
            shutdown = b;
            break;
          default:
            break;
          }

          changeParams(); // recalculate piecewise function based in new params
        }
        command = strtok(0, ",");
      }
    }
  }
}

// output handler
void out(int value)
{
  value = value > 100 ? 100 : value;
  value = value < 0 ? 0 : value;

  value = map(value, 0, 100, 0, 180);

#ifdef DEBUG
  Serial.println(value);
  analogWrite(LED_BUILTIN, value);
#endif

  servo.write(value);
}

// smart reimplementation of delay function to output serial while delaying
void smartDelay(long ms)
{
  unsigned long now = millis();
  do
  {
    readData(); // read serial data from UI

    //TODO: change Pr to real sensor value
    //output actual output params to make feddback to UI
    sprintf(buffer, "Pr=%d\nPt=%d\nPEEP=%d\nTi=%d\nRR=%d\nPinsp=%d\nH=%d", fanValue, fanValue, PEEP, Ti, RR, Pinsp, shutdown);

    Serial.println(buffer);
    delay(100);
  } while (millis() - now < ms);
}

// Setup main function
void setup()
{
  // configures pins and serial
  pinMode(OUT_pin, OUTPUT);
  Serial.begin(9600);
  Serial.setTimeout(2000);
  changeParams(); //recalculate piecewise function first time

#ifdef DEBUG
  Serial.println("iniciando...");
  for (int i = 0; i < 3; i++)
  {
    Serial.print(times[i]);
    Serial.print(',');
  }
  Serial.println();
  for (int i = 0; i < 3; i++)
  {
    Serial.print(values[i]);
    Serial.print(',');
  }
  Serial.println();
#endif

  servo.attach(OUT_pin, 1000, 2000);

// init ESC with min/max vaues
#ifdef INIT_ESC
  servo.write(180);
  delay(3000);
  servo.write(0);
  delay(2000);
#ifdef DEBUG
  Serial.println("esc iniciado...");
#endif
#endif

  servo.write(0);
}

// Main LOOP
void loop()
{

  if (shutdown)
  {
    out(0);
    return;
  }

  // iterate over output piecewise funtion to make 1 cycle
  for (unsigned int i = 0; i < ARRAY_SIZE(times); i++)
  {
    fanValue = values[i]; // save actual fan value
    out(fanValue);        // send to fan an outout of 0-100 units
    smartDelay(times[i]); // wait times[1] ms until next value in piecewise function
  }

  // delay(15);
}