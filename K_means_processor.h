//
// Created by centimo on 03.11.2019.
//

#pragma once

#include <memory>
#include <vector>
#include <optional>
#include <deque>
#include <thread>
#include <mutex>

#include "utils.hpp"


class K_means_processor
{
  struct Point
  {
    const size_t _index;
    size_t _cluster;
  };

  struct Cluster
  {
    K_means_lib::utils::Atomic_buffer<double> _buffer;
    size_t _index;
    std::atomic<size_t> _size;

    Cluster(const Cluster& copy)
      : _buffer(copy._buffer),
        _index(copy._index),
        _size(copy._size.load())
    { }

    Cluster(std::vector<double>& source,
            size_t parts_number,
            size_t index,
            size_t size)
      : _buffer(source, parts_number),
        _index(index),
        _size(size)
    { }

    auto& operator[] (size_t index) const
    {
      return _buffer[index];
    }
  };

  struct Thread_data
  {
    K_means_lib::utils::Range<Point> _points_range;
    const size_t _index;
    mutable std::atomic<bool> _is_changed;

    std::thread _thread;

    Thread_data(K_means_processor& processor, K_means_lib::utils::Range<Point>&& range, size_t index);
  };

public:
  struct Cluster_result
  {
    std::vector<double> _center;
  };

private:
  void synchronize_threads();
  bool is_converged();
  void thread_worker(Thread_data& thread_data);

public:
  K_means_processor(
      std::vector< std::vector<double> >&& values_buffer,
      size_t dimensions_number,
      size_t points_number,
      size_t clusters_number,
      size_t threads_number);

  ~K_means_processor();

  void start();
  std::vector<Cluster_result> get_result();

private:
  const size_t _dimensions_number;

  std::vector< std::vector<double> > _buffer;
  std::vector<Point> _points;

  std::vector<Cluster> _clusters;

  std::vector<std::unique_ptr<Thread_data> > _threads;
  const size_t _threads_number;

  std::atomic<size_t> _synchronization_phase_in;
  std::atomic<size_t> _synchronization_phase_out;

  std::mutex _print_sync;
};