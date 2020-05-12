#include "DA7212.h"
#include "mbed.h"
#include "fsl_port.h"
#include "fsl_gpio.h"
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
#define signalLength (1024)

using namespace std; 

DA7212 audio;
uLCD_4DGL uLCD(D1, D0, D2);

Serial pc(USBTX, USBRX);

Timer main_timer;
DigitalOut redLED(LED1);
DigitalOut greenLED(LED2);
DigitalIn Switch(SW3);   // select the choice
InterruptIn button(SW2); // raised and enter the mode selection page 

EventQueue _queue_songmenu;
EventQueue _queue_playmusic;
EventQueue _queue_modemenu;

Thread _thread_songmenu; 
Thread _thread_playmusic;
Thread _thread_modemenu;

int now_song = 0;
//song menu = 0; mode menu = 1;
int now_menu = 0;

float signal_arr[signalLength];
int16_t waveform[kAudioTxBufferSize];
char serialInBuffer[bufferLength];
int serialCount = 0;
int idC=0;


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
static tflite::MicroOpResolver<5> micro_op_resolver;
tflite::MicroInterpreter* interpreter;
TfLiteTensor* model_input;
/**********************************/

class Song
{
    public:
        int length;
};
Song TWINKLE;
Song BIRTHDAY;


void loadSignal(void)
{
  greenLED = 0;
  int i = 0;
  serialCount = 0;
  audio.spk.pause();
  
  while(i < signalLength)
  {
    if(pc.readable())
    {
      serialInBuffer[serialCount] = pc.getc();
      serialCount++;
      if(serialCount == 5)
      {
        serialInBuffer[serialCount] = '\0';
        signal_arr[i] = (float) atof(serialInBuffer);
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

void loadSignalHandler(void) {_queue_playmusic.call(loadSignal);}

void playNoteC(void) {idC = _queue_playmusic.call_every(1, playNote, 261);}

void stopPlayNoteC(void) {_queue_playmusic.cancel(idC);}



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

void PlayMusic()
{
    _queue_playmusic.event(loadSignalHandler);
    _queue_playmusic.event(playNoteC);
    _queue_playmusic.event(stopPlayNoteC);

}

void Display_mode();
void Display_song()
{
    greenLED = 0;
    now_menu = 0;
    int choice = 0;
    int song_gesture_index = 0;
    uLCD.cls();
    uLCD.printf("\n****SONG MENU****\n");
    uLCD.printf("\n1.TWINKLE \n");
    uLCD.printf("\n  song length = 32 \n");
    uLCD.printf("\n2.HAPPY BIRTHDAY\n");
    uLCD.printf("\n  song length = 28 \n");
    
    // in choosing mode
    greenLED = 1;
    redLED = 0;
    // if SW2 is pressed, enter mode menu
    //button.rise(_queue_modemenu.event(Display_song));
    button.rise(_queue_modemenu.event(Display_mode));
    // button.rise(Display_mode);
    song_gesture_index = PredictGesture(interpreter->output(0)->data.f);

    // use gesture to define which song is going to play 
    if(song_gesture_index == 0) 
    { // next song
        now_song = (now_song + 1) %2;
    }
    else if(song_gesture_index == 1)
    { // prev song
        now_song = (now_song - 1) %2;
    }
    redLED = 1; 
    // if the switch is pressed then go to play mode
    if(Switch == 0)
    { //start to play the song
        _queue_playmusic.event(PlayMusic);
    }

    choice = now_song +1;
    uLCD.locate("100", "100");
    uLCD.printf("\nyour choice: %d\n", choice);

    //}


}

void Display_mode()
{
    greenLED = 0;
    redLED = 1;
    now_menu = 1;
    int choice = 0;
    int mode_gesture_index = 0;
    uLCD.cls();
    uLCD.printf("\n####MODE MENU####\n");
    uLCD.printf("\n1.previous song\n");
    uLCD.printf("\n2.next song\n");
    uLCD.printf("\n3.song menu\n");

    // in choosing mode
    greenLED = 1;
    mode_gesture_index = PredictGesture(interpreter->output(0)->data.f);

    // use gesture to define which mode
    if(mode_gesture_index == 0) 
    { // previous song
        now_song = (now_song - 1) %2;
    }
    else if(mode_gesture_index == 1)
    { // next song
        now_song = (now_song + 1) %2;
    }
    else
    { // song menu
        now_menu = 10;
    }

    if(Switch == 0)
    {
        if(now_menu == 10)
        { // enter song menu
            button.rise(_queue_songmenu.event(Display_song));
        }
        else
        { // start to play song
            _queue_playmusic.event(PlayMusic);
        }
        
    }

}

int main(void)
{
    main_timer.start();
    _thread_songmenu.start(callback(&_queue_songmenu, &EventQueue::dispatch_forever));
    _thread_playmusic.start(callback(&_queue_playmusic, &EventQueue::dispatch_forever));
    _thread_modemenu.start(callback(&_queue_modemenu, &EventQueue::dispatch_forever));
    
    TWINKLE.length = 48;
    BIRTHDAY.length = 28;

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

    int input_length = model_input->bytes / sizeof(float);

    TfLiteStatus setup_status = SetupAccelerometer(error_reporter);
    if (setup_status != kTfLiteOk) {
        error_reporter->Report("Set up failed\n");
        return -1;
    }

    error_reporter->Report("Set up successful...\n");
    /************************/

      // if SW2 is pressed, interrupt current and enter the mode menu
      // if SW3 is pressed, select the choice
        redLED = 1;
        greenLED = 1;
        uLCD.cls();
        uLCD.textbackground_color(BLACK);
        uLCD.printf("\nH E L L O\n");
        uLCD.textbackground_color(BLUE);
        uLCD.printf("\nPress SW2 to song menu.\n");
        uLCD.textbackground_color(BLACK);
        uLCD.printf("\nEnjoy!!\n"); 
        button.rise(_queue_modemenu.event(Display_song));

}




/*
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


int main(int argc, char* argv[]) 
{
  // Create an area of memory to use for input, output, and intermediate arrays.
  // The size of this will depend on the model you're using, and may need to be
  // determined by experimentation.
  constexpr int kTensorArenaSize = 60 * 1024;
  uint8_t tensor_arena[kTensorArenaSize];

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
    return -1;
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
    return -1;
  }

  int input_length = model_input->bytes / sizeof(float);

  TfLiteStatus setup_status = SetupAccelerometer(error_reporter);
  if (setup_status != kTfLiteOk) {
    error_reporter->Report("Set up failed\n");
    return -1;
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
    if (gesture_index < label_num) {
      error_reporter->Report(config.output_message[gesture_index]);
    }
  }
}
*/