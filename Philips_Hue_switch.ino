// Encoder library from pjrc - http://www.pjrc.com/teensy/td_libs_Encoder.html
#include <Encoder.h> 

#define SSID "ESP8266"
#define PASS "WIFIPASSWORD"
#define IP1 "192.168.1.237"
#define reqString1  "PUT /api/HomeAutoUser/groups/1/action HTTP/1.1\r\nkeep-alive\r\nHost: 192.168.1.237\r\nContent-Length: "
#define reqString2  "\r\nContent-Type: text/plain;charset=UTF-8\r\n\r\n"
#define reqString3  "GET /api/HomeAutoUser/groups/1/ HTTP/1.1\r\nHost: 192.168.1.237\r\n\r\n"
#define command1  "{\"on\":"
#define command2  ",\"bri\":"
#define command3  "}"

Encoder myEnc(2, 3);

long oldPosition  = 0;
unsigned long time1 = millis(), time2=0;

bool onOff = true,needToSend=false;
int brightness = 255;

int buttonState = 1, buttonStateCount = 0, buttonTransition = 0;


void setup()
{
    Serial.begin(115200);
    startESP8266();
    getStatusOfLights();  //  get current state of lights
    pinMode(4, INPUT);
}

void loop() {
    long newPosition = myEnc.read(); // get the current posisiton of the encoder
    if (newPosition != oldPosition) {
        int diff = newPosition - oldPosition;  // diff is the amount of change since last update
        brightness = constrain(brightness + diff, 0, 255); // update the brightness variable, constraining it to 0-255
        oldPosition = newPosition;  // old is new, new is old
        time2=millis();
        needToSend=true;
        onOff=true;
    }

    if(((millis()-time2)>250)&&needToSend){
        if (!sendHueOnOffCommand()) {
            startESP8266();
        }
        needToSend=false;
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////

    int newButtonState = digitalRead(4);
    int diff = newButtonState - buttonState;
    buttonTransition += diff;
    if (buttonTransition <= -1) {
        buttonStateCount++;
    }

    if (buttonTransition >= 1) {
        buttonStateCount--;
    }

    if (buttonStateCount == 5) {
        onOff=!onOff;
        if (!sendHueOnOffCommand()) {
            startESP8266();
        }
    }

    if (buttonStateCount == -5) {
        //  reset button
        buttonStateCount = 0;
        buttonTransition = 0;
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////

    if ((millis() - time1) > 1000000) { // reset timer when rollover happens.
        time1 = millis();
    }

    if ((millis() - time1) > 15000) { // 1 minute timer
        time1 = millis();
        //get update from hue bridge
        getStatusOfLights();  //  get current state of lights
    }
}

bool startESP8266() {
    int count = 0;
    while (Serial.available()) {
        Serial.read();
    }
    Serial.println("AT+RST");
    while (!Serial.find(".com]")) {
        if (count > 5) {
            count = 0;
            return false;
        }
        count++;
    }
    //delay(6000);
    String cmd = "AT+CWMODE=1";
    Serial.println(cmd);
    delay(1000);
    cmd = "AT+CWJAP=\"";
    cmd += SSID;
    cmd += "\",\"";
    cmd += PASS;
    cmd += "\"";
    while (Serial.available()) {
        Serial.read();
    }
    Serial.println(cmd);
    while (!Serial.find("OK")) {
        if (count > 10) {
            count = 0;
            return false;
        }
        count++;
    }
    //delay(6000);
    Serial.println("AT+CIPMUX=1");
    while (!Serial.find("OK")) {
        if (count > 3) {
            count = 0;
            return false;
        }
        count++;
    }
    //delay(2000);
    return true;
}

bool sendHueOnOffCommand() {

    //build command String. This is the data that is sent to the server
    String command = command1;
    command += onOff ? "true" : "false";
    command += command2;
    command += brightness;
    command += command3;

    // first, see if the ESP8266 has am IP address
    if (sendCommand("AT+CIFSR","192.168.1.",1)){
        // build temporary cmd String. This one tells the ESP8266 to open a TCP connection to IP1 on port 80 using connection slot 4
        String cmd = "AT+CIPSTART=4,\"TCP\",\"";
        cmd += IP1;
        cmd += "\",80";
        // send cmd, looking for response of "Linked", timeout after 3 seconds. If this fails, exit function with false
        if(!sendCommand(cmd,"Linked",3)){return false;};

        // build temporary cmd String. This one is the http request, including the data sent to the server.
        cmd = reqString1;
        cmd += command.length();
        cmd += reqString2;
        cmd += command;

        if(!sendCommand("AT+CIPSEND=4,"+String(cmd.length()+2),">",3)){return false;};
        if(!sendCommand(cmd,"OK",3)){return false;};
        while (!Serial.find("\r\nOK\r\n")) {};
        if(!sendCommand("AT+CIPCLOSE=5","Unlink",3)){return false;};
    } else {
        // if the ESP8266 does not have an IP address, clear the serial buffer and fail
        while (Serial.available()){Serial.read();}
        return false;
    }

    while (Serial.available()) {
        Serial.read();
    }
    return true;
}

bool getStatusOfLights(){
    if (sendCommand("AT+CIFSR","192.168.1.",1)){
        String cmd = "AT+CIPSTART=4,\"TCP\",\"";
        cmd += IP1;
        cmd += "\",80";
        if(!sendCommand(cmd,"Linked",3)){return false;};
        cmd = reqString3;
        if(!sendCommand("AT+CIPSEND=4,"+String(cmd.length()+2),">",3)){return false;};
        Serial.println(cmd);
        while (!Serial.find("any_on\":")) {};
        String res = Serial.readStringUntil('}');
        if(res=="true"){
            onOff=true;
        }else if(res=="false"){
            onOff=false;
        }
        while (!Serial.find("bri\":")) {};
        res = Serial.readStringUntil(',');;
        brightness = res.toInt();
        while (!Serial.find("Unlink")) {};
    } else {
        while (Serial.available()){Serial.read();}
        return false;
    }
    while (Serial.available()){Serial.read();}
    return true;
}

//  sends cmd, listens for response, times out if response not received in timeout seconds
//  cmd is the command to send, response is the expected response (i.e. "ok"), and timeout is obvious
bool sendCommand(String cmd, String response, int timeout){
    int count = 0;
    while (Serial.available()){Serial.read();}
    Serial.println(cmd);
    // convert response string to char array
    int str_len = response.length() + 1;
    char char_array[str_len];
    response.toCharArray(char_array, str_len);

    while (!Serial.find(char_array)) {
        if (count >= timeout) {
            count = 0;
            return false;
        }
        count++;
    }
    return true;
}
