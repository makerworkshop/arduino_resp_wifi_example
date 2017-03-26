// CC3000 library from Adafruit. See https://github.com/adafruit/Adafruit_CC3000_Library
#include <Adafruit_CC3000.h>
#include <SPI.h>

// RESP library for encoding/decoding RESP messages.
#include <resp.h>

// Bee library for communicating devices using RESP encoded messages.
#include <Bee.h>

// See https://github.com/adafruit/Adafruit_CC3000_Library/blob/master/examples/EchoServer/EchoServer.ino
#define ADAFRUIT_CC3000_IRQ   3
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10

// AP settings.
#define WLAN_SSID       "SSID NAME (32 CHARS MAX)"
#define WLAN_PASS       "SSID PASSWORD"

// AP security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

// Port 6379 is used for redis servers, let's use 16379 for bee servers.
#define LISTEN_PORT           16379

// Serial port speed for printing debug messages.
#define SERIAL_BAUD_RATE      115200

// WiFiConn is a communication channel for Bee that can be used to send and
// receive messages over Wi-Fi.
class WiFiConn :
public Bee {
  typedef Bee super;
  Adafruit_CC3000 *cc3000;
public:
  Adafruit_CC3000_Server *server;
  bool Open();
  bool Read(unsigned char *);
  bool Write(unsigned char *, int);
  bool Close();
  respObject *OnMessage(respObject *); // OnMessage is commented since it's
                                          // an optional method.
  bool displayConnectionDetails();
};

// Open connects to the AP and sets up a listening server.
bool WiFiConn::Open()
{
  Serial.println("Setting up CC3000...");

  this->cc3000 = new Adafruit_CC3000(
    ADAFRUIT_CC3000_CS,
    ADAFRUIT_CC3000_IRQ,
    ADAFRUIT_CC3000_VBAT,
    SPI_CLOCK_DIVIDER
  );

  this->server = new Adafruit_CC3000_Server(LISTEN_PORT);

  if (!this->cc3000->begin()) {
    Serial.println("Unable to initialize CC3000 chip. Check your wiring.");
    while(1);
  }

  if (!this->cc3000->deleteProfiles()) {
    Serial.println("Failed to clean after old CC3000 profiles.");
    while(1);
  }

  Serial.println("Connecting to AP...");
  if (!this->cc3000->connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println("Failed to connect to AP:");
    Serial.println(WLAN_SSID);
    while(1);
  }

  Serial.println("Waiting for DHCP data...");
  while (!this->cc3000->checkDHCP()) {
    delay(100);
  }

  while (!this->displayConnectionDetails()) {
    delay(100);
  }

  Serial.println("Starting up server...");
  this->server->begin();

  return true;
}


// Read reads a byte from the connected client (if any) and sets the value to
// <*b>.
bool WiFiConn::Read(unsigned char *b)
{
  Adafruit_CC3000_ClientRef client = this->server->available();
  if (client) {
    if (client.available() > 0) {
      *b = client.read();
      return true;
    }
  }
  return false;
}

// Write writes the given buffer to the serial port.
bool WiFiConn::Write(unsigned char *buf, int len)
{
  this->server->write(buf, len);
  return true;
}

// Close disables WiFi communication.
bool WiFiConn::Close()
{
  this->cc3000->stop();
  return true;
}

// displayConnectionDetails shows the IP address given by the DHCP server.
bool WiFiConn::displayConnectionDetails()
{
  uint32_t ipAddress;
  Serial.print("IP Address: ");

  if (!this->cc3000->getIPAddress(&ipAddress, NULL, NULL, NULL, NULL)) {
    Serial.println("?");
  } else {
    this->cc3000->printIPdotsRev(ipAddress);
    Serial.println("");
    return true;
  }
  return false;
}


// OnMessage implements some demo messages that are interpreted if
// super::OnMessage does not know how to handle them.
respObject *WiFiConn::OnMessage(respObject *in)
{
  respObject *out = NULL;

  // Calling super::OnMessage is completely optional, you may as well ignore
  // any pre-defined message interpretation.
  out = super::OnMessage(in);

  if (out == NULL) {

    // FOO command returns a status BAR message.
    // FOO
    // +BAR
    if (RESP_TOKEN_EQUALS(in, 0, "FOO")) {
      out = createRespString(RESP_OBJECT_STATUS, (unsigned char *)"BAR");
    }

    // SUM command sums two integers and returns the resulting number.
    // SUM 5 4
    // :9
    if (RESP_TOKEN_EQUALS(in, 0, (unsigned char *)"SUM")) {
      int a = RESP_TOKEN_TO_INT(in, 1);
      int b = RESP_TOKEN_TO_INT(in, 2);
      out = createRespInteger(a + b);
    }

  }

  return out;
}

// Global pointer to WiFiConn instance.
WiFiConn *conn;

void setup()
{
  Serial.begin(SERIAL_BAUD_RATE);

  // Setting up communication channel over Wi-Fi.
  conn = new WiFiConn();
  if (!conn->Open()) {
    Serial.println("Failed to configure Wi-Fi channel.");
    while(1);
  }
}

void loop()
{
  // Reading next message.
  conn->NextMessage();
}
