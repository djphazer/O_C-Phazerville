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

#ifdef ENABLE_APP_PONG

#include "HSApplication.h"

class Pong : public HSApplication {
private:
  int ball_delay; // The ball's delay at the next movement
  int ball_countdown; // Time (in increments of 60 microseconds) until the ball moves
  int hi_score; // The highest number of hits in a game since initialization

  uint32_t bleep_ticker = 0;
  uint32_t bloop_ticker = 0;

  // the ball's coordinates have greater precision, half-pixels
  // bitshift right by PRECISION for actual pixel position
  int ball_x;
  int ball_y;
  int vel_x;
  int vel_y;

public:

  static constexpr int PRECISION = 7;

  // Length of audio bloops upon bounce
  static constexpr int BEEP_PITCH = 800;
  static constexpr int BOOP_PITCH = 400;
  static constexpr int BEEP_LEN = 17 * 110; // 17 ticks == 1ms
  static constexpr int BOOP_LEN = 17 * 90;
  static constexpr int BEEP_TICKS = OC_CORE_ISR_FREQ / BEEP_PITCH;
  static constexpr int BOOP_TICKS = OC_CORE_ISR_FREQ / BOOP_PITCH;

  void Bleep() {
    const bool high = (OC::CORE::ticks % BEEP_TICKS) < (BEEP_TICKS / 2);
    const int amp = bleep_ticker * ONE_OCTAVE * 3 / BEEP_LEN * (high ? 1 : -1)
      + (OC::DAC::kOctaveZero == 0) * HSAPPLICATION_5V;
    Out(3, amp);
  }
  void Bloop() {
    const bool high = (OC::CORE::ticks % BOOP_TICKS) < (BOOP_TICKS / 2);
    const int amp = bloop_ticker * ONE_OCTAVE * 3 / BOOP_LEN * (high ? 1 : -1)
      + (OC::DAC::kOctaveZero == 0) * HSAPPLICATION_5V;
    Out(3, amp);
  }

  enum playerModeOption { MODE_NONE, MODE_HUMAN, MODE_CPU };

  /* Define the screen boundaries. There's a frame around the screen, so these numbers need to
   * take that into account.
   */
  static constexpr int BOUNDARY_TOP = 11 << PRECISION;
  static constexpr int BOUNDARY_BOTTOM = 61 << PRECISION;
  static constexpr int BOUNDARY_RIGHT = 116 << PRECISION;
  static constexpr int BOUNDARY_LEFT = 2 << PRECISION;

  /* Define player properties. INITIAL_BALL_DELAY is how many ISR cycles the ball takes to move. It
   * gets faster as the game goes on. PADDLE_DELAY is how many ISR cycles the player must wait before moving
   * again. This is to keep the game interesting at higher levels. PADDLE_WIDTH is the chunkiness of the paddle,
   * in pixels.
   *
   * Note: Each ISR cycle is about 60 microseconds.
   */
  static constexpr int INITIAL_BALL_DELAY = 400;
  static constexpr int PADDLE_DELAY = 200;
  static constexpr int PADDLE_WIDTH = 3 << PRECISION;

  /* TRIGGER_CYCLE_LENGTH specifies how many loop cycles a triggered event (like a hit) lasts. */
  static constexpr int TRIGGER_CYCLE_LENGTH = 400;

  /* This value is used for converting a ball's or paddle's Y position into a pitch value. This number was determined
   * experimentally, since I wasn't sure what the total range for pitch values is.
   */
  static constexpr int Y_POSITION_COEFF = 128;

  struct Player {
      int score; // The number of hits in this game
      playerModeOption player_mode = MODE_HUMAN;
      int paddle_x = 8 << PRECISION;
      int paddle_y = BOUNDARY_TOP;
      int paddle_h = 16 << PRECISION;
      int movement_countdown = 0; // Used to limit the speed

      void SetPosition(int ypos) {
        paddle_y = constrain(ypos, BOUNDARY_TOP - (paddle_h/2), BOUNDARY_BOTTOM - (paddle_h/2));
      }
      void MovePaddleUp() {
          if (movement_countdown <= 0) {
              paddle_y -= (1 << PRECISION);
              if (paddle_y < BOUNDARY_TOP) paddle_y = BOUNDARY_TOP;
              movement_countdown = PADDLE_DELAY;
          }
      }

      /* Like MovePaddleUp(), only more down */
      void MovePaddleDown() {
          if (movement_countdown <= 0) {
              paddle_y += (1 << PRECISION);
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
        graphics.drawRect(
          paddle_x >> PRECISION,
          paddle_y >> PRECISION,
          PADDLE_WIDTH >> PRECISION,
          paddle_h >> PRECISION
        );
      }

      int getScore() {return score;}
  };

  Player player1;
  Player player2;

    /* There are two types of game state properties: Those that should be initialized only once (like high score), and
     * those that need to be initialized after each game. Init() sets the first kind, and then calls StartNewGame()
     * to start a new game.
     */
    void Start() {
        hi_score = 27;
        player1.player_mode = MODE_HUMAN;
        player2.player_mode = MODE_CPU;
        player2.paddle_x = 116 << PRECISION;

        StartNewGame();
    }
    void Resume() {
      ResetMappings();
    }

    void ServeBall() {
        // Game state
        ball_countdown = ball_delay;

        // Ball properties
        ball_x = 32 << PRECISION;
        ball_y = random((BOUNDARY_TOP + 2), (BOUNDARY_BOTTOM - 2)); // Start off in a random spot
        vel_x = 1 << PRECISION;
        vel_y = (random(0, 100) > 50 ? 1 : -1) << PRECISION; // Start off in a random direction
    }

    void StartNewGame() {
        ball_delay = INITIAL_BALL_DELAY;
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
        if(player1.movement_countdown) --player1.movement_countdown;
        if(player2.movement_countdown) --player2.movement_countdown;

        MoveBall();

        // handle direct CV inputs
        const int paddle_range = BOUNDARY_BOTTOM - BOUNDARY_TOP;
        const int Y_CENTER = BOUNDARY_TOP + paddle_range/2;

        player1.SetPosition(BOUNDARY_BOTTOM - (player1.paddle_h/2) - (In(0) >> 7) * paddle_range / 60);
        player2.SetPosition(BOUNDARY_BOTTOM - (player2.paddle_h/2) - (In(3) >> 7) * paddle_range / 60);

        // TODO:find out if i still need NLM / Buchla tweaks????
        // tweak for NLM to center the inputs around 5 octaves (6V on 1.2V/oct)
        //if (NorthernLightModular && p1_cv) p1_cv -= HSAPPLICATION_5V;
        // check the detent again, just for NLM
        // if (move_cv < -HEMISPHERE_CENTER_DETENT) MovePaddleUp();
        // if (move_cv > HEMISPHERE_CENTER_DETENT) MovePaddleDown();

        /*if(Gate(0) && !Gate(1)) player1.MovePaddleUp();*/
        /*if(Gate(1) && !Gate(0)) player1.MovePaddleDown();*/
        /*if(Gate(2) && !Gate(3)) player2.MovePaddleUp();*/
        /*if(Gate(3) && !Gate(2)) player2.MovePaddleDown();*/

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
        // Using 7 extra bits for PRECISION, each pixel == 1 semitone.
        // 60 pixels == 60 semitones == 5V max
        int out_C = (paddle_range - ball_y + BOUNDARY_TOP) * (60 << 7) / paddle_range;

        // Player paddle position CV (0 to 4-ish volts), based on the center of the paddle
        // uint32_t out_D = ((paddle_y + (paddle_h / 2)) - BOUNDARY_TOP) * Y_POSITION_COEFF;

        Out(2, out_C);
        // Out(3, out_D);

        // make bloop noises
        if (bleep_ticker) {
          Bleep(); --bleep_ticker;
        }
        if (bloop_ticker) {
          Bloop(); --bloop_ticker;
        }
    }

    void MoveBall() {
        /* MoveBall() is called with each loop cycle. Moving the ball with each loop would make the
         * game unplayable, so movements are delayed with countdowns. ISR() is responsible for decrementing
         * counters.
         */
        if (ball_countdown <= 0) {
            // Move the ball based on the current direction
            ball_x += vel_x;
            ball_y += vel_y;

            // Check the playfield boundaries. Oh, yes, O_C will crash if you go too far out of bounds.
            if (ball_y > BOUNDARY_BOTTOM) {
              vel_y = -abs(vel_y);
              ball_y = BOUNDARY_BOTTOM; // snap to edge
              ClockOut(1);
              bleep_ticker = BEEP_LEN;
            }
            if (ball_y < BOUNDARY_TOP) {
              vel_y = abs(vel_y);
              ball_y = BOUNDARY_TOP; // snap to edge
              ClockOut(1);
              bleep_ticker = BEEP_LEN;
            }

            // Check collision with Player 1
            if (vel_x < 0
                && (ball_x <= player1.paddle_x + PADDLE_WIDTH)
                && (ball_x >= player1.paddle_x)
                && (ball_y <= player1.paddle_y + player1.paddle_h)
                && (ball_y >= player1.paddle_y)) {

              // If so, bounce the ball, increase the score, and set the hit
              // trigger to fire the reward CV trigger at the next loop() call.
              vel_x = abs(vel_x);
              vel_y = (ball_y - (player1.paddle_y + player1.paddle_h / 2))
                / ((player1.paddle_h / 4) >> PRECISION);
              ball_x = player1.paddle_x + PADDLE_WIDTH; // snap to edge
              ClockOut(0);
              bloop_ticker = BOOP_LEN;
            }

            // Check collision with Player 2
            if (vel_x > 0
                && (ball_x <= player2.paddle_x + PADDLE_WIDTH)
                && (ball_x >= player2.paddle_x)
                && (ball_y <= player2.paddle_y + player2.paddle_h)
                && (ball_y >= player2.paddle_y)) {

              // If so, bounce the ball, increase the score, and set the hit
              // trigger to fire the reward CV trigger at the next loop() call.
              vel_x = -abs(vel_x);
              vel_y = (ball_y - (player2.paddle_y + player2.paddle_h / 2))
                / ((player2.paddle_h / 4) >> PRECISION);
              ball_x = player2.paddle_x; // snap to edge
              ClockOut(0);
              bloop_ticker = BOOP_LEN;
            }

            if (ball_x < BOUNDARY_LEFT) {
              player2.score++;
              // Level up!!
              if (!(player2.score % 5)) LevelUp();
              ServeBall();
            }
            if (ball_x > BOUNDARY_RIGHT) {
              player1.score++;
              // Level up!!
              if (!(player1.score % 5)) LevelUp();
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
        graphics.drawFrame(ball_x >> PRECISION, ball_y >> PRECISION, 2, 2);
    }

    /* If the paddle countdown has elapsed, the paddle may move. Whenever the paddle is moved, the downdown begins again
     * to constrain the speed of the paddle. See the bottom of MoveBall() for more deets.
     */

    void View() {
        // Frame
        gfxFrame(0, 9, 128, 55);

        // Net
        gfxDottedLine( 64, 10, 64, 63);

        // Header
        gfxPos(1,1);
        int p1_score = player1.getScore();
        int p2_score = player2.getScore();

        gfxPrint(0,0,p1_score); // left
        gfxPrint(46,0,"HI:"); // center
        gfxPrint(hi_score); // center
        gfxPrint(110,0,p2_score); // right

        DrawGame();
    }

    void DrawGame() {
        // Game pieces
        DrawBall();
        player1.DrawPaddle();
        player2.DrawPaddle();
    }
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
  if (event == OC::APP_EVENT_RESUME) pong_instance.Resume();
}

void PONGGAME_loop() {
}

void PONGGAME_menu() {
    pong_instance.BaseView();
}

void PONGGAME_screensaver() {
    // Game pieces only
    pong_instance.DrawBall();
    pong_instance.player1.DrawPaddle();
    pong_instance.player2.DrawPaddle();
}

/* Controlling the game with the buttons is the worst experience ever, so this is really just here
 * to demonstrate how the buttons work.
 */
void PONGGAME_handleButtonEvent(const UI::Event &event) {
    if (UI::EVENT_BUTTON_DOWN == event.type && OC::CONTROL_BUTTON_L == event.control) {
        // pong_instance.SnapToBall();
    }
    if (UI::EVENT_BUTTON_PRESS == event.type) {
        switch (event.control) {
          case OC::CONTROL_BUTTON_UP:
                // pong_instance.MovePaddleUp();
                // pong_instance.ResetPaddle();
            break;

          case OC::CONTROL_BUTTON_DOWN:
                // pong_instance.MovePaddleDown();
                // pong_instance.ResetPaddle();
            break;

          case OC::CONTROL_BUTTON_R:
                // pong_instance.ToggleTwoPlayer();
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
    if (OC::CONTROL_ENCODER_L == event.control) {
        if (event.value < 0) pong_instance.player1.MovePaddleUp();
        if (event.value > 0) pong_instance.player1.MovePaddleDown();
    }
    if (OC::CONTROL_ENCODER_R == event.control) {
        if (event.value < 0) pong_instance.player2.MovePaddleDown();
        if (event.value > 0) pong_instance.player2.MovePaddleUp();
    }
}

#endif
