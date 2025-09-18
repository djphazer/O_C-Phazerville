---
layout: default
---
# Game of Life

![Screenshot 2024-06-13 15-03-56](https://github.com/djphazer/O_C-Phazerville/assets/109086194/0dc71c7f-689b-4a1a-ad6c-222d600afbbf)

**Game of Life** has been restored in Phazerville Suite v1.7!

Original wiki text from Chysn follows:

## Conway's Game of Life
**_Note: Game of Life was an experimental applet, and was retired from Hemisphere Suite after v1.3. It had only tangential musical utility, and (more importantly) used a relatively large amount of the O_C's limited volatile memory, which will be of more value to upcoming projects._**

Game of Life is a modulation source based on John Conway's 1970 cellular automaton Game of Life.

Controls
* Digital Ins: Digital 1 is a clock that generates the next generation. Digital 2 is a gate that draws a point at the current Traveler location.
* CV Ins: Input 1 sets the X position of the Traveler, and Input 2 sets the Y position of the Traveler
* Outputs: A/C is the Global Density, and B/C is the Local Density
* Encoder Push: Clears the play field
* Encoder Turn: Increases or decreases the effect of cells on the CV output

With each clock, Game of Life evaluates each cell (or pixel) the play field based on four rules:

**Rule 1**: If a live cell has fewer than two live neighbors ("neighbor" here meaning an adjacent cell in one of the eight principal directions), it dies.
**Rule 2**: If a live cell has more than three live neighbors, it dies.
**Rule 3**: If a live cell has two or three live neighbors, it continues to live.
**Rule 4**: If a dead cell has exactly three live neighbors, it comes to life.

Additionally, cells can be written onto the screen with the "Traveler," a point on the screen described by an X and Y value, and controlled via CV. The CV inputs control the X value (with 0 volts being on the left side the display, and 5 volts being on the right) and Y value (with 0 volts being on the top and 5 volts being on the bottom). A gate to input 2 draws a new cell at the position of the Traveler.

There are two CV outputs. The first output is Global Density, which is a CV value proportional to the total number of live cells on the play field. The more populous your board is, the higher the Global Density value will be.

The second output is Local Density, which is a CV value proportional to the number of live cells in the vicinity of the Traveler. If you move the Traveler around, even with a static or slowly-changing play field, you'll get different CV values from the Local Density output.

### Credits
Copied/Adapted from [Conway's Game of Life](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Conways's-Game-of-Life-(Retired)) by Chysn
