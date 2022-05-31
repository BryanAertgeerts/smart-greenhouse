#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <DHT.h>
#include <TaskScheduler.h>
#include <WiFi.h>
#include <PubSubClient.h>

//--------------------------------------------------------------------------------------------------------------------------

const char* ssid = "LAPTOMIUM";                   //wifi naam
const char* password = "123456789";               //wifi wachtwoord
const char* mqttServer = "192.168.137.66";        //IP addres van de raspberry
const int mqttPort = 1883;                        //de gebruikte port
const char* mqttUser = "username";                //uw gebruikers naam van MQTT
const char* mqttPassword = "Admin";               //uw wachtwoord van MQTT
const char* clientID = "MQTTInfluxDBBridge";      // MQTT client ID

//--------------------------------------------------------------------------------------------------------------------------

#define RST_PIN 2
#define SS_PIN 5

#define DHTPIN 4
#define DHTTYPE DHT11

#define GROWled 15
#define MOISTsensor 14
#define Pump 25
#define Heater 13
#define Cooler 12

//--------------------------------------------------------------------------------------------------------------------------

const unsigned long SECOND = 1000;
const unsigned long MINUTE = 60 * SECOND;
const unsigned long HOUR = 60 * MINUTE;

//--------------------------------------------------------------------------------------------------------------------------

int temp = 0;
int MOISTvalue;

//--------------------------------------------------------------------------------------------------------------------------

void GROWledrucola();
Task ledrucola(24 * HOUR, TASK_FOREVER, &GROWledrucola);

void GROWledPlant2();
Task ledplant2(24 * HOUR, TASK_FOREVER, &GROWledPlant2);

void TEMPdisplay();
Task TEMPdis(10 * SECOND, TASK_FOREVER, &TEMPdisplay);

void GROUNDmoist();
Task moist(1 * HOUR, TASK_FOREVER, &GROUNDmoist);

void verwarming();
Task TEMPcontrol(10 * SECOND, TASK_FOREVER, &verwarming);

//--------------------------------------------------------------------------------------------------------------------------

byte readCard[4];
String rucola = "93375299";  // REPLACE this Tag ID with your Tag ID!!!
String Plant2 = "B3B896D";
String tagID = "";

//--------------------------------------------------------------------------------------------------------------------------

// Create instances
MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);
Scheduler runner;
WiFiClient espClient;
PubSubClient client(espClient);

//-------------------------------------------------------------------------gdg-------------------------------------------------

void setup()
{
  pinMode(GROWled, OUTPUT);
  digitalWrite(GROWled, LOW);

  pinMode(Pump, OUTPUT);
  digitalWrite(Pump, LOW);

  pinMode(Heater, OUTPUT);
  digitalWrite(Heater, LOW);

  pinMode(Cooler, OUTPUT);
  digitalWrite(Cooler, LOW);

  //--------------------------------------------------------------------------------------------------------------------------

  runner.init();
  runner.addTask(ledrucola);
  ledrucola.enable();

  runner.addTask(ledplant2);
  ledplant2.enable();

  runner.addTask(TEMPdis);
  TEMPdis.enable();

  runner.addTask(moist);
  moist.enable();

  runner.addTask(TEMPcontrol);
  TEMPcontrol.enable();

  //--------------------------------------------------------------------------------------------------------------------------

  // Initiating
  Serial.begin(9600);
  SPI.begin(); // SPI bus
  mfrc522.PCD_Init(); // MFRC522
  WiFi.begin(ssid, password);

  //--------------------------------------------------------------------------------------------------------------------------

  dht.begin();
  lcd.begin(); // LCD screen
  lcd.backlight();

  lcd.clear();
  lcd.print(" Access Control ");
  lcd.setCursor(0, 1);
  lcd.print("Scan Your Card>>");


  //--------------------------------------------------------------------------------------------------------------------------

  //laat zien dat de wifi verbind, verbonden is of gefaald is met verbinden
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
  client.setServer(mqttServer, mqttPort);
  dht.begin();
}

//--------------------------------------------------------------------------------------------------------------------------

void loop()
{


  //--------------------------------------------------------------------------------------------------------------------------

  //laat zien dat de ESP32 verbind, verbonden is of gefaald is met verbinden met de MQTT server
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32Client", mqttUser, mqttPassword ))
    {
      Serial.println("connected");
    } else
    {
      Serial.print("failed with state ");
      Serial.println(client.state());
      delay(2000);
    }
  }

  //--------------------------------------------------------------------------------------------------------------------------


  //Wait until new tag is available
  while (getID())
  {
    lcd.clear();
    lcd.setCursor(0, 0);

    if (tagID == rucola)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("RUCOLA");
      Serial.println("---------  nieuwe plant ingesteld ---------");
      Serial.println("rucola");
      Serial.println(tagID);
      String rucla = "100";
      client.publish("greenhouse/bak1/cardruc", rucla.c_str());
      String plant2 = "1";
      client.publish("greenhouse/bak2/cardplant2", plant2.c_str());
    }
    else if (tagID == Plant2)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Plant2");
      Serial.println("---------  nieuwe plant ingesteld ---------");
      Serial.println("Plant2");
      Serial.println(tagID);
      String rucla = "1";
      client.publish("greenhouse/bak1/cardruc", rucla.c_str());
      String plant2 = "100";
      client.publish("greenhouse/bak2/cardplant2", plant2.c_str());
    }
    else
    {
      lcd.print(" Access Denied!");
      Serial.println("---------  toegang gevraagd  ---------");
      Serial.println("nice try but nop");
      Serial.println(tagID);
      delay(10 * SECOND);
      lcd.clear();
      lcd.print(" Access Control ");
      lcd.setCursor(0, 1);
      lcd.print("Scan Your Card>>");
    }
  }
  runner.execute();
}

//--------------------------------------------------------------------------------------------------------------------------

//Read new tag if available
boolean getID()
{
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return false;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) { //Since a PICC placed get Serial and continue
    return false;
  }
  tagID = "";
  for ( uint8_t i = 0; i < 4; i++) { // The MIFARE PICCs that we use have 4 byte UID
    //readCard[i] = mfrc522.uid.uidByte[i];
    tagID.concat(String(mfrc522.uid.uidByte[i], HEX)); // Adds the 4 bytes in a single String variable
  }
  tagID.toUpperCase();
  mfrc522.PICC_HaltA(); // Stop reading
  return true;
}

//--------------------------------------------------------------------------------------------------------------------------

void GROWledrucola()
{
  if (digitalRead(GROWled) == LOW && tagID == rucola)
  {
    digitalWrite(GROWled, HIGH);
    Serial.println("rucola   aan");
    String lichtONruc = "100";
    client.publish("greenhouse/bak1/licht", lichtONruc.c_str());

    delay(12 * HOUR);
  } else
  {
    digitalWrite(GROWled, LOW);
    Serial.println("rucola   uit");
    String lichtOFFruc = "1";
    client.publish("greenhouse/bak1/licht", lichtOFFruc.c_str());
  }
}
//--------------------------------------------------------------------------------------------------------------------------

void GROWledPlant2()
{
  if (digitalRead(GROWled) == LOW && tagID == Plant2)
  {
    digitalWrite(GROWled, HIGH);
    Serial.println("Plant 2  aan");
    String lichtON = "100";
    client.publish("greenhouse/bak2/licht", lichtON.c_str());
    delay(10 * HOUR);
  } else
  {
    digitalWrite(GROWled, LOW);
    Serial.println("plant 2  uit");
    String lichtOFF = "1";
    client.publish("greenhouse/bak2/licht", lichtOFF.c_str());
  }
}

//--------------------------------------------------------------------------------------------------------------------------

void TEMPdisplay()
{
  if (tagID == rucola || tagID == Plant2)
  {
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    temp = t;
    lcd.setCursor(0, 1);
    lcd.print("Temp: ");
    lcd.setCursor(7, 1);
    lcd.print(t);
    Serial.println(t);
    lcd.setCursor(13, 1);
    lcd.print((char)223);
    lcd.setCursor(14, 1);
    lcd.print("C ");
    String humi = String(h);
    String tempi = String(t);

    if (tagID == rucola)
    {
      client.publish("greenhouse/bak1/humi", humi.c_str());
      client.publish("greenhouse/bak1/tempi", tempi.c_str());
    }
    else if (tagID == Plant2)
    {
      client.publish("greenhouse/bak2/humi", humi.c_str());
      client.publish("greenhouse/bak2/tempi", tempi.c_str());
    }
  }
}

//--------------------------------------------------------------------------------------------------------------------------

void GROUNDmoist()
{
  MOISTvalue = analogRead(MOISTsensor);
  MOISTvalue = map(MOISTvalue, 1024, 4095, 100, 0);
  Serial.print(MOISTvalue);
  Serial.println("%");
  String MOISTstring = String(MOISTvalue);
  if (tagID == rucola)
  {
    client.publish("greenhouse/bak1/moistsensor", MOISTstring.c_str());
  }
  else if (tagID == Plant2)
  {
    client.publish("greenhouse/bak2/moistsensor", MOISTstring.c_str());
  }
  if (MOISTvalue <= 50)
  {
    digitalWrite(Pump, HIGH);
    Serial.println("PUMP     aan");
    String PUMPaan = "100";
    if (tagID == rucola)
    {
      client.publish("greenhouse/bak1/pomp", PUMPaan.c_str());
    }
    else if (tagID == Plant2)
    {
      client.publish("greenhouse/bak2/pomp", PUMPaan.c_str());
    }
    delay(250);
    digitalWrite(Pump, LOW);
    String PUMPoff = "1";
    if (tagID == rucola)
    {
      client.publish("greenhouse/bak1/pomp", PUMPoff.c_str());
    }
    else if (tagID == Plant2)
    {
      client.publish("greenhouse/bak2/pomp", PUMPoff.c_str());
    }
  }
}

//--------------------------------------------------------------------------------------------------------------------------

void verwarming()
{
  if (tagID == rucola || tagID == Plant2)
  {
    if (temp < 15)
    {
      digitalWrite(Cooler, LOW);
      digitalWrite(Heater, HIGH);
      String HEATON = "100";
      if (tagID == rucola)
      {
        client.publish("greenhouse/bak1/heater", HEATON.c_str());
      }
      else if (tagID == Plant2)
      {
        client.publish("greenhouse/bak2/heater", HEATON.c_str());
      }
    }
    if (temp >= 15)
    {
      digitalWrite(Cooler, LOW);
      digitalWrite(Heater, LOW);
      String HEATOFF = "1";
      if (tagID == rucola)
      {
        client.publish("greenhouse/bak1/heater", HEATOFF.c_str());
      }
      else if (tagID == Plant2)
      {
        client.publish("greenhouse/bak2/heater", HEATOFF.c_str());
      }
    }
    if (temp <= 17)
    {
      digitalWrite(Cooler, LOW);
      digitalWrite(Heater, LOW);
      String COOLEROFF = "1";
      if (tagID == rucola)
      {
        client.publish("greenhouse/bak1/cooler", COOLEROFF.c_str());
      }
      else if (tagID == Plant2)
      {
        client.publish("greenhouse/bak2/cooler", COOLEROFF.c_str());
      }
    }
    if (temp > 17)
    {
      digitalWrite(Cooler, HIGH);
      digitalWrite(Heater, LOW);
      String COOLERON = "100";
      if (tagID == rucola)
      {
        client.publish("greenhouse/bak1/cooler", COOLERON.c_str());
      }
      else if (tagID == Plant2)
      {
        client.publish("greenhouse/bak2/cooler", COOLERON.c_str());
      }
    }
  }
}

//--------------------------------------------------------------------------------------------------------------------------
