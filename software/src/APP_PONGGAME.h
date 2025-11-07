// (c) 2018, Jason Justian (chysn), Beize Maze
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// CV-controllable Pong game

#ifdef ENABLE_APP_PONG

#include <Arduino.h>
#include "OC_config.h"
#include "OC_apps.h"
#include "OC_ui.h"
#include "HSApplication.h"

/* Define the screen boundaries. There's a frame around the screen, so these numbers need to
 * take that into account.
 */
#define BOUNDARY_TOP 11
#define BOUNDARY_BOTTOM 61
#define BOUNDARY_RIGHT 116
#define BOUNDARY_LEFT 2

/* Define player properties. INITIAL_BALL_DELAY is how many ISR cycles the ball takes to move. It
 * gets faster as the game goes on. PADDLE_DELAY is how many ISR cycles the player must wait before moving
 * again. This is to keep the game interesting at higher levels. PADDLE_WIDTH is the chunkiness of the paddle,
 * in pixels.
 *
 * Note: Each ISR cycle is about 60 microseconds.
 */
#define INITIAL_BALL_DELAY 200
#define PADDLE_DELAY 200
#define PADDLE_WIDTH 3

/* TRIGGER_CYCLE_LENGTH specifies how many loop cycles a triggered event (like a hit) lasts. */
#define TRIGGER_CYCLE_LENGTH 400

/* This value is used for converting a ball's or paddle's Y position into a pitch value. This number was determined
 * experimentally, since I wasn't sure what the total range for pitch values is.
 */
#define Y_POSITION_COEFF 128

/*
 * When checking the ADC, if I just look for whether the value is positive or negative, then the value can't be
 * zeroed, and the player can't move the paddle with the onboard controls. This is probably because there's a little
 * noise that randomly swirls around 0. So this value simulates a center detent. This is another experimentally-
 * determined value.
 */
#define CENTER_DETENT 640

class Pong : public HSApplication {
public:
    /* There are two types of game state properties: Those that should be initialized only once (like high score), and
     * those that need to be initialized after each game. Init() sets the first kind, and then calls StartNewGame()
     * to start a new game.
     */
    void Start() {
        return_countdown = 0;
        bounce_countdown = 0;
        hi_score = 0;

        StartNewGame();
    }
    void Resume() { }

    void StartNewGame() {
        // Game state
        ball_delay = INITIAL_BALL_DELAY;
        ball_countdown = ball_delay;
        paddle_countdown = 0;
        score = 0;
        level_up_x_advance = 0; // Used to smoothly move the paddle forward after level-up

        // Ball properties
        ball_x = 64;
        ball_y = random((BOUNDARY_TOP + 2)*2, (BOUNDARY_BOTTOM - 2)*2); // Start off in a random spot
        dir_x = 1;
        dir_y = random(0, 100) > 50 ? 1 : -1; // Start off in a random direction

        // Player properties
        paddle_x = 8;
        paddle_y = 1 + BOUNDARY_TOP;
        paddle_h = 16;
    }

    /* I'm using the ISR for keeping track of object timing. The interrupt is timer-based, so each
     * timer cycle is about 60 microseconds.
     */
    void Controller() {
        ball_countdown--;
        paddle_countdown--;
        return_countdown--;
        bounce_countdown--;

        MoveBall();

        /* Handle input states:
         *
         * CV 1 is a paddle control. Negative values go up, positive values go down.
         */
        int move_cv = DetentedIn(0);

        // tweak for NLM to center the inputs around 5 octaves (6V on 1.2V/oct)
        if (NorthernLightModular && move_cv) move_cv -= HSAPPLICATION_5V;

        // check the detent again, just for NLM
        if (move_cv < -HEMISPHERE_CENTER_DETENT) MovePaddleUp();
        if (move_cv > HEMISPHERE_CENTER_DETENT) MovePaddleDown();

        /* Handle output states:
         *
         * Outputs are set as follows. Note that outs A and B are triggers, so it's just a small value transposed
         * five octaves up. The trigger time is handled by counting down from TRIGGER_CYCLE_LENGTH. There might be
         * a better way to do triggers, and I'll revisit this later.
         *
         * C and D outs are just scaled values. We have about a 60-step Y value, and I multiply that by Y_POSITION_COEFF
         * to get a pitch value to set. I tried to calibrate Y_POSITION_COEFF to get a CV range between 0 and 4-ish volts.
         */

        // Ball position CV (0 to 4-ish volts), based on the top of the ball
        uint32_t out_C = ((ball_y - BOUNDARY_TOP) * Y_POSITION_COEFF)/2;

        // Player paddle position CV (0 to 4-ish volts), based on the center of the paddle
        uint32_t out_D = ((paddle_y + (paddle_h / 2)) - BOUNDARY_TOP) * Y_POSITION_COEFF;

        Out(2, out_C);
        Out(3, out_D);
    }

    int get_score() {return score;}
    int get_hi_score() {return hi_score;}

    void MoveBall() {
        /* MoveBall() is called with each loop cycle. Moving the ball with each loop would make the
         * game unplayable, so movements are delayed with countdowns. ISR() is responsible for decrementing
         * counters.
         */
        if (ball_countdown <= 0) {
            // Move the ball based on the current direction
            ball_x += dir_x;
            ball_y += dir_y;

            // After a level-up, the paddle's x location is moved forward, one step at a time
            if (level_up_x_advance-- > 0) paddle_x++;

            // Check the playfield boundaries. Oh, yes, O_C will crash if you go too far out of bounds.
            if ((ball_x >> 1) > BOUNDARY_RIGHT || (ball_x>>1) < BOUNDARY_LEFT) {
                dir_x = -dir_x;
                //bounce_countdown = TRIGGER_CYCLE_LENGTH;
                ClockOut(1);
            }
            if ((ball_y>>1) > BOUNDARY_BOTTOM || (ball_y>>1) < BOUNDARY_TOP) {
                dir_y = -dir_y;
                //bounce_countdown = TRIGGER_CYCLE_LENGTH;
                ClockOut(1);
            }

            // All these conditions are just asking "Did the ball hit the paddle while traveling left?"
            if (((ball_x>>1) <= paddle_x + PADDLE_WIDTH) && ((ball_x>>1) >= paddle_x)
              && ((ball_y>>1) <= paddle_y + paddle_h) && ((ball_y>>1) >= paddle_y) && dir_x < 0) {
                // If so, bounce the ball, increase the score, and set the hit trigger to fire the reward
                // CV trigger at the next loop() call.
                dir_x = -dir_x;
                score++;
                //return_countdown = TRIGGER_CYCLE_LENGTH;
                ClockOut(0);

                // Level up!!
                if (!(score % 5)) LevelUp();
            }

            // Since the point of the game is to defend your left wall, this is when you lose :,(
            if (ball_x < 4) {
                if (score > hi_score) hi_score = score;
                StartNewGame();
            }

            ball_countdown = ball_delay; // Reset delay to start a new movement cycle
        }

        /* This is like the above, but for the paddle, and much simpler. The idea of the countdown is to constrain
         * the paddle from moving too fast, which is more of an issue when the paddle is under CV control. Surely
         * we don't want to make things too easy on your clever AI patches!
         */
        if (paddle_countdown <= 0) {paddle_countdown = 0;}
    }

    /* Performs the LevelUp. The game is designed to get brutal over time. The paddle gets smaller, the
     * ball gets faster, and the paddle gets closer to the wall. Fun times!
     */
    void LevelUp() {
        paddle_h--;
        ball_delay -= 25;
        if (paddle_x < 64) level_up_x_advance = 4;

        // Here are some points after which it doesn't get any harder
        if (paddle_h < 4) paddle_h = 4;
        if (ball_delay < 50) ball_delay = 50;
    }

    /*
     * The ball is just a little 2x2 square, with ball_x and ball_y describing the upper-left corner.
     */
    void DrawBall() {
      graphics.drawFrame(ball_x >> 1, ball_y >> 1, 2, 2);
    }

    /* The player paddle is a filled rectangle of fixed width and adjustable height. */
    void DrawPlayerPaddle() {
        graphics.drawRect(paddle_x, paddle_y, PADDLE_WIDTH, paddle_h);
    }

    /* I'm also drawing a paddle for Ornament and Crime, just as visual effect. It's really quite
     * an unfair farce, as it's totally unbeatable.
     */
    void DrawOCPaddle() {
        int oc_x = BOUNDARY_RIGHT + 2;
        int oc_y = (ball_y>>1) - 8;
        if (oc_y < BOUNDARY_TOP) oc_y = BOUNDARY_TOP;
        if (oc_y + 16 > BOUNDARY_BOTTOM) oc_y = BOUNDARY_BOTTOM - 16;
        graphics.drawRect(oc_x, oc_y, PADDLE_WIDTH, 16);
    }

    /* If the paddle countdown has elapsed, the paddle may move. Whenever the paddle is moved, the downdown begins again
     * to constrain the speed of the paddle. See the bottom of MoveBall() for more deets.
     */
    void MovePaddleUp() {
        if (paddle_countdown <= 0) {
            --paddle_y;
            if (paddle_y < BOUNDARY_TOP) paddle_y = BOUNDARY_TOP;
            paddle_countdown = PADDLE_DELAY;
        }
    }

    /* Like MovePaddleUp(), only more down */
    void MovePaddleDown() {
        if (paddle_countdown <= 0) {
            ++paddle_y;
            if (paddle_y > (BOUNDARY_BOTTOM - paddle_h)) paddle_y = BOUNDARY_BOTTOM - paddle_h;
            paddle_countdown = PADDLE_DELAY;
        }
    }

    /* Allows the paddle to be moved without an enforced delay, for use with encoder play */
    void ResetPaddle() {
        paddle_countdown = 0;
    }

    void SnapToBall() {
        paddle_y = (ball_y>>1) - (paddle_h/2);
    }

    void ToggleTwoPlayer() {
        twoplayermode ^= 1;
    }

    void View() {
        // Frame
        gfxFrame(0, 9, 128, 55);

        // Header
        gfxPos(1,1);
        int score = get_score();
        int hi_score = get_hi_score();
        if (score == 0 && hi_score > 0) {
            gfxPrint("Pong High : ");
            gfxPrint(hi_score);
        } else {
            gfxPrint("Pong Score: ");
            gfxPrint(score);
        }

        DrawGame();
    }

    void DrawGame() {
        // Game pieces
        DrawBall();
        DrawPlayerPaddle();
        DrawOCPaddle();
    }
private:
    int ball_delay; // The ball's delay at the next movement
    int ball_countdown; // Time (in increments of 60 microseconds) until the ball moves
    int paddle_countdown; // Time until the paddle may move
    int return_countdown; // Time until the return trigger (at Output A) ends
    int bounce_countdown; // Time until the bounce trigger (at Output B) ends
    int score; // The number of hits in this game
    int hi_score; // The highest number of hits in a game since initialization
    int level_up_x_advance;

    // the ball's coordinates have greater precision, half-pixels
    // bitshift right by 1 for actual pixel position
    int ball_x;
    int ball_y;
    int dir_x;
    int dir_y;

    int paddle_x;
    int paddle_y;
    int paddle_h;

    bool twoplayermode = false;
};

Pong pong_instance;

// App stubs
void PONGGAME_init() {
    pong_instance.BaseStart();
}

static constexpr size_t PONGGAME_storageSize() {
    return 0;
}

static size_t PONGGAME_save(void *storage) {
    return 0;
}

static size_t PONGGAME_restore(const void *storage) {
    return 0;
}

void PONGGAME_isr() {
    pong_instance.BaseController();
}

void PONGGAME_handleAppEvent(OC::AppEvent event) {
}

void PONGGAME_loop() {
}

void PONGGAME_menu() {
    pong_instance.BaseView();
}

void PONGGAME_screensaver() {
    // Game pieces only
    pong_instance.DrawBall();
    pong_instance.DrawPlayerPaddle();
    pong_instance.DrawOCPaddle();
}

/* Controlling the game with the buttons is the worst experience ever, so this is really just here
 * to demonstrate how the buttons work.
 */
void PONGGAME_handleButtonEvent(const UI::Event &event) {
    if (UI::EVENT_BUTTON_DOWN == event.type && OC::CONTROL_BUTTON_L == event.control) {
        pong_instance.SnapToBall();
    }
    if (UI::EVENT_BUTTON_PRESS == event.type) {
        switch (event.control) {
          case OC::CONTROL_BUTTON_UP:
                pong_instance.MovePaddleUp();
                pong_instance.ResetPaddle();
            break;

          case OC::CONTROL_BUTTON_DOWN:
                pong_instance.MovePaddleDown();
                pong_instance.ResetPaddle();
            break;

          case OC::CONTROL_BUTTON_R:
                pong_instance.ToggleTwoPlayer();
            break;
        }
    }
    if (UI::EVENT_BUTTON_LONG_PRESS == event.type && OC::CONTROL_BUTTON_L == event.control) {
      pong_instance.LevelUp();
    }
}

/* The UI::Event has a value property, which is positive when the encoder is turned clockwise and
 * negative when it's turned widdershins. I just wanted to say "widdershins."
 */
void PONGGAME_handleEncoderEvent(const UI::Event &event) {
    if (event.value < 0) pong_instance.MovePaddleUp();
    if (event.value > 0) pong_instance.MovePaddleDown();
    pong_instance.ResetPaddle();
}

#endif
