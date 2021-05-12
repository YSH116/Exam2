# Exam2
根據題意，我以3種角度量加速度做分類，先停留幾秒量測靜止的加速度，再利用向量內積得到我的角度。第一種是>60會在uLCD顯示2，第二種是介於30到60之間會在uLCD顯示1，第三種則是小於30之間會在uLCD顯示2，接著以陣列儲存所讀到的0或1或2的數字。
將這個數字分別對應tenserflow的RING、SLOPE以及自定義的and，對應後，送至mqtt做螢幕顯示這些GESTURE，我則是先以sprintf(buff, "data: %d, %d\n", num_buff[num_d], num_d);先在mqtt顯示成功
第一個數代表0或1或2，第二個數字代表我只存10個數就必須以mqtt傳回thread，不再繼續偵測。

