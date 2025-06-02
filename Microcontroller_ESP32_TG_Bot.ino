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

void SyncTime() // Запуск Синхронизации до 21:55+ по +5 времени
{
  time(&now);
  localtime_r(&now, &tm);

  Serial.print("\nStart Sync");
  int hour_int = tm.tm_hour+1;
  int minute_int = tm.tm_min;

  Serial.print("\nHour: " + String(hour_int));
  Serial.print(" : Minute: " + String(minute_int));

  while (hour_int != 21)
  {
    if (hour_int == 24)
        hour_int = 0;
    hour_int++;
    delay(oneHourMlSecond); // ЖДдём час
    Serial.print("\nCurrent hour: " + String(hour_int));
  }
  Serial.print("\n---------TIME 21:XX--------\n\n");
  while (minute_int < 50)
  {
    if (minute_int == 60)
        minute_int = 0;
    minute_int++;
    delay(oneMinuteMlSecond); // ЖДдём минуту
    Serial.print("\nCurrent minute: " + String(minute_int));
  }
  //Serial.print("\min:" + String(tm.tm_min));
  Serial.print("\n---------TIME 21:55+--------\n\n");
  Serial.print("\nSync - successful");
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

  if(tm.tm_year + 1900 == 1970)
  {
    while(tm.tm_year + 1900 == 1970)
    {
      delay(oneMinuteMlSecond*2);
      Serial.print("year not formated (1970) - reboot \n");
      configTime(NTP_TZ_SETTING, NTP_SERVER); // Установка времени 
      time(&now);
      localtime_r(&now, &tm);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String reqArsagera(String date_for_Arsa,  String url_api) // Функция для получения данных с Арсагера API
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
  Serial.println(url_api + date_for_Arsa);

  arsaClient.println(String("GET ") + url_api + date_for_Arsa + " HTTP/1.0"); // Кидаем запрос на Арсагера API с нужной датой
  arsaClient.println("Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7"); 
  arsaClient.println("user-agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/136.0.0.0 Safari/537.36"); 
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

  
  Serial.println("\nSTART - parsingArsagera\n");
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
  Serial.println("\nEND - parsingArsagera OK: " + value);
  return value.toFloat(); 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


String getTime(int countDay) // Дата должна быть за какой-то предыдущий период Число countDay определяет на сколько дней назад мы двигаемся // ОБЫЧНО: от 1 до последнего прогноза
{
  time(&now);
  now = now - countDay*oneDaySecond; // На сколько дней назад отправляемся
  localtime_r(&now, &tm);

  String dateFormated;
  
  // отдельно выписываем год месяц и дату и приводим к виду XX 
  String year = String(tm.tm_year + 1900);
  String month;
  if(tm.tm_mon + 1 < 10)
    month = "0" + String(tm.tm_mon + 1);
  else
    month = String(tm.tm_mon + 1);

  String day;
  if(tm.tm_mday < 10)
    day = "0" + String(tm.tm_mday);
  else
    day = String(tm.tm_mday);
  
  dateFormated = year + "-" + month + "-" + day;
  Serial.print("\n\nDate Formated: " + dateFormated);
  Serial.print("\n");
  
  return dateFormated; // Должен быть в виде "2025-04-22"
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float preValueMetrik_FA = 0;
float diffMetrik_FA = 0;
float buffValue_FA = 0;
float valueMetrik_FA;

float preValueMetrik_f64 = 0;
float diffMetrik_f64 = 0;
float buffValue_f64 = 0;
float valueMetrik_f64;

String smile_FA;
String smile_f64;

String url_FA = "https://arsagera.ru/api/v1/funds/fa/fund-metrics/?date=";
String url_f64 = "https://arsagera.ru/api/v1/funds/f64/fund-metrics/?date=";
int buffPreDay = 1;

void loop() 
{
  Serial.print("\nStart Loop"); 
  SyncTime(); // Запуск Синхронизации
  String date_for_Arsa = getTime(buffPreDay);  // Получаем дату для запроса

  String arsaDataFA = reqArsagera(date_for_Arsa, url_FA); // Передаем в запрос нужную дату 
  delay(oneMinuteMlSecond/6); // Делаем небольшую паузу между запросами
  String arsaDataF64 = reqArsagera(date_for_Arsa, url_f64); // Передаем в запрос нужную дату 
  
  while(true) // Цикл поиска последней существующей метрики
  {
    if(arsaDataFA != "0" && arsaDataF64 != "0") // Если ошибка конекта нет
    {
      Serial.print("\nArsagere requests FA:\n");
      Serial.print(arsaDataFA);
      Serial.print("\nArsagere requests 6.4:\n");
      Serial.print(arsaDataF64);

      buffValue_FA = parsingArsagera(arsaDataFA);
      buffValue_f64 = parsingArsagera(arsaDataF64);
      Serial.print("\n\nbuffValueFA:\n" + String(buffValue_FA));
      Serial.print("\n\nbuffValue6.4:\n" + String(buffValue_f64));

      if(buffValue_FA == -1) // Если ошибки парсинга: метрики - нет
      {
        buffPreDay++;
      } // Если метрика нашлась 
      else
        break;
    }
    delay(oneMinuteMlSecond/5); // Запрос раз в минуты Чтобы не перегружать API Арсагеры
    date_for_Arsa = getTime(buffPreDay);  // Получаем дату для запроса

    arsaDataFA = reqArsagera(date_for_Arsa, url_FA); // Передаем в запрос нужную дату 
    delay(oneMinuteMlSecond/6); // Делаем небольшую паузу между запросами
    arsaDataF64 = reqArsagera(date_for_Arsa, url_f64); // Передаем в запрос нужную дату 

    if(buffPreDay > 31) // Для непредвиденных ситуаций
      buffPreDay = 1;
  }
  

  valueMetrik_FA = buffValue_FA;
  valueMetrik_f64 = buffValue_f64;
  Serial.print("\nArsagereFA value:");
  Serial.print(String(valueMetrik_FA));

  Serial.print("\nArsagere6.4 value:");
  Serial.print(String(valueMetrik_f64));
  
  if (preValueMetrik_FA != 0) // Если не первый запрос
  {
    if(preValueMetrik_FA != valueMetrik_FA) // Если предыдущее и текущее значение равны Значит выбран скорее всего тот же день
    {
      if (valueMetrik_FA / preValueMetrik_FA >= 1)
      {
        diffMetrik_FA = round((valueMetrik_FA / preValueMetrik_FA - 1) * 1000) / 10;
        smile_FA = "📈";
      }
      else
      {
        diffMetrik_FA = round((valueMetrik_FA / preValueMetrik_FA - 1) * 1000) / 10;
        smile_FA = "📉";
      }

      if (valueMetrik_f64 / preValueMetrik_f64 >= 1)
      {
        diffMetrik_f64 = round((valueMetrik_f64 / preValueMetrik_f64 - 1) * 1000) / 10;
        smile_f64 = "📈";
      }
      else
      {
        diffMetrik_f64 = round((valueMetrik_f64 / preValueMetrik_f64 - 1) * 1000) / 10;
        smile_f64 = "📉";
      }

      preValueMetrik_FA = valueMetrik_FA;
      preValueMetrik_f64 = valueMetrik_f64;

      String message = "💰Биржевые ориентиры <b>Арсагера</b>💰\n Состояние на <b>" + date_for_Arsa + "</b>\n\nСтоимость паев:\n\n";
      message = message + "<b>Арсагера ФА</b> — <b><u>" + String(valueMetrik_FA) + "</u></b> рублей. \nЦена за пай изменилась на <b>" + diffMetrik_FA + "%</b>" + smile_FA + "\n\n";
      message = message + "<b>Арсагера 6.4</b> — <b><u>" + String(valueMetrik_f64) + "</u></b> рублей. \nЦена за пай изменилась на <b>" + diffMetrik_f64 + "%</b>" + smile_f64 + "\n\n";
      message = message + "#Арсагера";
      bot.sendMessage(CHAT_ID, message , "HTML");
    }
  }
  else // Если первый запрос
  {
    String message = "💰Биржевые ориентиры <b>Арсагера</b>💰\n Состояние на <b>" + date_for_Arsa + "</b>\n\nСтоимость паев:\n\n";
    message = message + "<b>Арсагера ФА</b> — <b><u>" + String(valueMetrik_FA) + "</u></b> рублей.\n\n";
    message = message + "<b>Арсагера 6.4</b> — <b><u>" + String(valueMetrik_f64) + "</u></b> рублей.\n\n";
    message = message + "#Арсагера";
    bot.sendMessage(CHAT_ID, message, "HTML");
    preValueMetrik_FA = valueMetrik_FA;
    preValueMetrik_f64 = valueMetrik_f64;
  }

  delay(oneHourMlSecond); // Делей на часок
  buffPreDay = 1; // Меняем значение отката дней на 1;
  Serial.print("\nEnd Loop");
}
