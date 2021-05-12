# Exam2
0.1.2數字代表gesture_id

main.pp在Exam2/src/model_deploy內且acc_mode模式，先將RPCfunction連到一個Thread，再用Thread控制一個function。


根據題意，我以3種角度測量加速度做分類，先停留幾秒量測靜止的加速度，再利用向量內積得到我的角度。第一種是>60會在uLCD顯示2，第二種是介於30到60之間會在uLCD顯示1，第三種則是小於30之間會在uLCD顯示2，接著以陣列儲存所讀到的0或1或2的數字。
將這個數字分別對應tenserflow的RING、SLOPE以及自定義的and，對應後，送至mqtt做螢幕顯示這些GESTURE，我則是先以sprintf(buff, "data: %d, %d\n", num_buff[num_d], num_d);先在mqtt顯示成功
第一個數代表0或1或2，第二個數字代表我只存10個數就必須以mqtt傳回thread，不再繼續偵測。

![Screenshot from 2021-05-12 02-56-08](https://user-images.githubusercontent.com/74852896/117957871-0e60a300-b34d-11eb-8e53-b3e404fe2eef.png)
![Screenshot from 2021-05-12 02-54-58](https://user-images.githubusercontent.com/74852896/117957873-0ef93980-b34d-11eb-9be6-e3411ee7bad1.png)
![20210512_175707_HDR](https://user-images.githubusercontent.com/74852896/117958898-0f460480-b34e-11eb-9b1c-e87a8a5f1255.jpg)




附圖為結果，第一張表示我開始量測不同角度，第二張是我利用mqtt回傳值，此時皆為大於60，因此第一位數皆為零，而第二個數從0數到9，最後一張則是我在LCD上顯示我的定義id。
