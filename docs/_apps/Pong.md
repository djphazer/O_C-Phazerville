---
layout: default
---

# Pong 2.0

This is a faithful receation of Pong home consoles that works significantly better than the original Pong app.

## How To Play

There are 2 game modes which can be changed by enableing or disabling either player using the button:

1. Solitaire - scores when you hit the ball
2. Versus - scores when you hit your opponent's wall

## Digital Inputs

Each player can use digital inputs or analog inputs by pushing the encoder in.

The oiginal detented digital input has been preserved. It uses the left CV input for both players. (1 and 3 respectively)

- Positive voltages move the paddle up
- Negative voltages move the paddle down

Encoders can be rotated to move the paddles up and down as well. The speed of the paddles has been improved for better playability.

In addition, you can use the Gate inputs to move the paddle up and down with classic on/off CV

- left gate - moves the paddle up
- right gate - moves the paddle down

## Analog Inputs

The left CV input for both players is a non-detented input that can be used to move the paddle with an external analog CV signal. The range is bipolar and closely resembles the natural CV range of the Ornament and Crime outputs.

## Recreate The Unbeatable CPU

The original Pong app featured an unbeatable CPU. You can now mimic this behavior in one of two ways: The simplest method is to disable a player by pressing the button associated with the side the player is on.

- up - enable/disable player 1
- down - enable/disable player 2

The second method which recreates the moving paddle that tracks the ball is to simply self-patch O_c from output 3 which tracks the Y position of the ball and feed it into the analog input for either paddle. You can lower the CPU difficulty by feeding the signal through a slew limiter, a delay module, or whatever other silliness you can dream up.

## Outputs

1. impact with a paddle
2. impact with a wall
3. Y position of the ball
4. bleep bloop sound effects

## Exercises

1. Create a patch that can be played by this game
2. Create a patch that can play this game

Note: Chysn's original high score of 27 has been inserted as the default for 1-player vs the wall. See if you can beat it!

## Credits

Original App © 2018-2022 by Jason Justian and Beige Maze Laboratories. Wiki text by [Chysn](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Pong), under MIT License

Improvements 2026 by [Swamp Flux](https://github.com/swampflux)
