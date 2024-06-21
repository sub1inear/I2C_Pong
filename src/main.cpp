#include <Arduboy2.h>
#include <Wire.h>

Arduboy2 arduboy;

// constants
// can be set to anything (unless reserved)
// must be 7 bits
constexpr int8_t TARGET_ADDRESS = 0x10;

constexpr uint8_t PADDLE_WIDTH = 4;
constexpr uint8_t PADDLE_HEIGHT = 16;

constexpr uint8_t BALL_WIDTH = 4;
constexpr uint8_t BALL_HEIGHT = 4;

constexpr uint8_t FONT_WIDTH = 5;
// offset from center for the score text
constexpr uint8_t SCORE_CENTER_OFFSET = 5;

// globals
enum Role : uint8_t {
    CONTROLLER,
    TARGET,
    NONE
};

Role role = NONE;

int8_t player_y[2] = {(HEIGHT - PADDLE_HEIGHT) / 2, (HEIGHT - PADDLE_HEIGHT) / 2};

int8_t player_score[2] = {0, 0};

bool player_a_button[2] = {false, false};

int8_t ball_x = PADDLE_WIDTH;
int8_t ball_y = player_y[0] + (PADDLE_HEIGHT - BALL_HEIGHT) / 2;

int8_t ball_dx = 0;
int8_t ball_dy = 0;

Role ball_start_side = CONTROLLER;

// must be volatile because it is changed in the callback
volatile bool handshake_completed = false;

// target callbacks
void data_receive(int bytes) { // callback for when controller sends a message
    while (Wire.available()) {
        // update globals
        player_y[CONTROLLER] = Wire.read();
        ball_x = Wire.read();
        ball_y = Wire.read();
        player_score[CONTROLLER] = Wire.read();
        player_score[TARGET]= Wire.read();
    }
}
void data_request() { // callback for when controller requests a message
    Wire.write(player_y[role]);
    Wire.write(arduboy.pressed(A_BUTTON));
}

void handshake_request() {
    Wire.write(true);
    handshake_completed = true;
}

// main functions

void setup() {
    // setup arduboy
    arduboy.begin();
    arduboy.setFrameRate(60);

    // init I2C
    power_twi_enable();
    arduboy.clear();
    
    arduboy.print("Pong V1.1\nWaiting for other\nplayer...");

    arduboy.display();

    // start auto handshake
    Wire.begin();
    Wire.requestFrom(TARGET_ADDRESS, 1);
    if (Wire.available()) { // already a target here
        role = CONTROLLER;
        handshake_completed = Wire.read();
    } else {
        role = TARGET;
        Wire.begin(TARGET_ADDRESS);
        Wire.onRequest(handshake_request);
    }

    while (!handshake_completed) {}
    
    // setup callbacks
    if (role == TARGET) {
        Wire.onRequest(data_request);
        Wire.onReceive(data_receive);
    }
}

void loop() {
    if (!arduboy.nextFrame()) {
        return;
    }
    arduboy.clear();
    arduboy.pollButtons();

    // update our y
    int8_t dy = (arduboy.pressed(DOWN_BUTTON) - arduboy.pressed(UP_BUTTON));
    player_y[role] += dy;
    
    // fence y
    if (player_y[role] < 0) {
        player_y[role] = 0;
    }
    if (player_y[role] > HEIGHT - PADDLE_HEIGHT) {
        player_y[role] = HEIGHT - PADDLE_HEIGHT;
    }

    if (role == CONTROLLER) {
        // get data
        Wire.requestFrom(TARGET_ADDRESS, 2);
        while (Wire.available()) {
            player_y[TARGET] = Wire.read();
            player_a_button[TARGET] = Wire.read();
        }
        // update player_a_button array
        player_a_button[CONTROLLER] = arduboy.pressed(A_BUTTON);

        // move ball if on paddle
        if (ball_dx == 0 && ball_dy == 0) {
            ball_y = player_y[ball_start_side] + (PADDLE_HEIGHT - BALL_HEIGHT) / 2;
            if (player_a_button[ball_start_side]) {
                if (ball_start_side == CONTROLLER) {
                    ball_dx = 1;
                } else {
                    ball_dx = -1;
                }
                ball_dy = 1;

            }
        }
        // update ball
        ball_x += ball_dx;
        ball_y += ball_dy;
        
        // check if ball goes off left or right side of the screen 
        if (ball_x < 0 || ball_x > WIDTH - BALL_WIDTH) {
            // increment score of other side and set ball x to loser's position
            if (ball_dx > 0) {
                player_score[CONTROLLER]++;
                ball_start_side = TARGET;
                ball_x = WIDTH - BALL_WIDTH - PADDLE_WIDTH;
            } else {
                player_score[TARGET]++;
                ball_start_side = CONTROLLER;
                ball_x = PADDLE_WIDTH;
            }
            // reset ball
            ball_dx = 0;
            ball_dy = 0;
            ball_y = player_y[ball_start_side] + (PADDLE_HEIGHT - BALL_HEIGHT) / 2;
        
        }
        // make ball bounce with paddle
        if (arduboy.collide(Rect(0, player_y[CONTROLLER], PADDLE_WIDTH, PADDLE_HEIGHT),
                            Rect(ball_x, ball_y, BALL_WIDTH, BALL_HEIGHT)) ||
            arduboy.collide(Rect(WIDTH - PADDLE_WIDTH, player_y[TARGET], PADDLE_WIDTH, PADDLE_HEIGHT),
                            Rect(ball_x, ball_y, BALL_WIDTH, BALL_HEIGHT))) {
            // set to edge of paddle to avoid ball going inside
            if (ball_dx > 0) {
                ball_x = WIDTH - BALL_WIDTH - PADDLE_WIDTH;
            } else {
                ball_x = PADDLE_WIDTH;
            }
            ball_dx = -ball_dx;
            
        }
        // make ball bounce off of top and bottom parts of screen
        if (ball_y < 0 || ball_y > HEIGHT - BALL_HEIGHT) {
            ball_dy = -ball_dy;
        }
        // send data over I2c
        Wire.beginTransmission(TARGET_ADDRESS);
        Wire.write(player_y[CONTROLLER]);
        Wire.write(ball_x);
        Wire.write(ball_y);
        Wire.write(player_score[CONTROLLER]);
        Wire.write(player_score[TARGET]);
        Wire.endTransmission();
    }

    // draw
    arduboy.fillRect(0, player_y[CONTROLLER], PADDLE_WIDTH, PADDLE_HEIGHT);
    arduboy.fillRect(WIDTH - PADDLE_WIDTH, player_y[TARGET], PADDLE_WIDTH, PADDLE_HEIGHT);
        
    arduboy.fillRect(ball_x, ball_y, BALL_WIDTH, BALL_HEIGHT);

    arduboy.setCursor(WIDTH / 2 - FONT_WIDTH - SCORE_CENTER_OFFSET, 0);
    arduboy.print(player_score[CONTROLLER]);
    arduboy.setCursor(WIDTH / 2 + SCORE_CENTER_OFFSET, 0);
    arduboy.print(player_score[TARGET]);

    arduboy.display();
}