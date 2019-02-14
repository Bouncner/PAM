#pragma once

#include "utilities.h"

template <typename E>
struct sequence {
public:
  using T = E;

  sequence() {}

  // copy constructor
  sequence(sequence& a) : s(a.s), e(a.e), allocated(false) {}

  // move constructor
  sequence(sequence&& b)
    : s(b.s), e(b.e), allocated(b.allocated) {
    b.s = b.e = NULL; b.allocated = false;}

  // copy assignment
  sequence& operator = (const sequence& b) {
    if (this != &b) {clear(); s = b.s; e = b.e; allocated = false;}
    return *this;
  }

  // move assignment
  sequence& operator = (sequence&& b) {
    if (this != &b) {
      clear(); s = b.s; e = b.e; allocated = b.allocated;
      b.s = b.e = NULL; b.allocated=false;}
    return *this;
  }

  sequence(const size_t n)
    : s(pbbs::new_array<E>(n)), allocated(true) {
    e = s + n;
  };

  static sequence<E> alloc_no_init(const size_t n) {
    sequence<E> r;
    r.s = pbbs::new_array_no_init<E>(n);
    r.e = r.s + n;
    r.allocated = true;
    return r;
  };

  sequence(const size_t n, T v)
    : s(pbbs::new_array_no_init<E>(n, true)), allocated(true) {
    e = s + n;
    auto f = [=] (size_t i) {new ((void*) (s+i)) T(v);};
    parallel_for(0, n, f, 0);
  };

  template <typename Func>
  sequence(const size_t n, const Func& f)
    : s(pbbs::new_array_no_init<E>(n)), allocated(true) {
    e = s + n;
    auto g = [&] (size_t i) {
      T x = f(i);
      new ((void*) (s+i)) T(x);};
    parallel_for(0, n, g, 0);
  }

  sequence(E* s, const size_t n, bool allocated = false)
    : s(s), e(s + n), allocated(allocated) {};

  sequence(E* s, E* e, bool allocated = false)
    : s(s), e(e), allocated(allocated) {};

  ~sequence() { clear();}


  template <typename F>
  static sequence<T> tabulate(size_t n, F f) {
    T* r = pbbs::new_array_no_init<T>(n);
    auto g = [&] (size_t i) {
      new ((void*) (r+i)) T(f(i));};
    parallel_for(0, n, g, pbbs::granularity(n));
    sequence<T> y(r,n);
    y.allocated = true;
    return y;
  }

  sequence copy(sequence& a) {
    return tabulate(e-s, [&] (size_t i) {return a[i];});
  }

  E& operator[] (const size_t i) const {return s[i];}

  sequence slice(size_t ss, size_t ee) {
    return sequence(s + ss, s + ee);
  }

  void update(const size_t i, T& v) {
    s[i] = v;
  }

  size_t size() { return e - s;}

  sequence as_sequence() {
    return sequence(s, e);
  }

  T* as_array() {return s;}
  T* start() {return s;}
  T* end() {return e;}
  bool is_allocated() {return allocated;}
  void set_allocated(bool a) {allocated = a;}

  void clear() {
    if (allocated) pbbs::delete_array<E>(s,e-s);
    allocated = false;
    s = e = NULL;
  }

private:
  E *s; // = NULL;
  E *e; // = NULL;
  bool allocated = false;
};

template <typename E, typename F>
struct func_sequence {
  using T = E;
  func_sequence(size_t n, F& _f) : f(&_f), s(0), e(n) {};
  func_sequence(size_t s, size_t e, F& _f) : f(&_f), s(s), e(e) {};
  T operator[] (const size_t i) {return (*f)(i+s);}
  func_sequence<T,F> slice(size_t ss, size_t ee) {
    return func_sequence<T,F>(s+ss,s+ee,*f); }
  size_t size() { return e - s;}
  sequence<T> as_sequence() {
    return sequence<T>::tabulate(e-s, [&] (size_t i) {return (*f)(i+s);});
  }
private:
  F *f;
  size_t s, e;
};

// used so second template argument can be inferred
template <class E, class F>
func_sequence<E,F> make_sequence (size_t n, F& f) {
  return func_sequence<E,F>(n,f);
}

// convenience method
template <class E>
sequence<E> make_sequence (E* seq, size_t n) {
  return sequence<E>(seq, n, false);
}
