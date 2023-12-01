#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <ui.h>
#include <Adafruit_NeoPixel.h>

#include "touch.h"

/*Don't forget to set Sketchbook location in File/Preferencesto the path of your UI project (the parent foder of this INO file)*/
#define I2C_SDA_PIN 17
#define I2C_SCL_PIN 18
#define TOUCH_RST -1 // 38
#define TOUCH_IRQ -1 // 0

#define TFT_BL 38
#define BUTTON_PIN 14
#define ENCODER_CLK 13 // CLK
#define ENCODER_DT 10  // DT

#define RX_PIN 44
#define TX_PIN 43

#define WS2812_PIN RX_PIN
#define NUMPIXELS 50
#define DELAYVAL 50
/*Change to your screen resolution*/
static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 480;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * screenHeight / 10];

Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
    1 /* CS */, 46 /* SCK */, 0 /* SDA */,
    2 /* DE */, 42 /* VSYNC */, 3 /* HSYNC */, 45 /* PCLK */,
    11 /* R0 */, 15 /* R1 */, 12 /* R2 */, 16 /* R3 */, 21 /* R4 */,
    39 /* G0/P22 */, 7 /* G1/P23 */, 47 /* G2/P24 */, 8 /* G3/P25 */, 48 /* G4/P26 */, 9 /* G5 */,
    4 /* B0 */, 41 /* B1 */, 5 /* B2 */, 40 /* B3 */, 6 /* B4 */
);

// Uncomment for 2.1" round display
Arduino_ST7701_RGBPanel *gfx = new Arduino_ST7701_RGBPanel(
    bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */,
    false /* IPS */, 480 /* width */, 480 /* height */,
    st7701_type5_init_operations, sizeof(st7701_type5_init_operations),
    true /* BGR */,
    10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */);

Adafruit_NeoPixel pixels(NUMPIXELS, WS2812_PIN, NEO_GRB + NEO_KHZ800);

uint32_t my_color[8];

int counter = 0;
int State;
int old_State;
int move_flag = 0;

int flush_delay = 500;

void setup()
{
    USBSerial.begin(115200); /* prepare for possible serial debug */

    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    USBSerial.println(LVGL_Arduino);
    USBSerial.println("I am LVGL_Arduino");

    my_color[0] = pixels.Color(255, 0, 0);
    my_color[1] = pixels.Color(0, 255, 0);
    my_color[2] = pixels.Color(0, 0, 255);
    my_color[3] = pixels.Color(255, 255, 0);
    my_color[4] = pixels.Color(0, 255, 255);
    my_color[5] = pixels.Color(255, 0, 255);
    my_color[6] = pixels.Color(128, 0, 128);
    my_color[7] = pixels.Color(0, 128, 0);

    pin_init();
    gfx->begin();

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    /*Change the following line to your display resolution*/
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /*Initialize the (dummy) input device driver*/
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // encoder
    static lv_indev_drv_t indev_drv2;
    lv_indev_drv_init(&indev_drv2);
    indev_drv2.type = LV_INDEV_TYPE_ENCODER;
    indev_drv2.read_cb = my_encoder_read;
    lv_indev_t *encoder_indev = lv_indev_drv_register(&indev_drv2);

    ui_init();

    lv_group_t *g = lv_group_create();
    lv_group_add_obj(g, ui_Arc1);
    lv_indev_set_group(encoder_indev, g);

    USBSerial.println("Setup done");

    pixels.begin();
    pixels.setBrightness(60);
    pixels.clear();
    pixels.show();

    xTaskCreatePinnedToCore(Task_TFT, "Task_TFT", 20480, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(Task_main, "Task_main", 40960, NULL, 3, NULL, 1);
}

void loop()
{
}
//---------------------------------------------------
void Task_TFT(void *pvParameters)
{
    while (1)
    {
        lv_timer_handler();
        vTaskDelay(10);
    }
}

void Task_main(void *pvParameters)
{
    while (1)
    {
        String temp = "Bink Delay:";
        temp = temp + flush_delay;
        lv_label_set_text(ui_Label3, temp.c_str());

        int brightness = lv_arc_get_value(ui_Arc1) * 10;
        pixels.setBrightness(brightness);
        fresh_led();
        vTaskDelay(flush_delay);
    }
}

//--------------------------

void pin_init()
{
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);

    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT, INPUT_PULLUP);
    old_State = digitalRead(ENCODER_CLK);

    attachInterrupt(ENCODER_CLK, encoder_irq, CHANGE);

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
}

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

#if (LV_COLOR_16_SWAP != 0)
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif

    lv_disp_flush_ready(disp);
}

/*Read the touchpad*/
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    int touchX = 0, touchY = 0;

    if (read_touch(&touchX, &touchY) == 1)
    {
        data->state = LV_INDEV_STATE_PR;

        data->point.x = (uint16_t)touchX;
        data->point.y = (uint16_t)touchY;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }
}

void my_encoder_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    if (counter > 0)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = LV_KEY_RIGHT;
        counter--;
        move_flag = 1;
    }

    else if (counter < 0)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = LV_KEY_LEFT;
        counter++;
        move_flag = 1;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void encoder_irq()
{
    State = digitalRead(ENCODER_CLK);
    if (State != old_State)
    {
        if (digitalRead(ENCODER_DT) == State)
        {
            counter++;
        }
        else
        {
            counter--;
        }
    }
    old_State = State; // the first position was changed
}

void fresh_led()
{

    for (int i = 0; i < NUMPIXELS; i++)
    { // For each pixel...

        // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
        // Here we're using a moderately bright green color:
        // pixels.setPixelColor(i, pixels.Color(0, 150, 0));

        pixels.setPixelColor(i, my_color[random(0, 7)]);

        // vTaskDelay(DELAYVAL); // Pause before next pass through loop
    }

    pixels.show(); // Send the updated pixel colors to the hardware.
}
