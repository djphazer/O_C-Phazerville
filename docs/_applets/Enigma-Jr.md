---
layout: default
---
# Enigma, Jr.

![Screenshot 2024-06-13 14-47-58](https://github.com/djphazer/O_C-Phazerville/assets/109086194/3a64ad53-73ff-4fc0-9d10-4e4f345ffaa3)

While Enigma imposes a track-song workflow, **Enigma, Jr.** allows you to organize your Turing Machines via CV. It allows playback and temporary manipulation of registers in your Enigma Register Library.

In many respects, Enigma, Jr. is similar to ShiftReg. But instead of generating a random sixteen-bit register, Enigma, Jr. uses a Register from the Enigma Register Library. [**Enigma**](Enigma) may be used to shape and curate this Library.

### I/O

|        |         1/3         |    2/4     |
| ------ | :-----------------: | :--------: |
| TRIG   |        Clock        | No effect  |
| CV INs | Transpose (on step) |  Organize  |
| OUTs   |     Assignable      | Assignable |

### UI Parameters
* Register selection
* Probability
* Output Assignment (Channel A and Channel B)
	* **Note-3:** 3-bit depth, 8 notes of selected scale
	* **Note-4:** 4-bit depth, 16 notes of selected scale
	* **Note-5:** 5-bit depth, 32 notes of selected scale
	* **Note-6:** 6-bit depth, 64 notes of selected scale
	* **Note-7:** 7-bit depth, 128 notes of selected scale
	* **Modulation / Expression:** The least-significant 8 bits are used to generate a modulation value that's scaled to 0-5 volts. Modulation and Expression are identical for CV
	* **Gate:** When bit 0 is 1, the assigned output emits 5V until the next time bit 0 is 0. That is, a high gate will span multiple rotations of the Register
	* **Trigger**: When bit 0 is 1, a trigger pulse is emitted at the assigned output


## Organize

CV 2 is the Organize control. When a Register has advanced to the end of its length, CV 2 is read, and a new Register is loaded, based on the CV 2 modulation. If your Library has Registers marked as Favorites, the CV will choose from among your Favorites. If you have no Favorites selected, the CV will choose at random from among all 40 Registers in the Library.

Enigma, Jr. is in Hemisphere Suite from v1.5.
