
/*
 * Sketch: esp8266-adds-station
 * Intended to be run on an ESP8266
 * 
 * Uses station mode on ESP8266
 */

String header = "HTTP/1.1 200 OK\r\nContent-Type: JSON\r\n\r\n";

String html_1 = R"=====(
<!DOCTYPE html>
<html>
  <head>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'/>
  <meta charset='utf-8'>
  <style>
    body {font-size:140%;}
    #main {display: table; margin: auto; padding: 0 10px 0 10px; }
    h2 {text-align:center; }
    #LED_button { padding:10px 10px 10px 10px; width:100%; background-color: #50FF50; font-size: 120%;}
   </style>

<script>
  function fetchWeather() {
    document.getElementById("submit_button").value = "Fetching weather...";
    station_id = document.getElementById("identifier").value;
    ajaxLoad(station_id);
  }

var ajaxRequest = null;
if (window.XMLHttpRequest) { ajaxRequest = new XMLHttpRequest(); }
                           { ajaxRequest = new ActiveXObject("Microsoft.XMLHTTP"); }

function ajaxLoad(station_id) {
  if (!ajaxRequest) {
    alert("AJAX is not supported.");
    return;
  }

  ajaxRequest.open("POST", "station_id=" + station_id, true);
  ajaxRequest.onreadystatechange = function () {
    if (ajaxRequest.readyState == 4 && ajaxRequest.status == 200) {
      var response = JSON.parse(ajaxRequest.responseText);
      document.getElementById("identifier").value = response.identifier;
      document.getElementById("flight_category").innerHTML = "Flight Category: " + response.flight_category;
      document.getElementById("ceiling").innerHTML = "Ceiling: " + response.ceiling;
      document.getElementById("wind_speed").innerHTML = "Wind speed: " + response.wind_speed;
      document.getElementById("submit_button").value = "Fetch Weather";
    }
  }
  ajaxRequest.send();
}
</script>

  <title>esp8266 ADDS Station</title>
</head>

<body>
  <div id='main'>
    <h2>esp8266 ADDS Station</h2>
    <p id="flight_category">Flight Category: </p>
    <p id="ceiling">Ceiling: </p>
    <p id="wind_speed">Winds: </p>
    <form>
      <div>
        <form action="javascript:fetchWeather()">
          <label for="name">Identifier:</label>
          <input type="text" id="identifier" name="station_id">
          <input type="button" id="submit_button" onclick="fetchWeather()" value="Fetch Weather" />
        </form>
      </div>
     </form>
    
  </div>
</body>
</html>

)=====";

#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>

// WIFI
const char* ssid =  "JamesBell'sNetwork_2.4";
const char* password = "Ju51!3@Io";

WiFiServer server(80);

// ADDS
const char* host = "aviationweather.gov";
const int httpsPort = 443;
const char* fingerprint = "1ce610e06d392674ee443a469b449977aca3d472";
String identifier = "KNFL";
String url = "/adds/dataserver_current/httpparam?dataSource=metars&requestType=retrieve&format=xml&hoursBeforeNow=3&mostRecent=true&stationString=";

String flight_category = "";
String visibility = "";
String sky_cover = "";
String cloud_base = "";
int wind_speed = 0;
String ceiling = "";

// NeoPixels
#define LED_PIN    12
#define LED_COUNT    18

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

String getValueforTag(String line, String tag) {
  String matchString;
  if (line.endsWith("</" + tag + ">")) {
    matchString = line;
    int idx = matchString.indexOf(tag + ">");
    int lineStart = idx + tag.length() + 1;
    int lineEnd = matchString.indexOf("</" + tag + ">");
    return matchString.substring(lineStart, lineEnd);
  } else {
    return "";
  }
}

String getValueforParameter(String line, String param) {
  int idx = line.indexOf(param + "=");
  if (idx != -1) {
    int paramStart = idx + param.length() + 2;
    int paramEnd = line.indexOf('"', paramStart);
    return line.substring(paramStart, paramEnd);
  }
  return "";
}

bool isDrinkingWeather(String flight_category, int wind_speed, String ceiling) {
  if (flight_category == "IFR" || ceiling.toInt() < 3000 || wind_speed > 25) {
    strip.clear();
    strip.fill(strip.Color(150, 0, 0));
    strip.show();
    return true;
  }
  strip.clear();
  strip.fill(strip.Color(0, 150, 0));
  strip.show();
  return false;
}

void fetchWeather() {
  // Use WiFiClientSecure to create TLS connection
  WiFiClientSecure client;

  Serial.printf("\nConnecting to %s ...", host);
  Serial.print("\nFetching weather for...");
  Serial.println(identifier);

  // initialize SHA-256 fingerprint
  Serial.printf("Using fingerprint '%s'\n", fingerprint);
  client.setFingerprint(fingerprint);

  // connect to host
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }
  Serial.println("connected");
  
  // verify fingerprint from host
  if (client.verify(fingerprint, host)) {
     Serial.println("certificate matches");
     Serial.println("[Sending a request]");
     client.print(String("GET ") + url + identifier + " HTTP/1.1\r\n" +
                  "Host: " + host + "\r\n" +
                  "User-Agent: BuildFailureDetectorESP8266\r\n" +
                  "Connection: close\r\n" +
                  "\r\n"
                  );

     // debug response to console
     Serial.println("[Response:]");
     
     while (client.connected() || client.available()){
        if (client.available()){
          String line = client.readStringUntil('\r');
//          Serial.println(line);
          String value = getValueforTag(line, "flight_category");
          if (value.length() > 0) {
            flight_category = value;
          }
          value = getValueforTag(line, "visibility_statute_mi");
          if (value.length() > 0) {
            visibility = value;
          }
          // get the sky cover and cloud base
          value = getValueforParameter(line, "sky_cover");
          if (value.length() > 0) {
            sky_cover = value;
            String base = getValueforParameter(line, "cloud_base_ft_agl");
            ceiling = "50000"; //initialize celing to a high value
            // if the sky cover is broken or overcast, we have a ceiling
            if (sky_cover == "BKN" || sky_cover == "OVC") {
              if (base.length() > 0) {
                cloud_base = base;
                if (cloud_base.toInt() < ceiling.toInt()) {
                  ceiling = cloud_base;
                }
              }
            } else {
              cloud_base = base;
            }
          }
          value = getValueforTag(line, "wind_speed_kt");
          if (value.length() > 0) {
            wind_speed = value.toInt();
          }
        }
     } // close the connection
     Serial.println("visibility: " + visibility + ", flight category: " + flight_category);
     Serial.println("sky cover: " + sky_cover + ", cloud base: " + cloud_base);
     Serial.println("ceiling: " +  (ceiling.toInt() >= 50000 ? "None" : ceiling + " feet"));
     Serial.println("wind speed: " + String(wind_speed) + " knots");
     Serial.println(isDrinkingWeather(flight_category, wind_speed, ceiling) ? "Drinking Weather" : "Flying Weather");
     client.stop();
     Serial.println("[Disconnected]"); 
  } else { // close the connection if certificate does not match
     Serial.println("certificate doesn't match");
     client.stop();
     Serial.println("\n[Disconnected]");
  }
}

void connectToWifi(){
  // connect to wifi
  Serial.println("connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // start a server
  server.begin();
  Serial.println("Server started"); 
}



void setup() {
  Serial.begin(115200);

  String request = "";

  // initialize NeoPixels
  strip.begin();
  strip.show();
  strip.setBrightness(50);

  // connect to wifi
  connectToWifi();
}

void loop() {

  // check if a client has connected
  WiFiClient client = server.available();
  if (!client) {
    return;
  }

  // read the first line of the request
  String request = client.readStringUntil('\r');

  Serial.print("request: ");
  Serial.println(request);

  if (request.indexOf("station_id") > 0) {
    // fetch weather for requested station id
    int idx = request.indexOf("station_id=");
    if (idx != -1) {
      int paramStart = idx + 11; // station_id=
      int paramEnd = paramStart + 4;
      identifier = request.substring(paramStart, paramEnd);
    }
    fetchWeather();

    // JSON 
    const size_t capacity = JSON_OBJECT_SIZE(6);
    DynamicJsonDocument doc(capacity);
    
    // serialize json response
    doc["identifier"] = identifier;
    doc["flight_category"] = flight_category;
    doc["ceiling"] = (ceiling.toInt() >= 50000 ? "None" : ceiling + " feet");
    doc["wind_speed"] = String(wind_speed) + " knots";

    serializeJson(doc, Serial);

    // send response to client
    client.print(header);
    serializeJson(doc, client); 
  } else {
    client.flush();
    client.print(header);
    client.print(html_1);
    delay(5);
  }
}
