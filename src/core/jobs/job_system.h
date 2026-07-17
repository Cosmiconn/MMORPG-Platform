#pragma once

// ---------------------------------------------------------------------------
// JobSystem
// ---------------------------------------------------------------------------
// Work-Stealing Job-System: N Worker-Threads, jeder mit eigener
// WorkStealingQueue. Ein Worker ohne eigene Arbeit stiehlt von zufaellig
// anderen Workern (siehe job_system.cpp: Worker::loop()).
//
// Zwei Nutzungsarten:
//   1. Fire-and-forget:  schedule(fn)            - keine Abhaengigkeiten
//   2. Abhaengigkeits-Graph: createTask/addDependency/submit - DAG-faehig
//
// Task-Speicher wird intern ueber PoolAllocator<Task> (P0-M2) verwaltet -
// die JobSystem-API selbst verlangt dafuer KEINEN Allocator-Parameter vom
// Aufrufer (JobSystem ist bewusst self-contained, analog zu einer eigenen
// kleinen "Arena" fuer Tasks). Ein TaskHandle ist nur bis zum Abschluss des
// zugehoerigen Tasks gueltig (siehe task.h).
//
// Thread-Safety: alle public-Methoden sind von jedem Thread aus aufrufbar.
// ---------------------------------------------------------------------------

#include "core/jobs/task.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace seed::jobs {

// Opakes Handle auf einen Task. Nur gueltig zwischen createTask() und dem
// Abschluss des Tasks (siehe task.h - Lebensdauer-Hinweis).
struct TaskHandle {
    Task* ptr = nullptr;

    explicit operator bool() const noexcept { return ptr != nullptr; }
};

// Konfiguration fuer JobSystem. Bewusst NICHT als verschachtelte Klasse
// (JobSystem::Config), da ein Default-Argument im JobSystem-Ctor sonst auf
// Default-Member-Initializer eines noch nicht vollstaendigen verschachtelten
// Typs zugreifen wuerde (Compiler-Fehler bei manchen Toolchains).
struct JobSystemConfig {
    // 0 bedeutet "hardware_concurrency() verwenden"; wird im Ctor
    // aufgeloest und mindestens auf 1 angehoben (Systeme, auf denen
    // hardware_concurrency() 0 liefert, z.B. manche Container-Limits).
    uint32_t numWorkers = 0;
    uint32_t queueCapacity = 256; // siehe work_stealing_queue.h fuer Details
};

class JobSystem {
public:
    using Config = JobSystemConfig; // Bequemlichkeits-Alias: JobSystem::Config

    explicit JobSystem(const JobSystemConfig& config = JobSystemConfig{});
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    // --- Fire-and-forget ---------------------------------------------------
    void schedule(std::function<void()> work, const char* name = "unnamed");

    // --- Abhaengigkeits-Graph ------------------------------------------------
    TaskHandle createTask(std::function<void()> work, const char* name = "unnamed");
    void addDependency(TaskHandle before, TaskHandle after);
    void submit(TaskHandle task);

    // --- Daten-Parallelitaet -------------------------------------------------
    // Teilt [0, count) in numWorkers-viele Chunks auf und verteilt sie als
    // Tasks. Blockiert, bis ALLE Chunks fertig sind (isolierter Wartepunkt,
    // unabhaengig von sonstiger, gleichzeitig laufender Arbeit im System).
    template<typename Func>
    void parallelFor(size_t count, Func&& func);

    // Globaler Synchronisationspunkt: wartet auf ALLE aktuell im System
    // befindlichen Tasks (nicht nur die eines bestimmten Aufrufs).
    //
    // WICHTIG: NICHT von innerhalb eines auf einem Worker laufenden Tasks
    // aufrufen (auch nicht transitiv ueber parallelFor()). Der aufrufende
    // Worker-Thread wuerde blockieren, statt selbst weiter Tasks abzuarbeiten
    // - bei ungluecklicher Verteilung ein Deadlock-Risiko. Vorgesehen fuer den
    // Aufruf vom Haupt-Thread (oder einem sonstigen Nicht-Worker-Thread).
    void barrier();

    // Blockiert den aufrufenden Thread, bis alle aktuell im System
    // befindlichen Tasks abgeschlossen sind. Dieselbe Einschraenkung wie
    // barrier() gilt.
    void waitForAll();

    uint32_t workerCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

    // Fuer parallelFor(): Chunk-Arbeit ohne Abhaengigkeiten einreihen und
    // eine Fertigstellungs-Benachrichtigung ueber ein eigenes, isoliertes
    // Zaehler/CV-Paar anstossen (siehe job_system.cpp fuer die Details -
    // die Implementierung der Template-Methode selbst ist unten inline).
    void scheduleRaw(std::function<void()> work, const char* name);
};

template<typename Func>
void JobSystem::parallelFor(size_t count, Func&& func) {
    if (count == 0) return;

    const uint32_t workers = std::max<uint32_t>(1, workerCount());
    const size_t chunkSize = (count + workers - 1) / workers;

    size_t chunkCount = 0;
    for (size_t start = 0; start < count; start += chunkSize) ++chunkCount;

    // Eigenstaendiges Zaehler/Mutex/CV-Tripel pro parallelFor()-Aufruf, damit
    // dieser Aufruf NICHT auf unabhaengige, gleichzeitig laufende Arbeit im
    // restlichen JobSystem wartet (im Unterschied zu waitForAll()/barrier(),
    // die absichtlich global sind). shared_ptr, weil die Chunk-Lambdas laenger
    // leben koennen als dieser Stack-Frame, falls parallelFor selbst innerhalb
    // eines anderen Tasks aufgerufen wird - der Aufrufer blockiert unten aber
    // ohnehin, bis alle Chunks fertig sind.
    auto remaining = std::make_shared<std::atomic<size_t>>(chunkCount);
    auto mutex = std::make_shared<std::mutex>();
    auto cv = std::make_shared<std::condition_variable>();

    for (size_t start = 0; start < count; start += chunkSize) {
        const size_t end = std::min(start + chunkSize, count);
        scheduleRaw(
            [&func, start, end, remaining, mutex, cv]() {
                for (size_t i = start; i < end; ++i) {
                    func(i);
                }
                if (remaining->fetch_sub(1, std::memory_order_acq_rel) == 1) {
                    std::lock_guard<std::mutex> lock(*mutex);
                    cv->notify_all();
                }
            },
            "parallelFor_chunk");
    }

    std::unique_lock<std::mutex> lock(*mutex);
    cv->wait(lock, [&] { return remaining->load(std::memory_order_acquire) == 0; });
}

} // namespace seed::jobs
