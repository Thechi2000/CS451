#include <atomic>
#include <optional>

// Lock-free queue implementation, based on
// https://web.archive.org/web/20190714042351/http://www.cs.rochester.edu/u/scott/papers/1996_PODC_queues.pdf
template <typename V> class LFQueue {
  public:
    LFQueue() : head_(nullptr), tail_(nullptr) {
        Entry *node = new Entry();
        head_ = node;
        tail_ = node;
    }

    void enqueue(const V &value) {
        Entry *node = new Entry();
        node->value = value;
        node->next = nullptr;

        while (true) {
            Entry *tail = tail_.load();

            // This may cause an error, see
            // https://web.archive.org/web/20190905090735/http://blog.shealevy.com/2015/04/23/use-after-free-bug-in-maged-m-michael-and-michael-l-scotts-non-blocking-concurrent-queue-algorithm/
            // For now, I will apply the ostrich algorithm
            Entry *next = tail->next;

            if (tail == tail_.load()) {
                if (next == nullptr) {
                    if (tail->next.compare_exchange_strong(next, node)) {
                        tail_.compare_exchange_strong(tail, node);
                        return;
                    }
                } else {
                    tail_.compare_exchange_strong(tail, next);
                }
            }
        }
    }
    std::optional<V> dequeue() {
        while (true) {
            Entry *head = head_.load();
            Entry *tail = tail_.load();
            Entry *next = head->next;

            if (head == head_.load()) {
                if (head == tail) {
                    if (next == nullptr) {
                        return std::nullopt;
                    }
                    tail_.compare_exchange_strong(tail, next);
                } else {
                    V value = next->value;
                    if (head_.compare_exchange_strong(head, next)) {
                        delete head;
                        return value;
                    }
                }
            }
        }
    }

    bool isEmpty() { return head_.load() == tail_.load(); }

  private:
    struct Entry {
        V value;
        std::atomic<Entry *> next;
    };

    std::atomic<Entry *> head_;
    std::atomic<Entry *> tail_;
};