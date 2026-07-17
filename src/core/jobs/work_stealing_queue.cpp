#include "core/jobs/work_stealing_queue.h"

// WorkStealingQueue ist ein Template (Kapazitaet als Compile-Zeit-Parameter,
// siehe Header-Kommentar), daher lebt die eigentliche Logik im Header.
// Diese Datei instanziiert explizit die von JobSystem verwendete Standard-
// Kapazitaet, damit nicht jede includierende Translation Unit die komplette
// Deque-Logik erneut instanziieren muss (Compile-Zeit-Optimierung).

namespace seed::jobs {

template class WorkStealingQueue<256>;

} // namespace seed::jobs
