//
// Created by centimo on 03.11.2019.
//

#include <cmath>
#include <random>
#include <set>

#include "K_means_processor.h"


using namespace K_means_lib::utils;

void K_means_processor::thread_worker(Thread_data& thread_data)
{
  std::vector<Cluster> local_clusters(_clusters);

  size_t cycles_counter = 0;

  while (true)
  {
    ++cycles_counter;
    bool is_changed_local = false;

    for (auto& point_holder : thread_data._points_holders_range)
    {
      auto previous_cluster = point_holder._cluster_index;
      double minimal_distance = std::numeric_limits<double>::max();
      for (size_t cluster_index = 0; cluster_index < local_clusters.size(); ++cluster_index)
      {
        auto current_distance =
            local_clusters[cluster_index]._center.
              squared_distance_to(point_holder._point, _dimensions_number);

        if (current_distance < minimal_distance)
        {
          minimal_distance = current_distance;
          point_holder._cluster_index = cluster_index;
        }
      }

      if (previous_cluster != point_holder._cluster_index)
      {
        is_changed_local = true;
      }

      thread_data._clusters[point_holder._cluster_index]._points_number += 1;
    }

    thread_data._is_changed.store(is_changed_local);

    for (auto& cluster : thread_data._clusters)
    {
      cluster.clear_center();
    }

    synchronize_threads();

    if (is_converged())
    {
      break;
    }

    for (auto& local_cluster : local_clusters)
    {
      local_cluster.clear_full();
    };

    for (size_t cluster_index = 0; cluster_index < local_clusters.size(); ++cluster_index)
    {
      for (const auto& thread : _threads)
      {
        local_clusters[cluster_index]._points_number += thread->_clusters[cluster_index]._points_number;
      }
    }

    for (auto& point_holder : thread_data._points_holders_range)
    {
      thread_data._clusters[point_holder._cluster_index]._center.sum_with_division(
          point_holder._point,
          local_clusters[point_holder._cluster_index]._points_number);
    }

    synchronize_threads();

    for (size_t i = 0; i < local_clusters.size(); ++i)
    {
      for (const auto& thread : _threads)
      {
        local_clusters[i]._center.sum(thread->_clusters[i]._center);
      }
    }

    for (auto& cluster : thread_data._clusters)
    {
      cluster.clear_points_number();
    }
  }

  std::scoped_lock lock(_print_sync);
  std::cout << "Thread id: " << thread_data._index << ", cycles: " << cycles_counter << std::endl;
}


void K_means_processor::synchronize_threads()
{
  size_t value = _threads_number;

  while (_synchronization_phase_out.load(std::memory_order::memory_order_relaxed) % _threads_number != 0)
  {  }

  _synchronization_phase_out.compare_exchange_strong(value, 0);

  value = _synchronization_phase_in.fetch_add(1) + 1;
  while (value % _threads_number != 0)
  {
    value = _synchronization_phase_in.load();
    value = _synchronization_phase_in.load(std::memory_order::memory_order_relaxed);
  }

  _synchronization_phase_in.compare_exchange_strong(value, 0);

  _synchronization_phase_out.fetch_add(1);
}

bool K_means_processor::is_converged()
{
  for (const auto& thread_data : _threads)
  {
    if (thread_data->_is_changed.load(std::memory_order::memory_order_relaxed))
    {
      return false;
    }
  }

  return true;
}

K_means_processor::K_means_processor(std::vector< std::vector<double> >& values_buffer,
                                     size_t dimensions_number,
                                     size_t points_number,
                                     size_t clusters_number,
                                     size_t threads_number = 1)
  : _dimensions_number(dimensions_number),
    _threads_number(threads_number),
    _synchronization_phase_in(0),
    _synchronization_phase_out(threads_number)
{
  _points_holders.reserve(points_number);
  for (size_t i = 0; i < points_number; ++i)
  {
    _points_holders.emplace_back(0);
    _points_holders.back()._point.swap(values_buffer[i]);
  }

  std::mt19937 generator;
  std::uniform_int_distribution<size_t> uniform_distribution(0, points_number - 1);
  std::set<size_t> random_points;

  do
  {
    random_points.insert(uniform_distribution(generator));
  }
  while (random_points.size() < clusters_number);

  _clusters.reserve(clusters_number);

  size_t cluster_index = 0;
  for (const auto& point_index : random_points)
  {
    _clusters.emplace_back(_points_holders[point_index]._point);

    ++cluster_index;
  }
}

void K_means_processor::start()
{
  _threads.resize(_threads_number);
  const size_t points_per_thread = _points_holders.size() / _threads_number;
  const size_t threads_with_additional_point = _points_holders.size()  % _threads_number;

  for (size_t i = 0; i < threads_with_additional_point; ++i)
  {
    _threads[i] = std::make_unique<Thread_data>(
        std::ref(*this),
        Range<Point_holder>(
          _points_holders[i * (points_per_thread + 1)],
          points_per_thread + 1
        ),
        i,
        _clusters.size()
    );
  }

  for (size_t i = 0; i < _threads_number - threads_with_additional_point; ++i)
  {
    _threads[i + threads_with_additional_point] = std::make_unique<Thread_data>(
        std::ref(*this),
        Range<Point_holder>(
            _points_holders[threads_with_additional_point * (points_per_thread + 1) + i * points_per_thread],
            points_per_thread
        ),
        threads_with_additional_point + i,
        _clusters.size()
    );
  }
}

std::vector< std::vector<double> > K_means_processor::get_result()
{
  for (auto& thread : _threads)
  {
    if (thread->_thread.joinable())
    {
      thread->_thread.join();
    }
  }

  std::vector< std::vector<double> > result(_clusters.size());

  for (size_t i = 0; i < _clusters.size(); ++i)
  {
    result[i] = _clusters[i]._center;
  }

  return result;
}

K_means_processor::~K_means_processor()
{
  for (auto& thread : _threads)
  {
    if (thread->_thread.joinable())
    {
      thread->_thread.join();
    }
  }
}

K_means_processor::Thread_data::Thread_data(K_means_processor& processor,
                                            Range<Point_holder>&& range,
                                            size_t index,
                                            size_t clusters_number)
  : _points_holders_range(range),
    _index(index),
    _is_changed(false),
    _thread(&K_means_processor::thread_worker, &processor, std::ref(*this)),
    _clusters(clusters_number, Cluster{processor._dimensions_number})
{ }


void K_means_processor::Cluster::clear_center()
{
  if (_points_number)
  {
    _center.assign(_center.size(), 0.0);
  }
}

void K_means_processor::Cluster::clear_points_number()
{
  _points_number = 0;
}

void K_means_processor::Cluster::clear_full()
{
  _center.assign(_center.size(), 0.0);
  clear_points_number();
}

auto& K_means_processor::Cluster::operator[] (size_t index)
{
  return _center[index];
}