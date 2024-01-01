#define GOL_ABS(X) (X < 0 ? -X : X)

class GameOfLife : public HemisphereApplet {
public:

    const char* applet_name() {
        return "Game/Life";
    }

    void Start() {
        for (int i = 0; i < 80; i++) board[i] = 0;
        weight = 30;
        tx = 0;
        ty = 0;

        // Start off with a sweet-looking board
        for (int x = 0; x < 6; x++)
        {
            AddToBoard(x + 26, x + 23);
            AddToBoard(x + 33, (5 - x) + 23);
        }
        AddToBoard(32, 28);
    }

    void Controller() {
        tx = ProportionCV(In(0), 63);
        ty = ProportionCV(In(1), 39);

        if (Clock(0)) {
            ProcessGameBoard(tx, ty);
        }
        if (Gate(1)) AddToBoard(tx, ty);

        int global_density_cv = Proportion(global_density, 1200 - (weight * 10), HEMISPHERE_MAX_CV);
        int local_density_cv = Proportion(local_density, 225, HEMISPHERE_MAX_CV);
        Out(0, constrain(global_density_cv, 0, HEMISPHERE_MAX_CV));
        Out(1, constrain(local_density_cv, 0, HEMISPHERE_MAX_CV));
    }

    void View() {
        DrawBoard();
        DrawIndicator();
        DrawCrosshairs();
    }

    void ScreensaverView() {
        DrawBoard();
        DrawIndicator();
    }

    void OnButtonPress() {
        for (int i = 0; i < 80; i++) board[i] = 0;
    }

    void OnEncoderMove(int direction) {
        weight = constrain(weight += direction, 0, 100);
    }

    uint64_t OnDataRequest() {
        uint64_t data = 0;
        Pack(data, PackLocation {0,6}, weight);
        return data;
    }

    void OnDataReceive(uint64_t data) {
        weight = Unpack(data, PackLocation {0,6});
    }

protected:
    void SetHelp() {
        //                               "------------------" <-- Size Guide
        help[HEMISPHERE_HELP_DIGITALS] = "1=Clock 2=Draw";
        help[HEMISPHERE_HELP_CVS]      = "1=X pos 2=Y pos";
        help[HEMISPHERE_HELP_OUTS]     = "A=Global B=Local";
        help[HEMISPHERE_HELP_ENCODER]  = "T=Weight P=Clear";
        //                               "------------------" <-- Size Guide
    }

private:
    uint64_t board[80]; // 64x40 board
    int weight; // Weight of each cell
    int global_density; // Count of all live cells
    int local_density; // Count of cells in the vicinity of the Traveler
    int tx;
    int ty;

    void DrawBoard() {
        for (int y = 0; y < 40; y++)
        {
            for (int b = 0; b < 2; b++)
            {
                for (int c = 0; c < 32; c++)
                {
                    if ((board[(y * 2) + b] >> c) & 0x01) gfxPixel(c + (32 * b), y + 22);
                }
            }
        }
    }

    void DrawIndicator() {
        // Output indicators
        ForEachChannel(ch)
        {
            gfxRect(1, 15 + (ch * 4), ProportionCV(ViewOut(ch), 62), 2);
        }
    }

    void DrawCrosshairs() {
        gfxLine(tx, 23, tx, 63);
        gfxLine(0, ty + 22, 62, ty + 22);
    }

    void ProcessGameBoard(int tx, int ty) {
        uint64_t next_gen[80];
        global_density = 0;
        local_density = 0;
        for (int y = 0; y < 40; y++)
        {
            next_gen[y * 2] = 0; // Clear next generation board
            next_gen[y * 2 + 1] = 0;
            for (int x = 0; x < 64; x++)
            {
                bool live = ValueAtCell(x, y);
                int ln = CountLiveNeighborsAt(x, y);

                // 1: Any live cell with fewer than two live neighbours dies (referred to as underpopulation or exposure).
                // 2: Any live cell with more than three live neighbours dies (referred to as overpopulation or overcrowding).
                // Nothing to do to observe these rules, since we start with an empty board

                // 3: Any live cell with two or three live neighbours lives, unchanged, to the next generation.
                // 4: Any dead cell with exactly three live neighbours will come to life.
                if (((ln == 2 || ln == 3) && live) || (ln == 3 && !live)) {
                    int i = y * 2;
                    int xb = x;
                    if (x > 31) {
                        i += 1;
                        xb -= 32;
                    }
                    next_gen[i] = next_gen[i] | (0x01 << xb);
                    global_density++;

                    if (GOL_ABS(tx - x) < 8 && GOL_ABS(ty - y) < 8) local_density++;
                }
            }
        }

        memcpy(&board, &next_gen, sizeof(next_gen));
    }

    int CountLiveNeighborsAt(int x, int y) {
        int count = 0;
        for (int nx = -1; nx < 2; nx++)
        {
            for (int ny = -1; ny < 2; ny++)
            {
                if (!(nx == 0 && ny == 0)) count += ValueAtCell(x + nx, y + ny);
            }
         }
        return count;
    }

    bool ValueAtCell(int x, int y) {
        // Toroid operation
        if (x > 63) x -= 64;
        if (x < 0) x += 64;
        if (y > 39) y -= 40;
        if (y < 0) y += 40;

        int i = y * 2;
        if (x > 31) {
            i += 1;
            x -= 32;
        }
        return ((board[i] >> x) & 0x01);
    }

    void AddToBoard(int x, int y) {
        int i = y * 2;
        int xb = x;
        if (x > 31) {
            i += 1;
            xb -= 32;
        }
        board[i] = board[i] | (0x01 << xb);
    }
};
