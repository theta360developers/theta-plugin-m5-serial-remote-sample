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

package com.theta360.m5serialremote;

import android.app.PendingIntent;
import android.content.Context;
import android.hardware.usb.UsbDeviceConnection;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;

import com.hoho.android.usbserial.driver.CdcAcmSerialDriver;
import com.hoho.android.usbserial.driver.ProbeTable;
//import com.theta360.pluginapplication.R;
import com.theta360.pluginapplication.task.TakePictureTask;
import com.theta360.pluginapplication.task.TakePictureTask.Callback;
import com.theta360.pluginlibrary.activity.PluginActivity;
import com.theta360.pluginlibrary.callback.KeyCallback;
import com.theta360.pluginlibrary.receiver.KeyReceiver;
import com.theta360.pluginlibrary.values.LedColor;
import com.theta360.pluginlibrary.values.LedTarget;

//THETAプラグインからのwebAPI(OSC)実行で使用
import com.theta360.pluginapplication.network.HttpConnector;
import android.content.Intent;
//シリアル通信まわりで使用
import android.hardware.usb.UsbManager;
import com.hoho.android.usbserial.driver.UsbSerialDriver;
import com.hoho.android.usbserial.driver.UsbSerialPort;
import com.hoho.android.usbserial.driver.UsbSerialProber;
import java.util.List;
import java.io.IOException;
//測位ログ関連で使用
import java.io.FileOutputStream;
import java.util.Arrays;
import java.text.SimpleDateFormat;
import java.util.Date;


public class MainActivity extends PluginActivity {
    private TakePictureTask.Callback mTakePictureTaskCallback = new Callback() {
        @Override
        public void onTakePicture(String fileUrl) {

        }
    };

    //シリアル通信関連
    private boolean mFinished;
    private UsbSerialPort port ;

    //USBデバイスへのパーミッション付与関連
    PendingIntent mPermissionIntent;
    private static final String ACTION_USB_PERMISSION = "com.android.example.USB_PERMISSION";

    //インターバル撮影関連
    private boolean isIntervalMode = false;
    private boolean isIntervalStat = false;
    private boolean sendReq = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        Log.d("REMOTE", "M:onCreate()");

        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);


        // Set enable to close by pluginlibrary, If you set false, please call close() after finishing your end processing.
        setAutoClose(true);
        // Set a callback when a button operation event is acquired.
        setKeyCallback(new KeyCallback() {
            @Override
            public void onKeyDown(int keyCode, KeyEvent event) {
                //---------------  customized code ---------------
                if (keyCode == KeyReceiver.KEYCODE_CAMERA) {
                    /*
                     * To take a static picture, use the takePicture method.
                     * You can receive a fileUrl of the static picture in the callback.
                     */
                    if (isIntervalMode){
                        if (isIntervalStat) {
                            isIntervalStat =false;
                        } else {
                            isIntervalStat =true;
                        }
                        sendReq = true;
                    } else {
                        new TakePictureTask(mTakePictureTaskCallback).execute();
                    }
                } else if (keyCode == KeyReceiver.KEYCODE_MEDIA_RECORD) {
                    if (isIntervalMode) {
                        if (isIntervalStat) {
                            isIntervalStat =false;
                            sendReq = true;
                        }
                        isIntervalMode = false;
                        notificationLedHide(LedTarget.LED7);
                    } else {
                        isIntervalMode = true;
                        notificationLedShow(LedTarget.LED7);
                    }
                }
                //-----------------------------------------
            }

            @Override
            public void onKeyUp(int keyCode, KeyEvent event) {
                /**
                 * You can control the LED of the camera.
                 * It is possible to change the way of lighting, the cycle of blinking, the color of light emission.
                 * Light emitting color can be changed only LED3.
                 */
                //SDKにかかれてたコードが不要なので消しておく。
                //notificationLedBlink(LedTarget.LED3, LedColor.BLUE, 1000);
            }

            @Override
            public void onKeyLongPress(int keyCode, KeyEvent event) {

            }
        });

    }

    @Override
    protected void onResume() {
        Log.d("REMOTE", "M:onResume()");

        //---------------  added code ---------------
        mFinished = true;

        // Find all available drivers from attached devices.
        UsbManager manager = (UsbManager) getSystemService(Context.USB_SERVICE);

        List<UsbSerialDriver> usb = UsbSerialProber.getDefaultProber().findAllDrivers(manager);
        //final ProbeTable probeTable = UsbSerialProber.getDefaultProbeTable();
        //probeTable.addProduct(0x1546,0x01a7,CdcAcmSerialDriver.class);
        //List<UsbSerialDriver> usb = new UsbSerialProber(probeTable).findAllDrivers(manager);

        // デバッグのため認識したデバイス数をしらべておく
        int usbNum = usb.size();
        Log.d("REMOTE","usb num =" + usbNum  );

        if (usb.isEmpty()) {
            Log.d("REMOTE","usb device is not connect."  );
            notificationLedBlink(LedTarget.LED3, LedColor.RED, 1000);
            //return;
            //port = null;
        } else {
            // Open a connection to the first available driver.
            UsbSerialDriver driver = usb.get(0);

            //USBデバイスへのパーミッション付与用（機器を刺したときスルーしてもアプリ起動時にチャンスを与えるだけ。なくても良い。）
            mPermissionIntent = PendingIntent.getBroadcast(this, 0, new Intent(ACTION_USB_PERMISSION), 0);
            manager.requestPermission( driver.getDevice() , mPermissionIntent);

            UsbDeviceConnection connection = manager.openDevice(driver.getDevice());
            if (connection == null) {
                // You probably need to call UsbManager.requestPermission(driver.getDevice(), ..)
                // パーミッションを与えた後でも、USB機器が接続されたままの電源Off->On だとnullになる... 刺しなおせばOK
                notificationLedBlink(LedTarget.LED3, LedColor.YELLOW, 500);
                Log.d("REMOTE","M:Can't open usb device.\n");

                port = null;
            } else {
                port = driver.getPorts().get(0);

                try {
                    port.open(connection);
                    port.setParameters(115200, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE);
                    //port.setParameters(9600, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE);

                    mFinished = false;
                    start_read_thread();

                } catch (IOException e) {
                    // Deal with error.
                    e.printStackTrace();
                    Log.d("REMOTE", "M:IOException");
                    notificationLedBlink(LedTarget.LED3, LedColor.YELLOW, 1000);
                    //return;
                } finally {
                    Log.d("REMOTE", "M:finally");
                }
            }
        }
        //-----------------------------------------

        super.onResume();
    }

    @Override
    protected void onPause() {
        // Do end processing
        //close();

        //---------------  added code ---------------
        Log.d("REMOTE", "M:onPause()");
        //インターバル撮影 可 の場合の後片付け
        if (isIntervalMode) {
            notificationLedHide(LedTarget.LED7);
            if (isIntervalStat) {
                isIntervalStat = false;
                sendReq = true;
                try {
                    //念のためのスレッド終了待ち
                    Thread.sleep(20);
                } catch (InterruptedException e){
                    // Deal with error.
                    e.printStackTrace();
                    Log.d("REMOTE", "T:InterruptedException");
                    notificationLedBlink(LedTarget.LED3, LedColor.YELLOW, 200);
                }
            }
        }

        //スレッドを終わらせる指示。終了待ちしていません。
        mFinished = true;

        //シリアル通信の後片付け ポート開けてない場合にはCloseしないこと
        if (port != null) {
            try {
                port.close();
                Log.d("REMOTE", "M:onDestroy() port.close()");
                notificationLedBlink(LedTarget.LED3, LedColor.BLUE, 200);
            }catch(IOException e){
                Log.d("REMOTE", "M:onDestroy() IOException");
                notificationLedBlink(LedTarget.LED3, LedColor.RED, 300);
            }
        } else {
            Log.d("REMOTE","M:port=null\n");
        }
        //-----------------------------------------

        super.onPause();
    }

    //---------------  added code ---------------
    //読み取りスレッド
    public void start_read_thread(){
        new Thread(new Runnable(){
            @Override
            public void run() {

                FileOutputStream  out;

                try {
                    //notificationLedBlink(LedTarget.LED3, LedColor.MAGENTA, 500);
                    Log.d("REMOTE", "Thread Start");

                    //Restriction on the number of log files
                    String[] files = fileList();
                    Arrays.sort(files);
                    int maxFileNum = 20;
                    if ( files.length >= maxFileNum) {
                        for( int i=0; i<= (files.length-maxFileNum); i++ ){
                            Log.d("REMOTE", "delet file:" + files[i] );
                            deleteFile(files[i]);
                        }
                    }

                    //Create logfile
                    SimpleDateFormat  df = new SimpleDateFormat("yyyyMMdd_HHmmss");
                    final Date date = new Date(System.currentTimeMillis());
                    String FileName = "REMOTE_Log_" + df.format(date) + ".txt";
                    out = openFileOutput(FileName,MODE_PRIVATE|MODE_APPEND);




                    //Serial Pooling
                    while(mFinished==false){

                        //シリアル通信 受信ポーリング部
                        byte buff[] = new byte[256];
                        int num = port.read(buff, buff.length);
                        if ( num > 0 ) {
                            String rcvStr = new String(buff, 0, num);
                            String[] splitSentence = rcvStr.split(",", 0);

                            Log.d("REMOTE", rcvStr);
                            out.write(buff,0, rcvStr.length());

                            String Result = "";
                            HttpConnector camera = new HttpConnector("127.0.0.1:8080");
                            if (splitSentence.length == 3) {
                                Result = camera.httpExec(splitSentence[0], splitSentence[1], splitSentence[2]);
                                Log.d("REMOTE", "camera.httpExec():" + Result);
                            } else if ( splitSentence.length == 2 ) {
                                //Log.d("REMOTE", "param1:" + splitSentence[0] + ", param2:" + splitSentence[1]);
                                Result = camera.httpExec(splitSentence[0], splitSentence[1], "");
                                Log.d("REMOTE", "camera.httpExec():" + Result);
                            } else if ( splitSentence.length >= 4 ) {
                                String param = "";
                                for (int i=2; i<splitSentence.length ; i++){
                                    param += splitSentence[i];
                                    if (i != (splitSentence.length-1)) {
                                        param += ",";
                                    }
                                }
                                Result = camera.httpExec(splitSentence[0], splitSentence[1], param);
                                Log.d("REMOTE", "camera.httpExec():" + Result);
                            } else {
                                //無処理にしたい
                                Result = "";
                            }
                            //send response
                            Result += "\n";
                            port.write(Result.getBytes(), Result.length());

                        }
                        //ポーリングが高頻度になりすぎないよう10msスリープする
                        Thread.sleep(10);

                        //インターバル撮影 開始/停止指示 の実行部
                        if (sendReq == true){
                            String Result ;
                            HttpConnector camera = new HttpConnector("127.0.0.1:8080");
                            if ( isIntervalStat ) {
                                Result = camera.setIntervalParam(4, 0);
                                Log.d("REMOTE", "T:camera.setIntervalParam()=" + Result);
                                Result = camera.startCapture("interval");
                                Log.d("REMOTE", "T:camera.startCapture()=" + Result);
                                notificationLedShow(LedTarget.LED6);
                            } else {
                                Result = camera.stopCapture();
                                Log.d("REMOTE", "T:camera.stopCapture()=" + Result);
                                notificationLedHide(LedTarget.LED6);
                            }
                            sendReq = false;
                        }
                    }
                    out.close();

                } catch (IOException e) {
                    // Deal with error.
                    e.printStackTrace();
                    Log.d("REMOTE", "T:IOException");
                    notificationLedBlink(LedTarget.LED3, LedColor.YELLOW, 100);
                } catch (InterruptedException e) {
                    // Deal with error.
                    e.printStackTrace();
                    Log.d("REMOTE", "T:InterruptedException");
                    notificationLedBlink(LedTarget.LED3, LedColor.YELLOW, 200);
                } finally {
                    Log.d("REMOTE", "T:finally");
                }
            }
        }).start();
    }
    //-----------------------------------------

}
