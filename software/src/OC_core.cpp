#include "OC_core.h"

extern char _heap_end[], *__brkval;

/*volatile*/ std::queue<Task> fn_queue;
//volatile bool fn_queue_lock = false;

void OC::CORE::DeferTask(Task func) {
  // This simply ignores Tasks from the ISR while flushing...
  // Hopefully that's more like debouncing or frame drops and not missed clocks...
  //if (!fn_queue_lock) // oh no!
  fn_queue.emplace(func);
}
void OC::CORE::FlushTasks() {
  if (fn_queue.empty()) return;
  //noInterrupts();
  //fn_queue_lock = true;
  while (!fn_queue.empty()) {
    fn_queue.front()();
    fn_queue.pop();
  }
  //interrupts();
  //fn_queue_lock = false;
}

int OC::CORE::FreeRam() {
#ifdef __IMXRT1062__
  auto heap = _heap_end - __brkval;
  return heap;
#else
  char top;
  return &top - __brkval;
#endif
}
