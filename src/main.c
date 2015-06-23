#include <pebble.h>

Window *window;
TextLayer *text_layer;
TextLayer *op_text_layer;

#define MAIN_WIDTH (144 - ACTION_BAR_WIDTH - 2)

struct tricalc_click_context {
  bool long_click;
  bool button_down;
  int click_count;
  ButtonId button;
  AppTimer* timer_handle;
};

char* buttonvalues[NUM_BUTTONS] = {"", "123.", "456E", "7890"};

char accumulator_string[100] = "\0";
uint32_t accumulator_string_offset = 0;

static void button_timer_reset(struct tricalc_click_context* clickcontext, uint32_t timeout_ms);  

/*FIXME*/
struct tricalc_click_context click_context = {false, false, 0, NUM_BUTTONS, NULL};
                                      
static void update_label(struct tricalc_click_context* clickcontext) {
  int clickid = (clickcontext->click_count - 1) % 4;
  APP_LOG(APP_LOG_LEVEL_DEBUG, clickcontext->long_click ? "l update" : "update" );
  if (!clickcontext->long_click) {
    if (clickcontext->button < NUM_BUTTONS && accumulator_string_offset < (sizeof(accumulator_string)-1)) {
      accumulator_string[accumulator_string_offset] = buttonvalues[clickcontext->button][clickid];
    }
    text_layer_set_text(text_layer, accumulator_string);
  } else {
    switch (clickid) {
      case 0:
        text_layer_set_text(op_text_layer, "*");
        break;
      case 1:
        text_layer_set_text(op_text_layer, "/");
        break;
      case 2:
        text_layer_set_text(op_text_layer, "+");
        break;
      case 3:
        text_layer_set_text(op_text_layer, "-");
        break;
    }
  }
}

static void action_commit(struct tricalc_click_context* clickcontext) {
  update_label(clickcontext);
  if (!clickcontext->long_click && accumulator_string_offset < (sizeof(accumulator_string)-1)) {
    accumulator_string_offset++;
    accumulator_string[accumulator_string_offset] = '\0';
  }
  text_layer_set_text(op_text_layer, "");
}

static void tricalc_clickcontext_reset(struct tricalc_click_context* clickcontext) {
  clickcontext->long_click = 0;
  clickcontext->button_down = 0;
  clickcontext->click_count = 0;
  clickcontext->button = NUM_BUTTONS;
  clickcontext->timer_handle = NULL;
}

static void button_timer_elapsed_handler(void *context) {
  struct tricalc_click_context* clickcontext = (struct tricalc_click_context*) context;
  
  if (clickcontext->button_down) {
    clickcontext->long_click = true;
    clickcontext->click_count++;
    /*XXX: avoid counter invalid log message*/
    clickcontext->timer_handle = NULL;
  APP_LOG(APP_LOG_LEVEL_DEBUG, clickcontext->long_click ? "l timer elapsed" : "timer elapsed" );
    update_label(clickcontext);
    button_timer_reset(clickcontext, 500);
  } else {
    action_commit(clickcontext);
  APP_LOG(APP_LOG_LEVEL_DEBUG, clickcontext->long_click ? "l timer elapsed" : "timer elapsed" );
    tricalc_clickcontext_reset(clickcontext);
  }
}

static void button_timer_reset(struct tricalc_click_context* clickcontext, uint32_t timeout_ms) {
  if (clickcontext->timer_handle) {
    app_timer_reschedule(clickcontext->timer_handle, timeout_ms);
  } else {
    clickcontext->timer_handle = app_timer_register(timeout_ms, button_timer_elapsed_handler, clickcontext);
  }
}

static void click_up_handler(ClickRecognizerRef recognizer, void *context) {
  struct tricalc_click_context* clickcontext = (struct tricalc_click_context*) context;
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "click up");
  clickcontext->button_down = false;
  /*XXX: this assumes timers and the app are in a single thread*/
  if (clickcontext->long_click) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "repeatingb");
    action_commit(clickcontext);
    app_timer_cancel(clickcontext->timer_handle);
    tricalc_clickcontext_reset(clickcontext);
  } else {
    clickcontext->click_count++;
    update_label(clickcontext);
  }
  
}

static void click_down_handler(ClickRecognizerRef recognizer, void *context) {
  struct tricalc_click_context* clickcontext = (struct tricalc_click_context*) context;
  ButtonId button = click_recognizer_get_button_id(recognizer);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "click down");
  /*deal with pending short button input*/
  if (button != clickcontext->button && !clickcontext->long_click && clickcontext->timer_handle) {
    action_commit(clickcontext);
    app_timer_cancel(clickcontext->timer_handle);
    tricalc_clickcontext_reset(clickcontext);
  }
  
  clickcontext->button = button;
  clickcontext->button_down = true;
  button_timer_reset(clickcontext, 500);
}

static void back_click_handler(ClickRecognizerRef recognizer, void *context) {
  struct tricalc_click_context* clickcontext = (struct tricalc_click_context*) context;
  ButtonId button = click_recognizer_get_button_id(recognizer);
  APP_LOG(APP_LOG_LEVEL_DEBUG, "back");
  /*deal with pending short button input*/
  if (clickcontext->timer_handle) {
    /*cancel current event*/
    app_timer_cancel(clickcontext->timer_handle);
    text_layer_set_text(op_text_layer, "");
    accumulator_string[accumulator_string_offset] = accumulator_string_offset ? '\0' : '0';
    text_layer_set_text(text_layer, accumulator_string);
  } else {
    if (accumulator_string_offset >= 1) {
      accumulator_string_offset--;
      accumulator_string[accumulator_string_offset] = accumulator_string_offset ? '\0' : '0';
      text_layer_set_text(text_layer, accumulator_string);
    } else {
      //back out if nothing left to exit
      //TODO: double click exit
      window_stack_pop(true);
    }
  }
  
  tricalc_clickcontext_reset(clickcontext);
  clickcontext->button = button;
  clickcontext->button_down = true;
}

static void click_config_provider(void* context) {
  /*Lots of bugs in click handling:
  * repeating click called n times after multi click done
  * undocumented delays
  * long click subscribe + repeating click subscribe doesn't work
  * repeating click fires on hold first time with repeating = false
  *
  * give up and use manual up/down.
  */
  /*XXX: repeat interval is overriden by long click*/
  //window_multi_click_subscribe(BUTTON_ID_SELECT, 1, 10, 300, false, select_click_handler);
  //window_single_repeating_click_subscribe(BUTTON_ID_SELECT, 500, select_repeating_click_handler);
  /*Work around lack of up listener for repeating click*/
  //window_long_click_subscribe(BUTTON_ID_SELECT, 700, select_long_click_down_handler, select_long_click_up_handler);
  window_raw_click_subscribe(BUTTON_ID_SELECT, click_down_handler, click_up_handler, NULL);
  window_raw_click_subscribe(BUTTON_ID_UP, click_down_handler, click_up_handler, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, click_down_handler, click_up_handler, NULL);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click_handler);
}

void handle_init(void) {
  window = window_create();
  
  window_set_click_config_provider_with_context(window, click_config_provider, (void*) &click_context);

  text_layer = text_layer_create(GRect(0, 144-30, MAIN_WIDTH, 30));
  text_layer_set_text_alignment(text_layer, GTextAlignmentRight);
  text_layer_set_font(text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text(text_layer, "0");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(text_layer));
  
  op_text_layer = text_layer_create(GRect(0, 144-40, 144, 20));
  text_layer_set_text(op_text_layer, "");
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(op_text_layer));
  window_stack_push(window, true);
}

void handle_deinit(void) {
  text_layer_destroy(text_layer);
  window_destroy(window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}


