#include <ModbusMaster.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>

WebServer server(80);

#define MQTT_MAX_PACKET_SIZE 512

// Defina os pinos para controle do DE e RE do MAX485
#define MAX485_DE 18
#define MAX485_RE 19

//#define MAXIM_DE 25


// Configurações de rede WiFi
const char* ssid = "CPC IOT";
const char* password = "XkBRtidP"; 

// Configurações do servidor MQTT
const char* mqtt_server = "172.16.8.243";
const int mqtt_port = 33883;
const char* mqtt_user = NULL;
const char* mqtt_password = NULL;
const char* mqtt_topic = "cpc/v0/d/casa_maquinas/hw:compressor:01/_raw/";

// Cria objetos WiFi e MQTT
WiFiClient espClient;
PubSubClient client(espClient);

// Crie um objeto ModbusMaster
ModbusMaster node;
int id_slave = 1;

void preTransmission()
{
  digitalWrite(MAX485_RE, 1);
  digitalWrite(MAX485_DE, 1);
  //digitalWrite(MAXIM_DE, 0);
}

void postTransmission()
{
  digitalWrite(MAX485_RE, 0);
  digitalWrite(MAX485_DE, 0);
  //digitalWrite(MAXIM_DE, 1);

}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando-se a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop até conseguir se reconectar ao MQTT
  while (!client.connected()) {
    Serial.print("Tentando se conectar ao MQTT...");
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("conectado");
      // Se inscreve no tópico "test/topic"
      client.subscribe("test/modbus");
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentaremos novamente em 5 segundos");
      delay(5000);
    }
  }
}

// Função de callback para tratar mensagens recebidas
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida no tópico [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

void setup()
{
  // Inicialize a comunicação serial
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // Serial2 para RX (GPIO16) e TX (GPIO17)

  setup_wifi();
  // Configura o servidor MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Inicialize os pinos de controle do MAX485
  pinMode(MAX485_RE, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
  //pinMode(MAXIM_DE, OUTPUT);

  
  // Configure os pinos de controle
  digitalWrite(MAX485_RE, 0);
  digitalWrite(MAX485_DE, 0);
  //digitalWrite(MAXIM_DE, 0);

  // Inicie o ModbusMaster
  node.begin(id_slave, Serial2); // ID do dispositivo escravo é 1
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  ElegantOTA.begin(&server);
  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  uint8_t result;
  uint8_t numero = 56; //quantidade de registradores para serem lidos
  uint16_t data[numero+1];

  // Leitura de 6 registradores a partir do endereço 0
  result = node.readHoldingRegisters(1, numero);
  //result = node.readInputRegisters(1, 1);
  //node.readInputRegisters(0,6);
  //node.writeSingleRegister(5,11);

  // Se a leitura foi bem-sucedida
  if (result == node.ku8MBSuccess)
  {
    for (uint8_t j = 1; j <= numero; j++)
    {
      data[j] = node.getResponseBuffer(j-1);
    }

    // Imprima os valores lidos
    for (uint8_t j = 1; j <= numero; j++)
    {
      Serial.print("Registrador ");
      Serial.print(j);
      Serial.print(": ");
      Serial.println(data[j]);
    }
    Serial.println("---------");
    if (!client.connected()) {
    reconnect();
  }
  client.loop();

  //Criação da string para mandar para node-red
  String registradores = "{";
  for (uint8_t n = 1; n <= numero; n++)
    {
      if(n<numero)
      {
      registradores = registradores + "\"Hreg " + n + "\"" + ":" + String(data[n]) + ",";
      }
      else
      {
      registradores = registradores + "\"Hreg " + n + "\"" + ":" + String(data[n]) + "}";
      }
    }
  Serial.println(registradores);

  //dividir mensagem para enviar mqtt
  for (uint8_t i = 0; i < numero; i += 10) {  // Envia 10 registradores por vez
    String partialMessage = "{";
    for (uint8_t j = i + 1; j <= i + 10 && j <= numero; j++) {
        partialMessage += "\"Hreg " + String(j) + "\":" + String(data[j]) + ",";
    }
    partialMessage.remove(partialMessage.length() - 1);  // Remove a última vírgula
    partialMessage += "}";
    
    client.publish(mqtt_topic, partialMessage.c_str());
}


  // Publica uma mensagem no tópico "test/topic"
  /*String message = registradores;

  client.publish("test/topic", message.c_str());*/

  }
  else
  {
    //cria string com o valor do erro em hexadecimal e passa para letra maiuscula
    String erro = String(result, HEX); 
    erro.toUpperCase();

    if (!client.connected()) {
    reconnect();}

    client.loop();
  
  // Publica uma mensagem no tópico "test/topic"
  String message = "Erro: " + erro;

  client.publish(mqtt_topic, message.c_str());

    Serial.print("Erro ao ler registradores: ");
    Serial.println(result, HEX);
    Serial.println("---------");
   }

  server.handleClient();
  ElegantOTA.loop();

  delay(2000);
}