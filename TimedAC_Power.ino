#include "Ticker.h"

#define RLY_1 19      //Active LOW
#define RLY_2 23      //Active LOW
#define BTN_LED 21    //Active HIGH
#define BTN 22        //Active LOW
#define BLUE_LED 5    //Active LOW
#define PIR_SENSOR 17 //Active HIGH

#define TIME_BASE_MS 10 //used for time interrupt
#define RELAY_TIMEOUT 10000 //15*60000 //15 min
/***********CLASSES******************/
class button{
  private:
  uint8_t pin;
  uint8_t state;
  uint16_t time_count;
  uint16_t time_base;
  bool active_high;

  public:
  bool isOn, isOff, p_trig, n_trig;
  uint16_t onTime;

  button (uint8_t pin, uint16_t time_base, bool active_high=true){
    this->pin=pin;
    this->time_base=time_base;
    this->active_high=active_high;
  }
  void updateTimeCount(){
    int32_t time_dif = (int32_t)this->time_count - this->time_base; 
    if (time_dif<0){
      this->time_count=0;
    }else{
      this->time_count=time_dif;
    }

    if(this->isOn){
      this->onTime+=this->time_base;
    }
  }
  void task(){
    bool pin_state=digitalRead(this->pin);
    switch(this->state){
      case 0:{ //state off
        this->isOff=true;
        this->isOn = false;
        this->p_trig=false;
        this->n_trig=false;
        if ((this->active_high && pin_state)||(!this->active_high && !pin_state)){
          //input transition to on state
          this->time_count=100;
          this->state=1;
        }
        break;
      }
      case 1:{ //wait time to expire
        if (!((this->active_high && pin_state)||(!this->active_high && !pin_state))){
          //on state lost
          this->state=0;
          break;
        }
        if (this->time_count==0){
          this->p_trig=true;
          this->onTime=0; //reset "time on" counter
          this->state=2;
        }
        break;
      }
      case 2:{ //state on
        this->isOff=false;
        this->isOn = true;
        this->p_trig=false;
        this->n_trig=false;
        if (!((this->active_high && pin_state)||(!this->active_high && !pin_state))){
          //input transition to off state
          this->time_count=100;
          this->state=3;
          break;
        }
        break;
      }
      case 3:{ //wait time to expire
        if ((this->active_high && pin_state)||(!this->active_high && !pin_state)){
          //off state lost
          this->state=2;
          break;
        }
        if (this->time_count==0){
          this->n_trig=true;
          this->state=0;
        }
        break;
      }
      default:{
        break;
      }
    }
  }
};

/********GLOBAL VARIABLES************/
Ticker myTimer;
uint8_t timer_new_tick=false;
button start_btn = button(BTN, TIME_BASE_MS, true);
uint8_t relayState=0;
uint32_t relayTimeCounter=0;

static uint16_t pwm_width=0;
static bool pwm_count_up=true;

/*******Function deffinitions********/
void onTimer(){
  timer_new_tick+=1;
  start_btn.updateTimeCount();
  if (relayTimeCounter<TIME_BASE_MS){
    relayTimeCounter=0;
  }else{
    relayTimeCounter-=TIME_BASE_MS;
  }
}

void pwm_up_down(uint8_t min_lim, uint8_t max_lim){
  if (pwm_count_up){
    if (pwm_width<max_lim){
      pwm_width+=1;
    }else{
      pwm_count_up=false;
    }
  }else{
    if (pwm_width>min_lim){
      pwm_width-=1;
    }else{
      pwm_count_up=true;
    }
  }
}

void pwm_up(uint8_t max_lim){
    if (pwm_width<max_lim){
      pwm_width+=1;
    }else{
      pwm_width=0;
    }
}

void pwm_down(uint8_t max_lim){
    if (pwm_width>=1){
      pwm_width-=1;
    }else{
      pwm_width=max_lim;
    }
}

void setup() {
  pinMode(RLY_1, OUTPUT);
  digitalWrite(RLY_1, HIGH);
  pinMode(RLY_2, OUTPUT);
  digitalWrite(RLY_2, HIGH);
  pinMode(BTN_LED, OUTPUT);
  digitalWrite(BTN_LED, LOW);
  pinMode(BLUE_LED, OUTPUT);
  digitalWrite(BLUE_LED, HIGH);
  pinMode(BTN, INPUT);
  pinMode(PIR_SENSOR, INPUT);

  myTimer.attach(TIME_BASE_MS/1000.0, &onTimer);
  relayTimeCounter=1000; //1sec
}

void loop() {

  switch(relayState){
    case 0: {//device started
      digitalWrite(RLY_1, HIGH);
      if (relayTimeCounter==0){
        relayState=1;
      }
    }
    case 1: {//relay off
      digitalWrite(RLY_1, HIGH);
      if(start_btn.isOff){
        relayState=2;
      }
      break;
    }
    case 2: {//relay off - can be turned on
      if(start_btn.isOn){
        relayState=3;
      }
      break;
    }
    case 3: {//relay on
      digitalWrite(RLY_1, LOW);
      if(start_btn.isOff){
        relayState=4;
      }
      break;
    }
    case 4: {//relay on - can be turned off
      if(start_btn.isOn && start_btn.onTime>=1000){
        relayState=1;
      }
      if (relayTimeCounter==0){
        relayState=1;
      }
      break;
    }
  }
  
  
  //control btn led - based on relay state
  if(timer_new_tick>=2){
    switch (relayState){
      case 0:{
        analogWrite(BTN_LED, 0);
        break;
      }
      case 1:{
        pwm_up_down(0, 50);
        analogWrite(BTN_LED, pwm_width);
        break;
      }
      case 2:{
        pwm_up_down(0, 50);
        analogWrite(BTN_LED, pwm_width);
        break;
      }
      case 3:{
        pwm_up_down(50, 150);
        analogWrite(BTN_LED, pwm_width);
        break;
      }
      case 4:{
        pwm_up_down(50,150);
        if(digitalRead(PIR_SENSOR)){
          analogWrite(BTN_LED, 255);
        }else{
          analogWrite(BTN_LED, pwm_width);
        }
        
        break;
      }
    }
    timer_new_tick=0;
  }
    
  //rearm the relay time counter
  if(start_btn.isOn){
    if(relayState>0){//device initialization done
      relayTimeCounter=RELAY_TIMEOUT;
    }
  }  

  if(digitalRead(PIR_SENSOR)){
    digitalWrite(BLUE_LED, LOW);
    if(relayState>0){//device initialization done
      relayTimeCounter=RELAY_TIMEOUT;
    }
  }else{
    digitalWrite(BLUE_LED, HIGH);
  }
  start_btn.task();
}
