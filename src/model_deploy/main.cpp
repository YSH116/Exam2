#include "accelerometer_handler.h"
#include "config.h"
#include "magic_wand_model_data.h"
#include "mbed.h"
#include "mbed_rpc.h"
#include "stm32l475e_iot01_accelero.h"
#include <cmath>
#include "uLCD_4DGL.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"

WiFiInterface *wifi;
void messageArrived(MQTT::MessageData& md);
void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client);
void close_mqtt();
Thread mqtt_thread(osPriorityHigh);
EventQueue mqtt_queue;

uLCD_4DGL uLCD(D1, D0, D2);
BufferedSerial pc(USBTX, USBRX);
char num_buff[20];
void acc_get(Arguments *in, Reply *out);
int PredictGesture(float* output);
RPCFunction a(&acc_get, "a");
void tf(); // detect gesture to select the maximum angle 
void acc(); 

Thread angle_thread;
Ticker flip;
constexpr int kTensorArenaSize = 60 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
double theta = 0;
double vector_length(int16_t *DataXYZ);
int num_d = 0;
bool acc_mode = false;
volatile int message_num = 0;
volatile int arrivedcount = 0;
volatile bool closed = false;

const char* topic = "Mbed";
bool go;

void acc_get(Arguments *in, Reply *out) { // RPC2 CALL
    printf("acc_mode\n");
    acc_mode = true;
    angle_thread.start(&acc);
}

void acc() {
    int16_t pDataXYZ[3] = {0}, stationary[3] = {0};
    double cos = 0;

    ThisThread::sleep_for(2s);
    BSP_ACCELERO_AccGetXYZ(stationary); // acc of stationary
    //printf("stationary_g: %d, %d, %d\n", stationary[0], stationary[1], stationary[2]);

    while (acc_mode && num_d < 10) {
      BSP_ACCELERO_AccGetXYZ(pDataXYZ);
      //printf("current_acc: %d, %d, %d ", pDataXYZ[0], pDataXYZ[1], pDataXYZ[2]);
      cos = (pDataXYZ[0]*stationary[0] + pDataXYZ[1]*stationary[1] + pDataXYZ[2]*stationary[2]) / (vector_length(pDataXYZ) * vector_length(stationary));
      theta = acos(cos) * 180 / 3.1415926;
      printf("Theta = %lf\n", theta);
      if (theta >= 60) {
        uLCD.printf("0\n");
        num_buff[num_d] = 0;
      }
      else if (theta < 60 && theta >= 30) {
        uLCD.printf("1\n");
        num_buff[num_d] = 1;
      }
      else if (theta < 30) {
        uLCD.printf("2\n");
        num_buff[num_d] = 2;
      }
      ThisThread::sleep_for(500ms);
// t.start(&tf);
    }
}

double vector_length(int16_t *DataXYZ) { 
  int power = 0;
  double length = 0;

  power = pow(DataXYZ[0], 2) + pow(DataXYZ[1], 2) + pow(DataXYZ[2], 2);
  length = sqrt(power);
  return length; 
}

int PredictGesture(float* output) {
  // How many times the most recent gesture has been matched in a row
  static int continuous_count = 0;
  // The result of the last prediction
  static int last_predict = -1;

  // Find whichever output has a probability > 0.8 (they sum to 1)
  int this_predict = -1;
  for (int i = 0; i < label_num; i++) {
    if (output[i] > 0.8) this_predict = i;
  }

  // No gesture was detected above the threshold
  if (this_predict == -1) {
    continuous_count = 0;
    last_predict = label_num;
    return label_num;
  }

  if (last_predict == this_predict) {
    continuous_count += 1;
  } else {
    continuous_count = 0;
  }
  last_predict = this_predict;

  // If we haven't yet had enough consecutive matches for this gesture,
  // report a negative result
  if (continuous_count < config.consecutiveInferenceThresholds[this_predict]) {
    return label_num;
  }
  // Otherwise, we've seen a positive result, so clear all our variables
  // and report it
  continuous_count = 0;
  last_predict = -1;

  return this_predict;
}

void tf() {

  // Whether we should clear the buffer next time we fetch data
  bool should_clear_buffer = false;
  bool got_data = false;

  // The gesture index of the prediction
  int gesture_index;

  // Set up logging.
  static tflite::MicroErrorReporter micro_error_reporter;
  tflite::ErrorReporter* error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  const tflite::Model* model = tflite::GetModel(g_magic_wand_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    error_reporter->Report(
        "Model provided is schema version %d not equal "
        "to supported version %d.",
        model->version(), TFLITE_SCHEMA_VERSION);
    return ;
  }

  // Pull in only the operation implementations we need.
  // This relies on a complete list of all the ops needed by this graph.
  // An easier approach is to just use the AllOpsResolver, but this will
  // incur some penalty in code space for op implementations that are not
  // needed by this graph.
  static tflite::MicroOpResolver<6> micro_op_resolver;
  micro_op_resolver.AddBuiltin(
      tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
      tflite::ops::micro::Register_DEPTHWISE_CONV_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_MAX_POOL_2D,
                               tflite::ops::micro::Register_MAX_POOL_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_CONV_2D,
                               tflite::ops::micro::Register_CONV_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                               tflite::ops::micro::Register_FULLY_CONNECTED());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                               tflite::ops::micro::Register_SOFTMAX());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_RESHAPE,
                               tflite::ops::micro::Register_RESHAPE(), 1);

  // Build an interpreter to run the model with
  static tflite::MicroInterpreter static_interpreter(
      model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
  tflite::MicroInterpreter* interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors
  interpreter->AllocateTensors();

  // Obtain pointer to the model's input tensor
  TfLiteTensor* model_input = interpreter->input(0);
  if ((model_input->dims->size != 4) || (model_input->dims->data[0] != 1) ||
      (model_input->dims->data[1] != config.seq_length) ||
      (model_input->dims->data[2] != kChannelNumber) ||
      (model_input->type != kTfLiteFloat32)) {
    error_reporter->Report("Bad input tensor parameters in model");
    return ;
  }

  int input_length = model_input->bytes / sizeof(float);

  TfLiteStatus setup_status = SetupAccelerometer(error_reporter);
  if (setup_status != kTfLiteOk) {
    error_reporter->Report("Set up failed\n");
    return ;
  }

  error_reporter->Report("Set up successful...\n");

  while (true) {

    // Attempt to read new data from the accelerometer
    got_data = ReadAccelerometer(error_reporter, model_input->data.f,
                                 input_length, should_clear_buffer);

    // If there was no new data,
    // don't try to clear the buffer again and wait until next time
    if (!got_data) {
      should_clear_buffer = false;
      continue;
    }

    // Run inference, and report any error
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
      error_reporter->Report("Invoke failed on index: %d\n", begin_index);
      continue;
    }

    // Analyze the results to obtain a prediction
    gesture_index = PredictGesture(interpreter->output(0)->data.f);

    // Clear the buffer next time we read data
    should_clear_buffer = gesture_index < label_num;

    // Produce an output
    if (gesture_index < label_num || buff[num] == 0) {
        error_reporter->Report(config.output_message[gesture_index]);
    }
  }
}

int main() {
    BSP_ACCELERO_Init();
    char buf[256], outbuf[256];

   FILE *devin = fdopen(&pc, "r");
   FILE *devout = fdopen(&pc, "w");
    
    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\r\n");
        return -1;
    }


    printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\nConnection error: %d\r\n", ret);
        return -1;
    }


    NetworkInterface* net = wifi;
    MQTTNetwork mqttNetwork(net);
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    //TODO: revise host to your IP
    const char* host = "172.20.10.8";
    printf("Connecting to TCP network...\r\n");

    SocketAddress sockAddr;
    sockAddr.set_ip_address(host);
    sockAddr.set_port(1883);

    printf("address is %s/%d\r\n", (sockAddr.get_ip_address() ? sockAddr.get_ip_address() : "None"),  (sockAddr.get_port() ? sockAddr.get_port() : 0) ); //check setting

    int rc = mqttNetwork.connect(sockAddr);//(host, 1883);
    if (rc != 0) {
            printf("Connection error.");
            return -1;
    }
    printf("Successfully connected!\r\n");

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "Mbed";

    if ((rc = client.connect(data)) != 0){
      printf("Fail to connect MQTT\r\n");
    }
    if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0){
      printf("Fail to subscribe\r\n");
    }
    mqtt_thread.start(callback(&mqtt_queue, &EventQueue::dispatch_forever));

    while(1) {
       
      memset(buf, 0, 256);      // clear buffer
     flip.attach(mqtt_queue.event(&publish_message, &client), 500ms);
      for(int i=0; ; i++) {
            char recv = fgetc(devin);
            if (recv == '\n') {
               printf("\r\n");
               break;
            }
            buf[i] = fputc(recv, devout);
      }
      //Call the static call method on the RPC class
      RPC::call(buf, outbuf);
      printf("%s\r\n", outbuf);
      
    }
    
    int num = 0;
    while (num != 5) {
            client.yield(100);
            ++num;
    }

    while (1) {
      if (closed) break;
      client.yield(500);
      ThisThread::sleep_for(500ms);
    }
    
    printf("Ready to close MQTT Network......\n");

    if ((rc = client.unsubscribe(topic)) != 0) {
      printf("Failed: rc from unsubscribe was %d\n", rc);
    }
    if ((rc = client.disconnect()) != 0) {
    printf("Failed: rc from disconnect was %d\n", rc);
    }

    mqttNetwork.disconnect();
    printf("Successfully closed!\n");
    return 0;
}
void messageArrived(MQTT::MessageData& md) {
    MQTT::Message &message = md.message;
    char msg[300];
    sprintf(msg, "Message arrived: QoS%d, retained %d, dup %d, packetID %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf(msg);
    ThisThread::sleep_for(1000ms);
    char payload[300];
    sprintf(payload, "Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    printf(payload);
    ++arrivedcount;
}

void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client) {
    message_num++;
    MQTT::Message message;
    char buff[100];
    
    if (num_d < 10) {
      sprintf(buff, "data: %d, %d\n", num_buff[num_d], num_d);
      message.qos = MQTT::QOS0;
      message.retained = false;
      message.dup = false;
      message.payload = (void*) buff;
      message.payloadlen = strlen(buff) + 1;
      int rc = client->publish(topic, message);
      // printf("rc:  %d\r\n", rc);
      printf("%s\r\n", buff);
      num_d++;
    }
}

void close_mqtt() {
  closed = true;
}




