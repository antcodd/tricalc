#include <pebble.h>
#include "tricalc.h"
#include "op_stack.h"

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

char* buttonvalues[NUM_BUTTONS] = {"    ", "123.", "456E", "7890"};
#define NUM_BUTTONVALUES 4

char* op_strings[OP_NUM_OPS] = {"+", "-", "*", "/", "ENTER"};

#define MAX_OPVALUES 4

/*TODO: better way?*/
typedef struct {
  tricalc_op ops[MAX_OPVALUES];
  size_t len;
} tricalc_opvalues;
tricalc_opvalues buttonvalues_op[NUM_BUTTONS] = {{{},0}, {{OP_PLUS, OP_MINUS}, 2}, {{OP_MULTIPLY, OP_DIVIDE}, 2}, {{OP_ENTER}, 1}};

char accumulator_string[100] = "\0";
uint32_t accumulator_string_offset = 0;
tricalc_op_stack stack;

static void button_timer_reset(struct tricalc_click_context* clickcontext, uint32_t timeout_ms);  

/*FIXME*/
struct tricalc_click_context click_context = {false, false, 0, NUM_BUTTONS, NULL};
                                      
tricalc_op get_op_for_button(struct tricalc_click_context* clickcontext) {
  int offset = (clickcontext->click_count - 1) % buttonvalues_op[clickcontext->button].len;
  
  return buttonvalues_op[clickcontext->button].ops[offset];
}

char get_value_for_button(struct tricalc_click_context* clickcontext) {
  int offset = (clickcontext->click_count - 1) % NUM_BUTTONVALUES;
  
  if (clickcontext->button < NUM_BUTTONS) {
    return buttonvalues[clickcontext->button][offset];
  }
  
  return '\0';
}

static void update_label(struct tricalc_click_context* clickcontext) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, clickcontext->long_click ? "l update" : "update" );
  if (!clickcontext->long_click) {
    if (accumulator_string_offset < (sizeof(accumulator_string)-1)) {
      accumulator_string[accumulator_string_offset] = get_value_for_button(clickcontext);
      accumulator_string[accumulator_string_offset+1] = '\0';
    }
    text_layer_set_text(text_layer, accumulator_string);
  } else {
    if (clickcontext->button < NUM_BUTTONS) {
      tricalc_op op = get_op_for_button(clickcontext);
      text_layer_set_text(op_text_layer, op_strings[op]);
    }
  }
}

static int perform_op(tricalc_op op, double a, double b,  double* result) {
  int arity = 2;
  
  if (result == NULL)
    return 0;
  
  switch (op) {
    case OP_PLUS:
      *result = b + a;
      break;
    case OP_MINUS:
      *result = b - a;
      break;
    case OP_MULTIPLY:
      *result = b * a;
      break;
    case OP_DIVIDE:
      *result = b / a;
      break;
    case OP_ENTER:
      /*XXX: this means current value is populated the next time, which may want to be configurable*/
      *result = a;
       arity = 1;
       break;
    default:
       return 0;
  }
  
  return arity;
}

static void action_commit(struct tricalc_click_context* clickcontext) {
  update_label(clickcontext);
  if (!clickcontext->long_click && accumulator_string_offset < (sizeof(accumulator_string)-1)) {
    accumulator_string_offset++;
    accumulator_string[accumulator_string_offset] = '\0';
  }
  if (clickcontext->long_click) {
    tricalc_op op = get_op_for_button(clickcontext);
    double a = (double) atoi(accumulator_string); //strtod(accumulator_string, NULL);
    double b = 0;
    double result = 0;
    int arity = 0;
    tricalc_op_stack_entry* item = tricalc_op_stack_peek(&stack);
    b = item ? item->val : a;
  APP_LOG(APP_LOG_LEVEL_DEBUG, "items %d", stack.items );
    
    arity = perform_op(op, a, b, &result);
    if (arity == 2) {
      tricalc_op_stack_pop_value(&stack, NULL);
    }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "items2 %d", stack.items );
    
    if (arity != 0) {
      if (op == OP_ENTER)
        tricalc_op_stack_push(&stack, result, op, a, b);
      /*XXX: Pebble snprintf doesn't support any float printing functions!*/
      /*TODO: Do this properly (ugh)*/
      snprintf(accumulator_string, sizeof(accumulator_string), "%d.%03d", (int) result, (int)((int64_t)(result*1000) % 1000));
      //reset accumulator so next input works correctly
      accumulator_string_offset = 0;
    }
  }
  APP_LOG(APP_LOG_LEVEL_DEBUG, "items3 %d", stack.items );
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
  tricalc_op_stack_init(&stack);
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


