
/***************************************************
  Criado por Douglas Lima Dantas
 ****************************************************/

#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <cc3000_PubSubClient.h>
#include <DHT.h>

#define DHTTYPE DHT11   // DHT 11 (voce pode usar o DHT22 tambem, dependendo do seu modelo
#define DHTPIN 4        // Pino digital onde leremos os dados do DHT

// Pinos usados para controle do CC3000
#define ADAFRUIT_CC3000_IRQ   2
#define ADAFRUIT_CC3000_VBAT  7
#define ADAFRUIT_CC3000_CS    10

// Uso do ICSP para cc3000
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT, SPI_CLOCK_DIVIDER);

#define WLAN_SSID       "<SEU_WIFI>"
#define WLAN_PASS       "<SUA_SENHA>"

// A seguranca pode ser WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2
#define WLAN_SECURITY   WLAN_SEC_WPA2

Adafruit_CC3000_Client client;

union ArrayToIp {
  byte array[4];
  uint32_t ip;
};

// Aqui voce coloca seu ip de trás para frente
ArrayToIp server = { 6, 1, 168, 192 };

#define GAS_PIN 0
#define LUM_PIN 2

// Funcao callback onde podemos ler o payload
void callback (char* topic, byte* payload, unsigned int lengh) {
}

cc3000_PubSubClient mqttclient(server.ip, 1883, callback, client, cc3000);

DHT dht(DHTPIN, DHTTYPE);

int vez;

void setup(void)
{
  vez = 0;
  Serial.begin(115200);
  
  dht.begin();

  Serial.println(F("Hello, CC3000!\n"));

  displayDriverMode();
  
  Serial.println(F("\nInicializando o CC3000 ..."));
  if (!cc3000.begin()) {
    Serial.println(F("Nao foi possivel inicializar o CC3000! Os cabos estao conectados corretamente?"));
    for(;;);
  }
  else {
    Serial.println(F("Carregou"));
  }

  uint16_t firmware = checkFirmwareVersion();
  if ((firmware != 0x113) && (firmware != 0x118)) {
    Serial.println(F("Versao do firmware errada!"));
    for(;;);
  }
  
  displayMACAddress();
  
  Serial.println(F("\nDeletando perfis de conexoes antigas"));
  if (!cc3000.deleteProfiles()) {
    Serial.println(F("Falhou!"));
    while(1);
  }

  char *ssid = WLAN_SSID;             /* Max 32 chars */
  Serial.print(F("\nTentando conectar-se a(o) ")); Serial.println(ssid);
  
  /* NOTE: Secure connections are not available in 'Tiny' mode! */
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Falhou!"));
    while(1);
  }
   
  Serial.println(F("Conectado!"));
  
  Serial.println(F("Requisitando DHCP"));
  while (!cc3000.checkDHCP()) {
    delay(100); // Você pode melhorar aqui inserindo um DHCP Timout
  }

  /* Mostra o IP address DNS, Gateway, etc. */  
  while (!displayConnectionDetails()) {
    delay(1000);
  }
   
   // Conecta ao Broker
   if (!client.connected()) {
     client = cc3000.connectTCP(server.ip, 1883);
     Serial.println("Conectando em TCP");
   }
   
   // did that last thing work? sweet, let's do something
   if(client.connected()) {
    if (mqttclient.connect("ArduinoUnoClient-CC3000-A2", "iotocean", "NZZOLd0O66oLS0vpajiA123")) {
      Serial.println(F("Publicando"));
      mqttclient.publish("/meuprojeto/sensores/debug","A2 está online");
    }
    else {
      Serial.println(F("Ouve um erro ao conectar-se ao broker"));
    }
   } 
}

void loop(void) {

  // Testa se ainda esta conectado
  if (!client.connected()) {
     client = cc3000.connectTCP(server.ip, 1883);
     
     if(client.connected()) {
       if (mqttclient.connect("ArduinoUnoClient-Office-A2")) {
          mqttclient.publish("/meuprojeto/sensores/debug","A2 está online");
       }
     } 
  } else {

    // É importante deixarmos um curto delay entre as leituras de sensores e também entre as publicacoes
    
    char strTemp[10];
    char strHum[10];
    float h = dht.readHumidity();
    delay(50);
    float t = dht.readTemperature();
    Serial.print("Vez: "); Serial.println(vez++);
    Serial.print("Temperatura: "); Serial.println(t);
    Serial.print("Hum: "); Serial.println(h);
    if (isnan(t) || isnan(h))
    {
        Serial.println("Falhou ao ler o sensor DHT");
    }
    delay(100);
    char gas_buf[10];
    char lum_buf[10];
    float nivelGas = pegaGas(GAS_PIN);
    dtostrf(nivelGas, 5, 2, gas_buf);
    delay(100);
    float nivelLuz = pegaLuminosidade(LUM_PIN, 10);
    dtostrf(nivelLuz, 5, 2, lum_buf);
    dtostrf(t, 5, 2, strTemp);
    dtostrf(h, 5, 2, strHum);
    char v[10];
    sprintf(v,"Vez: %d",vez-1);
    
    // Vamos publicar :D
    mqttclient.publish("/meuprojeto/sensores/a2/temperatura", strTemp);delay(100);
    mqttclient.publish("/meuprojeto/sensores/a2/humidade", strHum);delay(100);
    mqttclient.publish("/meuprojeto/sensores/a2/gas",gas_buf);delay(100);
    mqttclient.publish("/meuprojeto/sensores/a2/luminosidade",lum_buf);delay(100);
    mqttclient.publish("/meuprojeto/sensores/debug/vez",v);
  }
  delay(500);
}


void displayDriverMode(void)
{
  #ifdef CC3000_TINY_DRIVER
    Serial.println(F("CC3000 está configurando em modo 'Tiny'"));
  #else
    Serial.print(F("RX Buffer : "));
    Serial.print(CC3000_RX_BUFFER_SIZE);
    Serial.println(F(" bytes"));
    Serial.print(F("TX Buffer : "));
    Serial.print(CC3000_TX_BUFFER_SIZE);
    Serial.println(F(" bytes"));
  #endif
}

/**************************************************************************/
/*!
    @brief  Tenta ler informacoes internas do firmware Arduino
*/
/**************************************************************************/
uint16_t checkFirmwareVersion(void)
{
  uint8_t major, minor;
  uint16_t version;
  
#ifndef CC3000_TINY_DRIVER  
  if(!cc3000.getFirmwareVersion(&major, &minor))
  {
    Serial.println(F("Unable to retrieve the firmware version!\r\n"));
    version = 0;
  }
  else
  {
    Serial.print(F("Firmware V. : "));
    Serial.print(major); Serial.print(F(".")); Serial.println(minor);
    version = major; version <<= 8; version |= minor;
  }
#endif
  return version;
}

/**************************************************************************/
/*!
    @brief  Tries to read the 6-byte MAC address of the CC3000 module
*/
/**************************************************************************/
void displayMACAddress(void)
{
  uint8_t macAddress[6];
  
  if(!cc3000.getMacAddress(macAddress))
  {
    Serial.println(F("Nao foi possível obter o MAC Address!\r\n"));
  }
  else
  {
    Serial.print(F("MAC Address : "));
    cc3000.printHex((byte*)&macAddress, 6);
  }
}


/**************************************************************************/
/*!
    @brief  Recupera informacoes sobre a conexao
*/
/**************************************************************************/
bool displayConnectionDetails(void)
{
  uint32_t ipAddress, netmask, gateway, dhcpserv, dnsserv;
  
  if(!cc3000.getIPAddress(&ipAddress, &netmask, &gateway, &dhcpserv, &dnsserv))
  {
    Serial.println(F("Unable to retrieve the IP Address!\r\n"));
    return false;
  }
  else
  {
    Serial.print(F("\nIP Addr: ")); cc3000.printIPdotsRev(ipAddress);
    Serial.print(F("\nNetmask: ")); cc3000.printIPdotsRev(netmask);
    Serial.print(F("\nGateway: ")); cc3000.printIPdotsRev(gateway);
    Serial.print(F("\nDHCPsrv: ")); cc3000.printIPdotsRev(dhcpserv);
    Serial.print(F("\nDNSserv: ")); cc3000.printIPdotsRev(dnsserv);
    Serial.println();
    return true;
  }
}

/* Essa formula poderá ser melhorada. Aqui se pega a tensao da porta analogica */
float pegaTensao(int porta) {
  int sensorValue = analogRead(porta);
  return sensorValue * (5.0 / 1023.0);
}

// Mede a luminosidade do ambiente em LUX. R = resistencia do divisor de tensao em KOhms. Me baseei em http://emant.com/316002.page
float pegaLuminosidade (int porta, float r) {
  float tensao = pegaTensao(porta);
  return ((2500/tensao)-500)/r;
}

// A tensao na porta aumenta linearmente em relacao a concentracao de gas. Assim podemos medir em porcentagem.
float pegaGas (int porta) {
  float tensao = pegaTensao(porta);
  return (tensao/5)*100;
}

