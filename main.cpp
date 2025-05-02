#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include <WiFiClientSecure.h> // для HTTPS-запросов
#include <ESP8266HTTPClient.h> // для HTTP-запросов
#include <UniversalTelegramBot.h> // Для TG-bot
#include <ArduinoJson.h> // Для TG-bot и парсинга Json
#include <time.h> // Для времени

// Данные сети
const char* ssid = "XXX"; // Название сети
const char* password = "XXXX"; // Пароль сети

// Запускаем бот
#define BOTtoken "XXX"  // Токен бота
#define CHAT_ID "XXX" // ID пользователя или группы

#ifdef ESP8266 // Сертфикикат подключения дл TG
X509List cert(TELEGRAM_CERTIFICATE_ROOT);
#endif


WiFiClientSecure clientTgBot;
UniversalTelegramBot bot(BOTtoken, clientTgBot);

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
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi..");
    }
    // Выводим IP нашего контроллера 
    Serial.println(WiFi.localIP());
    Serial.println("Connect successful");
    delay(1000);


}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

String reqArsagera(String date_for_Arsa) // Функция для получения данных с Арсагера API
{
    WiFiClientSecure arsaClient; // Создаем клиента для подключения 

    arsaClient.setInsecure(); // Говорим Что будем подключаться без использования сертификатов
    Serial.print("Connecting to ");
    Serial.print("https://arsagera.ru\n");

    if (!arsaClient.connect("185.44.14.62", 443)) { // Конектимся на IP арсагеры 
        Serial.println("Connection FAILED To arsaURL");
        return "0"; // Если ошибка конекта возвращаемся 
    }
    else
    {
        Serial.println("Connection To arsaURL SUCCESSEFUL");
    }
    Serial.print("Requesting URL: ");
    Serial.println("https://arsagera.ru/api/v1/funds/fa/fund-metrics/?");

    arsaClient.println(String("GET https://arsagera.ru/api/v1/funds/fa/fund-metrics/?date=") + date_for_Arsa + " HTTP/1.0"); // Кидаем запрос на Арсагера API с нужной датой
    arsaClient.println("Host: 185.44.14.62"); // Хост арсагеры 
    arsaClient.println("Connection: close");
    arsaClient.println();

    Serial.println("Request sent");

    while (arsaClient.connected()) { // Получаем данные 
        String line = arsaClient.readStringUntil('\n');
        if (line == "\r") {
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

String parsingArsagera(String arsaData) // Функция вывод значение метрики "nav_per_share": XXXXX.XX
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
    Serial.println("START - parsingArsagera");
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

    if (flag == false) // В случае Если мы проишлись по данным и не нашли их То прекращаем дальнейший поиск 
        return "0";
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
        return "0";
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
        return "0";

    Serial.println("END - parsingArsagera");
    return value;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



String getTime()
{

    return "0";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop()
{
    String date_for_Arsa = "2025-04-29";

    String arsaData = reqArsagera(date_for_Arsa); // Передаем в запрос нужную дату 
    if (arsaData != "0") // Если ошибка конекта нет
    {

        Serial.print("Arsagere requests:");
        Serial.print(arsaData);

        String valueMetrik = parsingArsagera(arsaData);
        if (valueMetrik != "0") // Если ошибки парсинга метрики нет 
        {
            Serial.print("Arsagere value:");
            Serial.print(valueMetrik);

            bot.sendMessage(CHAT_ID, "💰Биржевые ориентиры <b>Арсагера ФА</b>💰 \n\nСтоимость пая на дату <b>" + date_for_Arsa + "</b> — <b><u>" + valueMetrik + "</u></b> рублей \n\n#Арсагера_ФА", "");

        }

    }


    delay(86400000); // Сутки
}
