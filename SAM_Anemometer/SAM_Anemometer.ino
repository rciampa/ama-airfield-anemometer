/*
   Author: Richard Ciampa
   Date:   6/11/2016

   This code takes the output from an Adafruit anemometer
   (product ID 1733) and converts it into a wind speed.
*/

//Required for ethernet shield
#include <Ethernet.h>

/*
   Set variables and constants for the anemometer
*/
float ms_wind_speed = 0;         // Wind speed in meters per second (m/s)
float gust_ms = 0;               // Gust speed in meters per second (m/s)

//Last connected to the server, in milliseconds
uint32_t lastConnectionTime = 0;
//Last wind speed update, in milliseconds
uint32_t lastWindSpeedUpdateTime = 0;

void setup(){  
  //Start the serial connection
  Serial.begin(9600);
  while (!Serial);
  
  if (Serial) {
    Serial.println(F("Starting AMA airfield anemometer............\n"));
  }

  /************** Ethernet Setup *************/
    do {
    byte * mac = set_mac();
    Serial.println(F("Starting ethernet controller....."));
    if (Ethernet.begin(mac)) {
      /*
        Wait a second for the ethernet controller to initialize and
        obtain an ip address lease.
      */
      delay(1000);
      break;
    }else{
      free(mac);//Free the memory so we can loop again
      Serial.println(F("[ERROR]: Failed to start ethernet controller\n"));
    }
  } while (true);

  print_general_info();
}

void loop(){
  if (millis() - lastWindSpeedUpdateTime > sensor_read_interval(3)) {
    read_wind_speed();
  }
  if (millis() - lastConnectionTime > posting_interval(20)) {
    http_post_request();
  }
}

char * request_host(){
  return "salinasareamodelers.org";
}

/*
   Checks to see if a connection to the wind speed server is available
*/
void check_server_connection() {
  EthernetClient client;
  uint8_t const_srvr_port = 80;
  Serial.print(F("\nConnection status: "));
  if (client.connect(request_host(), const_srvr_port)) {
    Serial.println(F("available"));
    //Disconnect from the server
    client.stop();
  } else {
    Serial.println(F("unavailable"));
  }
}

/*
   Makes a HTTP connection to the server and submits wind speed data.
*/
void http_post_request() {

  Serial.println(F("\nAttempting to update wind speed service..........\n"));
  //Initialize the library instance:
  EthernetClient client;
  uint8_t const_srvr_port = 80;

  //If there's a successful connection:
  if (client.connect(request_host(), const_srvr_port)) {
    Serial.println(F("connecting..."));
    //Send the HTTP GET request:
    client.print(F("GET /lib-anemometer.php?ms="));
    client.print(ms_wind_speed);
    client.print(F("&gust_ms="));
    client.print(gust_ms);
    client.println(F("&id=2300 HTTP/1.1"));
    client.print(F("Host: "));
    client.println(request_host());
    client.println(F("User-Agent: AMA-Anemometer/1.0"));
    client.println(F("Connection: close"));
    client.println();

       /*
     * Wait for the http request response stream to be available
     */
     Serial.print(F("\nWaiting for http response... "));
    do {
      if (client.available()) {
        Serial.println(F(" Done!\n"));
        print_client_data(&client);
        // note the time that the connection was made:
        lastConnectionTime = millis();
        Serial.println(F("Update completed.........."));
        gust_ms = 0;
        break;
      }
    } while (!client.available());

    // close any connection before send a new request.
    // This will free the socket on the WiFi shield
    client.flush();
    client.stop();
    
  } else {
    // if you couldn't make a connection:
    Serial.print(F("connection failed"));
  }
}

/*
   Reads the wind speed from the anemometer
*/
void read_wind_speed() {

  Serial.println(F("\nReading wind speed......"));

  // Variable stores the value direct from the analog pin
  int sensor_value = 0;
  // Variable that stores the voltage (in Volts) from the anemometer being sent to the analog pin
  float sensor_voltage = 0;
  //Get a value between 0 and 1023 from the analog pin connected to the anemometer
  sensor_value = analogRead(const_sensor_pin());
  //Convert sensor value to actual voltage
  sensor_voltage = sensor_value * const_vdc_conversion();

  //Convert voltage value to wind speed using range of max and min voltages and wind speed for the anemometer
  if (sensor_voltage <= const_voltage_min()) {
    // Check if voltage is below minimum value. If so, set wind speed to zero.
    ms_wind_speed = 0;
  } else {
    //For voltages above minimum value, use the linear relationship to calculate wind speed.
    ms_wind_speed = ((sensor_voltage - const_voltage_min()) * cosnt_wind_speed_max()) / (const_voltage_max() - const_voltage_min());
  }

  //Print voltage and windspeed to serial
  Serial.print(F("Voltage: "));
  Serial.print(sensor_voltage);
  Serial.print(F("\t"));
  Serial.print(F("Wind speed: "));
  Serial.println(ms_wind_speed);

  update_gust();
  lastWindSpeedUpdateTime = millis();
}

void update_gust() {
  if (ms_wind_speed > gust_ms) {
    gust_ms = ms_wind_speed;
  }
}

/*
   if there's incoming data from the net connection.
   send it out the serial port. This is for debugging
   purposes only:
*/
void print_client_data(EthernetClient * client) {
  while (client->available()) {
    char c = client->read();
    Serial.write(c);
  }
}

/*
   Defines the pin that the anemometer output is connected to
*/
const int const_sensor_pin () {
  return A0;
}

/*
   Anemometer ID
*/
const char * unit_id() {
  return "3625";
}

/*
   We are going to use dhcp, this will allow the ip address to
   be managed by lease reservations in the local dhcp server.

   We will only need to provide the mac address, a byte array to hold
   the media access control (MAC) physical address is required.
*/
byte * set_mac(){
  byte * mac = (byte *) malloc(6);
  mac[0] = 0xDE;
  mac[1] = 0xAD;
  mac[2] = 0xBE;
  mac[3] = 0xEF;
  mac[4] = 0xFE;
  mac[5] = 0xED;
  
  return mac;
}

/*
   Prints dhcp network configuration information to the serial port.
*/
void print_network_information() {
  Serial.println(F("\n------------- DHCP Network Configuration --------------"));
  Serial.print(F("IP address: "));
  Serial.println(Ethernet.localIP());
  Serial.print(F("Subnet mask: "));
  Serial.println(Ethernet.subnetMask());
  Serial.print(F("Gateway address: "));
  Serial.println(Ethernet.gatewayIP());
  Serial.println(F("-------------------------------------------------------"));
}

void print_general_info(){
  
  Serial.println(F("**************** AMA Airfield Amemometer ****************"));
  Serial.println(F("Anemometer Configuration:"));
  Serial.print(F("Anemometer connection pin: "));
  Serial.println(const_sensor_pin());
  Serial.print(F("Minimum anemometer input voltage: "));
  Serial.println(const_voltage_min());
  Serial.print(F("Maximum anemometer input voltage: "));
  Serial.println(const_voltage_max());
  Serial.print(F("Maximum wind speed [m/s, mph, knots]: "));
  Serial.println(cosnt_wind_speed_max());
  Serial.print(F("Note: AREF pin must have a constant 2.0 vdc supplied"));
  /*
    Print the ethernet controller ip configuration to thr serial
    port.
  */
  print_network_information();
  Serial.print(F("Air speed server: ")); Serial.print(request_host());
  check_server_connection();
  Serial.println(F("*********************************************************"));
}

/*
   delay between updates, in milliseconds the "L" is
   needed to use long type numbers
*/
uint32_t posting_interval(uint32_t milis){
  return milis * 1000L;
}

uint32_t sensor_read_interval(uint16_t milis){
//Delay between sensor readings, measured in milliseconds (ms)
return milis * 1500L;
}

/*

   Anemometer Technical Variables
   The following variables correspond to the anemometer sold by Adafruit,
   but could be modified to fit other anemometers.

*/


/*
   This constant maps the value provided from the analog read function,
   which ranges from 0 to 1023, to actual voltage, which ranges from 0V to 5V
*/
const float const_vdc_conversion() {
  return .0048828125;
}

/*
   Mininum output voltage from anemometer in mV.
*/
const float const_voltage_min() {
  return .4;
}

/*
   Maximum output voltage from anemometer in mV.
*/
const float const_voltage_max() {
  return 2.0;
}

/*
   Wind speed in meters/sec corresponding to maximum voltage
*/
const float cosnt_wind_speed_max() {
  return 32;
}

/*
   Wind speed in meters/sec corresponding to minimum voltage
*/
const float wind_speed_min() {
  return 0.0;
}

