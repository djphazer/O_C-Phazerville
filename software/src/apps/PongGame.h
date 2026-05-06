/** 
* APP_PONGGAME.h - CV Controllable Pong game for Ornament and Crime
*
* (c) 2018, Jason Justian (chysn), Beize Maze, Swamp Flux
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "HSApplication.h"

// Length of audio bloops upon bounce
#define BLOOP_LENGTH 1000 // 17 * 100ms
#define BLOOP_LOW 0x8
#define BLOOP_HIGH 0x4

enum playerModeOption {
    NONE,
    HUMAN,
    CPU
};

struct Player {
    int score; // The number of hits in this game
    int y_position;
    playerModeOption player_mode = HUMAN;
    int paddle_x = 8;
    int paddle_y = 1 + BOUNDARY_TOP;
    int paddle_h = 16;
    int movement_countdown = 0; // Used to limit the speed

    void MovePaddleUp() {
        if (movement_countdown <= 0) {
            --paddle_y;
            if (paddle_y < BOUNDARY_TOP) paddle_y = BOUNDARY_TOP;
            movement_countdown = PADDLE_DELAY;
        }
    }

    /* Like MovePaddleUp(), only more down */
    void MovePaddleDown() {
        if (movement_countdown <= 0) {
            ++paddle_y;
            if (paddle_y > (BOUNDARY_BOTTOM - paddle_h)) paddle_y = BOUNDARY_BOTTOM - paddle_h;
            movement_countdown = PADDLE_DELAY;
        }
    }

    /* Allows the paddle to be moved without an enforced delay, for use with encoder play */
    void ResetPaddle() {
        movement_countdown = 0;
    }

        /* The player paddle is a filled rectangle of fixed width and adjustable height. */
    void DrawPaddle() {
        graphics.drawRect(paddle_x, paddle_y, PADDLE_WIDTH, paddle_h);
    }

    int getScore() {return score;}
};

OC_APP_TRAITS(AppPong, TWOCCS("PO"), "Pong", "Pong");
class OC_APP_CLASS(AppPong), public HSApplication {
private:
    int ball_delay; // The ball's delay at the next movement
    int ball_countdown; // Time (in increments of 60 microseconds) until the ball moves
    int bloop_countdown;
    int bloop_pitch;
    int hi_score; // The highest number of hits in a game since initialization

    // the ball's coordinates have greater precision, half-pixels
    // bitshift right by 1 for actual pixel position
    int ball_x;
    int ball_y;
    int dir_x;
    int dir_y;

public:
  OC_APP_INTERFACE_DECLARE(AppPong);
  OC_APP_STORAGE_SIZE(0);

  /* Define the screen boundaries. There's a frame around the screen, so these numbers need to
   * take that into account.
   */
  static constexpr int BOUNDARY_TOP = 11;
  static constexpr int BOUNDARY_BOTTOM = 61;
  static constexpr int BOUNDARY_RIGHT = 116;
  static constexpr int BOUNDARY_LEFT = 2;
  static constexpr int Y_CENTER = 38;

  /* Define player properties. INITIAL_BALL_DELAY is how many ISR cycles the ball takes to move. It
   * gets faster as the game goes on. PADDLE_DELAY is how many ISR cycles the player must wait before moving
   * again. This is to keep the game interesting at higher levels. PADDLE_WIDTH is the chunkiness of the paddle,
   * in pixels.
   *
   * Note: Each ISR cycle is about 60 microseconds.
   */
  static constexpr int INITIAL_BALL_DELAY = 200;
  static constexpr int PADDLE_DELAY = 200;
  static constexpr int PADDLE_WIDTH = 3;

  /* TRIGGER_CYCLE_LENGTH specifies how many loop cycles a triggered event (like a hit) lasts. */
  static constexpr int TRIGGER_CYCLE_LENGTH = 400;

  /* This value is used for converting a ball's or paddle's Y position into a pitch value. This number was determined
   * experimentally, since I wasn't sure what the total range for pitch values is.
   */
  static constexpr int Y_POSITION_COEFF = 128;
  static constexpr int PRECISION = 1;

    Player player1;
    Player player2;

    /* There are two types of game state properties: Those that should be initialized only once (like high score), and
     * those that need to be initialized after each game. Init() sets the first kind, and then calls StartNewGame()
     * to start a new game.
     */
    void Start() {
        bloop_countdown = 0;
        hi_score = 27;
        player1.player_mode = HUMAN;
        player2.player_mode = CPU;
        player2.paddle_x = 116;

        StartNewGame();
    }

    void Resume() { }

    void ServeBall() {
        // Game state
        ball_delay = INITIAL_BALL_DELAY;
        ball_countdown = ball_delay;

        // Ball properties
        ball_x = 64;
        ball_y = random((BOUNDARY_TOP + 2)*2, (BOUNDARY_BOTTOM - 2)*2); // Start off in a random spot
        dir_x = 1;
        dir_y = random(0, 100) > 50 ? 1 : -1; // Start off in a random direction
    }

    void StartNewGame() {
        player1.score = 0;
        player2.score = 0;
        player1.movement_countdown = 0;
        player2.movement_countdown = 0;

        ServeBall();
    }

    /* I'm using the ISR for keeping track of object timing. The interrupt is timer-based, so each
     * timer cycle is about 60 microseconds.
     */
    void Controller() {
        ball_countdown--;
        bloop_countdown--;
        if(player1.movement_countdown) --player1.movement_countdown;
        if(player2.movement_countdown) --player2.movement_countdown;

        MoveBall();

        // handle direct CV inputs
        // TODO: eliminate paddle_h
        int paddle_range = BOUNDARY_BOTTOM - BOUNDARY_TOP - player1.paddle_h;

        player1.paddle_y = constrain(
            Y_CENTER - (In(0) >> 7) * paddle_range / 64,
            BOUNDARY_TOP,
            BOUNDARY_BOTTOM - player1.paddle_h
        );
        player2.paddle_y = constrain(
            Y_CENTER - (In(3) >> 7) * paddle_range / 64,
            BOUNDARY_TOP,
            BOUNDARY_BOTTOM - player2.paddle_h
        );

        // TODO:find out if i still need NLM / Buchla tweaks????
        // tweak for NLM to center the inputs around 5 octaves (6V on 1.2V/oct)
        //if (NorthernLightModular && p1_cv) p1_cv -= HSAPPLICATION_5V;
        // check the detent again, just for NLM
        // if (move_cv < -HEMISPHERE_CENTER_DETENT) MovePaddleUp();
        // if (move_cv > HEMISPHERE_CENTER_DETENT) MovePaddleDown();

        if(Gate(0) && !Gate(1)) player1.MovePaddleUp();
        if(Gate(1) && !Gate(0)) player1.MovePaddleDown();
        if(Gate(2) && !Gate(3)) player2.MovePaddleUp();
        if(Gate(3) && !Gate(2)) player2.MovePaddleDown();

        /* Handle output states:
         *
         * Outputs are set as follows. Note that outs A and B are triggers, so it's just a small value transposed
         * five octaves up. The trigger time is handled by counting down from BLOOP_LENGTH. There might be
         * a better way to do triggers, and I'll revisit this later.
         *
         * C and D outs are just scaled values. We have about a 60-step Y value, and I multiply that by Y_POSITION_COEFF
         * to get a pitch value to set. I tried to calibrate Y_POSITION_COEFF to get a CV range between 0 and 4-ish volts.
         */

        // Ball position CV (0 to 4-ish volts), based on the top of the ball
        uint32_t out_C = (((BOUNDARY_BOTTOM << PRECISION) - ball_y) * Y_POSITION_COEFF)/2;

        // Player paddle position CV (0 to 4-ish volts), based on the center of the paddle
        // uint32_t out_D = ((paddle_y + (paddle_h / 2)) - BOUNDARY_TOP) * Y_POSITION_COEFF;

        Out(2, out_C);
        // Out(3, out_D);

        // make bloop noises
        if(bloop_countdown > 0){
            GateOut(3, OC::CORE::ticks & bloop_pitch);
        }
    }

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

            // Check the playfield boundaries. Oh, yes, O_C will crash if you go too far out of bounds.
            if ((ball_y>>1) > BOUNDARY_BOTTOM || (ball_y>>1) < BOUNDARY_TOP) {
                dir_y = -dir_y;
                bloop_countdown = BLOOP_LENGTH;
                bloop_pitch = BLOOP_HIGH;
                ClockOut(1);
            }

            // Check collision with Player 1
            if (dir_x < 0 &&
                ((ball_x>>1) <= player1.paddle_x + PADDLE_WIDTH) && ((ball_x>>1) >= player1.paddle_x) &&
                ((ball_y>>1) <= player1.paddle_y + player1.paddle_h) && ((ball_y>>1) >= player1.paddle_y)) {
                
                // If so, bounce the ball, increase the score, and set the hit trigger to fire the reward
                // CV trigger at the next loop() call.
                dir_x = -dir_x;
                bloop_countdown = BLOOP_LENGTH;
                bloop_pitch = BLOOP_LOW;
                ClockOut(0);

                // Level up!!
                if (!(player1.score % 5)) LevelUp();
            }

            // Check collision with Player 2
            if (dir_x > 0 &&
                ((ball_x>>1) <= player2.paddle_x + PADDLE_WIDTH) && ((ball_x>>1) >= player2.paddle_x) &&
                ((ball_y>>1) <= player2.paddle_y + player2.paddle_h) && ((ball_y>>1) >= player2.paddle_y)) {

                // If so, bounce the ball, increase the score, and set the hit trigger to fire the reward
                // CV trigger at the next loop() call.
                dir_x = -dir_x;
                bloop_countdown = BLOOP_LENGTH;
                bloop_pitch = BLOOP_LOW;
                ClockOut(0);

                // Level up!!
                if (!(player2.score % 5)) LevelUp();
            }

            if (ball_x < (BOUNDARY_LEFT)<<1) {
                player2.score++;
                ServeBall();
            }
            if( ball_x > (BOUNDARY_RIGHT)<<1) {
                player1.score++;
                ServeBall();
            }

            if (player1.score > hi_score) {
                hi_score = player1.score;
                StartNewGame();
            }
            if (player2.score > hi_score) {
                hi_score = player2.score;
                StartNewGame();
            }

            ball_countdown = ball_delay; // Reset delay to start a new movement cycle
        }
    }

    /* Performs the LevelUp. The game is designed to get brutal over time. The paddle gets smaller, the
     * ball gets faster, and the paddle gets closer to the wall. Fun times!
     */
    void LevelUp() {
        // paddle_h--;
        ball_delay -= 25;
        // if (paddle_x < 64) level_up_x_advance = 4;

        // Here are some points after which it doesn't get any harder
        // if (paddle_h < 4) paddle_h = 4;
        if (ball_delay < 50) ball_delay = 50;
    }

    /*
     * The ball is just a little 2x2 square, with ball_x and ball_y describing the upper-left corner.
     */
    void DrawBall() const {
        graphics.drawFrame(ball_x >> 1, ball_y >> 1, 2, 2);
    }

    /* If the paddle countdown has elapsed, the paddle may move. Whenever the paddle is moved, the downdown begins again
     * to constrain the speed of the paddle. See the bottom of MoveBall() for more deets.
     */

    void View() const {
        // Frame
        gfxFrame(0, 9, 128, 55);

        // Net
        gfxDottedLine( 64, 10, 64, 63);

        // Header
        gfxPos(1,1);
        int p1_score = player1.getScore();
        int p2_score = player2.getScore();
        int hi_score = get_hi_score();

        gfxPrint(0,0,p1_score); // left
        gfxPrint(46,0,"HI:"); // center
        gfxPrint(hi_score); // center
        gfxPrint(110,0,p2_score); // right

        DrawGame();
    }

    void DrawGame() const {
        // Game pieces
        DrawBall();
        player1.DrawPaddle();
        player2.DrawPaddle();
    }
};

void AppPong::Init() {
  BaseStart();
}

size_t AppPong::SaveAppData(util::StreamBufferWriter &) const { return 0; }
size_t AppPong::RestoreAppData(util::StreamBufferReader &) { return 0; }

void AppPong::Process(OC::IOFrame *ioframe) {
  BaseController(ioframe);
}

void AppPong::HandleAppEvent(OC::AppEvent event) {
}

void AppPong::Loop() {
}

void AppPong::GetIOConfig(OC::IOConfig &ioconfig) const
{
  ioconfig.outputs[0].set("Paddle Trig", OC::OUTPUT_MODE_UNI);
  ioconfig.outputs[1].set("Wall Trig", OC::OUTPUT_MODE_UNI);
  ioconfig.outputs[2].set("Ball Y", OC::OUTPUT_MODE_UNI);
  ioconfig.outputs[3].set("Paddle Y", OC::OUTPUT_MODE_UNI);
}
void AppPong::DrawDebugInfo() const { }

void AppPong::DrawMenu() const {
  BaseView();
}

void AppPong::DrawScreensaver() const {
  // Game pieces only
  DrawGame();
}

/* Controlling the game with the buttons is the worst experience ever, so this is really just here
 * to demonstrate how the buttons work.
 */
void AppPong::HandleButtonEvent(const UI::Event &event) {
    if (UI::EVENT_BUTTON_DOWN == event.type && OC::CONTROL_BUTTON_L == event.control) {
        // a little cheat
        //SnapToBall();
    }
    if (UI::EVENT_BUTTON_PRESS == event.type) {
        switch (event.control) {
          case OC::CONTROL_BUTTON_UP:
            //MovePaddleUp();
            //ResetPaddle();
            break;

          case OC::CONTROL_BUTTON_DOWN:
            //MovePaddleDown();
            //ResetPaddle();
            break;

          case OC::CONTROL_BUTTON_R:
            //ToggleTwoPlayer();
            break;
        }
    }
    if (UI::EVENT_BUTTON_LONG_PRESS == event.type && OC::CONTROL_BUTTON_L == event.control) {
      LevelUp();
    }
}

/* The UI::Event has a value property, which is positive when the encoder is turned clockwise and
 * negative when it's turned widdershins. I just wanted to say "widdershins."
 */
void AppPong::HandleEncoderEvent(const UI::Event &event) {
    if (OC::CONTROL_ENCODER_L == event.control) {
        if (event.value < 0) player1.MovePaddleUp();
        if (event.value > 0) player1.MovePaddleDown();
    }
    if (OC::CONTROL_ENCODER_R == event.control) {
        if (event.value < 0) player2.MovePaddleDown();
        if (event.value > 0) player2.MovePaddleUp();
    }
}
