/* Pong in an applet!
 * It's just a bouncing ball right now, no paddle or controls.
 *
 * Velocities are randomized on every bounce.
 * Beep/Boop square waves on first CV output.
 * Clock triggers on second CV output.
 *
 * Boundaries are different when viewed in DrawFullScreen...
 */

class Ponglet : public HemisphereApplet {
  public:
    static constexpr int PRECISION = 7; // extra bits for sub-pixel
    static constexpr int MAX_VEL = 1 << 4;
    static constexpr int MIN_VEL = 1 << 3;
    static constexpr int LEFT   = 1  << PRECISION;
    static constexpr int RIGHT  = 62 << PRECISION;
    static constexpr int TOP    = 12 << PRECISION;
    static constexpr int BOTTOM = 62 << PRECISION;

    static constexpr int FAR_RIGHT  = 126 << PRECISION;

    static constexpr int BEEP_PITCH = 800;
    static constexpr int BOOP_PITCH = 400;

    static constexpr int BEEP_LEN = 17 * 110;
    static constexpr int BOOP_LEN = 17 * 90;

    static constexpr int BEEP_TICKS = OC_CORE_ISR_FREQ / BEEP_PITCH;
    static constexpr int BOOP_TICKS = OC_CORE_ISR_FREQ / BOOP_PITCH;

    int right_bound, left_bound;

    int ball_x, ball_y;
    int vel_x = MIN_VEL;
    int vel_y = MIN_VEL;

    uint32_t ticker = 0;
    uint32_t beep_ticker = 0;
    uint32_t boop_ticker = 0;

    const char* applet_name() {
      return "Ponglet";
    }
    const uint8_t* applet_icon() { return ZAP_ICON; }

    void Start() {
      Reset();
    }
    // void Unload() { }

    void Reset() {
      ball_x = random( (RIGHT - LEFT) ) + LEFT;
      ball_y = random( (BOTTOM - TOP) ) + TOP;
    };

    void Beep() {
      const bool high = (OC::CORE::ticks % BEEP_TICKS) < (BEEP_TICKS / 2);
      const int amp = beep_ticker * ONE_OCTAVE * 3 / BEEP_LEN * (high?1:-1);
      Out(0, amp);
    }
    void Boop() {
      const bool high = (OC::CORE::ticks % BOOP_TICKS) < (BOOP_TICKS / 2);
      const int amp = boop_ticker * ONE_OCTAVE * 3 / BOOP_LEN * (high?1:-1);
      Out(0, amp);
    }

    void Controller() {

      if (beep_ticker) {
        Beep(); --beep_ticker;
      }
      if (boop_ticker) {
        Boop(); --boop_ticker;
      }

      // throttling to 1/32 cycle for ball update
      if (++ticker & 0x1f) return;

      ball_x += vel_x;
      ball_y += vel_y;

      if (ball_x > right_bound) {
        ball_x = right_bound;
        vel_x = -(random(MAX_VEL - MIN_VEL) + MIN_VEL);
        ClockOut(1);
        beep_ticker = BEEP_LEN;
      }
      if (ball_x < left_bound) {
        ball_x = left_bound;
        vel_x = (random(MAX_VEL - MIN_VEL) + MIN_VEL);
        ClockOut(1);
        beep_ticker = BEEP_LEN;
      }

      if (ball_y > BOTTOM) {
        ball_y = BOTTOM;
        vel_y = -(random(MAX_VEL - MIN_VEL) + MIN_VEL);
        ClockOut(1);
        boop_ticker = BOOP_LEN;
      }
      if (ball_y < TOP) {
        ball_y = TOP;
        vel_y = (random(MAX_VEL - MIN_VEL) + MIN_VEL);
        ClockOut(1);
        boop_ticker = BOOP_LEN;
      }
    }

    void View() {
      right_bound = RIGHT;
      left_bound = LEFT;
      gfxRect(ball_x >> PRECISION, ball_y >> PRECISION, 3, 3);
    }
    void DrawFullScreen() {
      right_bound = FAR_RIGHT;
      left_bound = LEFT;
      graphics.drawRect(ball_x >> PRECISION, ball_y >> PRECISION, 3, 3);
    }

    // void OnButtonPress() { CursorToggle(); };
    // void AuxButton() { CancelEdit(); }
    void OnEncoderMove(int direction) {
    }

    uint64_t OnDataRequest() {
      return 0;
    }

    void OnDataReceive(uint64_t data) {
    }

  protected:
    void SetHelp() {
      //                    "-------" <-- Label size guide
      help[HELP_DIGITAL1] = "";
      help[HELP_DIGITAL2] = "";
      help[HELP_CV1]      = "";
      help[HELP_CV2]      = "";
      help[HELP_OUT1]     = "Audio";
      help[HELP_OUT2]     = "";
      help[HELP_EXTRA1] = "Beeps and Boops";
      help[HELP_EXTRA2] = "(paddle coming soon)";
      //                  "---------------------" <-- Extra text size guide
    }
};
