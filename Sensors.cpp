#include "Sensors.hpp"

// Humidity sensor
Adafruit_AM2320 am2320 = Adafruit_AM2320();

/******************************************************************************
*   S E N S O R   P R I V A T E   M E T H O D S
******************************************************************************/

String Sensors::hwSerialRead() {
    int try_sensor_read = 0;
    String txtMsg = "";
    while (txtMsg.length() < 32 && try_sensor_read++ < SENSOR_RETRY) {
        while (_serial->available() > 0) {
            char inChar = _serial->read();
            txtMsg += inChar;
        }
    }
    if (try_sensor_read > SENSOR_RETRY) {
        onPmSensorError("Generic sensor read fail!");
    }
    return txtMsg;
}

/**
 *  @brief Particulate meter sensor generic read 
 * 
 *  Devices supported:
 * 
 *  - Honeywell HPMA115S0
 *  - Plantower
 */
bool Sensors::pmGenericRead() {
    String txtMsg = hwSerialRead();
    if (txtMsg[0] == 66) {
        if (txtMsg[1] == 77) {
            if (devmode) Serial.print("-->[HPMA] read > done!");
            pm25 = txtMsg[6] * 256 + byte(txtMsg[7]);
            pm10 = txtMsg[8] * 256 + byte(txtMsg[9]);
            if (pm25 > 1000 && pm10 > 1000) {
                onPmSensorError("Honeywell out of range pm25 > 1000");
            }
            else
                return true;
        } else {
            onPmSensorError("invalid Honeywell sensor header!");
        }
    }
    onPmSensorError("Honeywell read error!");
    return false;
} 

/**
 *  @brief Panasonic SNGC particulate meter sensor read.
 */
bool Sensors::pmPanasonicRead() {
    String txtMsg = hwSerialRead();
    if (txtMsg[0] == 02) {
        if(devmode) Serial.print("-->[SNGC] read > done!");
        pm25 = txtMsg[6] * 256 + byte(txtMsg[5]);
        pm10 = txtMsg[10] * 256 + byte(txtMsg[9]);
        if (pm25 > 2000 && pm10 > 2000) {
            onPmSensorError("Panasonic out of range pm25 > 2000");
        }
        else
            return true;
    } else {
        onPmSensorError("invalid detection PM sensor header!");
    }
    return false;
}

/**
 *  @brief Sensirion SPS30 particulate meter sensor read.
 */
bool Sensors::pmSensirionRead() {
    uint8_t ret, error_cnt = 0;
    delay(35);  //Delay for sincronization
    do {
        ret = sps30.GetValues(&val);
        if (ret == ERR_DATALENGTH) {
            if (error_cnt++ > 3) {
                log_w("[SPS30] Error during reading values: %d", ret);
                return false;
            }
            delay(1000);
        } else if (ret != ERR_OK) {
            pmSensirionErrtoMess((char *)"-->[W][SPS30] Error during reading values: ", ret);
            return false;
        }
    } while (ret != ERR_OK);

    if (devmode) Serial.print("-->[SPS30] read > done!");

    pm25 = round(val.MassPM2);
    pm10 = round(val.MassPM10);

    if (pm25 > 1000 && pm10 > 1000) {
        onPmSensorError("Sensirion out of range pm25 > 1000");
        return false;
    }

    return true;
}

bool Sensors::pmSensorRead() {
    switch (device_type) {
        case Honeywell:
            return pmGenericRead();
            break;

        case Panasonic:
            return pmPanasonicRead();
            break;

        case Sensirion:
            return pmSensirionRead();
            break;

        default:
            return false;
            break;
    }
}

void Sensors::am2320Read() {
    humi = am2320.readHumidity();
    temp = am2320.readTemperature();
    if (isnan(humi)) humi = 0.0;
    if (isnan(temp)) temp = 0.0;
}

void Sensors::onPmSensorError(const char *msg) {
    Serial.print("-->[W][PMSENSOR] ");
    Serial.println(msg);
    if(_onErrorCb)_onErrorCb(msg);
}

void Sensors::pmSensirionErrtoMess(char *mess, uint8_t r) {
    char buf[80];
    Serial.print("-->[W][SENSIRION] ");
    Serial.print(mess);
    sps30.GetErrDescription(r, buf, 80);
    Serial.println(buf);
    onPmSensorError(mess);
}

void Sensors::pmSensirionErrorloop(char *mess, uint8_t r) {
    if (r) pmSensirionErrtoMess(mess, r);
    else Serial.println(mess);
}
/**
 * Particule meter sensor (PMS) init.
 * 
 * Hardware serial init for multiple PM sensors, like
 * Honeywell, Plantower, Panasonic, Sensirion, etc.
 * 
 * @param pms_type PMS type, please see DEVICE_TYPE enum.
 * @param pms_rx PMS RX pin.
 * @param pms_tx PMS TX pin.
 **/
bool Sensors::pmSensorInit(int pms_type, int pms_rx, int pms_tx) {
    // set UART for autodetection sensors (Honeywell, Plantower, Panasonic)
    if (pms_type <= 1) {
        Serial.println(F("-->[PMSENSOR] detecting Generic sensor.."));
        // Serial2.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
        Serial2.begin(9600, SERIAL_8N1, pms_rx, pms_tx);
    }
    // set UART for autodetection Sensirion sensor
    else if (pms_type == Sensirion) {
        Serial.println(F("-->[PMSENSOR] detecting Sensirion sensor.."));
        Serial2.begin(115200);
    }

    // starting auto detection loop 
    _serial = &Serial2;
    int try_sensor_init = 0;
    while (!pmSensorAutoDetect() && try_sensor_init++ <= 3);

    // get device selected..
    if (device_type >= 0) {
        if (devmode) Serial.println("");
        Serial.print(F("-->[PMSENSOR] detected: "));
        Serial.println(device_selected);
        return true;
    } else {
        Serial.println(F("-->[E][PMSENSOR] detection failed!"));
        return false;
    }
}
/**
 * @brief Generic PM sensor auto detection. 
 * 
 * In order UART config, this method looking up for
 * special header on Serial stream
 **/
bool Sensors::pmSensorAutoDetect() {
    // starting serial connection for generic PM sensors..
    delay(1000); // sync serial
    if (pmGenericRead()) {
        device_selected = "HONEYWELL";
        device_type = Honeywell;
        return true;   
    }
    delay(1000); // sync serial
    if (pmPanasonicRead()) {
        device_selected = "PANASONIC";
        device_type = Panasonic;
        return true;
    }
    delay(1000); // sync serial
    if (pmSensirionInit()) {
        device_selected = "SENSIRION";
        device_type = Sensirion;
        return true;
    }
    return false;
}

bool Sensors::pmSensirionInit() {
    // Begin communication channel
    Serial.println(F("-->[SPS30] starting SPS30 sensor.."));
    if(!devmode) sps30.EnableDebugging(0);
    // Begin communication channel;
    if (!sps30.begin(SP30_COMMS))
        pmSensirionErrorloop((char *)"-->[E][SPS30] could not initialize communication channel.", 0);
    // check for SPS30 connection
    if (!sps30.probe())
        pmSensirionErrorloop((char *)"-->[E][SPS30] could not probe / connect with SPS30.", 0);
    else {
        Serial.println(F("-->[SPS30] Detected SPS30."));
        getSensirionDeviceInfo();
    }
    // reset SPS30 connection
    if (!sps30.reset())
        pmSensirionErrorloop((char *)"-->[E][SPS30] could not reset.", 0);

    // start measurement
    if (sps30.start()==true) {
        Serial.println(F("-->[SPS30] Measurement OK"));
        return true;
    } else
        pmSensirionErrorloop((char *)"-->[E][SPS30] Could NOT start measurement", 0);

    if (SP30_COMMS == I2C_COMMS) {
        if (sps30.I2C_expect() == 4)
            Serial.println(F("-->[E][SPS30] Due to I2C buffersize only PM values  \n"));
    }
    return false;
}
/**
 * @brief : read and display Sensirion device info
 */
void Sensors::getSensirionDeviceInfo() { 
  char buf[32];
  uint8_t ret;
  SPS30_version v;

  //try to read serial number
  ret = sps30.GetSerialNumber(buf, 32);
  if (ret == ERR_OK) {
    Serial.print(F("-->[SPS30] Serial number : "));
    if(strlen(buf) > 0)  Serial.println(buf);
    else Serial.println(F("not available"));
  }
  else
    log_i("[SPS30] could not get serial number");

  // try to get product name
  ret = sps30.GetProductName(buf, 32);
  if (ret == ERR_OK)  {
    Serial.print(F("-->[SPS30] Product name  : "));
    if(strlen(buf) > 0)  Serial.println(buf);
    else Serial.println(F("not available"));
  }
  else
    log_i("[SPS30] could not get product name.");

  // try to get version info
  ret = sps30.GetVersion(&v);
  if (ret != ERR_OK) {
    log_i("[SPS30] Can not read version info");
    return;
  }

  Serial.print(F("-->[SPS30] Firmware level: "));  Serial.print(v.major);
  Serial.print("."); Serial.println(v.minor);

  if (SP30_COMMS != I2C_COMMS) {
    Serial.print(F("-->[SPS30] Hardware level: ")); Serial.println(v.HW_version);
    Serial.print(F("-->[SPS30] SHDLC protocol: ")); Serial.print(v.SHDLC_major);
    Serial.print("."); Serial.println(v.SHDLC_minor);
  }

  Serial.print(F("-->[SPS30] Library level : "));  Serial.print(v.DRV_major);
  Serial.print(".");  Serial.println(v.DRV_minor);
}

void Sensors::am2320Init() {
    am2320.begin();  // temp/humidity sensor
}

/// Print some sensors values
void Sensors::printValues() {
    if (devmode) {
        char output[100];
        sprintf(output, " PM1:%03d PM25:%03d PM10:%03d H:%02d%% T:%02d°C", pm1, pm25, pm10, (int)humi, (int)temp);
        Serial.println(output);
    }
}

/***********************************************************************************
 *  P U B L I C   M E T H O D S
 * *********************************************************************************/

/**
 * Main sensor loop.
 * All sensor read methods here, please call it on main loop.
 */
void Sensors::loop() {
    static uint_fast64_t pmLoopTimeStamp = 0;                 // timestamp for sensor loop check data
    if ((millis() - pmLoopTimeStamp > sample_time * 1000)) {  // sample time for each capture
        dataReady = false;
        pmLoopTimeStamp = millis();
        am2320Read();
        if(pmSensorRead()) {           
            if(_onDataCb) _onDataCb();
            dataReady = true;            // only if the main sensor is ready
        }else{
            dataReady = false;
        }
        printValues();
        
    }
}

/**
 * All sensors init.
 * 
 * Particle meter sensor (PMS) and AM2320 sensors init.
 * Please see the platformio.ini file for 
 * know what sensors is enable.
 * 
 * @param pms_type PMS type, please see DEVICE_TYPE enum.
 * @param pms_rx PMS RX pin.
 * @param pms_tx PMS TX pin.
 * @param debug enable PMS log output.
 */
void Sensors::init(int pms_type, int pms_rx, int pms_tx, bool debug) {

    // override with debug INFO level (>=3)
    if (CORE_DEBUG_LEVEL>=3) devmode = true;  
    else devmode = debug;
    
    if (!devmode) Serial.println("-->[SENSORS] debug is disable.");

    Serial.print("-->[SENSORS] sample time set to: ");
    Serial.println(sample_time);

    pmSensorInit(pms_type, pms_rx, pms_tx);

    // TODO: enable/disable via flag
    Serial.println("-->[AM2320] starting AM2320 sensor..");
    am2320Init();
}

/// set loop time interval for each sensor sample
void Sensors::setSampleTime(int seconds){
    sample_time = seconds;
}

void Sensors::restart(){
    _serial->flush();
    init();
    delay(100);
}

void Sensors::setOnDataCallBack(voidCbFn cb){
    _onDataCb = cb;
}

void Sensors::setOnErrorCallBack(errorCbFn cb){
    _onErrorCb = cb;
}

bool Sensors::isDataReady() {
    return dataReady;
}

uint16_t Sensors::getPM1() {
    return pm1;
}

String Sensors::getStringPM1() {
    char output[5];
    sprintf(output, "%03d", getPM1());
    return String(output);
}

uint16_t Sensors::getPM25() {
    return pm25;
}

String Sensors::getStringPM25() {
    char output[5];
    sprintf(output, "%03d", getPM25());
    return String(output);
}

uint16_t Sensors::getPM10() {
    return pm10;
}

String Sensors::getStringPM10() {
    char output[5];
    sprintf(output, "%03d", getPM10());
    return String(output);
}

float Sensors::getHumidity() {
    return humi;
}

float Sensors::getTemperature() {
    return temp;
}

float Sensors::getGas() {
    return gas;
}

float Sensors::getAltitude() {
    return alt;
}

float Sensors::getPressure() {
    return pres;
}

bool Sensors::isPmSensorConfigured(){
    return device_type>=0;
}

String Sensors::getPmDeviceSelected(){
    return device_selected;
}

int Sensors::getPmDeviceTypeSelected(){
    return device_type;
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_SENSORSHANDLER)
Sensors sensors;
#endif
