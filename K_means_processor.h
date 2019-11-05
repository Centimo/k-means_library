//
// Created by centimo on 03.11.2019.
//

#pragma once

#include <memory>
#include <vector>
#include <optional>
#include <deque>
#include <thread>

#include "Buffer.hpp"


class K_means_processor
{
private:
  struct Point
  {
    const Range _range;
    size_t _cluster;
  };

  struct Cluster
  {
    Atomic_buffer<float> _buffer;
    size_t _index;
    std::atomic<size_t> _size;

    Cluster(const Cluster& copy)
      : _buffer(copy._buffer),
        _index(copy._index),
        _size(copy._size.load())
    { }

    Cluster(Atomic_buffer<float>&& buffer, size_t index, size_t size)
      : _buffer(buffer),
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
    Range _points_range;
    const size_t _index;
    mutable std::atomic<bool> _is_changed;

    std::thread _thread;
  };

private:
  void synchronize_threads();
  bool is_converged();
  void thread_worker(const Thread_data& thread_data);

public:
  K_means_processor(Buffer<float>&& values_buffer, size_t points_number, size_t clusters_number, size_t threads_number);

private:
  Buffer<float> _buffer;
  Buffer<Point> _points;

  std::vector<Cluster> _clusters;

  std::vector<std::unique_ptr<Thread_data> > _threads;
  const size_t _thread_number;

  std::atomic<size_t> _synchronizer;
  std::atomic<bool> _is_sync_up;
};