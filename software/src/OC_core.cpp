#include "OC_core.h"
#include "src/UI/ui_event_queue.h"
#include <malloc.h>

extern "C" char _heap_end[], *__brkval;

UI::EventQueue<64> task_queue;

void OC::CORE::DeferTask(Task t) {
  task_queue.PushEvent(UI::EVENT_MISC, t, ticks & 0xffff, 0);
}
void OC::CORE::FlushTasks() {
  while (task_queue.available()) {
    auto event = task_queue.PullEvent();
    switch (event.control) {
      case PROCESS_IOFRAME:
        Process(event.value);
        break;
      default: break;
    }
  }
}

int OC::CORE::FreeRam() {
#ifdef __IMXRT1062__
  auto mi = mallinfo();
  auto heap = _heap_end - __brkval;
  return heap + mi.fordblks;
#else
  char top;
  return &top - __brkval;
#endif
}
