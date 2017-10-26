#include <memory>
#include <functional>
#include <stdio.h>

struct TheWeak {
  TheWeak() { printf("ctor %p\n", this); }
  ~TheWeak() { printf("~dtor %p\n", this); }
  std::weak_ptr<TheWeak> wptr;

  std::weak_ptr<TheWeak> Gimme() {
    return wptr;
  }
};

int main() {
  std::shared_ptr<TheWeak> tw(new TheWeak());
  tw->wptr = tw;
  printf("%ld\n", tw->wptr.use_count());
  std::weak_ptr<TheWeak> w2 = tw->Gimme();
  std::weak_ptr<TheWeak> w3 = tw->Gimme();
  printf("%ld %ld %ld\n", tw->wptr.use_count(), w2.use_count(), w3.use_count());

}
