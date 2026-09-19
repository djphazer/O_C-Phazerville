#include "OC_core.h"
#include "src/UI/ui_event_queue.h"
#include <malloc.h>

extern "C" char _heap_end[], *__brkval;

UI::EventQueue<IO_BUFFER_SIZE> task_queue;
size_t OC::CORE::queue_max = 0;

void OC::CORE::DeferTask(Task t) {
  task_queue.PushEvent(UI::EVENT_MISC, t, ticks & 0xffff, 0);
}
void OC::CORE::FlushTasks() {
  int i = 0; // yield after a certain number to prevent UI and gfx freeze
  while (task_queue.available() && i++ < 4000) {
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
  // always assume that one is still being processed
  return task_queue.available() + 1;
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
