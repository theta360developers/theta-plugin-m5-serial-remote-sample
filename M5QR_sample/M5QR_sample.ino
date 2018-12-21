/**
 * Copyright 2018 Ricoh Company, Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <M5Stack.h>      // M5STACK
#include <ArduinoJson.h>  // Add JSON Library  https://github.com/bblanchon/ArduinoJson



int  LognPressCntA = 0;
int  LognPressCntB = 0;
int  LognPressCntC = 0;

bool bLongPressFlagA = false;
bool bLongPressFlagB = false;
bool bLongPressFlagC = false;
bool bLongPressOneTimeA = false;
bool bLongPressOneTimeB = false;
bool bLongPressOneTimeC = false;

#define KEY_LONGPRESS_MS    1000
#define KEY_RET_NOP         0
#define KEY_RET_SHORT       1
#define KEY_RET_LONG        2

int  KeyReadA(void);
int  KeyReadB(void);
int  KeyReadC(void);

#define KEYCTRL_DISP_NOP    0
#define KEYCTRL_DISP_CHG    1
int  KeyControl(void);



bool  bDispReqClear = false;
bool  bDispReqArCode = false;
String  strLastFileURL = "";


#define   TAKE_PIC_STAT_DONE  0
#define   TAKE_PIC_STAT_BUSY  1

#define   INT_EXP_OFF         0
#define   INT_EXP_ON          1

#define   INT_EXP_STAT_STOP   0
#define   INT_EXP_STAT_RUN    1

#define   MOVE_STAT_STOP      0
#define   MOVE_STAT_REC       1

int     iTakePicStat      = TAKE_PIC_STAT_DONE;
int     iIntExpStat       = INT_EXP_STAT_STOP;    //For expansion
int     iIntExpOnOff      = INT_EXP_OFF;          //For expansion
int     iMoveStat         = MOVE_STAT_STOP;       //For expansion
int     iRecordedTime     = 0;                    //For expansion
int     iRecordableTime   = 0;                    //For expansion
String  strTakePicLastId  = "0";
String  strCaptureStatus  = "unknown";
String  strBattLevel      = "unknown";



int     ThetaAPI_Post_State(void);
int     ThetaAPI_Post_takePicture(void);
int     ThetaAPI_Post_commnads_status(void);



#define   HTTP_TIMEOUT_DISABLE  0 // never times out during transfer. 
#define   HTTP_TIMEOUT_NORMAL   1
#define   HTTP_TIMEOUT_STATE    2
#define   HTTP_TIMEOUT_STARTCAP 5
#define   HTTP_TIMEOUT_STOPCAP  70
#define   HTTP_TIMEOUT_CMDSTAT  2
#define   HTTP_TIMEOUT_LV       20  //sec

String ExecWebAPI(String strPostGet, String strUrl, String strData, unsigned int uiTimeoutSec);

#define   SERIAL_BUFF_BYTE      512
bool  bSerialBufEnter = false;
char  sSerialBuf[SERIAL_BUFF_BYTE];
String strSerialRcv = "";

// Receive Serial Task
void    ReceiveSerial(void * pvParameters);


//========================================================================
void setup() {
  Serial.begin(115200);        // SERIAL
  M5.begin();                   // M5STACK INITIALIZE
  
  M5.Lcd.setBrightness(200);   // BRIGHTNESS = MAX 255
  M5.Lcd.fillScreen(BLACK);    // CLEAR SCREEN
  //M5.Lcd.setRotation(0);      // SCREEN ROTATION = 0 : Horizontal display in the past.
  M5.Lcd.setRotation(1);       // SCREEN ROTATION = 1 : Current horizontal display.
  
  //-------------------------------------------------
  // Task WUP
  xTaskCreatePinnedToCore(
                  ReceiveSerial,     /* Function to implement the task */
                  "ReceiveSerial",   /* Name of the task */
                  4096,      /* Stack size in words */
                  NULL,      /* Task input parameter */
                  //3,         /* Priority of the task */
                  1,         /* Priority of the task */
                  NULL,      /* Task handle. */
                  0);        /* Core where the task should run */
                  //1);        /* Core where the task should run */ //The same core as the main loop 
  //-------------------------------------------------
    
}
//========================================================================

void loop() {
  bool  bDispChg = false;
  int   iRet;

  // Key operation monitoring
  iRet = KeyControl();
  if ( iRet == KEYCTRL_DISP_CHG ) {
    bDispChg = true;
  }

  // Camera condition monitoring
  if (iTakePicStat == TAKE_PIC_STAT_DONE) {
    ThetaAPI_Post_State();
  } else {
    ThetaAPI_Post_commnads_status();
    if ( iTakePicStat == TAKE_PIC_STAT_DONE ) {
        bDispReqArCode = true;
    }
  }


  // Always display part
  char cBattLevel[strBattLevel.length()+1];
  strBattLevel.toCharArray(cBattLevel, (strBattLevel.length()+1));
  char cCaptureStatus[strCaptureStatus.length()+1];
  strCaptureStatus.toCharArray(cCaptureStatus, (strCaptureStatus.length()+1));
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.printf("BUSY=%d, BATT=%s, CapStat=%s        ", iTakePicStat, cBattLevel, cCaptureStatus);
  // The trailing blank character string is provided so as not to cause the screen to flicker

  // Execute screen clear request
  if ( bDispReqClear == true ) {
    M5.Lcd.fillScreen(BLACK);     // CLEAR SCREEN
    bDispReqClear = false;
  }
  
  // Execution of QR code display request
  if ( bDispReqArCode == true ) {
    if ( strLastFileURL != "" ) {
      String strDispURL = strLastFileURL;
      strDispURL.replace("127.0.0.1:8080","192.168.1.1");  // Convert to URL for direct access 
      M5.Lcd.qrcode(strDispURL);
      
      bDispReqArCode = false ;
    }
  } else {
    M5.Lcd.setCursor(0, 40);
    M5.Lcd.print("        ");
  }
  
  // Screen update of key operation trigger
  if ( bDispChg == true ) {
    //NOP : Currently unused 
  }
  
  M5.update();
  //delay(25);
  delay(100);
}

//========================================================================

//-------------------------------------------
// Bottun Read functions
//-------------------------------------------
int  KeyReadA(void)
{
  int iRet = KEY_RET_NOP;
  
  //--- BtnA Read ---
  if ( M5.BtnA.wasPressed() ) {
    bLongPressFlagA=false;
    bLongPressOneTimeA=false;
    LognPressCntA=0;
  }
  if ( M5.BtnA.wasReleased() ) {
    if (bLongPressFlagA == false) {
      iRet = KEY_RET_SHORT;
    }
  } else {
    if ( M5.BtnA.pressedFor(KEY_LONGPRESS_MS) ) {   // Pressed for more than the specified time[ms]
      bLongPressFlagA=true;
      iRet = KEY_RET_LONG;
    } else {
      //NOP
    }
  }
  
  return (iRet);
}
//---
int  KeyReadB(void)
{
  int iRet = KEY_RET_NOP;
  
  //--- BtnA Read ---
  if ( M5.BtnB.wasPressed() ) {
    bLongPressFlagB=false;
    bLongPressOneTimeB=false;
    LognPressCntB=0;
  }
  if ( M5.BtnB.wasReleased() ) {
    if (bLongPressFlagB == false) {
      iRet = KEY_RET_SHORT;
    }
  } else {
    if ( M5.BtnB.pressedFor(KEY_LONGPRESS_MS) ) {   // Pressed for more than the specified time[ms] 
      bLongPressFlagB=true;
      iRet = KEY_RET_LONG;
    } else {
      //NOP
    }
  }
  
  return (iRet);
}
//---
int  KeyReadC(void)
{
  int iRet = KEY_RET_NOP;
  
  //--- BtnA Read ---
  if ( M5.BtnC.wasPressed() ) {
    bLongPressFlagC=false;
    bLongPressOneTimeC=false;
    LognPressCntC=0;
  }
  if ( M5.BtnC.wasReleased() ) {
    if (bLongPressFlagC == false) {
      iRet = KEY_RET_SHORT;
    }
  } else {
    if ( M5.BtnC.pressedFor(KEY_LONGPRESS_MS) ) {   // Pressed for more than the specified time[ms]
      bLongPressFlagC=true;
      iRet = KEY_RET_LONG;
    } else {
      //NOP
    }
  }
  
  return (iRet);
}
//----------------

//#define KEYCTRL_DISP_NOP    0
//#define KEYCTRL_DISP_CHG    1
int  KeyControl(void)
{
  int iRet = KEYCTRL_DISP_NOP;
  int iKeyVal;
  String Result ;
  
  //--- Center Button ---
  iKeyVal = KeyReadB();
  if (iKeyVal == KEY_RET_SHORT) {
    if ( (iTakePicStat == TAKE_PIC_STAT_DONE) && strCaptureStatus.equals("idle") ) {
    //Serial.print("Btn B Short\n");
    
      bDispReqClear = true;
      ThetaAPI_Post_takePicture();
      if ( iTakePicStat == TAKE_PIC_STAT_DONE ) {
          bDispReqArCode = true;
      }
      iRet = KEYCTRL_DISP_CHG;
      
    }
  } else if (iKeyVal == KEY_RET_LONG) {
    //Serial.print("Btn B Long\n");
  }

  //--- Left Button ---
  iKeyVal = KeyReadA();
  if (iKeyVal == KEY_RET_SHORT) {
    //Serial.print("Btn A Short\n");

    bDispReqClear = true;
    
  } else if (iKeyVal == KEY_RET_LONG) {
    //Serial.print("Btn A Long\n");
  }

  //--- Right Button ---
  iKeyVal = KeyReadC();
  if (iKeyVal == KEY_RET_SHORT) {
    //Serial.print("Btn C Short\n");
    
    bDispReqArCode = true ;
    
  } else if (iKeyVal == KEY_RET_LONG) {
    //Serial.print("Btn C Long\n");
  }
  
  return (iRet);
}

//-------------------------------------------
// THETA API functions
//-------------------------------------------
int     ThetaAPI_Post_State(void)
{
  int iRet=0;

  String strJson = ExecWebAPI("POST", "/osc/state", "", HTTP_TIMEOUT_STATE);
  
  iRet = strJson.length();
  if ( iRet != 0 ) {
    char sJson[iRet+1];
    strJson.toCharArray(sJson,iRet+1);
    StaticJsonBuffer<350> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(sJson);
    if (!root.success()) {
      //Serial.println("ThetaAPI_Post_State() : parseObject() failed.");
      iRet=-1;
    } else {
      
      const char* batteryLevel    = root["state"]["batteryLevel"];
      const char* _captureStatus  = root["state"]["_captureStatus"];
      const char* _recordedTime   = root["state"]["_recordedTime"];
      const char* _recordableTime = root["state"]["_recordableTime"];
      const char* _latestFileUrl  = root["state"]["_latestFileUrl"];

      strBattLevel = String(batteryLevel);
      strLastFileURL = String(_latestFileUrl);

      strCaptureStatus  = String(_captureStatus);
      iRecordedTime = atoi(_recordedTime);
      iRecordableTime = atoi(_recordableTime);
      
      if ( strCaptureStatus.equals("idle") ) {
        iMoveStat = MOVE_STAT_STOP;
        iIntExpStat = INT_EXP_STAT_STOP;
      } else {
        if ( (iRecordedTime==0) && (iRecordableTime==0) ) {
          iMoveStat = MOVE_STAT_STOP;
          iIntExpOnOff= INT_EXP_ON;
          iIntExpStat = INT_EXP_STAT_RUN ;
        } else {
          iMoveStat = MOVE_STAT_REC;
          iIntExpStat = INT_EXP_STAT_STOP;
        }
      }
      iRet=1;
    }
  }
  
  return iRet;
}

//----------------
int     ThetaAPI_Post_takePicture(void)
{
  int iRet=0;
  
  iTakePicStat = TAKE_PIC_STAT_BUSY;
  
  String strSendData = String("{\"name\": \"camera.takePicture\" }");
  String strJson = ExecWebAPI("POST", "/osc/commands/execute", strSendData, HTTP_TIMEOUT_NORMAL);
    
  iRet = strJson.length();
  if ( iRet != 0 ) {
    char sJson[iRet+1];
    strJson.toCharArray(sJson,iRet+1);
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(sJson);
    if (!root.success()) {
      //Serial.println("ThetaAPI_Post_takePicture() : parseObject() failed.");
      iRet=-1;
    } else {
      const char* sState  = root["state"];
      String strState = String(sState);
      //Serial.print("ThetaAPI_Post_takePicture() : state[" + strState + "], " );
      if ( strState.equals("error") ) {
        const char* sErrorCode = root["error"]["code"];
        const char* sErrorMessage = root["error"]["message"];
        //Serial.println("Code[" + String(sErrorCode) + "], Message[" + String(sErrorMessage) + "]");
        iTakePicStat = TAKE_PIC_STAT_DONE;
        iRet=-1;
      } else {  //inProgress
        const char* sId = root["id"];
        strTakePicLastId = String(sId);
        //Serial.println("id[" + strTakePicLastId + "]");
        iRet=1;
      }
    }
  }
  
  return iRet;
}

//----------------
int     ThetaAPI_Post_commnads_status(void)
{
  int iRet=0;
  if ( strTakePicLastId == "0" ) {
    return iRet;
  }
  
  String strSendData = "{\"id\":\"" + strTakePicLastId + "\"}" ;
  String strJson  = ExecWebAPI("POST", "/osc/commands/status", strSendData, HTTP_TIMEOUT_CMDSTAT);
  String strDbg = "";
  
  iRet = strJson.length();
  if ( iRet != 0 ) {
    char sJson[iRet+1];
    strJson.toCharArray(sJson,iRet+1);
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(sJson);
    if (!root.success()) {
      strDbg = "parseObject() failed.:[" + strJson + "]";
      iRet=-1;
    } else {
      const char* sState  = root["state"];
      String strState = String(sState);
      if ( strState.equals("error") ) {
        const char* sErrorCode = root["error"]["code"];
        const char* sErrorMessage = root["error"]["message"];
        strDbg = "Code[" + String(sErrorCode) + "], Message[" + String(sErrorMessage) + "]";
        iTakePicStat = TAKE_PIC_STAT_DONE;
        iRet=-1;
      } else if ( strState.equals("done") ) {
        const char* sFileUri = root["results"]["fileUri"];
        strDbg = "done : fileUri[" + String(sFileUri) + "]";
        iTakePicStat = TAKE_PIC_STAT_DONE;        
        iRet=1;
      } else {  // inProgress
        const char* sId = root["id"];
        strDbg = "inProgress : id[" + String(sId) + "]";
        iRet=1;
      }
    }
  }
  
  return iRet;
}

//========================================================================

String ExecWebAPI(String strPostGet, String strUrl, String strData, unsigned int uiTimeoutSec)
{
  unsigned long ulStartMs;
  unsigned long ulCurMs;
  unsigned long ulElapsedMs;
  unsigned long ulTimeoutMs;
  int           iResponse=0;

  ulTimeoutMs = (unsigned long)uiTimeoutSec * 1000;
  
  //Edit & Send CMD
  String SendCmd;
  if ( strData.length() > 0 ) {
    SendCmd = strPostGet + "," + strUrl + "," + strData;
  } else {
    SendCmd = strPostGet + "," + strUrl ;
  }
  bSerialBufEnter = false;
  strSerialRcv = "";
  Serial.print(SendCmd);

  //Receive response
  String line = "";
  ulStartMs = millis();
  while(1){
    if (bSerialBufEnter == true) {
      line = strSerialRcv ;
      bSerialBufEnter = false;
      break;
    } else {
      delay(10); //sleep 10ms
    }
    
    ulCurMs = millis();
    if ( ulCurMs > ulStartMs ) {
      ulElapsedMs = ulCurMs - ulStartMs;
    } else {
      ulElapsedMs = (4294967295 - ulStartMs) + ulCurMs + 1 ;
    }
    if ( (ulTimeoutMs!=0) && (ulElapsedMs>ulTimeoutMs) ) {
      break;
    }
  }
  
  return line ;
}

//========================================================================

void    ReceiveSerial(void * pvParameters)
{
  bool bEnd = false ;
  
  for(;;) {
    if ( Serial.available() > 0 ) {
      int iPos=0;
      while (Serial.available()) {
        char c = Serial.read();
        if (  c == '\n' ) {
          bEnd = true;
          break;
        } else {
          sSerialBuf[iPos] = c;
          iPos++;
        }
      }
      sSerialBuf[iPos] = 0x00;
      strSerialRcv += String(sSerialBuf);
      if ( bEnd == true ) {
        bSerialBufEnter = true;
        bEnd = false;
      }
      
    } else {
      delay(10); //sleep 10ms
    }
  }
}

//========================================================================
