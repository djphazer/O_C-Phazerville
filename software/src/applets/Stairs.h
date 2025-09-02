// Copyright (c) 2020, Logarhythm
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

// This Hemisphere applet is based on the Noise Engineering module "Clep Diaz."
// A number of steps (n) is set, and each clock input pulse causes the output to increase or decrease by 1/n of the max voltage
// depending on the direction setting of up, up/down, or down.
// If up/down mode is specified, the number of steps will be nearly doubled, but won't repeat the bottom and top steps (2n-2 steps.)
// Enabling "random" will cause each step value to deviate randomly by some % within the range of the prior and next step,
// keeping the direction of change but avoiding precise repetition.
// This version provides unipolar 0-5v output, divided into n steps (O&C hardware can only output -3v to +6v so this seemed sensible.)


#define HEM_STAIRS_MAX_STEPS 32
//#define HEM_STAIRS_GRAPH_SIZE 16
class Stairs : public HemisphereApplet {
public:

    enum StairsCursor {
        STEPS,
        DIRECTION,
        RANDOM
    };

    const char* applet_name() {
        return "Stairs";
    }
    const uint8_t* applet_icon() { return PhzIcons::stairs; }

    void Start() {
        steps = 1;
        dir = 0;
        rand = 0;
        cursor = 0;
        curr_step = 0;
        
        step_cv_lock = false;
        position_cv_lock = false;
        reset_gate = false;
        
        reverse = false;
        cv_out = 0;
        cv_rand = 0;

        /*
        graph_pos = 0;  // Current position on the graph
        for(int i=0; i<HEM_STAIRS_GRAPH_SIZE; ++i)
        {
          graph_points[i] = 0;
        }
        */
    }

    void Reset() {
        curr_step = (dir != 2) ? 0 : steps;  // Go to 0th or last step depending on direction
        reverse = (dir != 2) ? 0 : 1;  // Reset reverse (really just for up/down mode)
    }

    void Controller() {

        int curr_step_pv = curr_step;  // Detect if an input changes the step this update
        
        // CV input 0 (Step count)
        step_cv_lock = false;  // Track if cv is controlling the step count, for display
        if(DetentedIn(0) > 0)   // Is CV greater than 0v by a deadzone amount?
        {
          int num = ProportionCV(In(0), HEM_STAIRS_MAX_STEPS);  // Use this range so it's easy to reach max-1 just before 5v
          num = constrain(num, 0, HEM_STAIRS_MAX_STEPS-1);      // Constrain to max-1
          steps = num;  // Just overwrite user values
          step_cv_lock = true;  // Display this since it locks out user input
        }
  
        // CV input 1 (Position control)
        position_cv_lock = false;  // Track if position is under cv control
        if(DetentedIn(1) > 0)   // Is CV greater than 0v by a deadzone amount?
        {
          int num = ProportionCV(In(1), steps);
          num = constrain(num, 0, steps);
          curr_step = num;
          position_cv_lock = true;
        }


        // Digital Input 1: Reset pulse
        reset_gate = Gate(1);  // For display
        if (Clock(1)) {
            Reset();
            ClockOut(1);  // BOC pulse output
        }
        
        // Digital Input 2: Clock pulse
        if (Clock(0) && !reset_gate && !position_cv_lock)  // Don't clock if currently within a reset pulse, so overlapping clock+reset pulses go to step 0 instead of 1 and reset can "hold"
        {
            if(!reverse)
            {
              // Forward direction
              if(++curr_step > steps)
              {
                if(dir == 0)  // up
                {
                  curr_step = 0;
                  // In up mode, BOC should trigger when looping back to step 0
                  ClockOut(1);
                }
                else  // up/down
                {
                  reverse = true;
                  curr_step = (steps > 0 ? steps-1 : 0);  // Go to step before last unless too few steps
                }
              }

            }
            else
            {
              // Reverse direction
              if(curr_step > steps)
              {
                // Total steps have been manually changed to a number below the current position, so clamp
                curr_step = steps;
              }
              else 
              {
                --curr_step;

                // If in up/down mode, BOC should trigger when descending and arriving at step 0
                if(curr_step == 0 && dir == 1)
                {
                  ClockOut(1);
                }
                
                if(curr_step < 0)
                {
                  if(dir == 2) // down
                  {
                    curr_step = steps;
                    // In down mode, BOC puse should trigger when looping back to the end step
                    ClockOut(1);
                  }
                  else  // up/down
                  {
                    reverse = false;
                    curr_step = (steps > 0 ? 1 : 0);  // Go to 1 unless too few steps
                  }                  
                }                
              }
            }
        }


        // If the step has changed, update anything else that needs to
        // Note: Should BOC pulses be moved here to trigger via CV changing current step?
        if(curr_step != curr_step_pv && !position_cv_lock)
        {
          // Compute a new random offset if required
          if(rand)
          {
            cv_rand = Proportion(1, steps, HEMISPHERE_MAX_CV);  // 0-5v, scaled with fixed-point
            cv_rand = random(0, cv_rand/4);  // Deviate up to 1/x step amount
            // Randomly choose offset direction
            cv_rand *= (random(0,100) > 50) ? 1 : -1;
          }
        }

        // Steps will either be counting up or down, but it will always be an index into the cv range
        cv_out = Proportion(curr_step, steps, HEMISPHERE_MAX_CV);  // 0-5v, scaled with fixed-point
        if(rand && (curr_step != 0 && curr_step != steps))  // Don't randomize 1st and last steps so it always hits 0 and 5v?
        {
          cv_out += cv_rand;
          cv_out = constrain(cv_out, 0, HEMISPHERE_MAX_CV);  // (Not actually necessary if not randomizing start/end)        
        }

        /*
        // Record graph point
        if(curr_step != curr_step_pv)
        {
          if(++graph_pos >= HEM_STAIRS_GRAPH_SIZE)
          {
            graph_pos = 0;
          }

          // Record CV for this step as its plotted y coordinate
          graph_points[graph_pos] = ProportionCV(cv_out, 35);
        }
        */
        
        Out(0, cv_out);
    }

    void View() {
        DrawDisplay();
    }

    // void OnButtonPress() { }

    void OnEncoderMove(int direction) {
        if (!EditMode()) {
            MoveCursor(cursor, direction, 2);
            return;
        }

        switch ((StairsCursor)cursor) {
        case STEPS:
            steps = constrain( steps + direction, 0, HEM_STAIRS_MAX_STEPS-1);  // constrain includes max
            break;
        case DIRECTION:
            dir = constrain( dir + direction, 0, 2);

            // Don't change current direction if up/down mode
            if(dir != 1)
              reverse = (dir == 2);  // Change current trend to up or down if required

            break;
        case RANDOM:
            rand = 1-rand;
            break;
        }

    }
        
    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0, 5}, steps);
        Pack(data, PackLocation {5, 2}, dir);
        Pack(data, PackLocation {7, 1}, rand);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        steps = Unpack(data, PackLocation {0, 5});
        dir = Unpack(data, PackLocation {5, 2});
        rand = Unpack(data, PackLocation {7, 1});

        // Init from received data:
        reverse = (dir == 2);  // Set reverse if starting in down direction
        Reset();
    }

protected:
    void SetHelp() {
        //                    "-------" <-- Label size guide
        help[HELP_DIGITAL1] = "Clock";
        help[HELP_DIGITAL2] = "Reset";
        help[HELP_CV1]      = "Steps";
        help[HELP_CV2]      = "Pos";
        help[HELP_OUT1]     = "Step CV";
        help[HELP_OUT2]     = "BoC Trg";
        help[HELP_EXTRA1] = "";
        help[HELP_EXTRA2] = "";
       //                   "---------------------" <-- Extra text size guide
    }

private:
    int8_t steps;     // Number of steps, starting at 0v and ending at 5v (if > 0 steps)
    int8_t dir;       // 0 = up, 1 = up/down, 2 = down
    bool rand;        // 0 = no cv out randomization, 1 = random offsets are applied to each step
    int8_t curr_step; // Current step
    bool reverse;     // current movement direction
    int cv_out;       // CV currently being output (track for display)

    int cv_rand;            // track last computed random offset for cv
    bool step_cv_lock;      // 1 if cv is controlling the current step (show on display)
    bool position_cv_lock;  // 1 if cv is controlling the current step (show on display)
    bool reset_gate;        // Track if currently held in reset (show an icon)

    // Graphing
    //int8_t graph_pos;  // Current position on the graph
    //int8_t graph_points[HEM_STAIRS_GRAPH_SIZE];

    int cursor;     // StairsCursor

    void DrawDisplay()
    {
      // Show a stairs icon followed by steps value
      gfxBitmap(6, 15, 8, STAIRS_ICON); gfxPrint(16,15,steps+1);
      if(step_cv_lock)
      {
        gfxBitmap(16, 25, 8, CV_ICON);
      }

      // Direction selector (as an icon)
      gfxBitmap(34, 15, 8, (dir==0 ? UP_BTN_ICON : ( dir==1 ? UP_DOWN_ICON : DOWN_BTN_ICON )));

      // random wiggle on/off
      gfxBitmap(6, 35, 8, RANDOM_ICON);
      if(!rand)
      {
        gfxPrint(16,35, "off");
      }
      else
      {
        gfxPrint(16,35, "rnd");
        //gfxPos(16, 35); gfxPrintVoltage(cv_rand);  // Numeric readout for testing
      }

      // current/total steps
      gfxPrint(6+pad(100,curr_step+1), 55, curr_step+1); gfxPrint("/");gfxPrint(steps+1);  // Pad x enough to hold width steady
      if(reset_gate)
      {
        gfxBitmap(1, 55, 8, RESET_ICON);  // Indicate that Reset is holding the step
      }
      
      if(position_cv_lock) 
      {
        gfxBitmap(13, 45+3, 8, CV_ICON);  // Indicate that CV is holding the step
      }
      
      //gfxBitmap(1, 55, 8, CV_ICON); gfxPos(12, 55); gfxPrintVoltage(cv_out);  // Numeric readout for testing
      // Horiz. Level indicator
      //gfxInvert(9, 55, ProportionCV(cv_out, 54), 9);

      // Up/down indicator:
      int h = 1+ProportionCV(cv_out, 48);  // Always show 1   // was: 46
      //gfxInvert(52, 63-h, 9, h);
      gfxInvert(48, 63-h, 9, h);


      /*
      // Graph plot  -- Tested: This ends up looking too inconsistent given different step divisions over the smallish display resolution
      int8_t p = graph_pos;
      for(int i=0; i<HEM_STAIRS_GRAPH_SIZE; ++i)
      {
        int y = 63-graph_points[p];
        int x = 64-i*4;
        gfxLine(x, y, x-4, y);
        
        if(--p < 0)
        {
          p = HEM_STAIRS_GRAPH_SIZE-1;
        }
      }
      */
      
      // Cursor
      switch ((StairsCursor)cursor) {
      case STEPS:
        gfxCursor(16, 23, 15);  // flashing underline on the number
        break;
      case DIRECTION:
        gfxCursor(34, 23, 9);  // flashing underline on up/down icon
        break;
      case RANDOM:
        gfxCursor(16, 43, 20);  // flashing underline on the random setting
        break;
      }
    }
};
