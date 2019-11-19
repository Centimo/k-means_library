#include "K_means_lib.h"

#include <iostream>
#include <cstdlib>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/iostreams/stream.hpp>

namespace K_means_lib
{
  std::unique_ptr<K_means_processor> make_k_means_processor(const std::string& config_filename)
  {
    boost::property_tree::ptree settings;
    try
    {
      boost::property_tree::read_json(config_filename, settings);
    }
    catch (boost::property_tree::json_parser_error& error)
    {
      std::cerr << "File reading error: " << error.what() <<  std::endl;
      return nullptr;
    }

    auto data_filename_optional = settings.get_optional<std::string>("Data filename");
    if (!data_filename_optional)
    {
      std::cerr << "Can't find \"Data filename\" field" << std::endl;
      return nullptr;
    }

    auto clusters_number_optional = settings.get_optional<size_t>("Clusters number");
    if (!clusters_number_optional)
    {
      std::cerr << "Can't find \"Clusters number\" field" << std::endl;
      return nullptr;
    }

    size_t dimensions_number = 0;
    auto dimensions_number_optional = settings.get_optional<size_t>("Dimensions number");
    if (!dimensions_number_optional)
    {
      std::cout << "Can't find \"Dimensions number\" field" << std::endl;
    }
    else
    {
      dimensions_number = dimensions_number_optional.value();
    }

    auto points_number_optional = settings.get_optional<size_t>("Points number");

    std::vector< std::vector<double> > data_buffer;
    if (points_number_optional)
    {
      data_buffer.reserve(points_number_optional.value());
    }

    if (dimensions_number)
    {
      for (auto& point_vector : data_buffer)
      {
        point_vector.reserve(dimensions_number);
      }
    }

    auto threads_number_optional = settings.get_optional<size_t>("Threads number");
    size_t threads_number = threads_number_optional.get_value_or(1);
    if (threads_number == 0)
    {
      threads_number = 1;
    }

    boost::iostreams::mapped_file mmap;

    try
    {
      mmap.open(data_filename_optional.value(), boost::iostreams::mapped_file::readonly);
    }
    catch (const std::ios_base::failure& error)
    {
      std::cerr << "File reading error: " << error.what() <<  std::endl;
      return nullptr;
    }

    std::string_view file(mmap.const_data(), mmap.size());

    char* end = nullptr;
    size_t line_number = 1;
    size_t row_number = 1;
    size_t current_position = 0;
    size_t next_position = 0;

    while (line_number <= points_number_optional.value())
    {
      data_buffer.emplace_back(std::vector<double>());
      auto& point_vector = data_buffer.back();

      current_position = next_position;
      next_position = file.find_first_of("\n", next_position + 1);
      if (next_position == std::string_view::npos)
      {
        if (file.size() - 1 > current_position)
        {
          next_position = file.size() - 1;
        }
        else
        {
          break;
        }
      }

      std::string_view line_view(file.data() + current_position, next_position - current_position);

      auto shift = line_view.data();
      for (double value = std::strtod(shift, &end);
           shift != end && shift - line_view.data() < line_view.size();
           value = std::strtod(shift, &end))
      {
        shift = end;

        if (value == HUGE_VALF)
        {
          std::cerr << "Invalid value in line " << line_number << " row " << row_number << std::endl;
          value = 0.0;
        }

        if (errno == ERANGE)
        {
          std::cout << "Range error in line " <<  line_number << " row " << row_number << std::endl;
          errno = 0;
          value = 0.0;
        }

        point_vector.emplace_back(value);
        ++row_number;
      }

      if (dimensions_number && dimensions_number != point_vector.size())
      {
        std::cerr << "Invalid line " << line_number << " length." << std::endl;
        point_vector.resize(dimensions_number, 0.0);
      }
      else if (!dimensions_number)
      {
        dimensions_number = point_vector.size();
      }

      row_number = 1;

      ++line_number;
    }

    if (line_number == 1 || clusters_number_optional.value() < 2)
    {
      return nullptr;
    }

    return
      std::make_unique<K_means_processor>(
        std::move(data_buffer),
        dimensions_number,
        line_number - 1,
        clusters_number_optional.value(),
        threads_number
      );
  }

  void print_result_to_file(const std::string& result_filename, const std::vector<K_means_processor::Cluster_result>& result)
  {
    std::vector<std::string> result_strings;
    result_strings.reserve(result.size());
    unsigned long long total_file_size = 0;

    for (const auto& cluster_result : result)
    {
      std::string cluster_string;
      if (!cluster_result._center.empty())
      {
        for (size_t i = 0; i < cluster_result._center.size() - 1; ++i)
        {
          cluster_string += std::to_string(cluster_result._center[i]) + ", ";
        }

        cluster_string += std::to_string(cluster_result._center.back()) + "\n";
      }

      cluster_string += "\n";

      total_file_size += cluster_string.size();
      result_strings.emplace_back(std::move(cluster_string));
    }

    {  //Create a file
      std::filebuf fbuf;
      fbuf.open(result_filename, std::ios_base::in | std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
      //Set the size
      fbuf.pubseekoff(total_file_size - 1, std::ios_base::beg);
      fbuf.sputc(0);
    }


    boost::iostreams::mapped_file_params params;
    params.path = result_filename;
    params.length = total_file_size;
    params.flags = boost::iostreams::mapped_file::mapmode::readwrite;

    try
    {
      boost::iostreams::stream<boost::iostreams::mapped_file_sink> out(params);

      copy(result_strings.begin(), result_strings.end(), std::ostream_iterator<std::string>(out, "\n"));
    }
    catch (const std::ios_base::failure& error)
    {
      std::cerr << "File writing error: " << error.what() << std::endl;
    }
  }
}
