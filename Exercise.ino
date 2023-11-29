// SCOTT GONZALEZ BARRIOS
// CS120B Final Project
#include "Timer.h"
#include <IRremote.hpp>
#include <stdlib.h>

// GENERAL global variables
int rng = 0;      // store rng'd # during tick_rng (case: RNG_GENERATE)
int cnt = 0;      // store the # of successful rounds
int curr_cnt = 0; // store the current # index player is currently remembering

/////////////////////////////
// task variables and struct
const int period_rng = 1000, period_ir = 100, period_dispSeq = 1000, period_display = 5, period_buzzer = 100, period_GCD = 5, totalTasks = 5;
struct Task {
	unsigned short period;
	unsigned short timeElapsed;
	void (*tick) (); // function ran during task's tick
};
static struct Task tasks[totalTasks]; // five tasks

//////////////////////////////////////////////////////
// rng variables and funcs
enum states_rng{RNG_WAIT, RNG_GENERATE} state_rng = RNG_GENERATE; // to know whether or not to rng new number for sequence
int* rng_sequence;                    // to store the pointer to the dynamic array containing generated sequence
const int rng_sequenceBuffer = 15;    // the number of items that memory will be allocated for every resize
int rng_sequenceLength = 0;           // to track length of rng_sequence
int rng_sequenceCurrent = 0;          // to track index of next free space in rng_sequence

// allocate enough memory for dynamic array to store +rng_sequenceBuffer items
void rng_resizeArray(){
  int* placeholder = rng_sequence; // incase realloc fails, backup is saved
  rng_sequence = realloc(rng_sequence, (rng_sequenceBuffer + (rng_sequenceLength)) * sizeof(int)); 
  if(!rng_sequence){
    Serial.println("Memory realloc failed");
    rng_sequence = placeholder;
  }
  else{
    Serial.println("Memory realloc successful");
    rng_sequenceLength += rng_sequenceBuffer;
    for(int i = (rng_sequenceLength - rng_sequenceBuffer); i < rng_sequenceLength; i++){
      rng_sequence[i] = -1;
    }
  }
}

// allocate memory for a dynamic array that is rng_sequenceBuffer in length
void rng_createArray(){
  rng_sequence = (int*)calloc(rng_sequenceBuffer, sizeof(int)); 
  rng_sequenceLength += rng_sequenceBuffer;
  for(int i = 0; i < rng_sequenceLength; i++){
    rng_sequence[i] = -1;
  }
}

// using already allocated memory, reset values to -1 (aka null)
void rng_restartArray(){
  for(int i = 0; i < rng_sequenceLength; i++){
    rng_sequence[i] = -1;
  }
  // set the tracked index to 0 (restart)
  rng_sequenceCurrent = 0;
}

// print the current state of the sequence (including "-1")
void rng_printSequence(){
  for(int i = 0; i < rng_sequenceLength; i++){
    Serial.print(rng_sequence[i]);
    Serial.print(" ");
  }
  Serial.println();
}

//////////////////////////////////////////////////////
// Display Sequence Determinant Variables 
enum states_dispSeq{DISPSEQ_DISPLAY, DISPSEQ_NONE} state_dispSeq = DISPSEQ_NONE;  // to know whether the sequence or '-' should be displaying
int dispSeq_rng = 0;    // to know which # the user is on
int dispSeq_index = 0;  // to know which index is being displayed from the sequence.

////////////////////////
// INTRO IS INDEPENDENT OF OTHER TASKS
const int intro_pin = 12;         // output pin of passive buzzer
const int intro_numOfNotes = 32;  // number of notes in intro
int intro_currentNote = 0;  // current note being played
unsigned int intro_notes[] = {
  220, 220, 262, 220, // a3a3c4a3
  294, 220, 330, 294, // d4a3e4d4
  262, 262, 330, 262, // c4c4e4c4
  392, 220, 330, 262, // g4a3e4c4
  196, 196, 247, 196, // g3g3b3g3
  262, 196, 294, 262, // c4g3d4c4
  175, 175, 220, 175, // f3f3a3f3
  262, 275, 262, 247  // c4f3c4b3
}; // sequence of frequencies that plays crazy train

// plays note at intro_notes[i] hz on passive buzzer
void intro_playNote(int i){
  tone(intro_pin, intro_notes[i]);
}

// plays notes from intro_notes in sequence
void intro_play(){
  for(int i = 0; i < intro_numOfNotes; i++){
    intro_playNote(i);
    for(int j = 0; j < 45; j++){  // wait period * 40 between each note
      while(TimerFlag == 0){}
      TimerFlag = 0;
    }
  }
  noTone(intro_pin);
}

////////////////////////////
// display variables and functions
enum states_disp{DISP_CNT_ONES, DISP_CNT_TENS, DISP_RNG} state_disp = DISP_CNT_ONES; // to determine which digit is being displayed
const int gSegPins[] = {3,6,9,8,7,4,10}; // An array of pins of the arduino that are connected to segments a, b, c, d, e... g in that order.
const int dPins[] = {2,5,11};             // fourth, third and first digit (respectively)
unsigned char encodeInt[11] = {     // A map of integers to the respective values needed to display a value on one 7 seg digit.
    // 0     1     2     3     4     5     6     7     8     9     -
    0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x67, 0x40
};

// displays one number (between 0 and 9) "targetNum" on the digit conneceted to "digitPin"
void disp_displayTo7Seg(unsigned int targetNum, int digitPin) {
  // Make sure the pins are off while updating the segments iteratively
  digitalWrite(dPins[0], HIGH); 
  digitalWrite(dPins[1], HIGH);
  digitalWrite(dPins[2], HIGH);
  // Update the segments
  for (int k = 0; k < 7; ++k) {
      digitalWrite(gSegPins[k], encodeInt[targetNum] & (1 << k));
  }
  // Turn on the digit again
  digitalWrite(dPins[digitPin], LOW);
  return;
}

/////////////////////////////
// IR variables and functions
const int ir_receivePin = 16; // pin # the ir receiver is connected to 
const unsigned long ir_dictionary[] = {      // ith element corresponds to input # i (ex. 3910598400 == button "#1")
  3910598400,4077715200,3877175040,2707357440,4144561920,3810328320,2774204160,3175284480,2907897600,3041591040
};
int ir_input = 0;                        // stores decimal input from ir remote
int ir_wait_timeElapsed = 0;   // time elapsed between successful detection and next scan for detection
enum states_ir{IR_SCAN, IR_DETECTED, IR_DISPLAYINGSEQUENCE} state_ir = IR_SCAN; // should ir wait for detection, mark detection or pause to display sequence?
int ir_interpret(IRRawDataType data){  // return decimal equivalent of data
  for(int i = 0; i < 10; i++){
    if(data == ir_dictionary[i]){
      return i;
    }
  }
  return -1;
}

/////////////////////////////////
// buzzer variables
enum states_buzzer{BUZZ_WAIT, BUZZ_CLICKREGISTERED, BUZZ_MISS_P1, BUZZ_MISS_P2} state_buzzer = BUZZ_WAIT; // self explanatory
const int buzz_pin = 15; // pin connected to active buzzer
int buzz_current = 0;    // 0 = dont buzz, 1 = do buzz

///////////////////////////////
// ticks

// tick for rng
void tick_rng(){
  switch(state_rng){
  case RNG_WAIT:
    // rng_printSequence(); - only include during testing
  break;
  case RNG_GENERATE:
    rng = random(10); // rng between 0-9
    rng_sequence[rng_sequenceCurrent] = rng;  // store where first '-1' occurs
    rng_sequenceCurrent++;                    // move index along to next '-1'
    if(rng_sequenceCurrent >= rng_sequenceLength - 5){  // if only 5 spots are left, resize to add rng_sequenceBuffer # of items
      rng_resizeArray();
    }
    // have 7-seg display sequence and pause ir detector accordingly
    state_dispSeq = DISPSEQ_DISPLAY;
    state_ir = IR_DISPLAYINGSEQUENCE;
    state_rng = RNG_WAIT;
    break;
  }
}

// tick for the 7-seg display's d1 value determinant
void tick_displaySequence(){  
  switch(state_dispSeq){
  case DISPSEQ_DISPLAY:
    dispSeq_rng = rng_sequence[dispSeq_index];  // get to-be-displayed # from rng_sequence
    dispSeq_index++;                            // slide index to next-to-be-displayed #
    if(dispSeq_index >= rng_sequenceCurrent){   // if final number has been displayed
      state_dispSeq = DISPSEQ_NONE;
    }
  break;
  case DISPSEQ_NONE:  // display '-' on D1 and have ir detector start scanning
    dispSeq_rng = 10;
    state_ir = IR_SCAN;
  break;
  }
}

// tick for the 7 seg display
void tick_display(){  
  switch(state_disp){
  case DISP_CNT_ONES:
    disp_displayTo7Seg(cnt % 10, 1);    // display one's place digit on d3
    state_disp = DISP_CNT_TENS;
  break;
  case DISP_CNT_TENS:
    disp_displayTo7Seg(cnt / 10, 0);    // display ten's place digit on d4
    state_disp = DISP_RNG;
  break;
  case DISP_RNG:
    disp_displayTo7Seg(dispSeq_rng, 2); // display # from sequence or '-' on d1
    state_disp = DISP_CNT_ONES;         // restart to left hand side
  break;
  }
}

void game_processInput(int input);  // prototype

// tick for ir receiver
void tick_ir(){
  switch(state_ir){
  case IR_SCAN:
  if (IrReceiver.decode()){
      ir_input = ir_interpret(IrReceiver.decodedIRData.decodedRawData);
      if(ir_input >= 0){  // only process input if it is >-1 (0-9)
        game_processInput(ir_input);
        state_ir = IR_DETECTED;
      }
      IrReceiver.resume();
  }
  break;
  case IR_DETECTED:
  ir_wait_timeElapsed += period_ir;
  if(ir_wait_timeElapsed >= 300){ // wait 300 ms
    state_ir = IR_SCAN;
    ir_wait_timeElapsed = 0;
  }
  break;
  case IR_DISPLAYINGSEQUENCE:
  break;
  }
}

// tick for buzzer
void tick_buzzer(){
  switch(state_buzzer){
  case BUZZ_WAIT:
    digitalWrite(buzz_pin, LOW);
    break;
  case BUZZ_CLICKREGISTERED:
    digitalWrite(buzz_pin, HIGH);
    state_buzzer = BUZZ_WAIT;
    break;
  case BUZZ_MISS_P1:
    digitalWrite(buzz_pin, HIGH);
    state_buzzer = BUZZ_MISS_P2; // longer buzz
    break;
  case BUZZ_MISS_P2:
    digitalWrite(buzz_pin, LOW);
    state_buzzer = BUZZ_CLICKREGISTERED; 
    break;
  }
}


/////////////////////
// game funcs

// When all numbers in sequence have been successfully inputted
void game_roundCompleted(); 
void game_continue();
void game_mistake();       
// Process whether or not the input is correct and if the sequence has been fully memorized
void game_processInput(int input){
  if(input == rng_sequence[curr_cnt]){
    if((curr_cnt + 1) == rng_sequenceCurrent){
      game_roundCompleted();
    }
    else{
      game_continue();
    }
  }
  else{
    game_mistake();
  }
}

void game_roundCompleted(){
  curr_cnt = 0;
  cnt++;
  dispSeq_index = 0;
  state_rng = RNG_GENERATE;
  state_buzzer = BUZZ_CLICKREGISTERED;
}

// To move onto the next number in the sequence
void game_continue(){
  curr_cnt++;
  state_buzzer = BUZZ_CLICKREGISTERED;
}

// When the player input incorrect value
void game_mistake(){
  curr_cnt = 0;
  cnt = 0;
  dispSeq_rng = 10;
  dispSeq_index = 0;
  state_rng = RNG_GENERATE;
  state_buzzer = BUZZ_MISS_P1;
  state_disp = DISP_CNT_ONES;
  rng_restartArray();
}

// initialize the structs representing the tasks wished to be executed sequentially
void initTasks(){
  tasks[0].period = period_rng;
  tasks[0].timeElapsed = period_rng;
  tasks[0].tick = tick_rng;

  tasks[1].period = period_dispSeq;
  tasks[1].timeElapsed = period_dispSeq;
  tasks[1].tick = tick_displaySequence;

  tasks[2].period = period_display;
  tasks[2].timeElapsed = period_display;
  tasks[2].tick = tick_display;

  tasks[3].period = period_ir;
  tasks[3].timeElapsed = period_ir;
  tasks[3].tick = tick_ir;

  tasks[4].period = period_buzzer;
  tasks[4].timeElapsed = period_buzzer;
  tasks[4].tick = tick_buzzer;

  return;
}

// Schedule the created tasks (rng -> dispSeq -> display -> ir -> buzzer)
void scheduleTasks(){
  for(int i = 0; i < totalTasks; i++){
    tasks[i].timeElapsed += period_GCD;
    if(tasks[i].timeElapsed >= tasks[i].period){
      tasks[i].tick();
      tasks[i].timeElapsed = 0;
    }
  }
  return;
}

void setup() {
  pinMode(A0, INPUT);
  pinMode(buzz_pin, OUTPUT);
  pinMode(gSegPins[0], OUTPUT);
  pinMode(gSegPins[1], OUTPUT);
  pinMode(gSegPins[2], OUTPUT);
  pinMode(gSegPins[3], OUTPUT);
  pinMode(gSegPins[4], OUTPUT);
  pinMode(gSegPins[5], OUTPUT);
  pinMode(gSegPins[6], OUTPUT);
  pinMode(dPins[0], OUTPUT);
  pinMode(dPins[1], OUTPUT);
  pinMode(dPins[2], OUTPUT);

  TimerSet(period_GCD);
  TimerOn();
  Serial.begin(9600);

  intro_play();

  IrReceiver.begin(ir_receivePin, ENABLE_LED_FEEDBACK);

  int in = analogRead(A0);
  randomSeed(in);

  rng_createArray();

  initTasks();
}

void loop() {
  scheduleTasks();
  while(TimerFlag == 0){}
  TimerFlag = 0;
}
