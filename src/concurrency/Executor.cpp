#include <afina/concurrency/Executor.h>

namespace Afina {
namespace Concurrency {

Executor::Executor(std::string name, int low_watermark, int high_watermark, int max_queue_size, int idle_time)
    : _name(name), _low_watermark(low_watermark), _high_watermark(high_watermark), _max_queue_size(max_queue_size),
      _idle_time(idle_time) {}



} // namespace Concurrency
} // namespace Afina
