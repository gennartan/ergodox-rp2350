"""Exercise the real delayed-work handler with HID transport failures."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class MacroWorkerTest(unittest.TestCase):
    def test_delivery_retry_delay_and_disconnect(self):
        source = (ROOT / 'src/main.c').read_text()
        handler = source[source.index('static void macro_work_handler('):
                         source.index('static void process_key_event(')]
        harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define ARG_UNUSED(x) (void)(x)
#define K_MSEC(x) (x)
struct k_work { int unused; };
struct keyboard_runtime { int macro_phase; };
static struct keyboard_runtime keyboard = {1};
static int runtime_lock, macro_work;
typedef int k_spinlock_key_t;
static int k_spin_lock(int *lock) { (void)lock; return 0; }
static void k_spin_unlock(int *lock, int key) { (void)lock; (void)key; }
static bool keyboard_hid_ready = true, macro_report_pending = true;
static int advances, attempts, failures, scheduled, delay, last_phase;
static void keyboard_runtime_macro_cancel(struct keyboard_runtime *runtime) { runtime->macro_phase=0; }
static void keyboard_runtime_macro_advance(struct keyboard_runtime *runtime) {
 ++advances; runtime->macro_phase=runtime->macro_phase==3 ? 0 : runtime->macro_phase+1;
}
static uint32_t keyboard_runtime_macro_delay(const struct keyboard_runtime *runtime) {
 return runtime->macro_phase==3 ? 125 : 10;
}
static int submit_hid_report_from_runtime(void) {
 ++attempts; last_phase=keyboard.macro_phase;
 if(failures) { --failures; return -1; } return 0;
}
static void k_work_reschedule(int *work, int ms) { (void)work; ++scheduled; delay=ms; }
'''
        cases = r'''
int main(void) {
 macro_work_handler(NULL); /* initial empty report */
 assert(advances==0 && last_phase==1 && scheduled==1 && delay==10);
 failures=1; macro_work_handler(NULL); /* press fails */
 assert(advances==1 && last_phase==2 && macro_report_pending && delay==10);
 macro_work_handler(NULL); /* retry exactly the same press */
 assert(advances==1 && last_phase==2 && !macro_report_pending);
 macro_work_handler(NULL); /* release and honor delay */
 assert(advances==2 && last_phase==3 && delay==125);
 failures=1; macro_work_handler(NULL); /* final restore must also retry */
 assert(advances==3 && last_phase==0 && macro_report_pending);
 int count=scheduled; macro_work_handler(NULL);
 assert(advances==3 && last_phase==0 && !macro_report_pending && scheduled==count);
 keyboard.macro_phase=2; keyboard_hid_ready=false;
 count=attempts; macro_work_handler(NULL);
 assert(keyboard.macro_phase==0 && !macro_report_pending && attempts==count);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            source_path = Path(directory) / 'worker.c'
            binary = Path(directory) / 'worker'
            source_path.write_text(harness + handler + cases)
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            str(source_path), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
