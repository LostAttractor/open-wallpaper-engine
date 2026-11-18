module wescene.timer;
import rstd;

using namespace rstd::prelude;
using rstd::time::Duration;
using rstd::time::Instant;
using namespace owe;

FrameTimer::FrameTimer(Option<Callback> callback)
    : m_callback(rstd::move(callback)), m_timer(ThreadTimer::Callback::make([this] {
          auto callback = m_callback.lock().unwrap();
          if (callback->is_some() && ! m_frame_pending.load()) {
              m_frame_pending.store(true);
              (**callback)->operator()();
          }
      })) {
    SetRequiredFps(u16(15));
}
FrameTimer::~FrameTimer() { Stop(); }

u16    FrameTimer::RequiredFps() const { return m_req_fps.load(); }
double FrameTimer::FrameTime() const {
    return static_cast<double>(m_frametime.load().to_primitive()) / 1'000'000.0;
}
double FrameTimer::TargetFrameTime() const {
    return static_cast<double>(m_ideatime.load().to_primitive()) / 1'000'000.0;
}

void FrameTimer::SetRequiredFps(u16 value) {
    if (value == u16()) value = u16(1);
    auto samples = m_samples.lock().unwrap();
    m_req_fps.store(value);
    const auto fps   = u64(value.to_primitive());
    const auto ideal = (u64(1'000'000) + fps - u64(1)) / fps;
    m_ideatime.store(ideal);
    for (auto& sample : samples->values) sample = ideal;
    samples->next = usize();
    m_frametime.store(ideal);
    m_timer.SetInterval(Duration::from_micros(ideal));
}
void FrameTimer::FrameBegin() { m_clock = Instant::now(); }
void FrameTimer::FrameEnd() {
    auto samples                   = m_samples.lock().unwrap();
    samples->values[samples->next] = rstd::as_cast<u64>(m_clock.elapsed().as_micros());
    samples->next                  = (samples->next + usize(1)) % samples->values.len();
    u64 total {};
    for (auto sample : samples->values) total += sample;
    m_frametime.store(total / u64(5));
}
void FrameTimer::CompleteScheduledFrame() { m_frame_pending.store(false); }
void FrameTimer::SetCallback(Option<Callback> callback) {
    if (Running()) return;
    auto current = m_callback.lock().unwrap();
    rstd::mem::swap(*current, callback);
}
void FrameTimer::Run() { m_timer.Start(); }
void FrameTimer::Stop() { m_timer.Stop(); }
bool FrameTimer::Running() const { return m_timer.Running(); }
