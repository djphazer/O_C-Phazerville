// Copyright (c) 2022, Benjamin Rosenbach
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

#pragma once

#ifndef PROB_LOOP_LINKER_H_
#define PROB_LOOP_LINKER_H_

struct ProbLoopLinker {
    static ProbLoopLinker *instance;
    int loopStep;
    bool trig_q[4] = {0, 0, 0, 0};
    bool registered[2]; // { Div, Melo }
    bool isLooping;
    bool reseed;

    uint32_t last_advance_tick = 0; // To prevent double-advancing

    ProbLoopLinker() {
      isLooping = false;
      loopStep = 0;
      reseed = false;
      registered[0] = 0; // Div
      registered[1] = 0; // Melo
    }

    static ProbLoopLinker &get() {
        if (!instance) instance = new ProbLoopLinker;
        return *instance;
    }

    void RegisterDiv(HEM_SIDE hemisphere) {
      registered[0] = true;
    }
    void RegisterMelo(HEM_SIDE hemisphere) {
      registered[1] = true;
    }

    void UnloadDiv(HEM_SIDE hemisphere) {
      registered[0] = false;
      isLooping = false;
    }
    void UnloadMelo(HEM_SIDE hemisphere) {
      registered[1] = false;
    }

    bool IsLinked() {
      return (registered[0] && registered[1]);
    }

    void Trigger(int ch) {
        trig_q[ch] = true;
    }

    bool TrigPop(int ch) {
        if (IsLinked() && trig_q[ch]) {
            trig_q[ch] = false;
            return true;
        }
        return false;
    }

    void SetLooping(bool _isLooping) {
        isLooping = _isLooping;
    }

    bool IsLooping() {
        return isLooping;
    }

    void SetLoopStep(int _loopStep) {
        loopStep = _loopStep;
    }

    int GetLoopStep() {
        return loopStep;
    }

    void Reseed() {
        reseed = true;
    }

    bool ShouldReseed() {
        if (reseed) {
            reseed = false;
            return true;
        }
        return false;
    }

};

ProbLoopLinker *ProbLoopLinker::instance = 0;

#endif
