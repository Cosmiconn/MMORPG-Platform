#pragma once

// ---------------------------------------------------------------------------
// WorkStealingQueue
// ---------------------------------------------------------------------------
// Lock-freie Chase-Lev Work-Stealing-Deque fuer Task*.
//
// Zugriffsregeln (WICHTIG, sonst Data Race):
//   - push()/pop(): NUR vom Owner-Thread (dem Worker, dem diese Queue gehoert)
//   - steal():      von JEDEM ANDEREN Thread
//
// Der Owner arbeitet am "bottom"-Ende (LIFO, push/pop - beste Cache-Lokalitaet
// fuer den eigenen Call-Stack/Divide&Conquer-Stil), Diebe stehlen vom "top"-
// Ende (FIFO aus Sicht der Diebe - aeltere Tasks zuerst, das reduziert die
// Chance, dass zwei Diebe um denselben, gerade erst gepushten Task konkurrieren).
//
// Kapazitaet ist bewusst FIX (Compile-Zeit-Template-Parameter), nicht runtime-
// dynamisch: eine wachsende Lock-free-Deque waere deutlich komplexer (Buffer-
// Resize unter laufenden Stehl-Operationen) und ist fuer Phase 0 nicht
// noetig - JobSystem::Config::queueCapacity wird aktuell nur als Wunschwert
// entgegengenommen; die tatsaechliche Kapazitaet bleibt bei CapacityV (siehe
// job_system.cpp). Bei Ueberlauf faellt JobSystem automatisch auf die globale
// Injector-Queue zurueck (siehe dort) - es geht also nie ein Task verloren,
// nur die Cache-Lokalitaet verschlechtert sich in diesem Grenzfall.
//
// Algorithmus nach Chase & Lev (2005) / korrigiert nach Le et al. (2013,
// "Correct and Efficient Work-Stealing for Weak Memory Models"): pop()
// braucht eine Store-Load-Barriere zwischen dem Dekrementieren von 'bottom'
// und dem Lesen von 'top', um das Race um das letzte Element der Deque
// korrekt aufzuloesen; steal() analog zwischen dem Lesen von 'top' und
// 'bottom'. Umgesetzt ueber memory_order_seq_cst DIREKT auf den beteiligten
// Atomic-Operationen (nicht ueber eine freistehende
// std::atomic_thread_fence) - ThreadSanitizer modelliert freistehende
// Fences nicht zuverlaessig, seq_cst-Operationen auf den Atomics selbst
// dagegen vollstaendig; die Garantie ist dieselbe.
// ---------------------------------------------------------------------------

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

namespace seed::jobs {

class Task;

template<size_t CapacityV = 256>
class WorkStealingQueue {
    static_assert((CapacityV & (CapacityV - 1)) == 0,
                  "CapacityV muss eine Zweierpotenz sein (Modulo per Bitmaske).");

public:
    static constexpr size_t Capacity = CapacityV;

    WorkStealingQueue() = default;
    WorkStealingQueue(const WorkStealingQueue&) = delete;
    WorkStealingQueue& operator=(const WorkStealingQueue&) = delete;

    // Nur vom Owner-Thread aufrufen. Gibt false zurueck, wenn die Deque voll
    // ist (Aufrufer muss dann selbst einen Fallback waehlen).
    bool push(Task* task) noexcept {
        const int64_t b = m_bottom.load(std::memory_order_relaxed);
        const int64_t t = m_top.load(std::memory_order_acquire);
        if (b - t >= static_cast<int64_t>(Capacity)) {
            return false; // voll
        }
        m_buffer[static_cast<size_t>(b) & (Capacity - 1)]
            .store(task, std::memory_order_relaxed);
        // release: der Buffer-Schreibzugriff oben muss fuer jeden Thread
        // sichtbar sein, der diesen neuen bottom-Wert per acquire-Load sieht
        // (steal() unten). Bewusst KEINE separate atomic_thread_fence-
        // Instruktion (siehe Klassenkommentar): ThreadSanitizer modelliert
        // freistehende Fences nicht zuverlaessig, seq_cst/release/acquire
        // direkt auf den Atomics dagegen vollstaendig.
        m_bottom.store(b + 1, std::memory_order_release);
        return true;
    }

    // Nur vom Owner-Thread aufrufen.
    Task* pop() noexcept {
        int64_t b = m_bottom.load(std::memory_order_relaxed) - 1;
        // seq_cst auf bottom UND (unten) auf top bildet zusammen die
        // Store-Load-Barriere, die noetig ist, um das Race um das letzte
        // Element korrekt aufzuloesen (Dekker-artiges Muster, siehe Le et
        // al. 2013). Ersetzt eine vorherige Implementierung mit
        // relaxed-Stores + separater atomic_thread_fence(seq_cst) - gleiche
        // Garantie, aber von ThreadSanitizer vollstaendig nachvollziehbar.
        m_bottom.store(b, std::memory_order_seq_cst);
        int64_t t = m_top.load(std::memory_order_seq_cst);

        if (t > b) {
            // Deque war bereits leer (oder wurde durch einen Dieb geleert).
            m_bottom.store(b + 1, std::memory_order_relaxed);
            return nullptr;
        }

        Task* task = m_buffer[static_cast<size_t>(b) & (Capacity - 1)]
                         .load(std::memory_order_relaxed);

        if (t == b) {
            // Letztes Element: Race gegen alle Diebe ueber CAS auf 'top'
            // entscheiden.
            if (!m_top.compare_exchange_strong(t, t + 1,
                                                std::memory_order_seq_cst,
                                                std::memory_order_relaxed)) {
                // Ein Dieb war schneller.
                task = nullptr;
            }
            m_bottom.store(b + 1, std::memory_order_relaxed);
        }
        return task;
    }

    // Von JEDEM Thread (auch dem Owner - aber der sollte pop() bevorzugen)
    // aufrufbar.
    Task* steal() noexcept {
        // seq_cst auf top UND bottom - siehe Kommentar in pop(). acquire
        // alleine wuerde fuer das top/bottom-Race nicht reichen; seq_cst auf
        // beiden Seiten stellt die noetige globale Ordnung her, ganz ohne
        // separate Fence-Instruktion.
        int64_t t = m_top.load(std::memory_order_seq_cst);
        int64_t b = m_bottom.load(std::memory_order_seq_cst);

        if (t >= b) {
            return nullptr; // leer
        }

        Task* task = m_buffer[static_cast<size_t>(t) & (Capacity - 1)]
                         .load(std::memory_order_relaxed);

        if (!m_top.compare_exchange_strong(t, t + 1,
                                            std::memory_order_seq_cst,
                                            std::memory_order_relaxed)) {
            // Ein anderer Dieb (oder der Owner via pop()) war schneller.
            return nullptr;
        }
        return task;
    }

    // Nur naeherungsweise korrekt bei gleichzeitigen push/pop/steal-Aufrufen -
    // ausschliesslich fuer Diagnose/Logging gedacht, nie fuer Kontrollfluss-
    // Entscheidungen verwenden.
    size_t approxSize() const noexcept {
        const int64_t b = m_bottom.load(std::memory_order_relaxed);
        const int64_t t = m_top.load(std::memory_order_relaxed);
        return (b > t) ? static_cast<size_t>(b - t) : 0;
    }

private:
    std::array<std::atomic<Task*>, Capacity> m_buffer{};
    alignas(64) std::atomic<int64_t> m_top{0};
    alignas(64) std::atomic<int64_t> m_bottom{0};
};

} // namespace seed::jobs

#ifdef _MSC_VER
#pragma warning(pop)
#endif
