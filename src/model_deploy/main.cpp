#include "DA7212.h"
#include "mbed.h"
#include "fsl_port.h"
#include "fsl_gpio.h"
#include "iostream"
#include "stdint.h"
#include "cmath"
#include "uLCD_4DGL.h"
#include "accelerometer_handler.h"
#include "config.h"
#include "magic_wand_model_data.h"
#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"
#define bufferLength (32)
#define signalLength (48)

using namespace std; 

DA7212 audio;
uLCD_4DGL uLCD(D1, D0, D2);

Serial pc(USBTX, USBRX);



Timer main_timer;
DigitalOut redLED(LED1);
DigitalOut greenLED(LED2);
DigitalOut blueLED(LED3);
DigitalIn Switch(SW3);  // select the choice
InterruptIn Button(SW2);  // song to mode 

bool songtomode = false;

// EventQueue _queue_DNN;
EventQueue _queue_playmusic;
EventQueue _queue_mainmenu;

// Thread _thread_DNN; 
Thread _thread_playmusic;
Thread _thread_mainmenu(osPriorityNormal, 100*1024);

int now_song = 2;
int now_mode = 0;
//song menu = 0; mode menu = 1;
int now_menu = 0;
int gesture_id = 0;

float signal_arr[signalLength];
float signal_twinkle[signalLength];
float signal_birthday[signalLength];
float signal_tiger[signalLength];
int16_t waveform[kAudioTxBufferSize];
char serialInBuffer[bufferLength];
int serialCount = 0;
int idC=0;

int SELECT = 0;


/*******MODEL TRAINING PART**********/
// Create an area of memory to use for input, output, and intermediate arrays.
// The size of this will depend on the model you're using, and may need to be
// determined by experimentation.
constexpr int kTensorArenaSize = 60 * 1024;
uint8_t tensor_arena[kTensorArenaSize];

// Whether we should clear the buffer next time we fetch data
bool should_clear_buffer = false;
bool got_data = false;

// Set up the model
// Set up logging.
static tflite::MicroErrorReporter micro_error_reporter;
tflite::ErrorReporter* error_reporter = &micro_error_reporter;
const tflite::Model* model;
static tflite::MicroOpResolver<6> micro_op_resolver;
tflite::MicroInterpreter* interpreter;
TfLiteTensor* model_input;
int input_length;
/**********************************/

void Display_mode();
void Display_song();
void Gesture();

void loadSignal(void)
{
  uLCD.printf("\nStart to load signal...\n");
  greenLED = 0;
  int i = 0;
  serialCount = 0;
  audio.spk.pause();
  // twinkle
  while(i < signalLength)
  {
    if(pc.readable())
    {
      serialInBuffer[serialCount] = pc.getc();  // get char 
      serialCount++;
      if(serialCount == 5)
      {
        serialInBuffer[serialCount] = '\0';
        signal_twinkle[i] = (float) atof(serialInBuffer);
        serialCount = 0;
        i++;
      }
    }
  }  

  //birthday
  i = 0;
  serialCount = 0;
  while(i < signalLength)
  {
    if(pc.readable())
    {
      serialInBuffer[serialCount] = pc.getc();  // get char 
      serialCount++;
      if(serialCount == 5)
      {
        serialInBuffer[serialCount] = '\0';
        signal_birthday[i] = (float) atof(serialInBuffer);
        serialCount = 0;
        i++;
      }
    }
  }

  //two tigers
  i = 0;
  serialCount = 0;
  while(i < signalLength)
  {
    if(pc.readable())
    {
      serialInBuffer[serialCount] = pc.getc();  // get char 
      serialCount++;
      if(serialCount == 5)
      {
        serialInBuffer[serialCount] = '\0';
        signal_tiger[i] = (float) atof(serialInBuffer);
        serialCount = 0;
        i++;
      }
    }
  }
  greenLED = 1;
}

void playNote(int freq)
{
  for (int i = 0; i < kAudioTxBufferSize; i++)
  {
    waveform[i] = (int16_t) (signal_arr[(uint16_t) (i * freq * signalLength * 1. / kAudioSampleFrequency) % signalLength] * ((1<<16) - 1));
  }
  // the loop below will play the note for the duration of 1s
  for(int j = 0; j < kAudioSampleFrequency / kAudioTxBufferSize; ++j)
  {
    audio.spk.play(waveform, kAudioTxBufferSize);
  }
}

void playNote_Taiko(int freq)
{
  for (int i = 0; i < kAudioTxBufferSize; i++)
  {
    waveform[i] = (int16_t) (signal_arr[(uint16_t) (i * freq * signalLength * 1. / kAudioSampleFrequency) % signalLength] * ((1<<16) - 1));
  }
  // the loop below will play the note for the duration of 1s
  for(int j = 0; j < kAudioSampleFrequency / kAudioTxBufferSize; ++j)
  {
    audio.spk.play(waveform, kAudioTxBufferSize);
  }
}

void copymusic(int song)
{
  int i = 0;
  for(i=0; i<48; i++)
  {
    if(song == 0)
    { // twinkle
      signal_arr[i] = signal_twinkle[i];
    }
    else if(song == 1)
    {// birthday
      signal_arr[i] = signal_birthday[i];
    }
    else if (song == 2)
    {
      signal_arr[i] = signal_tiger[i];
    }
  }
}

void PlayMusic()
{  // start play song
  uLCD.cls();
  greenLED = 1;
  redLED = 1;
  blueLED = 1;
  if(now_song == 0)
  {
    uLCD.printf("\nplaying song: \n");
    uLCD.printf("\n TWINKLE\n");
  }
  else if(now_song == 1)
  {
    uLCD.printf("\nplaying song: \n");
    uLCD.printf("\n HAPPY BIRTHDAY\n");
  }
  else if(now_song == 2)
  {
    uLCD.printf("\nplaying song: \n");
    uLCD.printf("\n TWO TIGERS\n");
  }
  copymusic(now_song);
  wait(5);

  int i =0;
  int f = 0;
  while(i < 48)
  {
    f = int(1000*signal_arr[i]);
    uLCD.cls();
    uLCD.printf("\nfreq = %d\n", f);
    playNote(f);
    wait(0.1);
    i++;
  }
  SELECT = 0;
  Gesture();

}

// Return the result of the last prediction
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


void Gesture()
{
  greenLED = 1;
  redLED = 1;
  blueLED = 1;
  int gesture_index;
  uLCD.cls();
  uLCD.printf("\nin Gesture\n");
  
  wait(1);

  while( SELECT == 0 )
  {
    /////////////////////////////////////////////////
    // start to check gesture
    // Attempt to read new data from the accelerometer
    
    redLED = 0;
    uLCD.printf("\nStart Selecting\n");
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

    redLED = 1;
    /////////////////////////////////////////////////



    gesture_index = 0;
    //now_menu = 1;
    // now_mode = 3;
    // now_song = 0;
    uLCD.cls();
    uLCD.printf("\nNow Selection: \n");
    uLCD.printf("\nnow_menu =  %d \n ", now_menu);
    uLCD.printf("\nnow_mode =  %d \n ", now_mode);
    uLCD.printf("\nnow_song =  %d \n ", now_song);
    uLCD.printf("\ngesture index = %d\n", gesture_index);
    wait(3);

    if(now_menu == 0)
    { // song menu 
        if(gesture_index == 0)
        { // next
          now_song = (now_song + 1)%3;
          // if(now_song == 3)
          // {
          //   now_song = 0;
          // }
        }
        uLCD.printf("\nnow_song =  %d \n ", now_song);

        // change the screen display 
        if(now_song == 0)
        {
          uLCD.cls();
          uLCD.textbackground_color(BLACK);
          uLCD.printf("\n****SONG MENU****\n");
          uLCD.textbackground_color(BLUE);
          uLCD.printf(" SW2-> mode menu\n");
          uLCD.printf(" SW3-> play song\n");
          uLCD.textbackground_color(RED);
          uLCD.printf("\n1.TWINKLE \n");
          uLCD.textbackground_color(BLACK);
          uLCD.printf("  song length = 48 \n");
          uLCD.printf("\n2.HAPPY BIRTHDAY\n");
          uLCD.printf("  song length = 48 \n");
          uLCD.printf("\n3.TWO TIGERS\n");
          uLCD.printf("  song length = 48 \n");
        }
        else if (now_song == 1)
        {
          uLCD.cls();
          uLCD.textbackground_color(BLACK);
          uLCD.printf("\n****SONG MENU****\n");
          uLCD.textbackground_color(BLUE);
          uLCD.printf(" SW2-> mode menu\n");
          uLCD.printf(" SW3-> play song\n");
          uLCD.textbackground_color(BLACK);
          uLCD.printf("\n1.TWINKLE \n");
          uLCD.printf("  song length = 48 \n");
          uLCD.textbackground_color(RED);
          uLCD.printf("\n2.HAPPY BIRTHDAY\n");
          uLCD.textbackground_color(BLACK);
          uLCD.printf("  song length = 48 \n");
          uLCD.printf("\n3.TWO TIGERS\n");
          uLCD.printf("  song length = 48 \n");
        }
        else if (now_song == 2)
        {
          uLCD.cls();
          uLCD.textbackground_color(BLACK);
          uLCD.printf("\n****SONG MENU****\n");
          uLCD.textbackground_color(BLUE);
          uLCD.printf(" SW2-> mode menu\n");
          uLCD.printf(" SW3-> play song\n");
          uLCD.textbackground_color(BLACK);
          uLCD.printf("\n1.TWINKLE \n");
          uLCD.printf("  song length = 48 \n");
          uLCD.printf("\n2.HAPPY BIRTHDAY\n");
          uLCD.printf("  song length = 48 \n");
          uLCD.textbackground_color(RED);
          uLCD.printf("\n3.TWO TIGERS\n");
          uLCD.textbackground_color(BLACK);
          uLCD.printf("  song length = 48 \n");
        }

    }
    else if(now_menu == 1)
    { //mode menu
        if(gesture_index == 0)
        { // next
          now_mode = (now_mode + 1)%4;
          // if(now_song == 3)
          // {
          //   now_song = 0;
          // }
        }
        //change the screen display
        if(now_mode == 0)
        { // previous song
            uLCD.cls();
            uLCD.textbackground_color(BLACK);
            uLCD.printf("\n####MODE MENU####\n");
            uLCD.textbackground_color(BLUE);
            uLCD.printf(" SW2-> mode menu\n");
            uLCD.printf(" SW3-> play song\n");
            uLCD.textbackground_color(RED);
            uLCD.printf("\n1.previous song\n");
            uLCD.textbackground_color(BLACK);
            uLCD.printf("\n2.next song\n");
            uLCD.printf("\n3.song menu\n");
            uLCD.printf("\n4.Taiko\n");
        }
        else if (now_mode == 1)
        { // next song
            uLCD.cls();
            uLCD.textbackground_color(BLACK);
            uLCD.printf("\n####MODE MENU####\n");
            uLCD.textbackground_color(BLUE);
            uLCD.printf(" SW2-> mode menu\n");
            uLCD.printf(" SW3-> play song\n");
            uLCD.textbackground_color(BLACK);
            uLCD.printf("\n1.previous song\n");
            uLCD.textbackground_color(RED);
            uLCD.printf("\n2.next song\n");
            uLCD.textbackground_color(BLACK);
            uLCD.printf("\n3.song menu\n");
            uLCD.printf("\n4.Taiko\n");
        }
        else if(now_mode ==2)
        { // song menu
            uLCD.cls();
            uLCD.textbackground_color(BLACK);
            uLCD.printf("\n####MODE MENU####\n");
            uLCD.textbackground_color(BLUE);
            uLCD.printf(" SW2-> mode menu\n");
            uLCD.printf(" SW3-> song menu\n");
            uLCD.textbackground_color(BLACK);
            uLCD.printf("\n1.previous song\n");
            uLCD.printf("\n2.next song\n");
            uLCD.textbackground_color(RED);
            uLCD.printf("\n3.song menu\n");
            uLCD.textbackground_color(BLACK);
            uLCD.printf("\n4.Taiko\n");
        }
        else if(now_mode ==3)
        { // Taiko
            uLCD.cls();
            uLCD.textbackground_color(BLACK);
            uLCD.printf("\n####MODE MENU####\n");
            uLCD.textbackground_color(BLUE);
            uLCD.printf(" SW2-> mode menu\n");
            uLCD.printf(" SW3-> Taiko\n");
            uLCD.textbackground_color(BLACK);
            uLCD.printf("\n1.previous song\n");
            uLCD.printf("\n2.next song\n");
            uLCD.printf("\n3.song menu\n");
            uLCD.textbackground_color(RED);
            uLCD.printf("\n4.Taiko\n");
            uLCD.textbackground_color(BLACK);
        }
    }
    greenLED = 0;
    wait(1);
    greenLED = 1;
    
    
    // if SW2 is pressed
    if(songtomode)
    {
      songtomode = false;
      Display_mode();
    }
    // if SW3 is pressed
    if( Switch == 0 )
    {
      uLCD.printf("\nin switch == 0\n");
      if(now_menu == 0)
      {// play song 
        _queue_mainmenu.call(PlayMusic);
      }
      else
      {
        if(now_mode == 0)
        { //play song
          // prev song
          if (now_song == 0)
          {
            now_song = 2;
          }
          else
          {
            now_song = now_song -1;
          }
          _queue_mainmenu.call(PlayMusic);
        }
        else if( now_mode == 1) 
        { // next song
          if (now_song == 2)
          {
            now_song = 0;
          }
          else
          {
            now_song = now_song +1;
          }
          _queue_mainmenu.call(PlayMusic);
        }
        else if(now_mode == 2)
        { // go to song menu
          _queue_mainmenu.call(Display_song);
        }
        else
        { // taiko
          
        } 
      }
      SELECT = 1; // break out the while
    }
  }
  
  wait(1);
  
}


void Display_song()
{
    now_menu = 0; 
    SELECT = 0;
    Gesture();
}

void Display_mode()
{
    now_menu = 1;
    SELECT = 0;
    Gesture();
}

void StoM()
{
  songtomode = true;
}

int main(void)
{
    // _thread_DNN.start(callback(&_queue_DNN, &EventQueue::dispatch_forever));
    // _thread_playmusic.start(callback(&_queue_playmusic, &EventQueue::dispatch_forever));
    _thread_mainmenu.start(callback(&_queue_mainmenu, &EventQueue::dispatch_forever));
    
    
    Button.rise(&StoM);
    
    /******** SET UP THE MODEL ********/
    
    // Map the model into a usable data structure. This doesn't involve any
    // copying or parsing, it's a very lightweight operation.
    //const tflite::Model* model = tflite::GetModel(g_magic_wand_model_data);
    model = tflite::GetModel(g_magic_wand_model_data);


    if (model->version() != TFLITE_SCHEMA_VERSION) {
        error_reporter->Report(
            "Model provided is schema version %d not equal "
            "to supported version %d.",
            model->version(), TFLITE_SCHEMA_VERSION);
        return -1;
    }

    //static tflite::MicroOpResolver<5> micro_op_resolver;
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
    interpreter = &static_interpreter;
    //tflite::MicroInterpreter* interpreter = &static_interpreter;

    // Allocate memory from the tensor_arena for the model's tensors
    interpreter->AllocateTensors();

    // Obtain pointer to the model's input tensor
    //TfLiteTensor* model_input = interpreter->input(0);
    model_input = interpreter->input(0);
    if ((model_input->dims->size != 4) || (model_input->dims->data[0] != 1) ||
        (model_input->dims->data[1] != config.seq_length) ||
        (model_input->dims->data[2] != kChannelNumber) ||
        (model_input->type != kTfLiteFloat32)) {
        error_reporter->Report("Bad input tensor parameters in model");
        return -1;
    }

    input_length = model_input->bytes / sizeof(float);

    TfLiteStatus setup_status = SetupAccelerometer(error_reporter);
    if (setup_status != kTfLiteOk) {
        error_reporter->Report("Set up failed\n");
        return -1;
    }

    error_reporter->Report("Set up successful...\n");
    /*****************/
    
      // if SW2 is pressed, interrupt current and enter the mode menu
      // if SW3 is pressed, select the choice
    
    redLED = 1;
    greenLED = 1;
    blueLED = 1;
    uLCD.cls();
    uLCD.textbackground_color(BLACK);
    uLCD.printf("\nH E L L O\n");
    uLCD.printf("\nLat's load music first....\n");
    wait(5);
    loadSignal();
    audio.spk.pause();
    uLCD.printf("\nEnter Song Menu\n");
    uLCD.printf("\n in 5s !\n");
    uLCD.printf("\nEnjoy!!\n"); 
    wait(3);

    // enter song menu
    //_queue_mainmenu.call_in(5000, Display_song);
    now_menu = 1; 
    greenLED = 1;
    redLED = 1;
    blueLED = 1;
    uLCD.cls();
    SELECT = 0;
    Display_song(); 
}
