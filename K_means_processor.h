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
  struct Point_holder
  {
    K_means_lib::utils::Point _point;
    size_t _cluster_index;

    explicit
    Point_holder(size_t cluster_index)
      : _cluster_index(cluster_index)
    { }
  };

  struct Cluster
  {
    K_means_lib::utils::Point _center;
    size_t _points_number;

    Cluster(const Cluster& copy) = default;

    explicit
    Cluster(const std::vector<double>& source)
        : _center(source),
          _points_number(0)
    { }

    explicit
    Cluster(size_t length)
      : _points_number(0),
        _center(length, 0.0)
    { }

    void clear_center();
    void clear_points_number();
    void clear_full();
    auto& operator[] (size_t index);
  };

  struct Thread_data
  {
    K_means_lib::utils::Range<Point_holder> _points_holders_range;
    const size_t _index;
    mutable std::atomic<bool> _is_changed;
    std::vector<Cluster> _clusters;

    std::thread _thread;

    Thread_data(K_means_processor& processor,
                K_means_lib::utils::Range<Point_holder>&& range,
                size_t index,
                size_t clusters_number);
  };

private:
  void synchronize_threads();
  bool is_converged();
  void thread_worker(Thread_data& thread_data);

public:
  K_means_processor(
      std::vector< std::vector<double> >& values_buffer,
      size_t dimensions_number,
      size_t points_number,
      size_t clusters_number,
      size_t threads_number);

  ~K_means_processor();

  void start();
  std::vector< std::vector<double> > get_result();

private:
  const size_t _dimensions_number;

  std::vector<Point_holder> _points_holders;

  std::vector<Cluster> _clusters;

  std::vector<std::unique_ptr<Thread_data> > _threads;
  const size_t _threads_number;

  std::atomic<size_t> _synchronization_phase_in;
  std::atomic<size_t> _synchronization_phase_out;

  std::mutex _print_sync;
};