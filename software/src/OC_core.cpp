#include "OC_core.h"
#include "src/UI/ui_event_queue.h"
#include <malloc.h>

extern "C" char _heap_end[], *__brkval;

UI::EventQueue<64> task_queue;

void OC::CORE::DeferTask(Task t) {
  task_queue.PushEvent(UI::EVENT_MISC, t, ticks & 0xffff, 0);
}
void OC::CORE::FlushTasks() {
  int i = 0; // yield after a certain number to prevent UI and gfx freeze
  while (task_queue.available() && i++ < 16000) {
    const UI::Event event = task_queue.PullEvent();
    switch (event.control) {
      case PROCESS_IOFRAME:
        Process(event.value);
        break;
      default: break;
    }
  }
}
size_t OC::CORE::get_queue_size() {
  return task_queue.available();
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
