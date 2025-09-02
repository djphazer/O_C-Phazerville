---
layout: default
---
# Pong

Like twenty years ago, I owned a Kurzweil K2000. It had what we call today--but didn't call back then--an Easter Egg. It was a Pong game that you could play from the panel, and it generated MIDI notes when the ball bounced off a wall.

This is the Pong we all know and love, with a few twists. As a ball bounces its way across the screen, the player defends the left side of the screen with a "paddle," and the module defends the right side. It's an unfair game, though, because the module can't lose. As you return the ball and level up, the ball gets faster, and your paddle gets smaller and closer to your opponent. The odds are not in your favor!

## Controls

* Up/Down Buttons: Move the paddle up and down. This is really to illustrate the use of the buttons' event handler, and you really don't want to play the game with these things.
* Encoders: Both encoders move the paddle up and down.
* CV Input 1: Negative values move the paddle up, and positive values move the paddle down. There's a "center detent," a small range that doesn't move the paddle at all. This is to compensate for noise that gets into the ADC.
* Output A: When the ball bounces off your paddle, a short 5V trigger is sent to Output A.
* Output B: When the ball bounces off anything else, a short 5V trigger is sent to Output B.
* Output C: Sends 0 to 4-ish volts, based on the Y position of the ball. 0V is the top of the screen.
* Output D: Sends 0 to 4-ish volts, based on the Y position of the player paddle. 0V is the top of the screen.

## Exercises

1. Create a patch that can be played by this game
2. Create a patch that can play this game

My patch's high score is 24, using Maths and Distro. Update 8/31/2018: 27 with just Maths!

## Credits
App © 2018-2022 by Jason Justian and Beige Maze Laboratories. Wiki text by [Chysn](https://github.com/Chysn/O_C-HemisphereSuite/wiki/Pong), under MIT License
