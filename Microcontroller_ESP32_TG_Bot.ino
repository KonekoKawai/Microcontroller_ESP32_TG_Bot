#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

#include <WiFiClientSecure.h> // для HTTPS-запросов
#include <ESP8266HTTPClient.h> // для HTTP-запросов
#include <UniversalTelegramBot.h> // Для TG-bot
#include <ArduinoJson.h> // Для TG-bot и парсинга Json
#include <time.h>  

// Данные сети
const char* ssid = "XXX"; // Название сети
const char* password = "XXX"; // Пароль сети

#define oneDayMlSecond 86400000 // Сколько миллисекунд в дне
#define oneHourMlSecond 3600000 // Сколько миллисекунд в часе
#define oneMinuteMlSecond 60000 // Сколько миллисекунд в минуте

#define NTP_SERVER "at.pool.ntp.org"    
#define NTP_TZ_SETTING "MSK-3MSD,M3.5.0/2,M10.5.0/3" // Время МСК + 1

// Данные для бота
#define BOTtoken "XXX"  // Токен бота
#define CHAT_ID "XXX" // ID пользователя или группы

#ifdef ESP8266 // Сертфикикат подключения дл TG
  X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif

time_t now;  // Хранит секунды с эпохи юникс
tm tm;  // информация о времени в удобном виде 

// Для бота
WiFiClientSecure clientTgBot;
UniversalTelegramBot bot(BOTtoken, clientTgBot);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void SyncTime() // Запуск Синхронизация до 21:55+ по +5 времени
{

  int hour_int = tm.tm_hour+1;
  int minute_int = tm.tm_min;
  Serial.print("Hour: " + String(hour_int));
  Serial.print("Minute: " + String(minute_int));

  while (hour_int != 21)
  {
    if (hour_int == 24)
        hour_int = 0;
    hour_int++;
    delay(oneHourMlSecond); // ЖДдём час
    Serial.print("\nCurrent hour: " + hour_int);
  }
  Serial.print("\n---------TIME 21:XX--------\n\n");

  while (minute_int < 55)
  {
    if (minute_int == 60)
        minute_int = 0;
    minute_int++;
    delay(oneMinuteMlSecond); // ЖДдём минуту
    Serial.print("\nCurrent minute: " + minute_int);
  }
  Serial.print("\n---------TIME 21:55+--------\n\n");

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() { // Первичная настройка отладки + подключение к Wi-Fi и TG боту
  Serial.begin(115200); // Для отладки 
  delay(100);

  #ifdef ESP8266
    configTime(0, 0, "pool.ntp.org");      // получаем всемирное координированное время (UTC) через NTP
    clientTgBot.setTrustAnchors(&cert); // Получаем сертификат api.telegram.org
  #endif
 
  // Подключаемся к Wi-Fi
  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid, password);
  #ifdef ESP32
    clientTgBot.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Добавляем корневой сертификат для api.telegram.org
  #endif
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  // Выводим IP нашего контроллера 
  Serial.println(WiFi.localIP());
  Serial.println("Connect successful");
  delay(1000);

  configTime(NTP_TZ_SETTING, NTP_SERVER); // Установка времени 
  time(&now);
  localtime_r(&now, &tm);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String reqArsagera(String date_for_Arsa) // Функция для получения данных с Арсагера API
{
  WiFiClientSecure arsaClient; // Создаем клиента для подключения 

  arsaClient.setInsecure(); // Говорим Что будем подключаться без использования сертификатов
  Serial.print("\nConnecting to ");
  Serial.print("https://arsagera.ru\n");

  if (!arsaClient.connect("185.44.14.62", 443)) 
  { // Конектимся на IP арсагеры 
    Serial.println("Connection FAILED To arsaURL");
     return "0"; // Если ошибка конекта возвращаемся 
  }
  else
  {
    Serial.println("Connection To arsaURL SUCCESSEFUL");
  }
  Serial.print("\nRequesting URL: ");
  Serial.println("https://arsagera.ru/api/v1/funds/fa/fund-metrics/?date=" + date_for_Arsa);

  arsaClient.println(String("GET https://arsagera.ru/api/v1/funds/fa/fund-metrics/?date=") + date_for_Arsa + " HTTP/1.0"); // Кидаем запрос на Арсагера API с нужной датой
  arsaClient.println("Host: 185.44.14.62"); // Хост арсагеры 
  arsaClient.println("Connection: close");
  arsaClient.println();

  Serial.println("Request sent\n");

  while (arsaClient.connected()) 
  { // Получаем данные 
    String line = arsaClient.readStringUntil('\n');
    if (line == "\r") 
    {
        Serial.println("headers received");
        break;
    }
  }

  String arsaData;
  while (arsaClient.available()) // Выгружаем данные 
  {
      char c = arsaClient.read();
      arsaData = arsaData + c;
      //Serial.write(c); Для тестов и вывода в консоль
  }

  arsaClient.stop(); // Отключаемся
  return arsaData; // Передаем полученные данные обратно
  // Вид данных в этом случае:
  // Server: nginx
  // Date: Fri, 02 May 2025 11:29:35 GMT
  // Content-Type: application/json
  // Connection: close
  // Cache-Control: private, must-revalidate
  // pragma: no-cache
  // expires: -1
  //
  // {"data":[{"date":"2025-04-29","nav_per_share":15445.08,"total_net_assets":2641624248.55}]}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float parsingArsagera(String arsaData) // Функция вывод значение метрики "nav_per_share": XXXXX.XX
{
  // Принимаем arsaData 
  // Server: nginx
  // Date: Fri, 02 May 2025 11:29:35 GMT
  // Content-Type: application/json
  // Connection: close
  // Cache-Control: private, must-revalidate
  // pragma: no-cache
  // expires: -1

  // {"data":[{"date":"2025-04-29","nav_per_share":15445.08,"total_net_assets":2641624248.55}]}
  Serial.println("\nSTART - parsingArsagera");
  int count = 0;
  bool flag = false;
  String jsonArsaData;
  while (arsaData.length() > count) // Выходим из цикла Если прошлись по всем данным
  {
      if (arsaData[count] == '{' || flag == true)
      {
          jsonArsaData = jsonArsaData + arsaData[count];
          flag = true;
      }
      if (arsaData[count] == '}')
          break;
      count++;
  }

  if(flag == false) // В случае Если мы проишлись по данным и не нашли их То прекращаем дальнейший поиск 
    return -1;
  // {"data":[{"date":"2025-04-29","nav_per_share":15445.08,"total_net_assets":2641624248.55}


  String navPerShare;
  count = 0;
  flag = false;
  while (jsonArsaData.length() > count)
  {
      if (jsonArsaData[count] == 'n' || flag == true)
      {
          navPerShare = navPerShare + jsonArsaData[count];
          flag = true;
      }
      count++;
  }
  if (flag == false) // В случае Если мы проишлись по данным и не нашли их То прекращаем дальнейший поиск 
      return -1;
  // nav_per_share":15445.08,"total_net_assets":2641624248.55}  

  String value;
  count = 0;
  flag = false;
  while (navPerShare.length() > count)
  {
      if (navPerShare[count] == ':' || flag == true)
      {
          flag = true;
          if (navPerShare[count] == ',')
              break;
          if (navPerShare[count] != ':')
              value = value + navPerShare[count];  
      }
      count++;
  }

  if (flag == false) // В случае Если мы проишлись по данным и не нашли их То прекращаем дальнейший поиск 
      return -1;

  Serial.println("END - parsingArsagera: " + value);
  return value.toFloat(); 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


String getTime()
{
  String dateFormated;
  // отдельно выписываем год месяц и дату и приводим к виду XX 
  String year = String(tm.tm_year + 1900);
  String month;
  if(tm.tm_mon + 1 < 9)
    month = "0" + String(tm.tm_mon + 1);
  else
    month = String(tm.tm_mon + 1);

  String day;
  if(tm.tm_mday < 9)
  {
    if(tm.tm_mday == 1)
      day = "0" + String(tm.tm_mday);
    else
      day = "0" + String(tm.tm_mday-1);
  }
  else
    day = String(tm.tm_mday-1);
  
  dateFormated = year + "-" + month + "-" + day;
  Serial.print("\n\nDate Formated: " + dateFormated);
  Serial.print("\n");
  
  return dateFormated; // Должен быть в виде "2025-04-22"
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float preValueMetrik = 0;
float diffMetrik = 0;
float buffValue = 0;
float valueMetrik;
String smile;

void loop() 
{
  SyncTime(); // Синхронизируемся до 21:55
  String date_for_Arsa = getTime(); // Получаем дату для запроса
  String arsaData = reqArsagera(date_for_Arsa); // Передаем в запрос нужную дату 
  if(arsaData != "0") // Если ошибка конекта нет
  {

    Serial.print("Arsagere requests:");
    Serial.print(arsaData);

    buffValue = parsingArsagera(arsaData);
    if(buffValue == -1) // Если метрика не найдена 
    {
      Serial.print("\nbuffValue -1\n");
    }

    if(valueMetrik != 0) // Если ошибки парсинга метрики нет 
    {
      valueMetrik = buffValue;
      Serial.print("Arsagere value:");
      Serial.print(valueMetrik);
      
      if (valueMetrik != 0 and preValueMetrik != 0)
      {
        if (valueMetrik / preValueMetrik >= 1)
        {
          diffMetrik = round((valueMetrik / preValueMetrik - 1) * 1000) / 10;
          smile = "📈";
        }
        else
        {
          diffMetrik = round((valueMetrik / preValueMetrik - 1) * 1000) / 10;
          smile = "📉";
        }

        preValueMetrik = valueMetrik;
        bot.sendMessage(CHAT_ID, "💰Биржевые ориентиры <b>Арсагера ФА</b>💰 \n\nСтоимость пая на дату <b>" + date_for_Arsa + "</b> — <b><u>" + String(valueMetrik) + "</u></b> рублей \n\nЦена за пай изменилась на <b>" + diffMetrik + "%" + smile + "</b> \n\n#Арсагера_ФА", "HTML");
      }
      else
      {
        bot.sendMessage(CHAT_ID, "💰Биржевые ориентиры <b>Арсагера ФА</b>💰 \n\nСтоимость пая на дату <b>" + date_for_Arsa + "</b> — <b><u>" + String(valueMetrik) + "</u></b> рублей \n\n#Арсагера_ФА", "HTML");
        preValueMetrik = valueMetrik;
    
      }
      
    }

  }

  delay(oneDayMlSecond-2*oneHourMlSecond); // Сутки - 2 часа Для синхронизации
}
