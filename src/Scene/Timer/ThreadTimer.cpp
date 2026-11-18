module wescene.timer;
import rstd;

using namespace rstd::prelude;
using rstd::time::Duration;
using namespace owe;

ThreadTimer::ThreadTimer(Callback callback): m_callback(rstd::move(callback)) {}
ThreadTimer::~ThreadTimer() { Stop(); }

bool ThreadTimer::Running() const { return m_state.lock().unwrap()->running; }

void ThreadTimer::SetInterval(Duration interval) {
    auto state = m_state.lock().unwrap();
    if (state->interval == interval) return;
    state->interval = interval;
    m_condition.notify_one();
}

void ThreadTimer::Start() {
    auto operation = m_op_mutex.lock().unwrap();
    {
        auto state = m_state.lock().unwrap();
        if (state->running) return;
        state->running = true;
    }
    auto thread = rstd::thread::spawn([this] {
        for (;;) {
            {
                auto state = m_state.lock().unwrap();
                if (! state->running) break;
                const auto interval = state->interval;
                const auto elapsed =
                    m_condition.wait_timeout_while(state, interval, [interval](const State& value) {
                        return value.running && value.interval == interval;
                    });
                if (! elapsed.timed_out()) continue;
            }
            m_callback->operator()();
        }
    });
    if (thread.is_err()) {
        auto state     = m_state.lock().unwrap();
        state->running = false;
    }
    m_timer_thread = Some(rstd::move(thread).unwrap());
}

void ThreadTimer::Stop() {
    auto operation = m_op_mutex.lock().unwrap();
    if (! m_timer_thread) return;
    if (rstd::thread::current_id() == m_timer_thread->thread().id())
        rstd::panic { "ThreadTimer cannot stop from its callback" };
    {
        auto state     = m_state.lock().unwrap();
        state->running = false;
    }
    m_condition.notify_all();
    rstd::move(m_timer_thread.take().unwrap()).join().unwrap();
}
