#include "utilities.h"
#include "get_time.h"
#include "random.h"
#include "reducer.h"
#include "counting_sort.h"
#include "collect_reduce.h"
#include "random_shuffle.h"
#include "histogram.h"
#include "integer_sort.h"
#include "sample_sort.h"
#include "merge.h"
#include "merge_sort.h"
#include "bag.h"
#include "hash_table.h"
#include "sparse_mat_vec_mult.h"
#include "sequence_ops.h"
#include "monoid.h"

#include <iostream>
#include <ctype.h>
#include <math.h>

static timer bt;
using namespace std;
using uchar = unsigned char;

#define time(_var,_body)    \
  bt.start();               \
  _body;		    \
  double _var = bt.stop();

template<typename T>
double t_tabulate(size_t n) {
  auto f = [] (size_t i) {return i;};
  time(t, sequence<T>(n, f););
  return t;
}

template<typename T>
double t_map(size_t n) {
  sequence<T> In(n, (T) 1);
  auto f = [&] (size_t i) {return In[i];};
  time(t, sequence<T>(n, f));
  return t;
}

template<typename T>
double t_reduce_add(size_t n) {
  sequence<T> S(n, (T) 1);
  time(t, pbbs::reduce_add(S););
  return t;
}

double t_map_reduce_128(size_t n) {
  int stride = 16;
  sequence<size_t> S(n*stride, (size_t) 1);
  auto get = [&] (size_t i) {
    // gives marginal improvement (5% or so on aware)
    __builtin_prefetch (&S[(i+4)*stride], 0, 3);
    return S[i*stride];};
  auto T = make_sequence<size_t>(n, get);
  time(t, pbbs::reduce_add(T););
  return t;
}

template<typename T>
double t_scan_add(size_t n) {
  sequence<T> In(n, (T) 1);
  sequence<T> Out(n, (T) 0);
  time(t, pbbs::scan_add(In,Out););
  return t;
}

template<typename T>
double t_pack(size_t n) {
  sequence<bool> flags(n, [] (size_t i) -> bool {return i%2;});
  sequence<T> In(n, [] (size_t i) -> T {return i;});
  time(t, pbbs::pack(In, flags););
  return t;
}

template<typename T>
double t_split3_old(size_t n) {
  sequence<uchar> flags(n, [] (size_t i) -> uchar {return i%3;});
  sequence<T> In(n, [] (size_t i) -> T {return i;});
  sequence<T> Out(n, (T) 0);
  time(t, pbbs::split_three(In, Out, flags););
  return t;
}

template<typename T>
double t_split3(size_t n) {
  pbbs::random r(0);
  sequence<T> In(n, [&] (size_t i) {return r.ith_rand(i);});
  sequence<T> Out(n, (T) 0);
  time(t, pbbs::p_split3(In, Out, std::less<T>()););
  return t;
}

double t_histogram_reducer(size_t n) {
  pbbs::random r(0);
  constexpr int count = 1024;
  histogram_reducer<int,count> red;
  using aa = array<size_t,8>;
  sequence<aa> In(n, [&] (size_t i) {aa x; x[0] = r.ith_rand(i) % count; return x;});
  auto f = [&] (size_t i) { red->add_value(In[i][0]);};
  time(t, par_for(0, n, 100, f););
  //cout << red.get_value()[0] << endl;
  return t;
}

template<typename T>
double t_gather(size_t n) {
  pbbs::random r(0);
  sequence<T> in(n, [&] (size_t i) {return i;});
  sequence<T> idx(n, [&] (size_t i) {return r.ith_rand(i)%n;});
  //sequence<T> out(n);
  //time(t, parallel_for(size_t i=0; i<n; i++) {out[i] = in[idx[i]];});
  auto f = [&] (size_t i) {
    // prefetching helps significantly
    __builtin_prefetch (&in[idx[i+4]], 0, 1);
    return in[idx[i]];};
  // note problem with prefetching since will go over end for last 4 iterations
  time(t, sequence<T>(n-4, f););  
  return t;
}

template<typename T>
double t_scatter(size_t n) {
  pbbs::random r(0);
  sequence<T> out(n, (T) 0);
  sequence<T> idx(n, [&] (size_t i) {return r.ith_rand(i)%n;});
  auto f = [&] (size_t i) {
      __builtin_prefetch (&out[idx[i+4]], 1, 1);
      out[idx[i]] = i;};
  time(t, par_for(0, n-4, 10000, f););
  //time(t, parallel_for(size_t i=0; i < n; i++) {out[idx[i]] = i;});
  return t;
}

template<typename T>
double t_write_add(size_t n) {
  pbbs::random r(0);
  sequence<T> out(n, (T) 0);
  sequence<T> idx(n, [&] (size_t i) {return r.ith_rand(i)%n;});
  auto f = [&] (size_t i) {
    // putting write prefetch in slows it down
    //__builtin_prefetch (&out[idx[i+4]], 0, 1);
    pbbs::write_add(&out[idx[i]],1);};
  time(t, par_for(0, n-4, 10000, f););
  //time(t, parallel_for(size_t i=0; i<n-3; i++) {
  //pbbs::write_add(&out[idx[i]],1);});
  return t;
}

template<typename T>
double t_write_min(size_t n) {
  pbbs::random r(0);
  sequence<T> out(n, (T) n);
  sequence<T> idx(n, [&] (size_t i) {return r.ith_rand(i)%n;});
  auto f = [&] (size_t i) {
    // putting write prefetch in slows it down
    //__builtin_prefetch (&out[idx[i+4]], 1, 1);
    pbbs::write_min(&out[idx[i]], (T) i, pbbs::less<T>());};
  time(t, par_for(0, n-4, 10000, f););
  //time(t, parallel_for(size_t i=0; i<n-3; i++) {
  //pbbs::write_min(&out[idx[i]], (T) i, pbbs::less<T>());});
  return t;
}

template<typename T>
double t_shuffle(size_t n) {
  sequence<T> in(n, [&] (size_t i) {return i;});
  time(t, pbbs::random_shuffle(in,n););
  return t;
}

template<typename T>
double t_histogram(size_t n) {
  pbbs::random r(0);
  sequence<T> in(n, [&] (size_t i) {return r.ith_rand(i)%n;});
  sequence<T> out;
  time(t, out = pbbs::histogram<T>(in,n););
  return t;
}

template<typename T>
double t_histogram_few(size_t n) {
  pbbs::random r(0);
  sequence<T> in(n, [&] (size_t i) {return r.ith_rand(i)%256;});
  sequence<T> out;
  time(t, out = pbbs::histogram<T>(in,256););
  return t;
}

template<typename T>
double t_histogram_same(size_t n) {
  sequence<T> in(n, (T) 10311);
  sequence<T> out;
  time(t, out = pbbs::histogram<T>(in,n););
  return t;
}

template<typename T>
double t_sort(size_t n) {
  pbbs::random r(0);
  sequence<T> in(n, [&] (size_t i) {return r.ith_rand(i)%n;});
  sequence<T> out;
  time(t, out = pbbs::sample_sort(in, std::less<T>()););
  //for (size_t i = 1; i < n; i++)
  //  if (std::less<T>()(in[i],in[i-1])) {cout << i << endl; abort();}
  return t;
}

template<typename T>
double t_merge_sort(size_t n) {
  pbbs::random r(0);
  sequence<T> in(n, [&] (size_t i) {return r.ith_rand(i)%n;});
  sequence<T> out(n);
  time(t, pbbs::merge_sort(out, in, std::less<T>()););
  //for (size_t i = 1; i < n; i++)
  //  if (std::less<T>()(in[i],in[i-1])) {cout << i << endl; abort();}
  return t;
}

template<typename T>
double t_quicksort(size_t n) {
  pbbs::random r(0);
  sequence<T> in(n, [&] (size_t i) {return r.ith_rand(i)%n;});
  sequence<T> out(n);
  time(t, pbbs::p_quicksort(in, out, std::less<T>()););
  //for (size_t i = 1; i < n; i++)
  //  if (std::less<T>()(in[i],in[i-1])) {cout << i << endl; abort();}
  return t;
}

template<typename T>
double t_count_sort_2(size_t n) {
  pbbs::random r(0);
  size_t num_buckets = (1<<2);
  size_t mask = num_buckets - 1;
  sequence<T> in(n, [&] (size_t i) {return r.ith_rand(i);});
  auto f = [&] (size_t i) {return in[i] & mask;};
  auto keys = make_sequence<unsigned char>(n, f);

  time(t, pbbs::count_sort(in, in, keys, num_buckets););
  return t;
}

template<typename T>
double t_count_sort_8(size_t n) {
  pbbs::random r(0);
  size_t num_buckets = (1<<8);
  size_t mask = num_buckets - 1;
  sequence<T> in(n, [&] (size_t i) {return r.ith_rand(i);});
  auto f = [&] (size_t i) {return in[i] & mask;};
  auto keys = make_sequence<unsigned char>(n, f);
  time(t, pbbs::count_sort(in, in, keys, num_buckets););
  return t;
}

template<typename T>
double t_collect_reduce_pair(size_t n) {
  using par = pair<T,T>;
  pbbs::random r(0);
  sequence<par> S(n, [&] (size_t i) -> par {
      return par(r.ith_rand(i)%n,1);});
  //auto get_index = [] (par e) {return e.first;};
  //auto get_val = [] (par e) {return e.second;};
  //auto add = [&] (T a, T b) {return a + b;};
  //time(t, pbbs::collect_reduce<T>(S, n, get_index, get_val, 0, add););
  time(t, pbbs::collect_reduce_pair<Add<int>>(S););
  return t;
}

template<typename T>
double t_collect_reduce_8_n(size_t n) {
  pbbs::random r(0);
  size_t num_buckets = (1<<8);
  size_t mask = num_buckets - 1;
  sequence<T> in(n, [&] (size_t i) {return r.ith_rand(i);});
  auto bucket = [&] (size_t i) {return in[i] & mask;};
  auto add = [&] (T a, T b) {return a + b;};
  auto keys = make_sequence<unsigned char>(n, bucket);
  time(t, pbbs::collect_reduce<T>(in, keys, num_buckets, 0, add););
  return t;
}

template<typename T>
double t_collect_reduce_8(size_t n) {
  pbbs::random r(0);
  size_t num_buckets = (1<<8);
  size_t mask = num_buckets - 1;
  using sums = tuple<float,float,float,float>;

  sequence<sums> in(n, [&] (size_t i) -> sums {return sums(1.0,1.0,1.0,1.0);});
  auto bucket = [&] (size_t i) -> uchar { return r.ith_rand(i) & mask; };
  auto keys = make_sequence<unsigned char>(n, bucket);

  auto sum = [] (sums a, sums b) -> sums {
    return sums(get<0>(a)+get<0>(b), get<1>(a)+get<1>(b),
		get<2>(a)+get<2>(b), get<3>(a)+get<3>(b));
  };

  time(t,
       pbbs::collect_reduce<sums>(in,
				   //make_sequence<uchar>(n,key),
				   keys,
				   num_buckets,
				   sums(0.0,0.0,0.0,0.0),
				   sum););
  return t;
}


template<typename T>
double t_integer_sort_pair(size_t n) {
  using par = pair<T,T>;
  pbbs::random r(0);
  size_t bits = sizeof(T)*8;
  sequence<par> S(n, [&] (size_t i) -> par {
      return par(r.ith_rand(i),i);});
  auto first = [] (par a) {return a.first;};
  time(t, pbbs::integer_sort<T>(S,S,first,bits););
  return t;
}

template<typename T>
double t_integer_sort(size_t n) {
  pbbs::random r(0);
  size_t bits = sizeof(T)*8;
  sequence<T> S(n, [&] (size_t i) -> T {
      return r.ith_rand(i);});
  auto identity = [] (T a) {return a;};
  time(t, pbbs::integer_sort<T>(S,S,identity,bits););
  return t;
}

typedef unsigned __int128 long_int;
double t_integer_sort_128(size_t n) {
  pbbs::random r(0);
  size_t bits = 128;
  sequence<long_int> S(n, [&] (size_t i) -> long_int {
      return r.ith_rand(2*i) + (((long_int) r.ith_rand(2*i+1)) << 64) ;});
  auto identity = [] (long_int a) {return a;};
  time(t, pbbs::integer_sort<long_int>(S,S,identity,bits););
  return t;
}

template<typename T>
double t_merge(size_t n) {
  sequence<T> in1(n/2, [&] (size_t i) {return 2*i;});
  sequence<T> in2(n-n/2, [&] (size_t i) {return 2*i+1;});
  sequence<T> out(n, (T) 0);
  time(t, pbbs::merge(in1, in2, out, std::less<T>()););
  return t;
}

template<typename T>
double t_remove_duplicates(size_t n) {
  pbbs::random r(0);
  sequence<T> In(n, [&] (size_t i) -> T {return r.ith_rand(i) % n;});
  time(t, pbbs::remove_duplicates(In););
  return t;
}

template <typename T, typename F>
static T my_reduce(sequence<T> s, size_t start, size_t end, F f) {
  if (end - start == 1) return s[start];
  size_t h = (end + start)/2;
  if (h > 50) {
    T r;
    auto do_right = [&] () {r = my_reduce(s, h, end, f);};
    cilk_spawn do_right();
    T l = my_reduce(s, start, h, f);
    cilk_sync;
    return f(l,r);
  } else {
    T l = my_reduce(s, start, h, f);
    T r = my_reduce(s, h, end, f);
    return f(l,r);
  }
}

template<typename T>
double t_bag(size_t n) {
  using TB = bag<T>;
  TB::init();
  sequence<TB> In(n, [&] (size_t i) -> TB {return TB((T) i);});
  time(t, TB x = my_reduce(In, 0, n, TB::append); x.flatten(););
  return t;
}

template<typename s_size_t, typename T>
double t_mat_vec_mult(size_t n) {
  pbbs::random r(0);
  size_t degree = 5;
  size_t m = degree*n;
  sequence<s_size_t> starts(n+1, [&] (size_t i) {
      return degree*i;});
  sequence<s_size_t> columns(m, [&] (size_t i) {
      return r.ith_rand(i)%n;});
  sequence<T> values(m, (T) 1);
  sequence<T> in(n, (T) 1);
  sequence<T> out(n, (T) 0);
  auto add = [] (T a, T b) { return a + b;};
  auto mult = [] (T a, T b) { return a * b;};

  time(t, mat_vec_mult(starts, columns, values, in, out, mult, add););
  return t;
}
