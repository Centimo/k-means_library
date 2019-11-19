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
    std::vector<double> _center;
    size_t _index;

    Cluster(const Cluster& copy)
      : _center(copy._center),
        _index(copy._index)
    { }

    Cluster(std::vector<double>& source,
            size_t index,
            size_t size)
      : _center(source),
        _index(index)
    { }

    auto& operator[] (size_t index)
    {
      return _center[index];
    }
  };

  struct Thread_data
  {
    K_means_lib::utils::Range<Point> _points_range;
    const size_t _index;
    mutable std::atomic<bool> _is_changed;
    std::vector<size_t> _clusters_sizes;

    std::thread _thread;

    Thread_data(K_means_processor& processor,
                K_means_lib::utils::Range<Point>&& range,
                size_t index,
                size_t clusters_number);
  };

  double squared_distance_between(
      const std::vector<double>& first_point,
      const std::vector<double>& second_point,
      size_t size) const
  {
    double result = 0.0;
    for (size_t i = 0; i < size; ++i)
    {
      result +=
          (first_point[i] - second_point[i])
          * (first_point[i] - second_point[i]);
    }

    return result;
  }

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
  std::vector< std::vector<double> > get_result();

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