
#include <assert.h>
#include <functional>
#include <memory>
#include <utility>

template <typename T>
class FailsafeCallback {
 public:
  explicit FailsafeCallback(std::function<void(T)> callback, T fallback)
      : callback_(std::move(callback)), fallback_(std::move(fallback)) {}
  FailsafeCallback(FailsafeCallback&& other) noexcept {
    fallback_ = std::move(other.fallback_);
    callback_ = std::move(other.callback_);
    other.callback_ = nullptr;
  }
  FailsafeCallback& operator=(FailsafeCallback&& other) {
    InvokeFallbackIfCallbackDidntFire();
    fallback_ = std::move(other.fallback_);
    callback_ = std::move(other.callback_);
    other.callback_ = nullptr;
    return *this;
  }
  FailsafeCallback(const FailsafeCallback&) = delete;
  FailsafeCallback& operator=(const FailsafeCallback&) = delete;
  ~FailsafeCallback() { InvokeFallbackIfCallbackDidntFire(); }
  void operator()(T args) {
    assert(callback_);
    std::function<void(T)> callback = std::move(callback_);
    callback_ = nullptr;
    callback(args);
  }

 private:
  void InvokeFallbackIfCallbackDidntFire() {
    if (callback_)
      operator()(fallback_);
  }

  std::function<void(T)> callback_;
  T fallback_;
};

int main() {
  std::function<void(int)> f = [](int x) { printf("F: %d\n", x); };
  FailsafeCallback<int> cb(std::move(f), 1);
  // FailsafeCallback<void, int> cb2(std::move(cb));
  // cb2(3);
}
