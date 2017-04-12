#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <Wire.h>
#include <CurieIMU.h>
#include <SoftwareSerial.h>

#define MAX_BT_COMMAND_LENGTH 20   // максимальный размер команды по bluetooth
#define MAX_AVALIABLE_DEVICES 6    // максимальное количество устройств для обнаружения
#define PASSWORDS_COUNT 4                // количество хранимых (на текущий момент) паролей 
#define KEY_PIN 4                  // цифровой порт для входа в режим программирования HC-05
#define MESSAGING_DELAY 500

#define OLED_MOSI   9              // порты для OLED
#define OLED_CLK   10
#define OLED_DC    11
#define OLED_CS    12
#define OLED_RESET 13
Adafruit_SSD1306 display(OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

const float epsRotate = 0.09;
const float eps = 0.09;
const float slowerSecond = 4;
const int pairing_timeout = 5;                // таймаут для сопряжения в секундах
char command_buffer[MAX_BT_COMMAND_LENGTH];   // массив для получения команды по bluetooth
String passwords[PASSWORDS_COUNT] = {"0597", "1234", "0000", "0305"};
String mac_devices[MAX_AVALIABLE_DEVICES];
String names_of_devices[MAX_AVALIABLE_DEVICES];

SoftwareSerial BTSerial(2, 3);

void InitializeIMU()
{
  CurieIMU.begin();
  CurieIMU.setAccelerometerRange(2);
}

void InitializeCOM()
{
  BTSerial.begin(38400);
}

void AccelControl()        // дистанционное управление на акселерометре
{
  float ax, ay, az;
  CurieIMU.readAccelerometerScaled(ax, ay, az);

  String left;
  String right;
  createCommand(ax, ay, az, left, right);
  BTSerial.println(left + "\r\n");
  BTSerial.println(right + "\r\n");
}

void createCommand(float x, float y, float z, String& left, String& right)  // создание команд для посылки
{
  float newX = x * x;
  float newZ = z * z;
  right = "R";
  left = "L";

  float throttle = 50;
  float addSpeed = min(newZ * 100, 50);
  float secondThrottle = 50;

  if (z >= 0)
  {
    throttle += addSpeed;
    secondThrottle += (1 - newX) * addSpeed / slowerSecond;
  }
  else
  {
    throttle -= addSpeed;
    secondThrottle -= (1 - newX) * addSpeed / slowerSecond;
  }

  if (abs(x) > eps)
  {
    //танковый поворот
    if (abs(z) <= epsRotate)
    {
      addSpeed = min(newX * 100, 50);
      if (x > 0)
      {
        left += (int) (50 + addSpeed);
        right += (int) (50 - addSpeed);
      }
      else
      {
        right += (int)(50 + addSpeed);
        left += (int) (50 - addSpeed);
      }
    }
    else
    {
      //поворот
      if (x > 0)
      {
        left += (int)throttle;
        right += (int)secondThrottle;
      }
      else
      {
        right += (int)throttle;
        left += (int)secondThrottle;
      }
    }
  }
  else
  {
    left += (int)throttle;
    right += (int)throttle;
  }
}

String BT_Answer(String to_send)  //получение ответа от BT-модуля
{
  to_send += "\r\n";
  BTSerial.print(to_send);
  String command = GetCommand();
  BTSerial.flush();
  return command;
}

void GetDevices(String *devices)     // заполнение массива MAC-адресами доступных устройств
{
  BTSerial.print("AT+INQ\r\n");
  String device = "";

  for (int i = 0; ;++i)
  {
    device = GetCommand();
    if (device == "OK")         // когда устройства закончились
      return;
    BTSerial.flush();

    //отрезаем служебную команду +INQ: в начале:
    device = device.substring(5, device.length());

    int end_index = 0;
    //ищем индекс, где заканчивается реальный MAC-адрес:
    for (int j = 0; j < device.length(); j++)
    {
      char current = device[j];
      if (current == ',')
      {
        end_index = j;
        break;
      }
    }

    // отрезаем мусор в конце адреса:
    device = device.substring(0, end_index);
    // приводим адрес к готовому для дальнейших запросов виду:
    device.replace(':', ',');
    devices[i] = device;
  }
}

// заполнение массива names именами устройств по адресам из macs:
void MacToName(String* macs, String* names)
{
  for (int i = 0; i < MAX_AVALIABLE_DEVICES; ++i)
  {
    String current_mac = macs[i];
    if (current_mac == "")
    {
      names[i] = "";
      continue;
    }
    String answer = BT_Answer("AT+RNAME?" + current_mac);
    answer = answer.substring(7, answer.length());  // отрезаем мусор в начале ответа
    names[i] = answer;
  }
}

int ConnectToRobot()   // функция присоединения к роботу, возвращает коды ошибок
{
  BTSerial.flush();
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  String answer = BT_Answer("AT");
  display.setCursor(0, 0);
  display.println("AT");
  display.display();

  delay(MESSAGING_DELAY);

  display.setCursor(0, 10);
  display.println(answer);
  display.display();

  delay(MESSAGING_DELAY);
  /*
  BTSerial.flush();
  answer = BT_Answer("AT+ORGL");
  display.setCursor(0, 20);
  display.println("AT+ORGL");
  display.display();

  delay(MESSAGING_DELAY);
  
  display.setCursor(0, 30);
  display.println(answer);
  display.display();
  
  delay(MESSAGING_DELAY);

  BTSerial.flush();
  answer = BT_Answer("AT+RMAAD");
  display.setCursor(0, 40);
  display.println("AT+RMAAD");
  display.display();

  delay(MESSAGING_DELAY);
  
  display.setCursor(0, 50);
  display.println(answer);
  display.display();
  
  delay(MESSAGING_DELAY);

  BTSerial.flush();
  answer = BT_Answer("AT+ROLE=1");
  display.setCursor(64, 0);
  display.println("AT+ROLE=1");
  display.display();

  delay(MESSAGING_DELAY);

  display.setCursor(64, 10);
  display.println(answer);
  display.display();

  delay(MESSAGING_DELAY);

  BTSerial.flush();
  display.setCursor(64, 20);
  display.println("AT+RESET");
  display.display();
  answer = BT_Answer("AT+RESET");

  delay(MESSAGING_DELAY);

  display.setCursor(64, 30);
  display.println(answer);
  display.display();

  delay(MESSAGING_DELAY);
  */
  if (answer == "OK")
  {
    display.setCursor(0, 20);
    display.println("AT+INIT");
    display.display();

    delay(MESSAGING_DELAY);
    
    BTSerial.flush();
    answer = BT_Answer("AT+INIT");

    display.setCursor(0, 30);
    display.println(answer);
    display.display();
    
    delay(MESSAGING_DELAY);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("AT+INQ");
    display.display();

    delay(MESSAGING_DELAY);

    display.clearDisplay();

    GetDevices(mac_devices);
    if (mac_devices[0] == "")
      return -3;
    for (int i = 0; i < MAX_AVALIABLE_DEVICES; ++i)
    {
      display.setCursor(0, i * 10);
      display.println(mac_devices[i]);
    }
    display.display();

    delay(2000);

    display.clearDisplay();

    MacToName(mac_devices, names_of_devices);

    for (int i = 0; i < MAX_AVALIABLE_DEVICES; ++i)
    {
      display.setCursor(0, i * 10);
      display.println(names_of_devices[i]);
    }
    display.display();

    delay(2000);

    // TO DO: сделать селектор устройств и выбор нужного
    String variant = mac_devices[0];
    answer = Pair(variant, passwords);
    if (answer != "OK")
      return -2;

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Pairing successful");
    display.display();
    delay(1000);

    answer = Link(variant);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Link successful");
    display.display();
    delay(1000);
    
    digitalWrite(4, LOW);
    delay(500);
  }
  else
    return -1;
}

String Pair(String variant, String* passwords)
{
  String answer = "";
  for (int i = 0; i < PASSWORDS_COUNT; ++i)
  {
    BTSerial.print("AT+PSWD=" + passwords[i] + "\r\n");
    delay(1000);
    BTSerial.flush();
    answer = BT_Answer("AT+PAIR=" + variant + "," + (String)pairing_timeout);
    delay(1000);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(String(i) + ": " + answer);
    display.display();
    if (answer == "OK")
      break;
    if (i == PASSWORDS_COUNT - 1)
      return "CODE_INVALID_PSWD";
  }
  return "OK";
}

String Link(String variant)
{
  String answer = BT_Answer("AT+LINK=" + variant);
  if (answer != "OK")
    return "CODE_LINK_FAILURE";
  return "OK";
}

String GetCommand() // получение команды из COM-порта, если команды нет, то возвращается пустая строка
{
  unsigned long time_after = 0;
  unsigned long time_before = millis();
  while (BTSerial.available() == 0)   // ждем прихода данных
  {
    time_after = millis();
    if (time_after - time_before >= pairing_timeout * 1000)  // таймаут ожидания ответа
      return "";
  }

  int incoming_byte = BTSerial.readBytesUntil('\n', command_buffer, MAX_BT_COMMAND_LENGTH);   //грузим данные в буфер
  String command = command_buffer;
  command = command.substring(0, incoming_byte);
  command.replace("\r", "");
  command.replace("\n", "");
  return command;
}

void setup()
{
  InitializeCOM();
  InitializeIMU();

  pinMode(KEY_PIN, OUTPUT);
  digitalWrite(KEY_PIN, HIGH);

  for (int i = 0; i < SSD1306_LCDHEIGHT * SSD1306_LCDWIDTH / 8; ++i)
    buffer[i] = 0x88 ^ buffer[i]; 
  
  display.begin(SSD1306_SWITCHCAPVCC);
  display.display();
  delay(6000);

  ConnectToRobot();

  while (true)
  {
    AccelControl();
    delay(100);
  }
}

void loop()
{
}

