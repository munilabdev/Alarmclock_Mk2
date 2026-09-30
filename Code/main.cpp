#include <BackgroundAudio.h>
#include <ESP32I2SAudio.h>

#include <SD.h>
#include <SPI.h>

#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include <U8g2lib.h>

#include <PrayerTimes.h>

/*Definition der States*/
enum States {
    MAIN,
    ADHAN,
    ALARM,
    SETTINGS
};

/*Definiere Variablen*/
int A = 13;
int B = 27;
int SET = 14;
unsigned char zustand = MAIN;
int currentPrayer = 1;
int fajrH, fajrM, sunriseH, sunriseM, dhuhrH, dhuhrM, asrH, asrM, maghribH, maghribM, ishaH, ishaM;
int todaysDay = 0, todaysMonth = 0, todaysYear = 0;
int casecount;
unsigned int counter;
/*Bool'sche Werte*/
boolean currSET;
boolean summertime = true;
boolean adhanstatus = false;
boolean alarmstatus = false;
/*char-Strings*/
char fajrTime[10], sunriseTime[10], dhuhrTime[10], asrTime[10], maghribTime[10], ishaTime[10];
char prayerTime[10] = "00:00:00";
char *alarmtime = "00:05:00";
char currentTime[10];


/*Gebetszeiten*/
PrayerTimes pt(51.495, 6.562, 120);
PrayerTimesResult result;

/*WiFi-Konfiguration*/
const char *ssid = "******";
const char *password = "******";

/*NTP-Konfiguration*/
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.t-online.de", 7200);

/*Display-Konfiguration*/
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

/*Audio-Konfiguration für Lautsprecher*/
ESP32I2SAudio audio(25, 32, 33); // BCLK, LRC, DOUT
BackgroundAudioMP3Class<RawDataBuffer<32 * 1024>> BMP(audio);


uint8_t filebuff[1024];
File audiofile;
bool AdhanisPlaying = false;
bool AlarmisPlaying = false;

/*Deklaration der Nebenfunktionen*/
void printDate();
void calculatePrayers();
void printTime();
void printPrayerTime();
void playAdhan();
void handleAudio();
void playAlarm();
void checkDir();
void printSetMenu();
void drawEmptyBox(int, int);
void drawTickedBox(int, int);
void pressBut();
void setAlarm(char *);

void setup() {
    Serial.begin(115200);

    pinMode(A, INPUT);
    pinMode(B, INPUT);
    pinMode(SET, INPUT_PULLUP);
    
    attachInterrupt(digitalPinToInterrupt(A), checkDir, FALLING);
    attachInterrupt(digitalPinToInterrupt(SET), pressBut, LOW);


    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.setDrawColor(1);
    u8g2.setCursor(20, 32);
    u8g2.print("Initialisiere");
    u8g2.sendBuffer();
    u8g2.setFontMode(1);

    /*Initialisierung SD-Karte*/
    SD.begin();

    /*Initialisierung WiFi*/
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print("Attempting to connect to WEP network, SSID: ");
        Serial.println(ssid);

        // wait 10 seconds for connection:
        delay(10000);
    }

    pt.setCustomMethod(19, 17, true, 90);
    pt.setHighLatitudeRule(ANGLE_BASED);
    
    timeClient.begin();
    calculatePrayers();
}

void loop() {
    timeClient.update();
    
    handleAudio();
    Serial.println(currSET);

    if (strcmp(currentTime, prayerTime) == 0){
        zustand = ADHAN;
    }
    else if (strcmp(currentTime, alarmtime) == 0){
        zustand = ALARM;
    }

    if(casecount > 0 && zustand == MAIN){
        zustand = SETTINGS;
    }
    else if (casecount == 0 && zustand == SETTINGS){
        zustand = MAIN;
    }

    switch(currentPrayer){
        case 1:
            strcpy(prayerTime, fajrTime);
            break;
        case 2:
            strcpy(prayerTime, sunriseTime);
            break;
        case 3:
            strcpy(prayerTime, dhuhrTime);
            break;
        case 4:
            strcpy(prayerTime, asrTime);
            break;
        case 5:
            strcpy(prayerTime, maghribTime);
            break;
        case 6:
            strcpy(prayerTime, ishaTime);
            break;
    }

    switch (zustand){
        case MAIN:
            printDate();
            printTime();
            printPrayerTime();
            u8g2.sendBuffer();
            break;
        case ADHAN:
            if (currentPrayer != 2 && adhanstatus){
                playAdhan();
            }
            if (currentPrayer < 6)
                currentPrayer++;
            else{
                currentPrayer = 1;
                calculatePrayers();
            }
            zustand = MAIN;
            break;
        case ALARM:
            playAlarm();
            zustand = MAIN;
            break;
        case SETTINGS:
            printSetMenu();
            switch (casecount){
                case 1:
                    if (currSET){
                        summertime = !summertime;
                        currSET = false;
                    }
                    break;
                case 2:
                    if (currSET){
                        adhanstatus = !adhanstatus;
                        currSET = false;
                    }
                    break;
                case 3:
                    if (currSET){
                        alarmstatus = !alarmstatus;
                        currSET = false;
                    }
                    break;
                case 4:
                    if (currSET)
                        setAlarm(alarmtime);
                    break;
                }
            break;
    }

    
}

void calculatePrayers(){
    
    todaysDay = timeClient.getFormattedDateTime("%d").toInt();
    todaysMonth = timeClient.getFormattedDateTime("%m").toInt();
    todaysYear = timeClient.getFormattedDateTime("%Y").toInt();

    pt.calculate(todaysDay, todaysMonth, todaysYear, fajrH, fajrM, sunriseH, sunriseM, dhuhrH, dhuhrM, asrH, asrM, maghribH, maghribM, ishaH, ishaM);

    sprintf(fajrTime, "%02d:%02d:00", fajrH, fajrM);
    sprintf(sunriseTime, "%02d:%02d:00", sunriseH, sunriseM);
    sprintf(dhuhrTime, "%02d:%02d:00", dhuhrH, dhuhrM);
    sprintf(asrTime, "%02d:%02d:00", asrH, asrM);
    sprintf(maghribTime, "%02d:%02d:00", maghribH, maghribM);
    sprintf(ishaTime, "%02d:%02d:00", ishaH, ishaM);
}


void printDate(){
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 16);
    u8g2.setCursor(3, 13);
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.print(timeClient.getFormattedDateTime("%d.%m.%Y"));
}

void printTime(){

    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 16, 128, 32);
    u8g2.setCursor(6, 42);
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_7x14B_tf);
    timeClient.getFormattedDateTime("%H:%M:%S").toCharArray(currentTime, sizeof(currentTime));
    u8g2.print(currentTime);
}

void printPrayerTime(){

    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 48, 128, 16);
    u8g2.setCursor(3, 61);
    u8g2.setDrawColor(1);
    if(currentPrayer == 1)
        u8g2.print("Fajr:");
    else if (currentPrayer == 2)
        u8g2.print("Sunrise:");
    else if (currentPrayer == 3)
        u8g2.print("Dhuhr:");
    else if (currentPrayer == 4)
        u8g2.print("Asr:");
    else if (currentPrayer == 5)
        u8g2.print("Maghrib:");
    else if (currentPrayer == 6)
        u8g2.print("Isha:");
    u8g2.setCursor(54, 61);
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.print(prayerTime);
}

void playAdhan() {

    audiofile = SD.open("/Adhan.mp3");
    AdhanisPlaying = true;
    BMP.begin();
}

void handleAudio() {

    if (AdhanisPlaying && audiofile) {       
        if (BMP.availableForWrite()) {
            int len = audiofile.read(filebuff, 1024);
            if (len > 0) {
                BMP.write(filebuff, len);
            } else {
                audiofile.close();
                AdhanisPlaying = false;
            }
        }
    }

    if (AlarmisPlaying && audiofile) {       
        if (BMP.availableForWrite()) {
            int len = audiofile.read(filebuff, 1024);
            if (len > 0) {
                BMP.write(filebuff, len);
            } else {
                audiofile.close();
                AlarmisPlaying = false;
            }
        }
    }
}

void playAlarm() {

    audiofile = SD.open("/Alarm.mp3");
    AlarmisPlaying = true;
    BMP.begin();
}

void checkDir(){
    if (digitalRead(B) != LOW){
        if(casecount < 5) casecount++;
        if (counter < 60) counter++;
        else counter = 0;
    }
    else{
        if(casecount > 0) casecount--;
        if (counter > 0) counter--;
        else counter = 59;
    }
}

void printSetMenu(){
    
    u8g2.clearBuffer();

    u8g2.setDrawColor(2);
    switch(casecount){
        case 1:
            u8g2.drawBox(0, 0, 128, 12);
            break;
        case 2:
            u8g2.drawBox(0, 13, 128, 12);
            break;
        case 3:
            u8g2.drawBox(0, 26, 128, 12);
            break;
        case 4:
            u8g2.drawBox(0, 39, 128, 12);
            break;
        case 5:
            u8g2.drawBox(0, 52, 128, 12);
    }

    if(summertime)
        drawTickedBox(3, 1);
    else drawEmptyBox(3, 1);
    u8g2.setCursor(14, 11);
    u8g2.print("Sommerzeit");
    
    if(adhanstatus)
        drawTickedBox(3, 14);
    else drawEmptyBox(3, 14);
    u8g2.setCursor(14, 24);
    u8g2.print("Adhan");
    
    if(alarmstatus)
        drawTickedBox(3, 27);
    else drawEmptyBox(3, 27);
    u8g2.setCursor(14, 37);
    u8g2.print("Alarm");
    
    u8g2.setCursor(14, 50);
    u8g2.printf("%s", alarmtime);
    
    u8g2.drawCircle(6, 58, 3);
    u8g2.setCursor(14, 63);
    u8g2.print("WiFi");
    
    u8g2.setDrawColor(1);

    u8g2.sendBuffer();
}

void drawEmptyBox(int x, int y){
    u8g2.drawBox(x, y, 10, 10);
    u8g2.drawBox(x+1, y+1, 8, 8);
}

void drawTickedBox(int x, int y){
    u8g2.drawBox(x, y, 10, 10);
    u8g2.drawBox(x+1, y+1, 8, 8);
    u8g2.setCursor(x+2, y+10);
    u8g2.print("X");
}

void pressBut(){
    currSET = true;
}

void setAlarm(char *p){
    int i = 0;
    while (i < 7){
        if (currSET == true) {
            counter = 0;
            i =+ 3;
            currSET = false;
        }

        p[i] = counter / 10 + '0';
        p[i+1] = counter % 10 + '0';
    }
}