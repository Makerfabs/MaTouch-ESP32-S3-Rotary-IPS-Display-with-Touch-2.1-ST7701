#include <Arduino_GFX_Library.h>

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

int counter = 0;
int State;
int old_State;

int move_flag = 0;
int button_flag = 0;
int flesh_flag = 1;

void setup()
{
    USBSerial.begin(115200);
    pin_init();

    // Init Display
    gfx->begin();

    ledcSetup(0, 0, 8);
    ledcAttachPin(RX_PIN, 0);
    ledcWrite(0, 128);
}

void loop()
{

    if (digitalRead(BUTTON_PIN) == 0)
    {

        button_flag += 1;
        digitalWrite(TX_PIN, button_flag % 2);
        flesh_flag = 1;
        while (digitalRead(BUTTON_PIN) == 0)
            delay(100);

        USBSerial.println("Button Press");
    }

    if (move_flag == 1)
    {
        USBSerial.print("Position: ");
        USBSerial.println(counter);
        if (counter < 1)
            ledcWrite(0, 0);
        else
        {
            ledcWrite(0, 128);
            ledcSetup(0, (counter + 4) * 40, 8);
        }

        move_flag = 0;
        flesh_flag = 1;
    }
    if (flesh_flag == 1)
        page_1();

    delay(100);
}
void pin_init()
{
    pinMode(TFT_BL, OUTPUT);
    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT, INPUT_PULLUP);
    pinMode(RX_PIN, OUTPUT);
    pinMode(TX_PIN, OUTPUT);

    digitalWrite(TFT_BL, HIGH);
    old_State = digitalRead(ENCODER_CLK);

    attachInterrupt(ENCODER_CLK, encoder_irq, CHANGE);
}

void encoder_irq()
{
    State = digitalRead(ENCODER_CLK);
    if (State != old_State)
    {
        if (digitalRead(ENCODER_DT) == State)
        {
            counter++;
            if (counter > 20)
                counter = 20;
        }
        else
        {
            counter--;
            if (counter < 1)
                counter = 0;
        }
    }
    old_State = State; // the first position was changed
    move_flag = 1;
}

void page_1()
{
    String temp = "";
    gfx->fillScreen(WHITE);
    gfx->setTextSize(4);
    gfx->setTextColor(BLACK);
    gfx->setCursor(120, 100);
    gfx->println(F("Makerfabs"));

    gfx->setTextSize(3);
    gfx->setCursor(30, 160);
    gfx->println(F("2.1inch TFT with Touch "));

    gfx->setTextSize(4);
    gfx->setCursor(60, 240);
    temp = temp + "Speed: " + counter / 2;
    gfx->println(temp);

    gfx->setTextSize(4);
    gfx->setCursor(60, 280);
    temp = "";
    temp = temp + "Dir: ";
    if (button_flag % 2 == 1)
        temp = temp + " Right";
    else
        temp = temp + " Left";
    gfx->println(temp);

    flesh_flag = 0;
}